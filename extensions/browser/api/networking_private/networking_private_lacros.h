// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_LACROS_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_LACROS_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "extensions/browser/api/networking_private/lacros_networking_private_observer.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Lacros NetworkingPrivateDelegate implementation.

class NetworkingPrivateLacros : public NetworkingPrivateDelegate {
 public:
  explicit NetworkingPrivateLacros(content::BrowserContext* browser_context);

  NetworkingPrivateLacros(const NetworkingPrivateLacros&) = delete;
  NetworkingPrivateLacros& operator=(const NetworkingPrivateLacros&) = delete;

  ~NetworkingPrivateLacros() override;

  // NetworkingPrivateApi
  void GetProperties(const std::string& guid,
                     PropertiesCallback callback) override;
  void GetManagedProperties(const std::string& guid,
                            PropertiesCallback callback) override;
  void GetState(const std::string& guid,
                DictionaryCallback success_callback,
                FailureCallback failure_callback) override;
  void SetProperties(const std::string& guid,
                     base::Value properties,
                     bool allow_set_shared_config,
                     VoidCallback success_callback,
                     FailureCallback failure_callback) override;
  void CreateNetwork(bool shared,
                     base::Value properties,
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

  void AddObserver(NetworkingPrivateDelegateObserver* observer) override;
  void RemoveObserver(NetworkingPrivateDelegateObserver* observer) override;

 private:
  const bool is_primary_user_;

  std::unique_ptr<LacrosNetworkingPrivateObserver> lacros_observer_;

  base::WeakPtrFactory<NetworkingPrivateLacros> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_LACROS_H_
