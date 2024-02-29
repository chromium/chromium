// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_API_H_

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace networking_private {

extern const char kErrorAccessToSharedConfig[];
extern const char kErrorInvalidArguments[];
extern const char kErrorInvalidNetworkGuid[];
extern const char kErrorInvalidNetworkOperation[];
extern const char kErrorNetworkUnavailable[];
extern const char kErrorNotSupported[];
extern const char kErrorPolicyControlled[];
extern const char kErrorSimLocked[];
extern const char kErrorUnconfiguredNetwork[];

}  // namespace networking_private

// Implements the chrome.networkingPrivate.getProperties method.
class NetworkingPrivateGetPropertiesFunction : public ExtensionFunction {
 public:
  NetworkingPrivateGetPropertiesFunction() = default;

  NetworkingPrivateGetPropertiesFunction(
      const NetworkingPrivateGetPropertiesFunction&) = delete;
  NetworkingPrivateGetPropertiesFunction& operator=(
      const NetworkingPrivateGetPropertiesFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getProperties",
                             NETWORKINGPRIVATE_GETPROPERTIES)

 protected:
  ~NetworkingPrivateGetPropertiesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Result(std::optional<base::Value::Dict> result,
              const std::optional<std::string>& error);
};

// Implements the chrome.networkingPrivate.getManagedProperties method.
class NetworkingPrivateGetManagedPropertiesFunction : public ExtensionFunction {
 public:
  NetworkingPrivateGetManagedPropertiesFunction() = default;

  NetworkingPrivateGetManagedPropertiesFunction(
      const NetworkingPrivateGetManagedPropertiesFunction&) = delete;
  NetworkingPrivateGetManagedPropertiesFunction& operator=(
      const NetworkingPrivateGetManagedPropertiesFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getManagedProperties",
                             NETWORKINGPRIVATE_GETMANAGEDPROPERTIES)

 protected:
  ~NetworkingPrivateGetManagedPropertiesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Result(std::optional<base::Value::Dict> result,
              const std::optional<std::string>& error);
};

// Implements the chrome.networkingPrivate.getState method.
class NetworkingPrivateGetStateFunction : public ExtensionFunction {
 public:
  NetworkingPrivateGetStateFunction() = default;

  NetworkingPrivateGetStateFunction(const NetworkingPrivateGetStateFunction&) =
      delete;
  NetworkingPrivateGetStateFunction& operator=(
      const NetworkingPrivateGetStateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getState",
                             NETWORKINGPRIVATE_GETSTATE)

 protected:
  ~NetworkingPrivateGetStateFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Success(base::Value::Dict result);
  void Failure(const std::string& error);
};

// Implements the chrome.networkingPrivate.setProperties method.
class NetworkingPrivateSetPropertiesFunction : public ExtensionFunction {
 public:
  NetworkingPrivateSetPropertiesFunction() = default;

  NetworkingPrivateSetPropertiesFunction(
      const NetworkingPrivateSetPropertiesFunction&) = delete;
  NetworkingPrivateSetPropertiesFunction& operator=(
      const NetworkingPrivateSetPropertiesFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.setProperties",
                             NETWORKINGPRIVATE_SETPROPERTIES)

 protected:
  ~NetworkingPrivateSetPropertiesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Success();
  void Failure(const std::string& error);
};

// Implements the chrome.networkingPrivate.createNetwork method.
class NetworkingPrivateCreateNetworkFunction : public ExtensionFunction {
 public:
  NetworkingPrivateCreateNetworkFunction() = default;

  NetworkingPrivateCreateNetworkFunction(
      const NetworkingPrivateCreateNetworkFunction&) = delete;
  NetworkingPrivateCreateNetworkFunction& operator=(
      const NetworkingPrivateCreateNetworkFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.createNetwork",
                             NETWORKINGPRIVATE_CREATENETWORK)

 protected:
  ~NetworkingPrivateCreateNetworkFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Success(const std::string& guid);
  void Failure(const std::string& error);
};

