// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_SERVICE_CLIENT_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_SERVICE_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

// The reason given for the service disconnect.
enum class ServiceDisconnectReason : uint32_t {
  // No reason provided, likely a service crash or similar error.
  kUnspecified = 0,
  // The device's GPU is unsupported.
  kGpuBlocked = 1,
  // The chrome_ml shared library could not be loaded.
  kFailedToLoadLibrary = 2,
};

// Manages a remote that can timeout and reconnect on-demand.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) ServiceClient final {
 public:
  using Remote = ::mojo::Remote<mojom::OnDeviceModelService>;
  using PendingReceiver = ::mojo::PendingReceiver<mojom::OnDeviceModelService>;
  using LaunchFn = ::base::RepeatingCallback<void(PendingReceiver)>;
  using OnDisconnectFn =
      ::base::RepeatingCallback<void(ServiceDisconnectReason)>;

  explicit ServiceClient(LaunchFn launch_fn);
  ~ServiceClient();

  void set_on_disconnect_fn(OnDisconnectFn on_disconnect_fn) {
    on_disconnect_fn_ = std::move(on_disconnect_fn);
  }

  // Get the service remote, launching the service if it's not already bound.
  Remote& Get();

  base::WeakPtr<ServiceClient> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool is_bound() { return remote_.is_bound(); }

  // Notify of a pending use, warming up the service and temporarily extending
  // idle timeout.
  void AddPendingUsage();

  // Remove a pending usage, allowing the idle timeout to be shortened.
  void RemovePendingUsage();

 private:
  ServiceDisconnectReason OnDisconnect(uint32_t custom_reason,
                                       const std::string& description);

  LaunchFn launch_fn_;
  OnDisconnectFn on_disconnect_fn_ =
      base::DoNothingAs<void(ServiceDisconnectReason)>();
  int pending_uses_ = 0;
  Remote remote_;
  base::WeakPtrFactory<ServiceClient> weak_ptr_factory_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_SERVICE_CLIENT_H_
