// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_chromeos.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_certificate_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/onc/onc_translator.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"

using ::ash::NetworkCertificateHandler;
using ::ash::NetworkHandler;
using ::ash::NetworkStateHandler;
using ::ash::NetworkTypePattern;
using ::ash::TechnologyStateController;
using extensions::NetworkingPrivateDelegate;

namespace private_api = extensions::api::networking_private;

namespace {

NetworkStateHandler* GetStateHandler() {
  return NetworkHandler::Get()->network_state_handler();
}

TechnologyStateController* GetTechnologyStateController() {
  return NetworkHandler::Get()->technology_state_controller();
}

ash::ManagedNetworkConfigurationHandler* GetManagedConfigurationHandler() {
  return NetworkHandler::Get()->managed_network_configuration_handler();
}

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path,
                            std::string* error) {
  const ash::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    *error = extensions::networking_private::kErrorInvalidNetworkGuid;
    return false;
  }
  *service_path = network->path();
  return true;
}

bool IsSharedNetwork(const std::string& service_path) {
  const ash::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromServicePath(
          service_path, true /* configured only */);
  if (!network) {
    return false;
  }

  return !network->IsPrivate();
}

bool GetPrimaryUserIdHash(content::BrowserContext* browser_context,
                          std::string* user_hash,
                          std::string* error) {
  std::string context_user_hash =
      extensions::ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          browser_context);

  // Currently Chrome OS only configures networks for the primary user.
  // Configuration attempts from other browser contexts should fail.
  if (context_user_hash != ash::LoginState::Get()->primary_user_hash()) {
    // Disallow class requiring a user id hash from a non-primary user context
    // to avoid complexities with the policy code.
    LOG(ERROR) << "networkingPrivate API call from non primary user: "
               << context_user_hash;
    if (error) {
      *error = "Error.NonPrimaryUser";
    }
    return false;
  }
  if (user_hash) {
    *user_hash = context_user_hash;
  }
  return true;
}

void AppendDeviceState(
    const std::string& type,
    const ash::DeviceState* device,
    NetworkingPrivateDelegate::DeviceStateList& device_state_list) {
  DCHECK(!type.empty());
  NetworkTypePattern pattern = ash::onc::NetworkTypePatternFromOncType(type);
  NetworkStateHandler::TechnologyState technology_state =
      GetStateHandler()->GetTechnologyState(pattern);
  private_api::DeviceStateType state = private_api::DeviceStateType::kNone;
  switch (technology_state) {
    case NetworkStateHandler::TECHNOLOGY_UNAVAILABLE:
      if (!device) {
        return;
      }
      // If we have a DeviceState entry but the technology is not available,
      // assume the technology is not initialized.
      state = private_api::DeviceStateType::kUninitialized;
      break;
    case NetworkStateHandler::TECHNOLOGY_AVAILABLE:
      state = private_api::DeviceStateType::kDisabled;
      break;
    case NetworkStateHandler::TECHNOLOGY_DISABLING:
      state = private_api::DeviceStateType::kDisabled;
      break;
    case NetworkStateHandler::TECHNOLOGY_UNINITIALIZED:
      state = private_api::DeviceStateType::kUninitialized;
      break;
    case NetworkStateHandler::TECHNOLOGY_ENABLING:
      state = private_api::DeviceStateType::kEnabling;
      break;
    case NetworkStateHandler::TECHNOLOGY_ENABLED:
      state = private_api::DeviceStateType::kEnabled;
      break;
    case NetworkStateHandler::TECHNOLOGY_PROHIBITED:
      state = private_api::DeviceStateType::kProhibited;
      break;
  }
  DCHECK_NE(private_api::DeviceStateType::kNone, state);
  private_api::DeviceStateProperties& properties =
      device_state_list.emplace_back();
  properties.type = private_api::ParseNetworkType(type);
  properties.state = state;
  if (device && state == private_api::DeviceStateType::kEnabled) {
    properties.scanning = device->scanning();
  }
  if (device && type == ::onc::network_config::kCellular) {
    bool sim_present = !device->IsSimAbsent();
    properties.sim_present = sim_present;
    if (sim_present) {
      properties.sim_lock_status.emplace();
      properties.sim_lock_status->lock_enabled = device->sim_lock_enabled();
      properties.sim_lock_status->lock_type = device->sim_lock_type();
      properties.sim_lock_status->retries_left = device->sim_retries_left();
    }
  }
  if (device && type == ::onc::network_config::kWiFi) {
    properties.managed_network_available =
        GetStateHandler()->GetAvailableManagedWifiNetwork();
  }
}

