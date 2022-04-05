/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "binding_handler.h"
#ifdef CONFIG_CHIP_LIB_SHELL
#include "shell_commands.h"
#endif

#include <logging/log.h>
LOG_MODULE_DECLARE(app, CONFIG_MATTER_LOG_LEVEL);

using namespace chip;
using namespace chip::app;

void BindingHandler::Init()
{
#ifdef CONFIG_CHIP_LIB_SHELL
	SwitchCommands::RegisterSwitchCommands();
#endif
	DeviceLayer::PlatformMgr().ScheduleWork(InitInternal);
}

void BindingHandler::OnOffProcessCommand(CommandId aCommandId, const EmberBindingTableEntry &aBinding,
					 DeviceProxy *aDevice, void *aContext)
{
	CHIP_ERROR ret = CHIP_NO_ERROR;

	auto onSuccess = [](const ConcreteCommandPath &commandPath, const StatusIB &status, const auto &dataResponse) {
		LOG_DBG("Binding command applied successfully!");
	};

	auto onFailure = [](CHIP_ERROR error) {
		LOG_INF("Binding command was not applied! Reason: %" CHIP_ERROR_FORMAT, error.Format());
	};

	switch (aCommandId) {
	case Clusters::OnOff::Commands::Toggle::Id:
		Clusters::OnOff::Commands::Toggle::Type toggleCommand;
		if (aDevice) {
			ret = Controller::InvokeCommandRequest(aDevice->GetExchangeManager(),
							       aDevice->GetSecureSession().Value(), aBinding.remote,
							       toggleCommand, onSuccess, onFailure);
		} else {
			NodeId sourceNodeId = Server::GetInstance()
						      .GetFabricTable()
						      .FindFabricWithIndex(aBinding.fabricIndex)
						      ->GetNodeId();
			Messaging::ExchangeManager &exchangeMgr = Server::GetInstance().GetExchangeManager();
			ret = Controller::InvokeGroupCommandRequest(&exchangeMgr, aBinding.fabricIndex,
								    aBinding.groupId, sourceNodeId, toggleCommand);
		}
		break;

	case Clusters::OnOff::Commands::On::Id:
		Clusters::OnOff::Commands::On::Type onCommand;
		if (aDevice) {
			ret = Controller::InvokeCommandRequest(aDevice->GetExchangeManager(),
							       aDevice->GetSecureSession().Value(), aBinding.remote,
							       onCommand, onSuccess, onFailure);
		} else {
			NodeId sourceNodeId = Server::GetInstance()
						      .GetFabricTable()
						      .FindFabricWithIndex(aBinding.fabricIndex)
						      ->GetNodeId();
			Messaging::ExchangeManager &exchangeMgr = Server::GetInstance().GetExchangeManager();
			ret = Controller::InvokeGroupCommandRequest(&exchangeMgr, aBinding.fabricIndex,
								    aBinding.groupId, sourceNodeId, onCommand);
		}
		break;

	case Clusters::OnOff::Commands::Off::Id:
		Clusters::OnOff::Commands::Off::Type offCommand;
		if (aDevice) {
			ret = Controller::InvokeCommandRequest(aDevice->GetExchangeManager(),
							       aDevice->GetSecureSession().Value(), aBinding.remote,
							       offCommand, onSuccess, onFailure);
		} else {
			NodeId sourceNodeId = Server::GetInstance()
						      .GetFabricTable()
						      .FindFabricWithIndex(aBinding.fabricIndex)
						      ->GetNodeId();
			Messaging::ExchangeManager &exchangeMgr = Server::GetInstance().GetExchangeManager();
			ret = Controller::InvokeGroupCommandRequest(&exchangeMgr, aBinding.fabricIndex,
								    aBinding.groupId, sourceNodeId, onCommand);
		}
		break;
	default:
		LOG_DBG("Invalid binding command data - commandId is not supported");
		break;
	}
	if (CHIP_NO_ERROR != ret) {
		LOG_ERR("Invoke Unicast Command Request ERROR: %s", ErrorStr(ret));
	}
}

void BindingHandler::LevelControlProcessCommand(CommandId aCommandId, const EmberBindingTableEntry &aBinding,
						DeviceProxy *aDevice, void *aContext)
{
	auto onSuccess = [](const ConcreteCommandPath &commandPath, const StatusIB &status, const auto &dataResponse) {
		LOG_DBG("Binding command applied successfully!");
	};

	auto onFailure = [](CHIP_ERROR error) {
		LOG_INF("Binding command was not applied! Reason: %" CHIP_ERROR_FORMAT, error.Format());
	};

	CHIP_ERROR ret = CHIP_NO_ERROR;

	switch (aCommandId) {
	case Clusters::LevelControl::Commands::MoveToLevel::Id: {
		Clusters::LevelControl::Commands::MoveToLevel::Type moveToLevelCommand;
		BindingData *data = reinterpret_cast<BindingData *>(aContext);
		moveToLevelCommand.level = data->Value;
		if (aDevice) {
			ret = Controller::InvokeCommandRequest(aDevice->GetExchangeManager(),
							       aDevice->GetSecureSession().Value(), aBinding.remote,
							       moveToLevelCommand, onSuccess, onFailure);
		} else {
			NodeId sourceNodeId = Server::GetInstance()
						      .GetFabricTable()
						      .FindFabricWithIndex(aBinding.fabricIndex)
						      ->GetNodeId();
			Messaging::ExchangeManager &exchangeMgr = Server::GetInstance().GetExchangeManager();
			ret = Controller::InvokeGroupCommandRequest(&exchangeMgr, aBinding.fabricIndex,
								    aBinding.groupId, sourceNodeId, moveToLevelCommand);
		}
	} break;
	default:
		LOG_DBG("Invalid binding command data - commandId is not supported");
		break;
	}
	if (CHIP_NO_ERROR != ret) {
		LOG_ERR("Invoke Group Command Request ERROR: %s", ErrorStr(ret));
	}
}