// Implements the chrome.networkingPrivate.createNetwork method.
class NetworkingPrivateForgetNetworkFunction : public ExtensionFunction {
 public:
  NetworkingPrivateForgetNetworkFunction() = default;

  NetworkingPrivateForgetNetworkFunction(
      const NetworkingPrivateForgetNetworkFunction&) = delete;
  NetworkingPrivateForgetNetworkFunction& operator=(
      const NetworkingPrivateForgetNetworkFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.forgetNetwork",
                             NETWORKINGPRIVATE_FORGETNETWORK)

 protected:
  ~NetworkingPrivateForgetNetworkFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Success();
  void Failure(const std::string& error);
};

// Implements the chrome.networkingPrivate.getNetworks method.
class NetworkingPrivateGetNetworksFunction : public ExtensionFunction {
 public:
  NetworkingPrivateGetNetworksFunction() = default;

  NetworkingPrivateGetNetworksFunction(
      const NetworkingPrivateGetNetworksFunction&) = delete;
  NetworkingPrivateGetNetworksFunction& operator=(
      const NetworkingPrivateGetNetworksFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getNetworks",
                             NETWORKINGPRIVATE_GETNETWORKS)

 protected:
  ~NetworkingPrivateGetNetworksFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Success(base::Value::List network_list);
  void Failure(const std::string& error);
};

// Implements the chrome.networkingPrivate.getVisibleNetworks method.
class NetworkingPrivateGetVisibleNetworksFunction : public ExtensionFunction {
 public:
  NetworkingPrivateGetVisibleNetworksFunction() = default;

  NetworkingPrivateGetVisibleNetworksFunction(
      const NetworkingPrivateGetVisibleNetworksFunction&) = delete;
  NetworkingPrivateGetVisibleNetworksFunction& operator=(
      const NetworkingPrivateGetVisibleNetworksFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getVisibleNetworks",
                             NETWORKINGPRIVATE_GETVISIBLENETWORKS)

 protected:
  ~NetworkingPrivateGetVisibleNetworksFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Success(base::Value::List network_list);
  void Failure(const std::string& error);
};

// Implements the chrome.networkingPrivate.getEnabledNetworkTypes method.
class NetworkingPrivateGetEnabledNetworkTypesFunction
    : public ExtensionFunction {
 public:
  NetworkingPrivateGetEnabledNetworkTypesFunction() = default;

  NetworkingPrivateGetEnabledNetworkTypesFunction(
      const NetworkingPrivateGetEnabledNetworkTypesFunction&) = delete;
  NetworkingPrivateGetEnabledNetworkTypesFunction& operator=(
      const NetworkingPrivateGetEnabledNetworkTypesFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getEnabledNetworkTypes",
                             NETWORKINGPRIVATE_GETENABLEDNETWORKTYPES)

 protected:
  ~NetworkingPrivateGetEnabledNetworkTypesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Result(base::Value::List enabled_networks_onc_types);
};

// Implements the chrome.networkingPrivate.getDeviceStates method.
class NetworkingPrivateGetDeviceStatesFunction : public ExtensionFunction {
 public:
  NetworkingPrivateGetDeviceStatesFunction() = default;

  NetworkingPrivateGetDeviceStatesFunction(
      const NetworkingPrivateGetDeviceStatesFunction&) = delete;
  NetworkingPrivateGetDeviceStatesFunction& operator=(
      const NetworkingPrivateGetDeviceStatesFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getDeviceStates",
                             NETWORKINGPRIVATE_GETDEVICESTATES)

 protected:
  ~NetworkingPrivateGetDeviceStatesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Result(
      std::optional<NetworkingPrivateDelegate::DeviceStateList> device_states);
};

// Implements the chrome.networkingPrivate.enableNetworkType method.
class NetworkingPrivateEnableNetworkTypeFunction : public ExtensionFunction {
 public:
  NetworkingPrivateEnableNetworkTypeFunction() = default;

