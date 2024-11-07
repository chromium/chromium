// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_SERVICE_CLIENT_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_SERVICE_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

// Manages a remote that can timeout and reconnect on-demand.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) ServiceClient final {
 public:
  using Remote = ::mojo::Remote<mojom::OnDeviceModelService>;
  using PendingReceiver = ::mojo::PendingReceiver<mojom::OnDeviceModelService>;
  using LaunchFn = ::base::RepeatingCallback<void(PendingReceiver)>;

  explicit ServiceClient(LaunchFn launch_fn);
  ~ServiceClient();

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
  LaunchFn launch_fn_;
  int pending_uses_ = 0;
  Remote remote_;
  base::WeakPtrFactory<ServiceClient> weak_ptr_factory_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_SERVICE_CLIENT_H_
