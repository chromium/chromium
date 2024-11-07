// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/service_client.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-shared.h"

namespace on_device_model {

ServiceClient::ServiceClient(ServiceClient::LaunchFn launch_fn)
    : launch_fn_(launch_fn), weak_ptr_factory_(this) {}
ServiceClient::~ServiceClient() = default;

ServiceClient::Remote& ServiceClient::Get() {
  if (!remote_) {
    launch_fn_.Run(remote_.BindNewPipeAndPassReceiver());
    remote_.set_disconnect_with_reason_handler(
        base::BindOnce(&ServiceClient::OnDisconnect, base::Unretained(this))
            .Then(on_disconnect_fn_));
    remote_.reset_on_idle_timeout(base::TimeDelta());
  }
  return remote_;
}

ServiceDisconnectReason ServiceClient::OnDisconnect(
    uint32_t custom_reason,
    const std::string& description) {
  remote_.reset();
  LOG(ERROR) << "Unexpected on_device_model service disconnect: "
             << description;
  switch (custom_reason) {
    case static_cast<uint32_t>(ServiceDisconnectReason::kGpuBlocked):
      return ServiceDisconnectReason::kGpuBlocked;
    case static_cast<uint32_t>(ServiceDisconnectReason::kFailedToLoadLibrary):
      return ServiceDisconnectReason::kFailedToLoadLibrary;
    default:
      return ServiceDisconnectReason::kUnspecified;
  }
}

void ServiceClient::AddPendingUsage() {
  if (pending_uses_ == 0) {
    // Start the service if necessary, and set a longer timeout to keep it
    // alive while waiting for the usage.
    Get().reset_on_idle_timeout(base::Minutes(1));
  }
  pending_uses_++;
}

void ServiceClient::RemovePendingUsage() {
  pending_uses_--;
  if (pending_uses_ == 0) {
    remote_.reset_on_idle_timeout(base::TimeDelta());
  }
}

}  // namespace on_device_model