  NetworkingPrivateEnableNetworkTypeFunction(
      const NetworkingPrivateEnableNetworkTypeFunction&) = delete;
  NetworkingPrivateEnableNetworkTypeFunction& operator=(
      const NetworkingPrivateEnableNetworkTypeFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.enableNetworkType",
                             NETWORKINGPRIVATE_ENABLENETWORKTYPE)

 protected:
  ~NetworkingPrivateEnableNetworkTypeFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Result(bool success);
};

// Implements the chrome.networkingPrivate.disableNetworkType method.
class NetworkingPrivateDisableNetworkTypeFunction : public ExtensionFunction {
 public:
  NetworkingPrivateDisableNetworkTypeFunction() = default;

  NetworkingPrivateDisableNetworkTypeFunction(
      const NetworkingPrivateDisableNetworkTypeFunction&) = delete;
  NetworkingPrivateDisableNetworkTypeFunction& operator=(
      const NetworkingPrivateDisableNetworkTypeFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.disableNetworkType",
                             NETWORKINGPRIVATE_DISABLENETWORKTYPE)

 protected:
  ~NetworkingPrivateDisableNetworkTypeFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Result(bool success);
};

// Implements the chrome.networkingPrivate.requestNetworkScan method.
class NetworkingPrivateRequestNetworkScanFunction : public ExtensionFunction {
 public:
  NetworkingPrivateRequestNetworkScanFunction() = default;

  NetworkingPrivateRequestNetworkScanFunction(
      const NetworkingPrivateRequestNetworkScanFunction&) = delete;
  NetworkingPrivateRequestNetworkScanFunction& operator=(
      const NetworkingPrivateRequestNetworkScanFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.requestNetworkScan",
                             NETWORKINGPRIVATE_REQUESTNETWORKSCAN)

 protected:
  ~NetworkingPrivateRequestNetworkScanFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Result(bool success);
};

// Implements the chrome.networkingPrivate.startConnect method.
class NetworkingPrivateStartConnectFunction : public ExtensionFunction {
 public:
  NetworkingPrivateStartConnectFunction() = default;

  NetworkingPrivateStartConnectFunction(
      const NetworkingPrivateStartConnectFunction&) = delete;
  NetworkingPrivateStartConnectFunction& operator=(
      const NetworkingPrivateStartConnectFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.startConnect",
                             NETWORKINGPRIVATE_STARTCONNECT)

 protected:
  ~NetworkingPrivateStartConnectFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Success();
  void Failure(const std::string& guid, const std::string& error);
};

// Implements the chrome.networkingPrivate.startDisconnect method.
class NetworkingPrivateStartDisconnectFunction : public ExtensionFunction {
 public:
  NetworkingPrivateStartDisconnectFunction() = default;

  NetworkingPrivateStartDisconnectFunction(
      const NetworkingPrivateStartDisconnectFunction&) = delete;
  NetworkingPrivateStartDisconnectFunction& operator=(
      const NetworkingPrivateStartDisconnectFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.startDisconnect",
                             NETWORKINGPRIVATE_STARTDISCONNECT)

 protected:
  ~NetworkingPrivateStartDisconnectFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Success();
  void Failure(const std::string& error);
};

// Implements the chrome.networkingPrivate.startActivate method.
class NetworkingPrivateStartActivateFunction : public ExtensionFunction {
 public:
  NetworkingPrivateStartActivateFunction() = default;

  NetworkingPrivateStartActivateFunction(
      const NetworkingPrivateStartActivateFunction&) = delete;
  NetworkingPrivateStartActivateFunction& operator=(
      const NetworkingPrivateStartActivateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.startActivate",
                             NETWORKINGPRIVATE_STARTACTIVATE)

 protected:
  ~NetworkingPrivateStartActivateFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Success();
  void Failure(const std::string& error);
};

