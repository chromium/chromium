// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_discovery.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/webauth/scoped_virtual_authenticator_environment.h"
#include "device/fido/fido_device.h"

namespace content {

VirtualFidoDiscovery::VirtualFidoDiscovery(
    ScopedVirtualAuthenticatorEnvironment* environment,
    ::device::FidoTransportProtocol transport)
    : FidoDeviceDiscovery(transport), environment_(environment) {}

VirtualFidoDiscovery::~VirtualFidoDiscovery() {
  environment_->OnDiscoveryDestroyed(this);
}

void VirtualFidoDiscovery::AddVirtualDevice(
    std::unique_ptr<::device::FidoDevice> device) {
  // The real implementation would never notify the client's observer about
  // devices before the client calls Start(), mimic the same behavior.
  if (is_start_requested()) {
    FidoDeviceDiscovery::AddDevice(std::move(device));
  } else {
    devices_pending_discovery_start_.push_back(std::move(device));
  }
}

bool VirtualFidoDiscovery::RemoveVirtualDevice(base::StringPiece device_id) {
  DCHECK(is_start_requested());
  return ::device::FidoDeviceDiscovery::RemoveDevice(device_id);
}

void VirtualFidoDiscovery::StartInternal() {
  for (auto& device : devices_pending_discovery_start_)
    FidoDeviceDiscovery::AddDevice(std::move(device));
  devices_pending_discovery_start_.clear();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&VirtualFidoDiscovery::NotifyDiscoveryStarted,
                                base::Unretained(this), true /* success */));
}

}  // namespace content
