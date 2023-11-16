// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_lacros.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/networking_private.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/networking_private.h"

using extensions::NetworkingPrivateDelegate;

namespace private_api = extensions::api::networking_private;

namespace {

// Error message to signal that the API was not found on the Ash side..
constexpr char kErrorApiNotFound[] = "Error.ApiNotFound";

// Error message to signal that the user is not authorized for this API.
constexpr char kErrorNotPrimaryUser[] = "Error.OnlyCallableByPrimaryUser";

// Get the networking private API (or null if not available).
mojo::Remote<crosapi::mojom::NetworkingPrivate>* GetNetworkingPrivateRemote() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::NetworkingPrivate>()) {
    return &(lacros_service->GetRemote<crosapi::mojom::NetworkingPrivate>());
  }
  return nullptr;
}

// Get the networking private API, passing in if |is_primary_user| and if an
// error occurs, the failure |callback| gets called.
mojo::Remote<crosapi::mojom::NetworkingPrivate>*
GetNetworkingPrivateRemoteAndCheck(
    bool is_primary_user,
    extensions::NetworkingPrivateDelegate::FailureCallback& callback) {
  if (!is_primary_user) {
    std::move(callback).Run(kErrorNotPrimaryUser);
    return nullptr;
  }
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private) {
    std::move(callback).Run(kErrorApiNotFound);
  }
  return networking_private;
}

// Find out if the given context is owned by the primary user.
bool IsPrimaryUser(content::BrowserContext* browser_context) {
  return extensions::ExtensionsBrowserClient::Get()->IsFromMainProfile(
      browser_context);
}

// Following are several adapter callbacks which make the mojo callbacks
// compatible with the NetworkingPrivateDelegate callbacks.
//
// Some The crosapi::mojom::NetworkingPrivate calls expect a single callback,
// whereas the API passes in two (one for success and one for failure).
// A failure will be signaled by a non empty string. These functions will
// merge them together again.
//
// See networking_private_ash.cc for the reverse operation.

// This function merges the void and the error callback into a mojo callback.
using VoidSuccessOrFailureCallback =
    base::OnceCallback<void(const std::string& error_message)>;

VoidSuccessOrFailureCallback VoidAdapterCallback(
    extensions::NetworkingPrivateDelegate::VoidCallback success,
    extensions::NetworkingPrivateDelegate::FailureCallback failure) {
  return base::BindOnce(
      [](extensions::NetworkingPrivateDelegate::VoidCallback success,
         extensions::NetworkingPrivateDelegate::FailureCallback failure,
         const std::string& error) {
        if (!error.empty()) {
          std::move(failure).Run(error);
        } else {
          std::move(success).Run();
        }
      },
      std::move(success), std::move(failure));
}

// This function merges the string and the error callback into a mojo callback.
using StringSuccessOrFailureCallback = base::OnceCallback<void(
    crosapi::mojom::StringSuccessOrErrorReturnPtr result)>;

StringSuccessOrFailureCallback StringAdapterCallback(
    extensions::NetworkingPrivateDelegate::StringCallback success,
    extensions::NetworkingPrivateDelegate::FailureCallback failure) {
  return base::BindOnce(
      [](extensions::NetworkingPrivateDelegate::StringCallback success,
         extensions::NetworkingPrivateDelegate::FailureCallback failure,
         crosapi::mojom::StringSuccessOrErrorReturnPtr result) {
        if (result->is_error()) {
          std::move(failure).Run(result->get_error());
        } else {
          std::move(success).Run(result->get_success_result());
        }
      },
      std::move(success), std::move(failure));
}

// This function merges the dictionary and the error callback into a mojo
// callback.
using DictionarySuccessOrFailureCallback = base::OnceCallback<void(
    crosapi::mojom::DictionarySuccessOrErrorReturnPtr result)>;

