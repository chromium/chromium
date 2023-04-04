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

ExtensionFunction::ResponseAction
NetworkingPrivateGetPropertiesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetManagedPropertiesFunction

ExtensionFunction::ResponseAction
NetworkingPrivateGetManagedPropertiesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetStateFunction

ExtensionFunction::ResponseAction NetworkingPrivateGetStateFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetPropertiesFunction

ExtensionFunction::ResponseAction
NetworkingPrivateSetPropertiesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateCreateNetworkFunction

ExtensionFunction::ResponseAction
NetworkingPrivateCreateNetworkFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateForgetNetworkFunction

ExtensionFunction::ResponseAction
NetworkingPrivateForgetNetworkFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetNetworksFunction

ExtensionFunction::ResponseAction NetworkingPrivateGetNetworksFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetVisibleNetworksFunction

ExtensionFunction::ResponseAction
NetworkingPrivateGetVisibleNetworksFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetEnabledNetworkTypesFunction

ExtensionFunction::ResponseAction
NetworkingPrivateGetEnabledNetworkTypesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetDeviceStatesFunction

ExtensionFunction::ResponseAction
NetworkingPrivateGetDeviceStatesFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateEnableNetworkTypeFunction

ExtensionFunction::ResponseAction
NetworkingPrivateEnableNetworkTypeFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateDisableNetworkTypeFunction

ExtensionFunction::ResponseAction
NetworkingPrivateDisableNetworkTypeFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateRequestNetworkScanFunction

ExtensionFunction::ResponseAction
NetworkingPrivateRequestNetworkScanFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartConnectFunction

ExtensionFunction::ResponseAction NetworkingPrivateStartConnectFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartDisconnectFunction

ExtensionFunction::ResponseAction
NetworkingPrivateStartDisconnectFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartActivateFunction

ExtensionFunction::ResponseAction
NetworkingPrivateStartActivateFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetCaptivePortalStatusFunction

ExtensionFunction::ResponseAction
NetworkingPrivateGetCaptivePortalStatusFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateUnlockCellularSimFunction

ExtensionFunction::ResponseAction
NetworkingPrivateUnlockCellularSimFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetCellularSimStateFunction

ExtensionFunction::ResponseAction
NetworkingPrivateSetCellularSimStateFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSelectCellularMobileNetworkFunction

ExtensionFunction::ResponseAction
NetworkingPrivateSelectCellularMobileNetworkFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetGlobalPolicyFunction

ExtensionFunction::ResponseAction
NetworkingPrivateGetGlobalPolicyFunction::Run() {
  return RespondNow(Error(kStubError));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetCertificateListsFunction

ExtensionFunction::ResponseAction
NetworkingPrivateGetCertificateListsFunction::Run() {
  return RespondNow(Error(kStubError));
}

}  // namespace extensions
