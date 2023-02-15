// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_network_controller_chromeos.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace extensions {

namespace {

// Frequency at which networks should be scanned when not connected to a network
// or when connected to a non-preferred network.
const int kScanIntervalSec = 10;

void HandleEnableWifiError(const std::string& error_name) {
  LOG(WARNING) << "Unable to enable wifi: " << error_name;
}

// Returns a human-readable name for the network described by |network|.
std::string GetNetworkName(const ash::NetworkState& network) {
  return !network.name().empty()
             ? network.name()
             : base::StringPrintf("[%s]", network.type().c_str());
}

// Returns true if shill is either connected or connecting to a network.
bool IsConnectedOrConnecting() {
  ash::NetworkStateHandler* state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  return state_handler->ConnectedNetworkByType(
             ash::NetworkTypePattern::Default()) ||
         state_handler->ConnectingNetworkByType(
             ash::NetworkTypePattern::Default());
}

}  // namespace

ShellNetworkController::ShellNetworkController(
    const std::string& preferred_network_name)
    : state_(STATE_IDLE),
      preferred_network_name_(preferred_network_name),
      preferred_network_is_active_(false) {
  ash::NetworkStateHandler* state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  state_handler->AddObserver(this, FROM_HERE);
  ash::NetworkHandler::Get()
      ->technology_state_controller()
      ->SetTechnologiesEnabled(
          ash::NetworkTypePattern::Primitive(shill::kTypeWifi), true,
          base::BindRepeating(&HandleEnableWifiError));

  // If we're unconnected, trigger a connection attempt and start scanning.
  NetworkConnectionStateChanged(nullptr);
}

ShellNetworkController::~ShellNetworkController() {
  ash::NetworkHandler::Get()->network_state_handler()->RemoveObserver(
      this, FROM_HERE);
}

void ShellNetworkController::NetworkListChanged() {
  VLOG(1) << "Network list changed";
  ConnectIfUnconnected();
}

void ShellNetworkController::NetworkConnectionStateChanged(
    const ash::NetworkState* network) {
  if (network) {
    VLOG(1) << "Network connection state changed:"
            << " name=" << GetNetworkName(*network)
            << " type=" << network->type() << " path=" << network->path()
            << " state=" << network->connection_state();
  } else {
    VLOG(1) << "Network connection state changed: [none]";
  }

  const ash::NetworkState* wifi_network = GetActiveWiFiNetwork();
  preferred_network_is_active_ =
      wifi_network && wifi_network->name() == preferred_network_name_;
  VLOG(2) << "Active WiFi network is "
          << (wifi_network ? wifi_network->name() : std::string("[none]"));

  if (preferred_network_is_active_ ||
      (preferred_network_name_.empty() && wifi_network)) {
    SetScanningEnabled(false);
  } else {
    SetScanningEnabled(true);
    ConnectIfUnconnected();
  }
}

void ShellNetworkController::SetCellularAllowRoaming(bool allow_roaming) {
  ash::NetworkHandler* handler = ash::NetworkHandler::Get();
  ash::NetworkStateHandler::NetworkStateList network_list;

  base::Value::Dict properties;
  properties.Set(shill::kCellularAllowRoamingProperty, allow_roaming);

  handler->network_state_handler()->GetVisibleNetworkListByType(
      ash::NetworkTypePattern::Cellular(), &network_list);

  for (const ash::NetworkState* network : network_list) {
    handler->network_configuration_handler()->SetShillProperties(
        network->path(), properties, base::DoNothing(),
        ash::network_handler::ErrorCallback());
  }
}

const ash::NetworkState* ShellNetworkController::GetActiveWiFiNetwork() {
  ash::NetworkStateHandler* state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  const ash::NetworkState* network = state_handler->FirstNetworkByType(
      ash::NetworkTypePattern::Primitive(shill::kTypeWifi));
  return network &&
                 (network->IsConnectedState() || network->IsConnectingState())
             ? network
             : nullptr;
}

void ShellNetworkController::SetScanningEnabled(bool enabled) {
  const bool currently_enabled = scan_timer_.IsRunning();
  if (enabled == currently_enabled)
    return;

  VLOG(1) << (enabled ? "Starting" : "Stopping") << " scanning";
  if (enabled) {
    RequestScan();
    scan_timer_.Start(FROM_HERE, base::Seconds(kScanIntervalSec), this,
                      &ShellNetworkController::RequestScan);
  } else {
    scan_timer_.Stop();
  }
}

void ShellNetworkController::RequestScan() {
  VLOG(1) << "Requesting scan";
  ash::NetworkHandler::Get()->network_state_handler()->RequestScan(
      ash::NetworkTypePattern::Default());
}

void ShellNetworkController::ConnectIfUnconnected() {
  // Don't do anything if the default network is already the preferred one or if
  // we have a pending request to connect to it.
  if (preferred_network_is_active_ ||
      state_ == STATE_WAITING_FOR_PREFERRED_RESULT)
    return;

  const ash::NetworkState* best_network = nullptr;
  bool can_connect_to_preferred_network = false;

  ash::NetworkHandler* handler = ash::NetworkHandler::Get();
  ash::NetworkStateHandler::NetworkStateList network_list;
  handler->network_state_handler()->GetVisibleNetworkListByType(
      ash::NetworkTypePattern::WiFi(), &network_list);
  for (ash::NetworkStateHandler::NetworkStateList::const_iterator it =
           network_list.begin();
       it != network_list.end(); ++it) {
    const ash::NetworkState* network = *it;
    if (!network->connectable())
      continue;

    if (!preferred_network_name_.empty() &&
        network->name() == preferred_network_name_) {
      best_network = network;
      can_connect_to_preferred_network = true;
      break;
    } else if (!best_network) {
      best_network = network;
    }
  }

  // Don't switch networks if we're already connecting/connected and wouldn't be
  // switching to the preferred network.
  if ((IsConnectedOrConnecting() || state_ != STATE_IDLE) &&
      !can_connect_to_preferred_network)
    return;

  if (!best_network) {
    VLOG(1) << "Didn't find any connectable networks";
    return;
  }

  VLOG(1) << "Connecting to network " << GetNetworkName(*best_network)
          << " with path " << best_network->path() << " and strength "
          << best_network->signal_strength();
  state_ = can_connect_to_preferred_network
               ? STATE_WAITING_FOR_PREFERRED_RESULT
               : STATE_WAITING_FOR_NON_PREFERRED_RESULT;
  handler->network_connection_handler()->ConnectToNetwork(
      best_network->path(),
      base::BindOnce(&ShellNetworkController::HandleConnectionSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ShellNetworkController::HandleConnectionError,
                          weak_ptr_factory_.GetWeakPtr()),
      false /* check_error_state */, ash::ConnectCallbackMode::ON_COMPLETED);
}

void ShellNetworkController::HandleConnectionSuccess() {
  VLOG(1) << "Successfully connected to network";
  state_ = STATE_IDLE;
}

void ShellNetworkController::HandleConnectionError(
    const std::string& error_name) {
  LOG(WARNING) << "Unable to connect to network: " << error_name;
  state_ = STATE_IDLE;
}

}  // namespace extensions
