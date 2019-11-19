// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_config/networking_config_service.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "extensions/common/api/networking_config.h"

namespace extensions {

namespace {

bool IsValidNonEmptyHexString(const std::string& input) {
  size_t count = input.size();
  if (count == 0 || (count % 2) != 0)
    return false;
  for (const char& c : input)
    if (!base::IsHexDigit<char>(c))
      return false;
  return true;
}

std::string LookUpExtensionName(content::BrowserContext* context,
                                std::string extension_id) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(context);
  DCHECK(extension_registry);
  const extensions::Extension* extension = extension_registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  if (extension == nullptr)
    return std::string();
  return extension->name();
}

}  // namespace

NetworkingConfigService::AuthenticationResult::AuthenticationResult()
    : authentication_state(NetworkingConfigService::NOTRY) {
}

NetworkingConfigService::AuthenticationResult::AuthenticationResult(
    ExtensionId extension_id,
    std::string guid,
    AuthenticationState authentication_state)
    : extension_id(extension_id),
      guid(guid),
      authentication_state(authentication_state) {
}

NetworkingConfigService::NetworkingConfigService(
    content::BrowserContext* browser_context,
    std::unique_ptr<EventDelegate> event_delegate,
    ExtensionRegistry* extension_registry)
    : browser_context_(browser_context),
      event_delegate_(std::move(event_delegate)) {
  registry_observer_.Add(extension_registry);
}

NetworkingConfigService::~NetworkingConfigService() = default;

void NetworkingConfigService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  UnregisterExtension(extension->id());
}

std::string NetworkingConfigService::LookupExtensionIdForHexSsid(
    std::string hex_ssid) const {
  // Transform hex_ssid to uppercase.
  transform(hex_ssid.begin(), hex_ssid.end(), hex_ssid.begin(), toupper);

  const auto it = hex_ssid_to_extension_id_.find(hex_ssid);
  if (it == hex_ssid_to_extension_id_.end())
    return std::string();
  return it->second;
}

bool NetworkingConfigService::IsRegisteredForCaptivePortalEvent(
    const std::string& extension_id) const {
  return event_delegate_->HasExtensionRegisteredForEvent(extension_id);
}

bool NetworkingConfigService::RegisterHexSsid(std::string hex_ssid,
                                              const std::string& extension_id) {
  if (!IsValidNonEmptyHexString(hex_ssid)) {
    LOG(ERROR) << "\'" << hex_ssid << "\' is not a valid hex encoded string.";
    return false;
  }

  // Transform hex_ssid to uppercase.
  transform(hex_ssid.begin(), hex_ssid.end(), hex_ssid.begin(), toupper);

  // If |hex_ssid| is already in the map, i.e. if a hex ssid is already
  // registered, this call should fail. TODO(stevenjb): Return an error code so
  // that the extension API can respond with a better error.
  if (!hex_ssid_to_extension_id_.insert(make_pair(hex_ssid, extension_id))
           .second) {
    LOG(ERROR) << "\'" << hex_ssid << "\' is already registered.";
    return false;
  }

  chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->SetCaptivePortalProviderForHexSsid(
          hex_ssid, extension_id,
          LookUpExtensionName(browser_context_, extension_id));
  return true;
}

void NetworkingConfigService::UnregisterExtension(
    const std::string& extension_id) {
  for (auto it = hex_ssid_to_extension_id_.begin();
       it != hex_ssid_to_extension_id_.end();) {
    if (it->second == extension_id) {
      chromeos::NetworkHandler::Get()
          ->network_state_handler()
          ->SetCaptivePortalProviderForHexSsid(it->first, "", "");
      it = hex_ssid_to_extension_id_.erase(it);
    } else {
      ++it;
    }
  }
}

const NetworkingConfigService::AuthenticationResult&
NetworkingConfigService::GetAuthenticationResult() const {
  return authentication_result_;
}

void NetworkingConfigService::ResetAuthenticationResult() {
  authentication_result_ = AuthenticationResult();
  authentication_callback_.Reset();
}

void NetworkingConfigService::SetAuthenticationResult(
    const AuthenticationResult& authentication_result) {
  authentication_result_ = authentication_result;
  if (authentication_result.authentication_state != SUCCESS) {
    LOG(WARNING) << "Captive Portal Authentication failed.";
    return;
  }

  if (!authentication_callback_.is_null()) {
    authentication_callback_.Run();
    authentication_callback_.Reset();
  }
}