DictionarySuccessOrFailureCallback DictionaryAdapterCallback(
    extensions::NetworkingPrivateDelegate::DictionaryCallback success,
    extensions::NetworkingPrivateDelegate::FailureCallback failure) {
  return base::BindOnce(
      [](extensions::NetworkingPrivateDelegate::DictionaryCallback success,
         extensions::NetworkingPrivateDelegate::FailureCallback failure,
         crosapi::mojom::DictionarySuccessOrErrorReturnPtr result) {
        if (result->is_error()) {
          std::move(failure).Run(result->get_error());
        } else {
          std::move(success).Run(std::move(result->get_success_result()));
        }
      },
      std::move(success), std::move(failure));
}

// This function merges the list and the error callback into a mojo callback.
using ListValueSuccessOrFailureCallback = base::OnceCallback<void(
    crosapi::mojom::ListValueSuccessOrErrorReturnPtr result)>;

ListValueSuccessOrFailureCallback ListValueAdapterCallback(
    extensions::NetworkingPrivateDelegate::NetworkListCallback success,
    extensions::NetworkingPrivateDelegate::FailureCallback failure) {
  return base::BindOnce(
      [](extensions::NetworkingPrivateDelegate::NetworkListCallback success,
         extensions::NetworkingPrivateDelegate::FailureCallback failure,
         crosapi::mojom::ListValueSuccessOrErrorReturnPtr result) {
        if (result->is_error()) {
          std::move(failure).Run(result->get_error());
        } else {
          std::move(success).Run(std::move(result->get_success_result()));
        }
      },
      std::move(success), std::move(failure));
}

// This adapter will handle the call back from ash which passes back a
// base::Value::List object.
using ValueListMojoCallback =
    base::OnceCallback<void(std::optional<base::Value::List>)>;
using ValueListDelegateCallback =
    base::OnceCallback<void(base::Value::List result)>;
ValueListMojoCallback ValueListAdapterCallback(
    ValueListDelegateCallback result_callback) {
  return base::BindOnce(
      [](ValueListDelegateCallback callback,
         std::optional<base::Value::List> result) {
        if (!result) {
          std::move(callback).Run(base::Value::List());
        } else {
          std::move(callback).Run(std::move(*result));
        }
      },
      std::move(result_callback));
}

// This adapter will handle the properties call from Mojo back to Lacros using
// the PropertiesSuccessOrErrorReturn mojo union of possible results.
// It assumes that there can be either a result - or an error, but not both.
using PropertiesMojoCallback = base::OnceCallback<void(
    crosapi::mojom::PropertiesSuccessOrErrorReturnPtr result)>;
using PropertiesDelegateCallback =
    base::OnceCallback<void(std::optional<::base::Value::Dict> result,
                            const std::optional<std::string>& error)>;

PropertiesMojoCallback PropertiesAdapterCallback(
    PropertiesDelegateCallback result_callback) {
  return base::BindOnce(
      [](PropertiesDelegateCallback callback,
         crosapi::mojom::PropertiesSuccessOrErrorReturnPtr result) {
        if (result->is_error()) {
          std::move(callback).Run(std::nullopt, std::move(result->get_error()));
        } else {
          std::move(callback).Run(std::optional<::base::Value::Dict>(std::move(
                                      result->get_success_result().GetDict())),
                                  std::nullopt);
        }
      },
      std::move(result_callback));
}

// Converting the crosapi::mojom::GetDeviceStateList returned value into the
// internally used datastructure DeviceStateList and forward it to the callback
// handler from the caller.
using DeviceStateListPtr =
    std::optional<std::vector<std::optional<::base::Value::Dict>>>;

void DeviceStateListCallbackAdapter(
    extensions::NetworkingPrivateDelegate::DeviceStateListCallback callback,
    DeviceStateListPtr result) {
  if (!result) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  auto list =
      std::optional<extensions::NetworkingPrivateDelegate::DeviceStateList>();
  for (auto& item : *result) {
    if (item) {
      list->emplace_back(
          extensions::api::networking_private::DeviceStateProperties::FromValue(
              base::Value(std::move(*item)))
              .value());
    } else {
      list->emplace_back();
    }
  }
  std::move(callback).Run(std::move(list));
}

}  // namespace