void NetworkHandlerFailureCallback(
    NetworkingPrivateDelegate::FailureCallback callback,
    const std::string& error_name) {
  std::move(callback).Run(error_name);
}

// Returns the string corresponding to |key|. If the property is a managed
// dictionary, returns the active value. If the property does not exist or
// has no active value, returns an empty string.
std::string GetStringFromDictionary(const base::Value::Dict& dictionary,
                                    const std::string& key) {
  const std::string* result = dictionary.FindString(key);
  if (result) {
    return *result;
  }
  const base::Value::Dict* managed = dictionary.FindDict(key);
  if (managed) {
    result = managed->FindString(::onc::kAugmentationActiveSetting);
  }
  return result ? *result : std::string();
}

base::Value::Dict* GetThirdPartyVPNDictionary(base::Value::Dict* dictionary) {
  const std::string type =
      GetStringFromDictionary(*dictionary, ::onc::network_config::kType);
  if (type != ::onc::network_config::kVPN) {
    return nullptr;
  }
  base::Value::Dict* vpn_dict =
      dictionary->FindDict(::onc::network_config::kVPN);
  if (!vpn_dict) {
    return nullptr;
  }
  if (GetStringFromDictionary(*vpn_dict, ::onc::vpn::kType) !=
      ::onc::vpn::kThirdPartyVpn) {
    return nullptr;
  }
  base::Value::Dict* third_party_vpn =
      dictionary->FindDict(::onc::vpn::kThirdPartyVpn);
  return third_party_vpn;
}

const ash::DeviceState* GetCellularDeviceState(const std::string& guid) {
  const ash::NetworkState* network_state = nullptr;
  if (!guid.empty()) {
    network_state = GetStateHandler()->GetNetworkStateFromGuid(guid);
  }
  const ash::DeviceState* device_state = nullptr;
  if (network_state) {
    device_state =
        GetStateHandler()->GetDeviceState(network_state->device_path());
  }
  if (!device_state) {
    device_state =
        GetStateHandler()->GetDeviceStateByType(NetworkTypePattern::Cellular());
  }
  return device_state;
}

private_api::Certificate GetCertDictionary(
    const NetworkCertificateHandler::Certificate& cert) {
  private_api::Certificate api_cert;
  api_cert.hash = cert.hash;
  api_cert.issued_by = cert.issued_by;
  api_cert.issued_to = cert.issued_to;
  api_cert.hardware_backed = cert.hardware_backed;
  api_cert.device_wide = cert.device_wide;
  if (!cert.pem.empty()) {
    api_cert.pem = cert.pem;
  }
  if (!cert.pkcs11_id.empty()) {
    api_cert.pkcs11_id = cert.pkcs11_id;
  }
  return api_cert;
}

constexpr char kCaptivePortalStatusUnknown[] = "Unknown";
constexpr char kCaptivePortalStatusOffline[] = "Offline";
constexpr char kCaptivePortalStatusOnline[] = "Online";
constexpr char kCaptivePortalStatusPortal[] = "Portal";
constexpr char kCaptivePortalStatusUnrecognized[] = "Unrecognized";

