// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_api.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/onc/onc_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/common/api/networking_private.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/feature_provider.h"

#include "base/logging.h"

namespace extensions {

namespace {

const int kDefaultNetworkListLimit = 1000;

const char kPrivateOnlyError[] = "Requires networkingPrivate API access.";

const char* const kPrivatePropertyPathsForSet[] = {
    "Cellular.APN", "ProxySettings", "StaticIPConfig", "VPN.Host",
    "VPN.IPsec",    "VPN.L2TP",      "VPN.OpenVPN",    "VPN.ThirdPartyVPN",
};

const char* const kPrivatePropertyPathsForGet[] = {
    "Cellular.APN",  "Cellular.APNList", "Cellular.LastGoodAPN",
    "Cellular.ESN",  "Cellular.ICCID",   "Cellular.IMEI",
    "Cellular.IMSI", "Cellular.MDN",     "Cellular.MEID",
    "Cellular.MIN",  "Ethernet.EAP",     "VPN.IPsec",
    "VPN.L2TP",      "VPN.OpenVPN",      "WiFi.EAP",
    "WiMax.EAP",
};

NetworkingPrivateDelegate* GetDelegate(
    content::BrowserContext* browser_context) {
  return NetworkingPrivateDelegateFactory::GetForBrowserContext(
      browser_context);
}

bool HasPrivateNetworkingAccess(const Extension* extension,
                                Feature::Context context,
                                const GURL& source_url,
                                int context_id) {
  return ExtensionAPI::GetSharedInstance()
      ->IsAvailable("networkingPrivate", extension, context, source_url,
                    CheckAliasStatus::NOT_ALLOWED, context_id)
      .is_available();
}

// Indicates which filter should be used - filter for properties allowed to
// be returned by getProperties methods, or the filter for properties settable
// by setProperties/createNetwork methods.
enum class PropertiesType { GET, SET };

// Filters out all properties that are not allowed for the extension in the
// provided context.
// Returns list of removed keys.
std::vector<std::string> FilterProperties(base::Value::Dict& properties,
                                          PropertiesType type,
                                          const Extension* extension,
                                          Feature::Context context,
                                          const GURL& source_url,
                                          int context_id) {
  if (HasPrivateNetworkingAccess(extension, context, source_url, context_id))
    return std::vector<std::string>();

  const char* const* filter = nullptr;
  size_t filter_size = 0;
  if (type == PropertiesType::GET) {
    filter = kPrivatePropertyPathsForGet;
    filter_size = std::size(kPrivatePropertyPathsForGet);
  } else {
    filter = kPrivatePropertyPathsForSet;
    filter_size = std::size(kPrivatePropertyPathsForSet);
  }

  std::vector<std::string> removed_properties;
  for (size_t i = 0; i < filter_size; ++i) {
    // networkingPrivate uses sub dictionaries for Shill properties with a
    // '.' separator, so we use `RemoveByDottedPath` here, not `Remove`.
    if (properties.RemoveByDottedPath(filter[i])) {
      removed_properties.push_back(filter[i]);
    }
  }
  return removed_properties;
}

bool CanChangeSharedConfig(const Extension* extension,
                           Feature::Context context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return context == Feature::WEBUI_CONTEXT;
#else
  return true;
#endif
}

std::string InvalidPropertiesError(const std::vector<std::string>& properties) {
  DCHECK(!properties.empty());
  return "Error.PropertiesNotAllowed: [" + base::JoinString(properties, ", ") +
         "]";
}

}  // namespace

namespace private_api = api::networking_private;

namespace networking_private {

// static
const char kErrorAccessToSharedConfig[] = "Error.CannotChangeSharedConfig";
const char kErrorInvalidArguments[] = "Error.InvalidArguments";
const char kErrorInvalidNetworkGuid[] = "Error.InvalidNetworkGuid";
const char kErrorInvalidNetworkOperation[] = "Error.InvalidNetworkOperation";
const char kErrorNetworkUnavailable[] = "Error.NetworkUnavailable";
const char kErrorNotSupported[] = "Error.NotSupported";
const char kErrorPolicyControlled[] = "Error.PolicyControlled";
const char kErrorSimLocked[] = "Error.SimLocked";
const char kErrorUnconfiguredNetwork[] = "Error.UnconfiguredNetwork";

}  // namespace networking_private

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetPropertiesFunction

NetworkingPrivateGetPropertiesFunction::
    ~NetworkingPrivateGetPropertiesFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateGetPropertiesFunction::Run() {
  std::unique_ptr<private_api::GetProperties::Params> params =
      private_api::GetProperties::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->GetProperties(
          params->network_guid,
          base::BindOnce(&NetworkingPrivateGetPropertiesFunction::Result,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetPropertiesFunction::Result(
    absl::optional<base::Value::Dict> result,
    const absl::optional<std::string>& error) {
  if (!result) {
    Respond(Error(error.value_or("Failed")));
    return;
  }
  FilterProperties(result.value(), PropertiesType::GET, extension(),
                   source_context_type(), source_url(), context_id());
  Respond(OneArgument(base::Value(std::move(*result))));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetManagedPropertiesFunction

NetworkingPrivateGetManagedPropertiesFunction::
    ~NetworkingPrivateGetManagedPropertiesFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateGetManagedPropertiesFunction::Run() {
  std::unique_ptr<private_api::GetManagedProperties::Params> params =
      private_api::GetManagedProperties::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->GetManagedProperties(
          params->network_guid,
          base::BindOnce(&NetworkingPrivateGetManagedPropertiesFunction::Result,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetManagedPropertiesFunction::Result(
    absl::optional<base::Value::Dict> result,
    const absl::optional<std::string>& error) {
  if (!result) {
    Respond(Error(error.value_or("Failed")));
    return;
  }
  FilterProperties(result.value(), PropertiesType::GET, extension(),
                   source_context_type(), source_url(), context_id());
  Respond(OneArgument(base::Value(std::move(*result))));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetStateFunction

NetworkingPrivateGetStateFunction::~NetworkingPrivateGetStateFunction() =
    default;

ExtensionFunction::ResponseAction NetworkingPrivateGetStateFunction::Run() {
  std::unique_ptr<private_api::GetState::Params> params =
      private_api::GetState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->GetState(
          params->network_guid,
          base::BindOnce(&NetworkingPrivateGetStateFunction::Success, this),
          base::BindOnce(&NetworkingPrivateGetStateFunction::Failure, this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetStateFunction::Success(base::Value::Dict result) {
  FilterProperties(result, PropertiesType::GET, extension(),
                   source_context_type(), source_url(), context_id());
  Respond(OneArgument(base::Value(std::move(result))));
}

void NetworkingPrivateGetStateFunction::Failure(const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetPropertiesFunction

NetworkingPrivateSetPropertiesFunction::
    ~NetworkingPrivateSetPropertiesFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateSetPropertiesFunction::Run() {
  std::unique_ptr<private_api::SetProperties::Params> params =
      private_api::SetProperties::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::Value::Dict properties = params->properties.ToValue();

  std::vector<std::string> not_allowed_properties =
      FilterProperties(properties, PropertiesType::SET, extension(),
                       source_context_type(), source_url(), context_id());
  if (!not_allowed_properties.empty())
    return RespondNow(Error(InvalidPropertiesError(not_allowed_properties)));

  GetDelegate(browser_context())
      ->SetProperties(
          params->network_guid, std::move(properties),
          CanChangeSharedConfig(extension(), source_context_type()),
          base::BindOnce(&NetworkingPrivateSetPropertiesFunction::Success,
                         this),
          base::BindOnce(&NetworkingPrivateSetPropertiesFunction::Failure,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateSetPropertiesFunction::Success() {
  Respond(NoArguments());
}

void NetworkingPrivateSetPropertiesFunction::Failure(const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateCreateNetworkFunction

NetworkingPrivateCreateNetworkFunction::
    ~NetworkingPrivateCreateNetworkFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateCreateNetworkFunction::Run() {
  std::unique_ptr<private_api::CreateNetwork::Params> params =
      private_api::CreateNetwork::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->shared &&
      !CanChangeSharedConfig(extension(), source_context_type())) {
    return RespondNow(Error(networking_private::kErrorAccessToSharedConfig));
  }

  base::Value::Dict properties_dict = params->properties.ToValue();

  std::vector<std::string> not_allowed_properties =
      FilterProperties(properties_dict, PropertiesType::SET, extension(),
                       source_context_type(), source_url(), context_id());
  if (!not_allowed_properties.empty())
    return RespondNow(Error(InvalidPropertiesError(not_allowed_properties)));

  GetDelegate(browser_context())
      ->CreateNetwork(
          params->shared, base::Value(std::move(properties_dict)),
          base::BindOnce(&NetworkingPrivateCreateNetworkFunction::Success,
                         this),
          base::BindOnce(&NetworkingPrivateCreateNetworkFunction::Failure,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateCreateNetworkFunction::Success(const std::string& guid) {
  Respond(ArgumentList(private_api::CreateNetwork::Results::Create(guid)));
}

void NetworkingPrivateCreateNetworkFunction::Failure(const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateForgetNetworkFunction

NetworkingPrivateForgetNetworkFunction::
    ~NetworkingPrivateForgetNetworkFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateForgetNetworkFunction::Run() {
  std::unique_ptr<private_api::ForgetNetwork::Params> params =
      private_api::ForgetNetwork::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->ForgetNetwork(
          params->network_guid,
          CanChangeSharedConfig(extension(), source_context_type()),
          base::BindOnce(&NetworkingPrivateForgetNetworkFunction::Success,
                         this),
          base::BindOnce(&NetworkingPrivateForgetNetworkFunction::Failure,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateForgetNetworkFunction::Success() {
  Respond(NoArguments());
}

void NetworkingPrivateForgetNetworkFunction::Failure(const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetNetworksFunction

NetworkingPrivateGetNetworksFunction::~NetworkingPrivateGetNetworksFunction() =
    default;

ExtensionFunction::ResponseAction NetworkingPrivateGetNetworksFunction::Run() {
  std::unique_ptr<private_api::GetNetworks::Params> params =
      private_api::GetNetworks::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string network_type = private_api::ToString(params->filter.network_type);
  const bool configured_only =
      params->filter.configured ? *params->filter.configured : false;
  const bool visible_only =
      params->filter.visible ? *params->filter.visible : false;
  const int limit =
      params->filter.limit ? *params->filter.limit : kDefaultNetworkListLimit;

  GetDelegate(browser_context())
      ->GetNetworks(
          network_type, configured_only, visible_only, limit,
          base::BindOnce(&NetworkingPrivateGetNetworksFunction::Success, this),
          base::BindOnce(&NetworkingPrivateGetNetworksFunction::Failure, this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetNetworksFunction::Success(
    base::Value::List network_list) {
  return Respond(OneArgument(base::Value(std::move(network_list))));
}

void NetworkingPrivateGetNetworksFunction::Failure(const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetVisibleNetworksFunction

NetworkingPrivateGetVisibleNetworksFunction::
    ~NetworkingPrivateGetVisibleNetworksFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateGetVisibleNetworksFunction::Run() {
  std::unique_ptr<private_api::GetVisibleNetworks::Params> params =
      private_api::GetVisibleNetworks::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // getVisibleNetworks is deprecated - allow it only for apps with
  // networkingPrivate permissions, i.e. apps that might have started using it
  // before its deprecation.
  if (!HasPrivateNetworkingAccess(extension(), source_context_type(),
                                  source_url(), context_id())) {
    return RespondNow(Error(kPrivateOnlyError));
  }

  std::string network_type = private_api::ToString(params->network_type);
  const bool configured_only = false;
  const bool visible_only = true;

  GetDelegate(browser_context())
      ->GetNetworks(
          network_type, configured_only, visible_only, kDefaultNetworkListLimit,
          base::BindOnce(&NetworkingPrivateGetVisibleNetworksFunction::Success,
                         this),
          base::BindOnce(&NetworkingPrivateGetVisibleNetworksFunction::Failure,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetVisibleNetworksFunction::Success(
    base::Value::List network_properties_list) {
  Respond(OneArgument(base::Value(std::move(network_properties_list))));
}

void NetworkingPrivateGetVisibleNetworksFunction::Failure(
    const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetEnabledNetworkTypesFunction

NetworkingPrivateGetEnabledNetworkTypesFunction::
    ~NetworkingPrivateGetEnabledNetworkTypesFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateGetEnabledNetworkTypesFunction::Run() {
  // getEnabledNetworkTypes is deprecated - allow it only for apps with
  // networkingPrivate permissions, i.e. apps that might have started using it
  // before its deprecation.
  if (!HasPrivateNetworkingAccess(extension(), source_context_type(),
                                  source_url(), context_id())) {
    return RespondNow(Error(kPrivateOnlyError));
  }

  GetDelegate(browser_context())
      ->GetEnabledNetworkTypes(base::BindOnce(
          &NetworkingPrivateGetEnabledNetworkTypesFunction::Result, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetEnabledNetworkTypesFunction::Result(
    base::Value::List enabled_networks_onc_types) {
  if (enabled_networks_onc_types.empty()) {
    return Respond(Error(networking_private::kErrorNotSupported));
  }
  base::Value::List enabled_networks_list;
  for (const auto& entry : enabled_networks_onc_types) {
    const std::string& type = entry.GetString();
    if (type == ::onc::network_type::kEthernet) {
      enabled_networks_list.Append(
          private_api::ToString(private_api::NETWORK_TYPE_ETHERNET));
    } else if (type == ::onc::network_type::kWiFi) {
      enabled_networks_list.Append(
          private_api::ToString(private_api::NETWORK_TYPE_WIFI));
    } else if (type == ::onc::network_type::kCellular) {
      enabled_networks_list.Append(
          private_api::ToString(private_api::NETWORK_TYPE_CELLULAR));
    } else {
      LOG(ERROR) << "networkingPrivate: Unexpected type: " << type;
    }
  }
  return Respond(OneArgument(base::Value(std::move(enabled_networks_list))));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetDeviceStatesFunction

NetworkingPrivateGetDeviceStatesFunction::
    ~NetworkingPrivateGetDeviceStatesFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateGetDeviceStatesFunction::Run() {
  GetDelegate(browser_context())
      ->GetDeviceStateList(base::BindOnce(
          &NetworkingPrivateGetDeviceStatesFunction::Result, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetDeviceStatesFunction::Result(
    std::unique_ptr<NetworkingPrivateDelegate::DeviceStateList> device_states) {
  if (!device_states)
    return Respond(Error(networking_private::kErrorNotSupported));

  base::Value::List device_state_list;
  for (const auto& properties : *device_states)
    device_state_list.Append(properties->ToValue());
  return Respond(OneArgument(base::Value(std::move(device_state_list))));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateEnableNetworkTypeFunction

NetworkingPrivateEnableNetworkTypeFunction::
    ~NetworkingPrivateEnableNetworkTypeFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateEnableNetworkTypeFunction::Run() {
  std::unique_ptr<private_api::EnableNetworkType::Params> params =
      private_api::EnableNetworkType::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->EnableNetworkType(
          private_api::ToString(params->network_type),
          base::BindOnce(&NetworkingPrivateEnableNetworkTypeFunction::Result,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}
void NetworkingPrivateEnableNetworkTypeFunction::Result(bool success) {
  if (!success)
    return Respond(Error(networking_private::kErrorNotSupported));
  return Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateDisableNetworkTypeFunction

NetworkingPrivateDisableNetworkTypeFunction::
    ~NetworkingPrivateDisableNetworkTypeFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateDisableNetworkTypeFunction::Run() {
  std::unique_ptr<private_api::DisableNetworkType::Params> params =
      private_api::DisableNetworkType::Params::Create(args());

  GetDelegate(browser_context())
      ->DisableNetworkType(
          private_api::ToString(params->network_type),
          base::BindOnce(&NetworkingPrivateDisableNetworkTypeFunction::Result,
                         this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateDisableNetworkTypeFunction::Result(bool success) {
  if (!success)
    return Respond(Error(networking_private::kErrorNotSupported));
  return Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateRequestNetworkScanFunction

NetworkingPrivateRequestNetworkScanFunction::
    ~NetworkingPrivateRequestNetworkScanFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateRequestNetworkScanFunction::Run() {
  std::unique_ptr<private_api::RequestNetworkScan::Params> params =
      private_api::RequestNetworkScan::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string network_type = private_api::ToString(params->network_type);
  GetDelegate(browser_context())
      ->RequestScan(
          network_type,
          base::BindOnce(&NetworkingPrivateRequestNetworkScanFunction::Result,
                         this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateRequestNetworkScanFunction::Result(bool success) {
  if (!success)
    return Respond(Error(networking_private::kErrorNotSupported));
  return Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartConnectFunction

NetworkingPrivateStartConnectFunction::
    ~NetworkingPrivateStartConnectFunction() = default;

ExtensionFunction::ResponseAction NetworkingPrivateStartConnectFunction::Run() {
  std::unique_ptr<private_api::StartConnect::Params> params =
      private_api::StartConnect::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->StartConnect(
          params->network_guid,
          base::BindOnce(&NetworkingPrivateStartConnectFunction::Success, this),
          base::BindOnce(&NetworkingPrivateStartConnectFunction::Failure, this,
                         params->network_guid));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateStartConnectFunction::Success() {
  Respond(NoArguments());
}

void NetworkingPrivateStartConnectFunction::Failure(const std::string& guid,
                                                    const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartDisconnectFunction

NetworkingPrivateStartDisconnectFunction::
    ~NetworkingPrivateStartDisconnectFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateStartDisconnectFunction::Run() {
  std::unique_ptr<private_api::StartDisconnect::Params> params =
      private_api::StartDisconnect::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->StartDisconnect(
          params->network_guid,
          base::BindOnce(&NetworkingPrivateStartDisconnectFunction::Success,
                         this),
          base::BindOnce(&NetworkingPrivateStartDisconnectFunction::Failure,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateStartDisconnectFunction::Success() {
  Respond(NoArguments());
}

void NetworkingPrivateStartDisconnectFunction::Failure(
    const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartActivateFunction

NetworkingPrivateStartActivateFunction::
    ~NetworkingPrivateStartActivateFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateStartActivateFunction::Run() {
  if (!HasPrivateNetworkingAccess(extension(), source_context_type(),
                                  source_url(), context_id())) {
    return RespondNow(Error(kPrivateOnlyError));
  }

  std::unique_ptr<private_api::StartActivate::Params> params =
      private_api::StartActivate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->StartActivate(
          params->network_guid, params->carrier ? *params->carrier : "",
          base::BindOnce(&NetworkingPrivateStartActivateFunction::Success,
                         this),
          base::BindOnce(&NetworkingPrivateStartActivateFunction::Failure,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateStartActivateFunction::Success() {
  Respond(NoArguments());
}

void NetworkingPrivateStartActivateFunction::Failure(const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetCaptivePortalStatusFunction

NetworkingPrivateGetCaptivePortalStatusFunction::
    ~NetworkingPrivateGetCaptivePortalStatusFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateGetCaptivePortalStatusFunction::Run() {
  std::unique_ptr<private_api::GetCaptivePortalStatus::Params> params =
      private_api::GetCaptivePortalStatus::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->GetCaptivePortalStatus(
          params->network_guid,
          base::BindOnce(
              &NetworkingPrivateGetCaptivePortalStatusFunction::Success, this),
          base::BindOnce(
              &NetworkingPrivateGetCaptivePortalStatusFunction::Failure, this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetCaptivePortalStatusFunction::Success(
    const std::string& result) {
  Respond(ArgumentList(private_api::GetCaptivePortalStatus::Results::Create(
      private_api::ParseCaptivePortalStatus(result))));
}

void NetworkingPrivateGetCaptivePortalStatusFunction::Failure(
    const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateUnlockCellularSimFunction

NetworkingPrivateUnlockCellularSimFunction::
    ~NetworkingPrivateUnlockCellularSimFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateUnlockCellularSimFunction::Run() {
  if (!HasPrivateNetworkingAccess(extension(), source_context_type(),
                                  source_url(), context_id())) {
    return RespondNow(Error(kPrivateOnlyError));
  }

  std::unique_ptr<private_api::UnlockCellularSim::Params> params =
      private_api::UnlockCellularSim::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->UnlockCellularSim(
          params->network_guid, params->pin, params->puk ? *params->puk : "",
          base::BindOnce(&NetworkingPrivateUnlockCellularSimFunction::Success,
                         this),
          base::BindOnce(&NetworkingPrivateUnlockCellularSimFunction::Failure,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateUnlockCellularSimFunction::Success() {
  Respond(NoArguments());
}

void NetworkingPrivateUnlockCellularSimFunction::Failure(
    const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetCellularSimStateFunction

NetworkingPrivateSetCellularSimStateFunction::
    ~NetworkingPrivateSetCellularSimStateFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateSetCellularSimStateFunction::Run() {
  if (!HasPrivateNetworkingAccess(extension(), source_context_type(),
                                  source_url(), context_id())) {
    return RespondNow(Error(kPrivateOnlyError));
  }

  std::unique_ptr<private_api::SetCellularSimState::Params> params =
      private_api::SetCellularSimState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->SetCellularSimState(
          params->network_guid, params->sim_state.require_pin,
          params->sim_state.current_pin,
          params->sim_state.new_pin ? *params->sim_state.new_pin : "",
          base::BindOnce(&NetworkingPrivateSetCellularSimStateFunction::Success,
                         this),
          base::BindOnce(&NetworkingPrivateSetCellularSimStateFunction::Failure,
                         this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateSetCellularSimStateFunction::Success() {
  Respond(NoArguments());
}

void NetworkingPrivateSetCellularSimStateFunction::Failure(
    const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSelectCellularMobileNetworkFunction

NetworkingPrivateSelectCellularMobileNetworkFunction::
    ~NetworkingPrivateSelectCellularMobileNetworkFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateSelectCellularMobileNetworkFunction::Run() {
  if (!HasPrivateNetworkingAccess(extension(), source_context_type(),
                                  source_url(), context_id())) {
    return RespondNow(Error(kPrivateOnlyError));
  }

  std::unique_ptr<private_api::SelectCellularMobileNetwork::Params> params =
      private_api::SelectCellularMobileNetwork::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())
      ->SelectCellularMobileNetwork(
          params->network_guid, params->network_id,
          base::BindOnce(
              &NetworkingPrivateSelectCellularMobileNetworkFunction::Success,
              this),
          base::BindOnce(
              &NetworkingPrivateSelectCellularMobileNetworkFunction::Failure,
              this));
  // Success() or Failure() might have been called synchronously at this point.
  // In that case this function has already called Respond(). Return
  // AlreadyResponded() in that case.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateSelectCellularMobileNetworkFunction::Success() {
  Respond(NoArguments());
}

void NetworkingPrivateSelectCellularMobileNetworkFunction::Failure(
    const std::string& error) {
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetGlobalPolicyFunction

NetworkingPrivateGetGlobalPolicyFunction::
    ~NetworkingPrivateGetGlobalPolicyFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateGetGlobalPolicyFunction::Run() {
  GetDelegate(browser_context())
      ->GetGlobalPolicy(base::BindOnce(
          &NetworkingPrivateGetGlobalPolicyFunction::Result, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetGlobalPolicyFunction::Result(
    absl::optional<base::Value::Dict> policy_dict) {
  // private_api::GlobalPolicy is a subset of the global policy dictionary
  // (by definition), so use the api setter/getter to generate the subset.
  std::unique_ptr<private_api::GlobalPolicy> policy(
      private_api::GlobalPolicy::FromValue(
          base::Value(std::move(policy_dict.value()))));
  DCHECK(policy);
  return Respond(
      ArgumentList(private_api::GetGlobalPolicy::Results::Create(*policy)));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetCertificateListsFunction

NetworkingPrivateGetCertificateListsFunction::
    ~NetworkingPrivateGetCertificateListsFunction() = default;

ExtensionFunction::ResponseAction
NetworkingPrivateGetCertificateListsFunction::Run() {
  if (!HasPrivateNetworkingAccess(extension(), source_context_type(),
                                  source_url(), context_id())) {
    return RespondNow(Error(kPrivateOnlyError));
  }

  GetDelegate(browser_context())
      ->GetCertificateLists(base::BindOnce(
          &NetworkingPrivateGetCertificateListsFunction::Result, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingPrivateGetCertificateListsFunction::Result(
    absl::optional<base::Value::Dict> certificate_lists) {
  return Respond(
      OneArgument(base::Value(std::move(certificate_lists.value()))));
}

}  // namespace extensions
