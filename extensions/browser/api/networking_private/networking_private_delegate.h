// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/api/networking_private.h"

namespace extensions {

class NetworkingPrivateDelegateObserver;

// Base class for platform dependent networkingPrivate API implementations.
// All inputs and results for this class use ONC values. See
// networking_private.idl for descriptions of the expected inputs and results.
class NetworkingPrivateDelegate : public KeyedService {
 public:
  using DictionaryCallback = base::OnceCallback<void(base::Value::Dict)>;
  using VoidCallback = base::OnceCallback<void()>;
  using BoolCallback = base::OnceCallback<void(bool)>;
  using StringCallback = base::OnceCallback<void(const std::string&)>;
  using NetworkListCallback = base::OnceCallback<void(base::Value::List)>;
  using EnabledNetworkTypesCallback =
      base::OnceCallback<void(base::Value::List)>;
  using FailureCallback = base::OnceCallback<void(const std::string&)>;
  using DeviceStateList =
      std::vector<api::networking_private::DeviceStateProperties>;
  using DeviceStateListCallback =
      base::OnceCallback<void(std::optional<DeviceStateList>)>;
  using GetGlobalPolicyCallback =
      base::OnceCallback<void(std::optional<base::Value::Dict>)>;
  using GetCertificateListsCallback =
      base::OnceCallback<void(base::Value::Dict)>;

  // Returns |result| on success, or |result|=nullopt and |error| on failure.
  using PropertiesCallback =
      base::OnceCallback<void(std::optional<base::Value::Dict> result,
                              const std::optional<std::string>& error)>;

  // Delegate for forwarding UI requests, e.g. for showing the account UI.
  class UIDelegate {
   public:
    UIDelegate();

    UIDelegate(const UIDelegate&) = delete;
    UIDelegate& operator=(const UIDelegate&) = delete;

    virtual ~UIDelegate();

    // Navigate to the account details page for the cellular network associated
    // with |guid|.
    virtual void ShowAccountDetails(const std::string& guid) const = 0;
  };

  NetworkingPrivateDelegate();

  NetworkingPrivateDelegate(const NetworkingPrivateDelegate&) = delete;
  NetworkingPrivateDelegate& operator=(const NetworkingPrivateDelegate&) =
      delete;

  ~NetworkingPrivateDelegate() override;

  void set_ui_delegate(std::unique_ptr<UIDelegate> ui_delegate) {
    ui_delegate_ = std::move(ui_delegate);
  }

  const UIDelegate* ui_delegate() { return ui_delegate_.get(); }

  // All methods are asynchronous
  virtual void GetProperties(const std::string& guid,
                             PropertiesCallback callback) = 0;
  virtual void GetManagedProperties(const std::string& guid,
                                    PropertiesCallback callback) = 0;
  virtual void GetState(const std::string& guid,
                        DictionaryCallback success_callback,
                        FailureCallback failure_callback) = 0;
  virtual void SetProperties(const std::string& guid,
                             base::Value::Dict properties,
                             bool allow_set_shared_config,
                             VoidCallback success_callback,
                             FailureCallback failure_callback) = 0;
  virtual void CreateNetwork(bool shared,
                             base::Value::Dict properties,
                             StringCallback success_callback,
                             FailureCallback failure_callback) = 0;
  virtual void ForgetNetwork(const std::string& guid,
                             bool allow_forget_shared_config,
                             VoidCallback success_callback,
                             FailureCallback failure_callback) = 0;
  virtual void GetNetworks(const std::string& network_type,
                           bool configured_only,
                           bool visible_only,
                           int limit,
                           NetworkListCallback success_callback,
                           FailureCallback failure_callback) = 0;
  virtual void StartConnect(const std::string& guid,
                            VoidCallback success_callback,
                            FailureCallback failure_callback) = 0;
  virtual void StartDisconnect(const std::string& guid,
                               VoidCallback success_callback,
                               FailureCallback failure_callback) = 0;
  virtual void StartActivate(const std::string& guid,
                             const std::string& carrier,
                             VoidCallback success_callback,
                             FailureCallback failure_callback);
  virtual void GetCaptivePortalStatus(const std::string& guid,
                                      StringCallback success_callback,
                                      FailureCallback failure_callback) = 0;
  virtual void UnlockCellularSim(const std::string& guid,
                                 const std::string& pin,
                                 const std::string& puk,
                                 VoidCallback success_callback,
                                 FailureCallback failure_callback) = 0;
  virtual void SetCellularSimState(const std::string& guid,
                                   bool require_pin,
                                   const std::string& current_pin,
                                   const std::string& new_pin,
                                   VoidCallback success_callback,
                                   FailureCallback failure_callback) = 0;
  virtual void SelectCellularMobileNetwork(
      const std::string& guid,
      const std::string& network_id,
      VoidCallback success_callback,
      FailureCallback failure_callback) = 0;

  // Returns a list of ONC type strings.
  virtual void GetEnabledNetworkTypes(EnabledNetworkTypesCallback callback) = 0;

  // Returns a list of DeviceStateProperties.
  virtual void GetDeviceStateList(DeviceStateListCallback callback) = 0;

  // Returns a dictionary of global policy values (may be empty). Note: the
  // dictionary is expected to be a superset of the networkingPrivate
  // GlobalPolicy dictionary. Any properties not in GlobalPolicy will be
  // ignored.
  virtual void GetGlobalPolicy(GetGlobalPolicyCallback callback) = 0;

  // Returns a dictionary of certificate lists.
  virtual void GetCertificateLists(GetCertificateListsCallback callback) = 0;

  // Returns true if the ONC network type |type| is enabled.
  virtual void EnableNetworkType(const std::string& type,
                                 BoolCallback callback) = 0;

  // Returns true if the ONC network type |type| is disabled.
  virtual void DisableNetworkType(const std::string& type,
                                  BoolCallback callback) = 0;

  // Returns true if a scan was requested. It may take many seconds for a scan
  // to complete. The scan may or may not trigger API events when complete.
  // |type| is the type of network to request a scan for; if empty, scans for
  // all supported network types except Cellular, which must be requested
  // explicitly.
  virtual void RequestScan(const std::string& type, BoolCallback callback) = 0;

  // These functions are "fire and forget" - so in a way synchronous and not.

  // Optional methods for adding a NetworkingPrivateDelegateObserver for
  // implementations that require it (non-chromeos).
  virtual void AddObserver(NetworkingPrivateDelegateObserver* observer);
  virtual void RemoveObserver(NetworkingPrivateDelegateObserver* observer);

 private:
  // Interface for UI methods. May be null.
  std::unique_ptr<UIDelegate> ui_delegate_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_H_