// This returns backwards compatible strings previously provided by
// NetworkPortalDetector.
// static
std::string PortalStatusString(ash::NetworkState::PortalState portal_state) {
  using PortalState = ash::NetworkState::PortalState;
  switch (portal_state) {
    case PortalState::kUnknown:
      return kCaptivePortalStatusUnknown;
    case PortalState::kOnline:
      return kCaptivePortalStatusOnline;
    case PortalState::kPortalSuspected:
    case PortalState::kPortal:
    case PortalState::kNoInternet:
      return kCaptivePortalStatusPortal;
  }
  return kCaptivePortalStatusUnrecognized;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

namespace extensions {

NetworkingPrivateChromeOS::NetworkingPrivateChromeOS(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

NetworkingPrivateChromeOS::~NetworkingPrivateChromeOS() = default;

void NetworkingPrivateChromeOS::GetProperties(const std::string& guid,
                                              PropertiesCallback callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    NET_LOG(ERROR) << "GetProperties failed: " << error;
    std::move(callback).Run(std::nullopt, error);
    return;
  }

  std::string user_id_hash;
  if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
    NET_LOG(ERROR) << "GetProperties failed: " << error;
    std::move(callback).Run(std::nullopt, error);
    return;
  }

  GetManagedConfigurationHandler()->GetProperties(
      user_id_hash, service_path,
      base::BindOnce(&NetworkingPrivateChromeOS::GetPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr(), guid,
                     std::move(callback)));
}

void NetworkingPrivateChromeOS::GetManagedProperties(
    const std::string& guid,
    PropertiesCallback callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    NET_LOG(ERROR) << "GetManagedProperties failed: " << error;
    std::move(callback).Run(std::nullopt, error);
    return;
  }

  std::string user_id_hash;
  if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
    NET_LOG(ERROR) << "GetManagedProperties failed: " << error;
    std::move(callback).Run(std::nullopt, error);
    return;
  }

  GetManagedConfigurationHandler()->GetManagedProperties(
      user_id_hash, service_path,
      base::BindOnce(&NetworkingPrivateChromeOS::GetPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr(), guid,
                     std::move(callback)));
}

void NetworkingPrivateChromeOS::GetState(const std::string& guid,
                                         DictionaryCallback success_callback,
                                         FailureCallback failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    std::move(failure_callback).Run(error);
    return;
  }

  const ash::NetworkState* network_state =
      GetStateHandler()->GetNetworkStateFromServicePath(
          service_path, false /* configured_only */);
  if (!network_state) {
    std::move(failure_callback)
        .Run(networking_private::kErrorNetworkUnavailable);
    return;
  }

  base::Value::Dict network_properties =
      ash::network_util::TranslateNetworkStateToONC(network_state);
  AppendThirdPartyProviderName(&network_properties);

  std::move(success_callback).Run(std::move(network_properties));
}

void NetworkingPrivateChromeOS::SetProperties(
    const std::string& guid,
    base::Value::Dict properties,
    bool allow_set_shared_config,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  const ash::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    std::move(failure_callback)
        .Run(extensions::networking_private::kErrorInvalidNetworkGuid);
    return;
  }
  if (network->profile_path().empty()) {
    std::move(failure_callback)
        .Run(extensions::networking_private::kErrorUnconfiguredNetwork);
    return;
  }
  if (IsSharedNetwork(network->path())) {
    if (!allow_set_shared_config) {
      std::move(failure_callback)
          .Run(networking_private::kErrorAccessToSharedConfig);
      return;
    }
  } else {
    std::string user_id_hash;
    std::string error;
    // Do not allow changing a non-shared network from secondary users.
    if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
      std::move(failure_callback).Run(error);
      return;
    }
  }

  NET_LOG(USER) << "networkingPrivate.setProperties for: "
                << NetworkId(network);
  GetManagedConfigurationHandler()->SetProperties(
      network->path(), properties, std::move(success_callback),
      base::BindOnce(&NetworkHandlerFailureCallback,
                     std::move(failure_callback)));
}

void NetworkHandlerCreateCallback(
    NetworkingPrivateDelegate::StringCallback callback,
    const std::string& service_path,
    const std::string& guid) {
  std::move(callback).Run(guid);
}

void NetworkingPrivateChromeOS::CreateNetwork(
    bool shared,
    base::Value::Dict properties,
    StringCallback success_callback,
    FailureCallback failure_callback) {
  std::string user_id_hash, error;
  // Do not allow configuring a non-shared network from a non-primary user.
  if (!shared &&
      !GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
    std::move(failure_callback).Run(error);
    return;
  }

  const std::string guid =
      GetStringFromDictionary(properties, ::onc::network_config::kGUID);
  NET_LOG(USER) << "networkingPrivate.CreateNetwork. GUID=" << guid;
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, properties,
      base::BindOnce(&NetworkHandlerCreateCallback,
                     std::move(success_callback)),
      base::BindOnce(&NetworkHandlerFailureCallback,
                     std::move(failure_callback)));
}

