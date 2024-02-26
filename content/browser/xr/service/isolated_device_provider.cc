// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/isolated_device_provider.h"

#include "base/functional/bind.h"
#include "content/browser/xr/service/xr_device_service.h"
#include "content/browser/xr/xr_utils.h"
#include "content/public/browser/xr_integration_client.h"

namespace {
constexpr int kMaxRetries = 3;
}

namespace content {

void IsolatedVRDeviceProvider::Initialize(
    device::VRDeviceProviderClient* client,
    content::WebContents* initializing_web_contents) {
  client_ = client;
  SetupDeviceProvider();
}

bool IsolatedVRDeviceProvider::Initialized() {
  return initialized_;
}

void IsolatedVRDeviceProvider::OnDeviceAdded(
    mojo::PendingRemote<device::mojom::XRRuntime> device,
    device::mojom::XRDeviceDataPtr device_data,
    device::mojom::XRDeviceId device_id) {
  client_->AddRuntime(device_id, std::move(device_data), std::move(device));
  active_device_ids_.insert(device_id);
}

void IsolatedVRDeviceProvider::OnDeviceRemoved(device::mojom::XRDeviceId id) {
  client_->RemoveRuntime(id);
  active_device_ids_.erase(id);
}

void IsolatedVRDeviceProvider::OnServerError() {
  // An error occurred - any devices we have added are now disconnected and
  // should be removed.
  for (auto& id : active_device_ids_) {
    client_->RemoveRuntime(id);
  }
  active_device_ids_.clear();

  // At this point, XRRuntimeManagerImpl may be blocked waiting for us to return
  // that we've enumerated all runtimes/devices.  If we lost the connection to
  // the service, we'll try again.  If we've already tried too many times,
  // then just assume we won't ever get devices, so report we are done now.
  // This will unblock WebXR/WebVR promises so they can reject indicating we
  // never found devices.
  if (!initialized_ && retry_count_ >= kMaxRetries) {
    OnDevicesEnumerated();
  } else {
    device_provider_.reset();
    receiver_.reset();
    retry_count_++;
    SetupDeviceProvider();
  }
}

void IsolatedVRDeviceProvider::OnDevicesEnumerated() {
  if (!initialized_) {
    initialized_ = true;
    client_->OnProviderInitialized();
  }

  // Either we've hit the max retries and given up (in which case we don't have
  // a device provider which could error out and cause us to retry) or we've
  // successfully gotten the device provider again after a retry, and we should
  // reset our count in case it gets disconnected.
  retry_count_ = 0;
}

void IsolatedVRDeviceProvider::SetupDeviceProvider() {
  GetXRDeviceService()->BindRuntimeProvider(
      device_provider_.BindNewPipeAndPassReceiver(),
      CreateXRDeviceServiceHost());
  device_provider_.set_disconnect_handler(base::BindOnce(
      &IsolatedVRDeviceProvider::OnServerError, base::Unretained(this)));

  device_provider_->RequestDevices(receiver_.BindNewPipeAndPassRemote());
}

IsolatedVRDeviceProvider::IsolatedVRDeviceProvider() = default;

// Default destructor handles renderer_host_map_ cleanup.
IsolatedVRDeviceProvider::~IsolatedVRDeviceProvider() = default;

}  // namespace content
