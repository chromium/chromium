// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/api/networking_private/networking_private_event_router_factory.h"

namespace extensions {

namespace {
const char kStubError[] = "networkingPrivate not implemented";
}  // namespace

NetworkingPrivateEventRouterFactory*
NetworkingPrivateEventRouterFactory::GetInstance() {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetPropertiesFunction

NetworkingPrivateGetPropertiesFunction::
    ~NetworkingPrivateGetPropertiesFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateGetPropertiesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetManagedPropertiesFunction

NetworkingPrivateGetManagedPropertiesFunction::
    ~NetworkingPrivateGetManagedPropertiesFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateGetManagedPropertiesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetStateFunction

NetworkingPrivateGetStateFunction::~NetworkingPrivateGetStateFunction() {}

ExtensionFunction::ResponseAction NetworkingPrivateGetStateFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetPropertiesFunction

NetworkingPrivateSetPropertiesFunction::
    ~NetworkingPrivateSetPropertiesFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateSetPropertiesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateCreateNetworkFunction

NetworkingPrivateCreateNetworkFunction::
    ~NetworkingPrivateCreateNetworkFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateCreateNetworkFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateForgetNetworkFunction

NetworkingPrivateForgetNetworkFunction::
    ~NetworkingPrivateForgetNetworkFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateForgetNetworkFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetNetworksFunction

NetworkingPrivateGetNetworksFunction::~NetworkingPrivateGetNetworksFunction() {}

ExtensionFunction::ResponseAction NetworkingPrivateGetNetworksFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetVisibleNetworksFunction

NetworkingPrivateGetVisibleNetworksFunction::
    ~NetworkingPrivateGetVisibleNetworksFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateGetVisibleNetworksFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetEnabledNetworkTypesFunction

NetworkingPrivateGetEnabledNetworkTypesFunction::
    ~NetworkingPrivateGetEnabledNetworkTypesFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateGetEnabledNetworkTypesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetDeviceStatesFunction

NetworkingPrivateGetDeviceStatesFunction::
    ~NetworkingPrivateGetDeviceStatesFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateGetDeviceStatesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateEnableNetworkTypeFunction

NetworkingPrivateEnableNetworkTypeFunction::
    ~NetworkingPrivateEnableNetworkTypeFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateEnableNetworkTypeFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateDisableNetworkTypeFunction

NetworkingPrivateDisableNetworkTypeFunction::
    ~NetworkingPrivateDisableNetworkTypeFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateDisableNetworkTypeFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateRequestNetworkScanFunction

NetworkingPrivateRequestNetworkScanFunction::
    ~NetworkingPrivateRequestNetworkScanFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateRequestNetworkScanFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartConnectFunction

NetworkingPrivateStartConnectFunction::
    ~NetworkingPrivateStartConnectFunction() {}

ExtensionFunction::ResponseAction NetworkingPrivateStartConnectFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartDisconnectFunction

NetworkingPrivateStartDisconnectFunction::
    ~NetworkingPrivateStartDisconnectFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateStartDisconnectFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartActivateFunction

NetworkingPrivateStartActivateFunction::
    ~NetworkingPrivateStartActivateFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateStartActivateFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetCaptivePortalStatusFunction

NetworkingPrivateGetCaptivePortalStatusFunction::
    ~NetworkingPrivateGetCaptivePortalStatusFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateGetCaptivePortalStatusFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateUnlockCellularSimFunction

NetworkingPrivateUnlockCellularSimFunction::
    ~NetworkingPrivateUnlockCellularSimFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateUnlockCellularSimFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetCellularSimStateFunction

NetworkingPrivateSetCellularSimStateFunction::
    ~NetworkingPrivateSetCellularSimStateFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateSetCellularSimStateFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSelectCellularMobileNetworkFunction

NetworkingPrivateSelectCellularMobileNetworkFunction::
    ~NetworkingPrivateSelectCellularMobileNetworkFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateSelectCellularMobileNetworkFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetGlobalPolicyFunction

NetworkingPrivateGetGlobalPolicyFunction::
    ~NetworkingPrivateGetGlobalPolicyFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateGetGlobalPolicyFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetCertificateListsFunction

NetworkingPrivateGetCertificateListsFunction::
    ~NetworkingPrivateGetCertificateListsFunction() {}

ExtensionFunction::ResponseAction
NetworkingPrivateGetCertificateListsFunction::Run() {
  return RespondNow(Error(kStubError));
}

}  // namespace extensions