void NetworkingPrivateChromeOS::ForgetNetwork(
    const std::string& guid,
    bool allow_forget_shared_config,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    std::move(failure_callback).Run(error);
    return;
  }

  const ash::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromServicePath(
          service_path, true /* configured only */);
  if (!network) {
    std::move(failure_callback)
        .Run(networking_private::kErrorNetworkUnavailable);
    return;
  }

  std::string user_id_hash;
  // Don't allow non-primary user to remove private configs - the private
  // configs belong to the primary user (non-primary users' network configs
  // never get loaded by shill).
  if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error) &&
      network->IsPrivate()) {
    std::move(failure_callback).Run(error);
    return;
  }

  if (!allow_forget_shared_config && !network->IsPrivate()) {
    std::move(failure_callback)
        .Run(networking_private::kErrorAccessToSharedConfig);
    return;
  }

  onc::ONCSource onc_source = onc::ONC_SOURCE_UNKNOWN;
  if (GetManagedConfigurationHandler()->FindPolicyByGUID(user_id_hash, guid,
                                                         &onc_source)) {
    // Prevent a policy controlled configuration removal.
    if (onc_source == onc::ONC_SOURCE_DEVICE_POLICY) {
      allow_forget_shared_config = false;
    } else {
      std::move(failure_callback)
          .Run(networking_private::kErrorPolicyControlled);
      return;
    }
  }

  if (allow_forget_shared_config) {
    GetManagedConfigurationHandler()->RemoveConfiguration(
        service_path, std::move(success_callback),
        base::BindOnce(&NetworkHandlerFailureCallback,
                       std::move(failure_callback)));
  } else {
    GetManagedConfigurationHandler()->RemoveConfigurationFromCurrentProfile(
        service_path, std::move(success_callback),
        base::BindOnce(&NetworkHandlerFailureCallback,
                       std::move(failure_callback)));
  }
}

void NetworkingPrivateChromeOS::GetNetworks(
    const std::string& network_type,
    bool configured_only,
    bool visible_only,
    int limit,
    NetworkListCallback success_callback,
    FailureCallback failure_callback) {
  // When requesting configured Ethernet networks, include EthernetEAP.
  NetworkTypePattern pattern =
      (!visible_only && network_type == ::onc::network_type::kEthernet)
          ? NetworkTypePattern::EthernetOrEthernetEAP()
          : ash::onc::NetworkTypePatternFromOncType(network_type);
  base::Value::List network_properties_list =
      ash::network_util::TranslateNetworkListToONC(pattern, configured_only,
                                                   visible_only, limit);

  for (auto& value : network_properties_list) {
    base::Value::Dict& value_dict = value.GetDict();
    if (GetThirdPartyVPNDictionary(&value_dict)) {
      AppendThirdPartyProviderName(&value_dict);
    }
  }

  std::move(success_callback).Run(std::move(network_properties_list));
}

void NetworkingPrivateChromeOS::StartConnect(const std::string& guid,
                                             VoidCallback success_callback,
                                             FailureCallback failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    std::move(failure_callback).Run(error);
    return;
  }

  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path, std::move(success_callback),
      base::BindOnce(&NetworkHandlerFailureCallback,
                     std::move(failure_callback)),
      true /* check_error_state */, ash::ConnectCallbackMode::ON_STARTED);
}

void NetworkingPrivateChromeOS::StartDisconnect(
    const std::string& guid,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    std::move(failure_callback).Run(error);
    return;
  }

  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      service_path, std::move(success_callback),
      base::BindOnce(&NetworkHandlerFailureCallback,
                     std::move(failure_callback)));
}

void NetworkingPrivateChromeOS::StartActivate(
    const std::string& guid,
    const std::string& specified_carrier,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  const ash::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    std::move(failure_callback)
        .Run(extensions::networking_private::kErrorInvalidNetworkGuid);
    return;
  }

  if (ui_delegate()) {
    ui_delegate()->ShowAccountDetails(guid);
  }
  std::move(success_callback).Run();
}

void NetworkingPrivateChromeOS::GetCaptivePortalStatus(
    const std::string& guid,
    StringCallback success_callback,
    FailureCallback failure_callback) {
  const ash::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    std::move(failure_callback)
        .Run(extensions::networking_private::kErrorInvalidNetworkGuid);
    return;
  }
  if (!network->IsConnectedState()) {
    std::move(success_callback).Run(kCaptivePortalStatusOffline);
    return;
  }
  std::move(success_callback)
      .Run(PortalStatusString(network->GetPortalState()));
}

