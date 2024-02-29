// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CHROMEOS_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CHROMEOS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Chrome OS NetworkingPrivateDelegate implementation.

class NetworkingPrivateChromeOS : public NetworkingPrivateDelegate {
 public:
  // |verify_delegate| is passed to NetworkingPrivateDelegate and may be NULL.
  explicit NetworkingPrivateChromeOS(content::BrowserContext* browser_context);

  NetworkingPrivateChromeOS(const NetworkingPrivateChromeOS&) = delete;
  NetworkingPrivateChromeOS& operator=(const NetworkingPrivateChromeOS&) =
      delete;

  ~NetworkingPrivateChromeOS() override;

  // NetworkingPrivateApi
  void GetProperties(const std::string& guid,
                     PropertiesCallback callback) override;
  void GetManagedProperties(const std::string& guid,
                            PropertiesCallback callback) override;
  void GetState(const std::string& guid,
                DictionaryCallback success_callback,
                FailureCallback failure_callback) override;
  void SetProperties(const std::string& guid,
                     base::Value::Dict properties,
                     bool allow_set_shared_config,
                     VoidCallback success_callback,
                     FailureCallback failure_callback) override;
  void CreateNetwork(bool shared,
                     base::Value::Dict properties,
                     StringCallback success_callback,
                     FailureCallback failure_callback) override;
  void ForgetNetwork(const std::string& guid,
                     bool allow_forget_shared_config,
                     VoidCallback success_callback,
                     FailureCallback failure_callback) override;
  void GetNetworks(const std::string& network_type,
                   bool configured_only,
                   bool visible_only,
                   int limit,
                   NetworkListCallback success_callback,
                   FailureCallback failure_callback) override;
  void StartConnect(const std::string& guid,
                    VoidCallback success_callback,
                    FailureCallback failure_callback) override;
  void StartDisconnect(const std::string& guid,
                       VoidCallback success_callback,
                       FailureCallback failure_callback) override;
  void StartActivate(const std::string& guid,
                     const std::string& carrier,
                     VoidCallback success_callback,
                     FailureCallback failure_callback) override;
  void GetCaptivePortalStatus(const std::string& guid,
                              StringCallback success_callback,
                              FailureCallback failure_callback) override;
  void UnlockCellularSim(const std::string& guid,
                         const std::string& pin,
                         const std::string& puk,
                         VoidCallback success_callback,
                         FailureCallback failure_callback) override;
  void SetCellularSimState(const std::string& guid,
                           bool require_pin,
                           const std::string& current_pin,
                           const std::string& new_pin,
                           VoidCallback success_callback,
                           FailureCallback failure_callback) override;
  void SelectCellularMobileNetwork(const std::string& guid,
                                   const std::string& network_id,
                                   VoidCallback success_callback,
                                   FailureCallback failure_callback) override;
  void GetEnabledNetworkTypes(EnabledNetworkTypesCallback callback) override;
  void GetDeviceStateList(DeviceStateListCallback callback) override;
  void GetGlobalPolicy(GetGlobalPolicyCallback callback) override;
  void GetCertificateLists(GetCertificateListsCallback callback) override;
  void EnableNetworkType(const std::string& type,
                         BoolCallback callback) override;
  void DisableNetworkType(const std::string& type,
                          BoolCallback callback) override;
  void RequestScan(const std::string& type, BoolCallback callback) override;

 private:
  // Callback for both GetProperties and GetManagedProperties. Copies
  // |dictionary| and appends any networkingPrivate API specific properties,
  // then calls |callback| with the result.
  void GetPropertiesCallback(const std::string& guid,
                             PropertiesCallback callback,
                             const std::string& service_path,
                             std::optional<base::Value::Dict> dictionary,
                             std::optional<std::string> error);

  // Populate ThirdPartyVPN.ProviderName with the provider name for third-party
  // VPNs. The provider name needs to be looked up from the list of extensions
  // which is not available to the chromeos/ash/components/network module.
  void AppendThirdPartyProviderName(base::Value::Dict* dictionary);

  raw_ptr<content::BrowserContext> browser_context_;
  base::WeakPtrFactory<NetworkingPrivateChromeOS> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CHROMEOS_H_