namespace extensions {

NetworkingPrivateLacros::NetworkingPrivateLacros(
    content::BrowserContext* browser_context)
    : is_primary_user_(IsPrimaryUser(browser_context)) {}

NetworkingPrivateLacros::~NetworkingPrivateLacros() = default;

void NetworkingPrivateLacros::GetProperties(const std::string& guid,
                                            PropertiesCallback callback) {
  if (!is_primary_user_) {
    std::move(callback).Run(std::nullopt, kErrorNotPrimaryUser);
    return;
  }
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private) {
    std::move(callback).Run(std::nullopt, kErrorApiNotFound);
    return;
  }
  (*networking_private)
      ->GetProperties(std::move(guid),
                      PropertiesAdapterCallback(std::move(callback)));
}

void NetworkingPrivateLacros::GetManagedProperties(
    const std::string& guid,
    PropertiesCallback callback) {
  if (!is_primary_user_) {
    std::move(callback).Run(std::nullopt, kErrorNotPrimaryUser);
    return;
  }
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private) {
    std::move(callback).Run(std::nullopt, kErrorApiNotFound);
    return;
  }
  (*networking_private)
      ->GetManagedProperties(std::move(guid),
                             PropertiesAdapterCallback(std::move(callback)));
}

void NetworkingPrivateLacros::GetState(const std::string& guid,
                                       DictionaryCallback success_callback,
                                       FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->GetState(std::move(guid),
                 DictionaryAdapterCallback(std::move(success_callback),
                                           std::move(failure_callback)));
}

void NetworkingPrivateLacros::SetProperties(const std::string& guid,
                                            base::Value::Dict properties,
                                            bool allow_set_shared_config,
                                            VoidCallback success_callback,
                                            FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->SetProperties(std::move(guid), std::move(properties),
                      allow_set_shared_config,
                      VoidAdapterCallback(std::move(success_callback),
                                          std::move(failure_callback)));
}

void NetworkingPrivateLacros::CreateNetwork(bool shared,
                                            base::Value::Dict properties,
                                            StringCallback success_callback,
                                            FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->CreateNetwork(shared, base::Value(std::move(properties)),
                      StringAdapterCallback(std::move(success_callback),
                                            std::move(failure_callback)));
}

void NetworkingPrivateLacros::ForgetNetwork(const std::string& guid,
                                            bool allow_forget_shared_config,
                                            VoidCallback success_callback,
                                            FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->ForgetNetwork(std::move(guid), allow_forget_shared_config,
                      VoidAdapterCallback(std::move(success_callback),
                                          std::move(failure_callback)));
}

void NetworkingPrivateLacros::GetNetworks(const std::string& network_type,
                                          bool configured_only,
                                          bool visible_only,
                                          int limit,
                                          NetworkListCallback success_callback,
                                          FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->GetNetworks(std::move(network_type), configured_only, visible_only,
                    limit,
                    ListValueAdapterCallback(std::move(success_callback),
                                             std::move(failure_callback)));
}

void NetworkingPrivateLacros::StartConnect(const std::string& guid,
                                           VoidCallback success_callback,
                                           FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->StartConnect(std::move(guid),
                     VoidAdapterCallback(std::move(success_callback),
                                         std::move(failure_callback)));
}

void NetworkingPrivateLacros::StartDisconnect(
    const std::string& guid,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->StartDisconnect(std::move(guid),
                        VoidAdapterCallback(std::move(success_callback),
                                            std::move(failure_callback)));
}

void NetworkingPrivateLacros::StartActivate(
    const std::string& guid,
    const std::string& specified_carrier,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->StartActivate(std::move(guid), std::move(specified_carrier),
                      VoidAdapterCallback(std::move(success_callback),
                                          std::move(failure_callback)));
}