void NetworkingPrivateChromeOS::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  const ash::DeviceState* device_state = GetCellularDeviceState(guid);
  if (!device_state) {
    std::move(failure_callback)
        .Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  std::string lock_type = device_state->sim_lock_type();
  if (lock_type.empty()) {
    // Sim is already unlocked.
    std::move(failure_callback)
        .Run(networking_private::kErrorInvalidNetworkOperation);
    return;
  }

  // Unblock or unlock the SIM.
  if (lock_type == shill::kSIMLockPuk) {
    NetworkHandler::Get()->network_device_handler()->UnblockPin(
        device_state->path(), puk, pin, std::move(success_callback),
        base::BindOnce(&NetworkHandlerFailureCallback,
                       std::move(failure_callback)));
  } else {
    NetworkHandler::Get()->network_device_handler()->EnterPin(
        device_state->path(), pin, std::move(success_callback),
        base::BindOnce(&NetworkHandlerFailureCallback,
                       std::move(failure_callback)));
  }
}

void NetworkingPrivateChromeOS::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  const ash::DeviceState* device_state = GetCellularDeviceState(guid);
  if (!device_state) {
    std::move(failure_callback)
        .Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  if (!device_state->sim_lock_type().empty()) {
    // The SIM needs to be unlocked before the state can be changed.
    std::move(failure_callback).Run(networking_private::kErrorSimLocked);
    return;
  }

  // TODO(benchan): Add more checks to validate the parameters of this method
  // and the state of the SIM lock on the cellular device. Consider refactoring
  // some of the code by moving the logic into shill instead.

  // If |new_pin| is empty, we're trying to enable (require_pin == true) or
  // disable (require_pin == false) SIM locking.
  if (new_pin.empty()) {
    NetworkHandler::Get()->network_device_handler()->RequirePin(
        device_state->path(), require_pin, current_pin,
        std::move(success_callback),
        base::BindOnce(&NetworkHandlerFailureCallback,
                       std::move(failure_callback)));
    return;
  }

  // Otherwise, we're trying to change the PIN from |current_pin| to
  // |new_pin|, which also requires SIM locking to be enabled, i.e.
  // require_pin == true.
  if (!require_pin) {
    std::move(failure_callback).Run(networking_private::kErrorInvalidArguments);
    return;
  }

  NetworkHandler::Get()->network_device_handler()->ChangePin(
      device_state->path(), current_pin, new_pin, std::move(success_callback),
      base::BindOnce(&NetworkHandlerFailureCallback,
                     std::move(failure_callback)));
}

void NetworkingPrivateChromeOS::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  const ash::DeviceState* device_state = GetCellularDeviceState(guid);
  if (!device_state) {
    std::move(failure_callback)
        .Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  NetworkHandler::Get()->network_device_handler()->RegisterCellularNetwork(
      device_state->path(), network_id, std::move(success_callback),
      base::BindOnce(&NetworkHandlerFailureCallback,
                     std::move(failure_callback)));
}

void NetworkingPrivateChromeOS::GetEnabledNetworkTypes(
    EnabledNetworkTypesCallback callback) {
  NetworkStateHandler* state_handler = GetStateHandler();

  base::Value::List network_list;

  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Ethernet())) {
    network_list.Append(::onc::network_type::kEthernet);
  }
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::WiFi())) {
    network_list.Append(::onc::network_type::kWiFi);
  }
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Cellular())) {
    network_list.Append(::onc::network_type::kCellular);
  }

  std::move(callback).Run(std::move(network_list));
}