void NetworkingConfigService::OnGotProperties(
    const std::string& extension_id,
    const std::string& guid,
    const base::Closure& authentication_callback,
    const std::string& service_path,
    const base::DictionaryValue& onc_network_config) {
  authentication_result_ = NetworkingConfigService::AuthenticationResult(
      std::string(), guid, NetworkingConfigService::NOTRY);
  authentication_callback_ = authentication_callback;

  // Try to extract |bssid| field.
  const base::DictionaryValue* wifi_with_state = nullptr;
  std::string bssid;
  std::unique_ptr<Event> event;
  if (onc_network_config.GetDictionaryWithoutPathExpansion(
          ::onc::network_config::kWiFi, &wifi_with_state) &&
      wifi_with_state->GetStringWithoutPathExpansion(::onc::wifi::kBSSID,
                                                     &bssid)) {
    event = CreatePortalDetectedEventAndDispatch(extension_id, guid, &bssid);
  } else {
    event = CreatePortalDetectedEventAndDispatch(extension_id, guid, nullptr);
  }

  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

void NetworkingConfigService::OnGetPropertiesFailed(
    const std::string& extension_id,
    const std::string& guid,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  LOG(WARNING) << "Failed to determine BSSID for network with guid " << guid
               << ": " << error_name;
  std::unique_ptr<Event> event =
      CreatePortalDetectedEventAndDispatch(extension_id, guid, nullptr);
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

std::unique_ptr<Event>
NetworkingConfigService::CreatePortalDetectedEventAndDispatch(
    const std::string& extension_id,
    const std::string& guid,
    const std::string* bssid) {
  const chromeos::NetworkState* network = chromeos::NetworkHandler::Get()
                                              ->network_state_handler()
                                              ->GetNetworkStateFromGuid(guid);
  if (!network)
    return nullptr;

  // Populate the NetworkInfo object.
  api::networking_config::NetworkInfo network_info;
  network_info.type = api::networking_config::NETWORK_TYPE_WIFI;
  const std::vector<uint8_t>& raw_ssid = network->raw_ssid();
  std::string hex_ssid = base::HexEncode(raw_ssid.data(), raw_ssid.size());
  network_info.hex_ssid = std::make_unique<std::string>(hex_ssid);
  network_info.ssid = std::make_unique<std::string>(network->name());
  network_info.guid = std::make_unique<std::string>(network->guid());
  if (bssid)
    network_info.bssid.reset(new std::string(*bssid));
  std::unique_ptr<base::ListValue> results =
      api::networking_config::OnCaptivePortalDetected::Create(network_info);
  std::unique_ptr<Event> event(
      new Event(events::NETWORKING_CONFIG_ON_CAPTIVE_PORTAL_DETECTED,
                api::networking_config::OnCaptivePortalDetected::kEventName,
                std::move(results)));
  return event;
}

void NetworkingConfigService::DispatchPortalDetectedEvent(
    const std::string& extension_id,
    const std::string& guid,
    const base::Closure& authentication_callback) {
  // This dispatch starts a new cycle of portal detection and authentication.
  // Ensure that any old |authentication_callback_| is dropped.
  authentication_callback_.Reset();

  // Determine |service_path| of network identified by |guid|.
  chromeos::NetworkHandler* network_handler = chromeos::NetworkHandler::Get();
  const chromeos::NetworkState* network =
      network_handler->network_state_handler()->GetNetworkStateFromGuid(guid);
  if (!network)
    return;
  const std::string service_path = network->path();

  // We do not provide |userhash| here because we only care about properties
  // that are not affected by policy, i.e BSSID.
  network_handler->managed_network_configuration_handler()->GetProperties(
      "" /* empty userhash */, service_path,
      base::Bind(&NetworkingConfigService::OnGotProperties,
                 weak_factory_.GetWeakPtr(), extension_id, guid,
                 authentication_callback),
      base::Bind(&NetworkingConfigService::OnGetPropertiesFailed,
                 weak_factory_.GetWeakPtr(), extension_id, guid));
}

}  // namespace extensions
