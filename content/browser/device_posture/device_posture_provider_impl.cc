// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_posture/device_posture_provider_impl.h"

#include "base/functional/bind.h"
#include "content/browser/device_posture/device_posture_platform_provider.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {

// static
DevicePostureProviderImpl* DevicePostureProviderImpl::GetOrCreate(
    WebContents* web_contents) {
  CHECK(web_contents);
  WebContentsUserData<DevicePostureProviderImpl>::CreateForWebContents(
      web_contents);
  return WebContentsUserData<DevicePostureProviderImpl>::FromWebContents(
      web_contents);
}

DevicePostureProviderImpl::DevicePostureProviderImpl(WebContents* web_contents)
    : WebContentsUserData<DevicePostureProviderImpl>(*web_contents) {
  platform_provider_ = DevicePosturePlatformProvider::Create(web_contents);
  // We need to  listen to disconnections so that if there is nobody interested
  // in posture changes we can shutdown the native backends.
  posture_clients_.set_disconnect_handler(base::BindRepeating(
      &DevicePostureProviderImpl::OnRemoteDisconnect, base::Unretained(this)));
}

DevicePostureProviderImpl::~DevicePostureProviderImpl() = default;

void DevicePostureProviderImpl::Bind(
    mojo::PendingReceiver<blink::mojom::DevicePostureProvider> receiver) {
  receivers_.Add(this, std::move(receiver));
}

DevicePosturePlatformProvider* DevicePostureProviderImpl::platform_provider()
    const {
  return platform_provider_.get();
}

void DevicePostureProviderImpl::AddListenerAndGetCurrentPosture(
    mojo::PendingRemote<blink::mojom::DevicePostureClient> client,
    AddListenerAndGetCurrentPostureCallback callback) {
  if (posture_clients_.empty()) {
    platform_provider_->AddObserver(this);
  }
  posture_clients_.Add(std::move(client));
  blink::mojom::DevicePostureType posture =
      platform_provider_->GetDevicePosture();
  std::move(callback).Run(posture);
}

void DevicePostureProviderImpl::OverrideDevicePostureForEmulation(
    blink::mojom::DevicePostureType emulated_posture) {
  // Notify the related clients about the new posture.
  is_posture_emulated_ = true;
  for (auto& client : posture_clients_) {
    client->OnPostureChanged(emulated_posture);
  }
}

void DevicePostureProviderImpl::DisableDevicePostureOverrideForEmulation() {
  // If the posture is not being overridden, bail out earlier to avoid
  // needlessly calling OnPostureChanged().
  if (!is_posture_emulated_) {
    return;
  }

  // Restore the original posture from the platform.
  is_posture_emulated_ = false;
  for (auto& client : posture_clients_) {
    client->OnPostureChanged(platform_provider_->GetDevicePosture());
  }
}

void DevicePostureProviderImpl::OnDevicePostureChanged(
    const blink::mojom::DevicePostureType& posture) {
  // If we receive a posture change from the platform but we're emulating it we
  // shouldn't notify the clients.
  if (is_posture_emulated_) {
    return;
  }

  for (auto& client : posture_clients_) {
    client->OnPostureChanged(posture);
  }
}

void DevicePostureProviderImpl::OnRemoteDisconnect(
    mojo::RemoteSetElementId id) {
  if (posture_clients_.empty()) {
    // We're not interested in receiving posture changes.
    platform_provider_->RemoveObserver(this);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DevicePostureProviderImpl);

}  // namespace content