void NetworkingPrivateChromeOS::GetDeviceStateList(
    DeviceStateListCallback callback) {
  std::set<std::string> technologies_found;
  NetworkStateHandler::DeviceStateList devices;
  NetworkHandler::Get()->network_state_handler()->GetDeviceList(&devices);

  DeviceStateList device_state_list;
  for (const ash::DeviceState* device : devices) {
    std::string onc_type =
        ash::network_util::TranslateShillTypeToONC(device->type());
    AppendDeviceState(onc_type, device, device_state_list);
    technologies_found.insert(onc_type);
  }

  // For any technologies that we do not have a DeviceState entry for, append
  // an entry if the technology is available.
  const char* technology_types[] = {::onc::network_type::kEthernet,
                                    ::onc::network_type::kWiFi,
                                    ::onc::network_type::kCellular};
  for (const char* technology : technology_types) {
    if (base::Contains(technologies_found, technology)) {
      continue;
    }
    AppendDeviceState(technology, nullptr /* device */, device_state_list);
  }
  std::move(callback).Run(std::move(device_state_list));
}

void NetworkingPrivateChromeOS::GetGlobalPolicy(
    GetGlobalPolicyCallback callback) {
  base::Value::Dict result;
  const base::Value::Dict* global_network_config =
      GetManagedConfigurationHandler()->GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);

  if (global_network_config) {
    result.Merge(global_network_config->Clone());
  }
  std::move(callback).Run(std::move(result));
}

void NetworkingPrivateChromeOS::GetCertificateLists(
    GetCertificateListsCallback callback) {
  private_api::CertificateLists result;
  const std::vector<NetworkCertificateHandler::Certificate>& server_cas =
      NetworkHandler::Get()
          ->network_certificate_handler()
          ->server_ca_certificates();
  for (const auto& cert : server_cas) {
    result.server_ca_certificates.push_back(GetCertDictionary(cert));
  }

  std::vector<private_api::Certificate> user_cert_list;
  const std::vector<NetworkCertificateHandler::Certificate>& user_certs =
      NetworkHandler::Get()
          ->network_certificate_handler()
          ->client_certificates();
  for (const auto& cert : user_certs) {
    result.user_certificates.push_back(GetCertDictionary(cert));
  }
  std::move(callback).Run(result.ToValue());
}

void NetworkingPrivateChromeOS::EnableNetworkType(const std::string& type,
                                                  BoolCallback callback) {
  NetworkTypePattern pattern = ash::onc::NetworkTypePatternFromOncType(type);

  NET_LOG(USER) << __func__ << ":" << type;
  GetTechnologyStateController()->SetTechnologiesEnabled(
      pattern, true, ash::network_handler::ErrorCallback());

  std::move(callback).Run(true);
}

void NetworkingPrivateChromeOS::DisableNetworkType(const std::string& type,
                                                   BoolCallback callback) {
  NetworkTypePattern pattern = ash::onc::NetworkTypePatternFromOncType(type);

  NET_LOG(USER) << __func__ << ":" << type;
  GetTechnologyStateController()->SetTechnologiesEnabled(
      pattern, false, ash::network_handler::ErrorCallback());

  std::move(callback).Run(true);
}

void NetworkingPrivateChromeOS::RequestScan(const std::string& type,
                                            BoolCallback callback) {
  NetworkTypePattern pattern = ash::onc::NetworkTypePatternFromOncType(
      type.empty() ? ::onc::network_type::kAllTypes : type);
  GetStateHandler()->RequestScan(pattern);

  std::move(callback).Run(true);
}

// Private methods

void NetworkingPrivateChromeOS::GetPropertiesCallback(
    const std::string& guid,
    PropertiesCallback callback,
    const std::string& service_path,
    std::optional<base::Value::Dict> dictionary,
    std::optional<std::string> error) {
  if (dictionary) {
    AppendThirdPartyProviderName(&dictionary.value());
  }
  std::move(callback).Run(std::move(dictionary), std::move(error));
}

void NetworkingPrivateChromeOS::AppendThirdPartyProviderName(
    base::Value::Dict* dictionary) {
  base::Value::Dict* third_party_vpn = GetThirdPartyVPNDictionary(dictionary);
  if (!third_party_vpn) {
    return;
  }

  const ExtensionId extension_id = GetStringFromDictionary(
      *third_party_vpn, ::onc::third_party_vpn::kExtensionID);
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  for (const auto& extension : extensions) {
    if (extension->permissions_data()->HasAPIPermission(
            mojom::APIPermissionID::kVpnProvider) &&
        extension->id() == extension_id) {
      third_party_vpn->Set(::onc::third_party_vpn::kProviderName,
                           extension->name());
      break;
    }
  }
}

}  // namespace extensions