class NetworkingPrivateGetCaptivePortalStatusFunction
    : public ExtensionFunction {
 public:
  NetworkingPrivateGetCaptivePortalStatusFunction() = default;

  NetworkingPrivateGetCaptivePortalStatusFunction(
      const NetworkingPrivateGetCaptivePortalStatusFunction&) = delete;
  NetworkingPrivateGetCaptivePortalStatusFunction& operator=(
      const NetworkingPrivateGetCaptivePortalStatusFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getCaptivePortalStatus",
                             NETWORKINGPRIVATE_GETCAPTIVEPORTALSTATUS)

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~NetworkingPrivateGetCaptivePortalStatusFunction() override = default;

 private:
  void Success(const std::string& result);
  void Failure(const std::string& error);
};

class NetworkingPrivateUnlockCellularSimFunction : public ExtensionFunction {
 public:
  NetworkingPrivateUnlockCellularSimFunction() = default;

  NetworkingPrivateUnlockCellularSimFunction(
      const NetworkingPrivateUnlockCellularSimFunction&) = delete;
  NetworkingPrivateUnlockCellularSimFunction& operator=(
      const NetworkingPrivateUnlockCellularSimFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.unlockCellularSim",
                             NETWORKINGPRIVATE_UNLOCKCELLULARSIM)

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~NetworkingPrivateUnlockCellularSimFunction() override = default;

 private:
  void Success();
  void Failure(const std::string& error);
};

class NetworkingPrivateSetCellularSimStateFunction : public ExtensionFunction {
 public:
  NetworkingPrivateSetCellularSimStateFunction() = default;

  NetworkingPrivateSetCellularSimStateFunction(
      const NetworkingPrivateSetCellularSimStateFunction&) = delete;
  NetworkingPrivateSetCellularSimStateFunction& operator=(
      const NetworkingPrivateSetCellularSimStateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.setCellularSimState",
                             NETWORKINGPRIVATE_SETCELLULARSIMSTATE)

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~NetworkingPrivateSetCellularSimStateFunction() override = default;

 private:
  void Success();
  void Failure(const std::string& error);
};

class NetworkingPrivateSelectCellularMobileNetworkFunction
    : public ExtensionFunction {
 public:
  NetworkingPrivateSelectCellularMobileNetworkFunction() = default;

  NetworkingPrivateSelectCellularMobileNetworkFunction(
      const NetworkingPrivateSelectCellularMobileNetworkFunction&) = delete;
  NetworkingPrivateSelectCellularMobileNetworkFunction& operator=(
      const NetworkingPrivateSelectCellularMobileNetworkFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.selectCellularMobileNetwork",
                             NETWORKINGPRIVATE_SELECTCELLULARMOBILENETWORK)

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~NetworkingPrivateSelectCellularMobileNetworkFunction() override = default;

 private:
  void Success();
  void Failure(const std::string& error);
};

class NetworkingPrivateGetGlobalPolicyFunction : public ExtensionFunction {
 public:
  NetworkingPrivateGetGlobalPolicyFunction() = default;

  NetworkingPrivateGetGlobalPolicyFunction(
      const NetworkingPrivateGetGlobalPolicyFunction&) = delete;
  NetworkingPrivateGetGlobalPolicyFunction& operator=(
      const NetworkingPrivateGetGlobalPolicyFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getGlobalPolicy",
                             NETWORKINGPRIVATE_GETGLOBALPOLICY)

 protected:
  ~NetworkingPrivateGetGlobalPolicyFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Result(std::optional<base::Value::Dict> global_policies);
};

class NetworkingPrivateGetCertificateListsFunction : public ExtensionFunction {
 public:
  NetworkingPrivateGetCertificateListsFunction() = default;

  NetworkingPrivateGetCertificateListsFunction(
      const NetworkingPrivateGetCertificateListsFunction&) = delete;
  NetworkingPrivateGetCertificateListsFunction& operator=(
      const NetworkingPrivateGetCertificateListsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("networkingPrivate.getCertificateLists",
                             NETWORKINGPRIVATE_GETCERTIFICATELISTS)

 protected:
  ~NetworkingPrivateGetCertificateListsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Result(base::Value::Dict certificate_list);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_API_H_