void BindingHandler::LightSwitchChangedHandler(const EmberBindingTableEntry &binding, DeviceProxy *deviceProxy,
					       void *context)
{
	VerifyOrReturn(context != nullptr, LOG_ERR("Invalid context for Light switch handler"););
	BindingData *data = static_cast<BindingData *>(context);

	if (binding.type == EMBER_MULTICAST_BINDING && data->IsGroup) {
		switch (data->ClusterId) {
		case Clusters::OnOff::Id:
			OnOffProcessCommand(data->CommandId, binding, nullptr, context);
			break;
		case Clusters::LevelControl::Id:
			LevelControlProcessCommand(data->CommandId, binding, nullptr, context);
			break;
		default:
			ChipLogError(NotSpecified, "Invalid binding group command data");
			break;
		}
	} else if (binding.type == EMBER_UNICAST_BINDING && !data->IsGroup) {
		switch (data->ClusterId) {
		case Clusters::OnOff::Id:
			OnOffProcessCommand(data->CommandId, binding, deviceProxy, context);
			break;
		case Clusters::LevelControl::Id:
			LevelControlProcessCommand(data->CommandId, binding, deviceProxy, context);
			break;
		default:
			ChipLogError(NotSpecified, "Invalid binding unicast command data");
			break;
		}
	}
}

void BindingHandler::InitInternal(intptr_t aArg)
{
	LOG_INF("Initialize binding Handler");
	auto &server = Server::GetInstance();
	if (CHIP_NO_ERROR !=
	    BindingManager::GetInstance().Init(
		    { &server.GetFabricTable(), server.GetCASESessionManager(), &server.GetPersistentStorage() })) {
		LOG_ERR("BindingHandler::InitInternal failed");
	}

	BindingManager::GetInstance().RegisterBoundDeviceChangedHandler(LightSwitchChangedHandler);
	PrintBindingTable();
}

bool BindingHandler::IsGroupBound()
{
	BindingTable &bindingTable = BindingTable::GetInstance();

	for (auto &entry : bindingTable) {
		if (EMBER_MULTICAST_BINDING == entry.type) {
			return true;
		}
	}
	return false;
}

void BindingHandler::PrintBindingTable()
{
	BindingTable &bindingTable = BindingTable::GetInstance();

	LOG_INF("Binding Table size: [%d]:", bindingTable.Size());
	uint8_t i = 0;
	for (auto &entry : bindingTable) {
		switch (entry.type) {
		case EMBER_UNICAST_BINDING:
			LOG_INF("[%d] UNICAST:", i++);
			LOG_INF("\t\t+ Fabric: %d\n \
            \t+ LocalEndpoint %d \n \
            \t+ ClusterId %d \n \
            \t+ RemoteEndpointId %d \n \
            \t+ NodeId %d",
				(int)entry.fabricIndex, (int)entry.local, (int)entry.clusterId.Value(),
				(int)entry.remote, (int)entry.nodeId);
			break;
		case EMBER_MULTICAST_BINDING:
			LOG_INF("[%d] GROUP:", i++);
			LOG_INF("\t\t+ Fabric: %d\n \
            \t+ LocalEndpoint %d \n \
            \t+ RemoteEndpointId %d \n \
            \t+ GroupId %d",
				(int)entry.fabricIndex, (int)entry.local, (int)entry.remote, (int)entry.groupId);
			break;
		case EMBER_UNUSED_BINDING:
			LOG_INF("[%d] UNUSED", i++);
			break;
		case EMBER_MANY_TO_ONE_BINDING:
			LOG_INF("[%d] MANY TO ONE", i++);
			break;
		default:
			break;
		}
	}
}

void BindingHandler::SwitchWorkerHandler(intptr_t aContext)
{
	VerifyOrReturn(aContext != 0, LOG_ERR("Invalid Swich data"));

	BindingData *data = reinterpret_cast<BindingData *>(aContext);
	LOG_INF("Notify Bounded Cluster | endpoint: %d cluster: %d", data->EndpointId, data->ClusterId);
	BindingManager::GetInstance().NotifyBoundClusterChanged(data->EndpointId, data->ClusterId,
								static_cast<void *>(data));

	Platform::Delete(data);
}