void NetworkingPrivateLacros::GetCaptivePortalStatus(
    const std::string& guid,
    StringCallback success_callback,
    FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->GetCaptivePortalStatus(
          std::move(guid), StringAdapterCallback(std::move(success_callback),
                                                 std::move(failure_callback)));
}

void NetworkingPrivateLacros::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->UnlockCellularSim(std::move(guid), std::move(pin), std::move(puk),
                          VoidAdapterCallback(std::move(success_callback),
                                              std::move(failure_callback)));
}

void NetworkingPrivateLacros::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->SetCellularSimState(std::move(guid), require_pin,
                            std::move(current_pin), std::move(new_pin),
                            VoidAdapterCallback(std::move(success_callback),
                                                std::move(failure_callback)));
}

void NetworkingPrivateLacros::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  auto* networking_private =
      GetNetworkingPrivateRemoteAndCheck(is_primary_user_, failure_callback);
  if (!networking_private) {
    return;
  }

  (*networking_private)
      ->SelectCellularMobileNetwork(
          std::move(guid), std::move(network_id),
          VoidAdapterCallback(std::move(success_callback),
                              std::move(failure_callback)));
}

void NetworkingPrivateLacros::GetEnabledNetworkTypes(
    EnabledNetworkTypesCallback callback) {
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private) {
    std::move(callback).Run(base::Value::List());
    return;
  }
  (*networking_private)
      ->GetEnabledNetworkTypes(ValueListAdapterCallback(std::move(callback)));
}

void NetworkingPrivateLacros::GetDeviceStateList(
    DeviceStateListCallback callback) {
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private || !is_primary_user_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  (*networking_private)
      ->GetDeviceStateList(
          base::BindOnce(&DeviceStateListCallbackAdapter, std::move(callback)));
}

void NetworkingPrivateLacros::GetGlobalPolicy(
    GetGlobalPolicyCallback callback) {
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private || !is_primary_user_) {
    std::move(callback).Run(base::Value::Dict());
    return;
  }
  (*networking_private)->GetGlobalPolicy(std::move(callback));
}

void NetworkingPrivateLacros::GetCertificateLists(
    GetCertificateListsCallback callback) {
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private || !is_primary_user_) {
    std::move(callback).Run(base::Value::Dict());
    return;
  }
  (*networking_private)->GetCertificateLists(std::move(callback));
}

void NetworkingPrivateLacros::EnableNetworkType(const std::string& type,
                                                BoolCallback callback) {
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private || !is_primary_user_) {
    std::move(callback).Run(false);
    return;
  }
  (*networking_private)
      ->EnableNetworkType(std::move(type), std::move(callback));
}

void NetworkingPrivateLacros::DisableNetworkType(const std::string& type,
                                                 BoolCallback callback) {
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private || !is_primary_user_) {
    std::move(callback).Run(false);
    return;
  }
  (*networking_private)
      ->DisableNetworkType(std::move(type), std::move(callback));
}

void NetworkingPrivateLacros::RequestScan(const std::string& type,
                                          BoolCallback callback) {
  auto* networking_private = GetNetworkingPrivateRemote();
  if (!networking_private || !is_primary_user_) {
    std::move(callback).Run(false);
    return;
  }
  (*networking_private)->RequestScan(std::move(type), std::move(callback));
}

void NetworkingPrivateLacros::AddObserver(
    NetworkingPrivateDelegateObserver* observer) {
  if (!lacros_observer_) {
    lacros_observer_ = std::make_unique<LacrosNetworkingPrivateObserver>();
  }
  lacros_observer_->AddObserver(observer);
}

void NetworkingPrivateLacros::RemoveObserver(
    NetworkingPrivateDelegateObserver* observer) {
  if (!lacros_observer_) {
    return;
  }

  lacros_observer_->RemoveObserver(observer);

  if (!lacros_observer_->HasObservers()) {
    lacros_observer_.reset();
  }
}

}  // namespace extensions
