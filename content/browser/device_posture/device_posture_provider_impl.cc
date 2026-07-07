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
    : WebContentsUserData<DevicePostureProviderImpl>(*web_contents),
      WebContentsObserver(web_contents) {
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

blink::mojom::DevicePostureType DevicePostureProviderImpl::GetCurrentPosture()
    const {
  return (is_posture_emulated_ && emulated_posture_)
             ? *emulated_posture_
             : platform_provider_->GetDevicePosture();
}

void DevicePostureProviderImpl::AddListenerAndGetCurrentPosture(
    mojo::PendingRemote<blink::mojom::DevicePostureClient> client,
    AddListenerAndGetCurrentPostureCallback callback) {
  if (posture_clients_.empty()) {
    platform_provider_->AddObserver(this);
  }
  posture_clients_.Add(std::move(client));
  blink::mojom::DevicePostureType posture = GetCurrentPosture();
  std::move(callback).Run(posture);
  last_dispatched_posture_ = posture;
}

void DevicePostureProviderImpl::OverrideDevicePostureForEmulation(
    blink::mojom::DevicePostureType emulated_posture) {
  is_posture_emulated_ = true;
  emulated_posture_ = emulated_posture;
  // If the page is hidden, we store the emulated posture but defer dispatching
  // the event until the page becomes visible again (handled in
  // OnVisibilityChanged).
  if (web_contents()->GetVisibility() != Visibility::VISIBLE) {
    return;
  }
  if (!last_dispatched_posture_ ||
      *last_dispatched_posture_ != emulated_posture) {
    for (auto& client : posture_clients_) {
      client->OnPostureChanged(emulated_posture);
    }
    last_dispatched_posture_ = emulated_posture;
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
  emulated_posture_.reset();
  if (web_contents()->GetVisibility() != Visibility::VISIBLE) {
    return;
  }
  blink::mojom::DevicePostureType posture =
      platform_provider_->GetDevicePosture();
  if (!last_dispatched_posture_ || *last_dispatched_posture_ != posture) {
    for (auto& client : posture_clients_) {
      client->OnPostureChanged(posture);
    }
    last_dispatched_posture_ = posture;
  }
}

void DevicePostureProviderImpl::OnDevicePostureChanged(
    const blink::mojom::DevicePostureType& posture) {
  // If we receive a posture change from the platform but we're emulating it,
  // or if the page is not visible, we shouldn't notify the clients.
  if (is_posture_emulated_ ||
      web_contents()->GetVisibility() != Visibility::VISIBLE) {
    return;
  }

  if (!last_dispatched_posture_ || *last_dispatched_posture_ != posture) {
    for (auto& client : posture_clients_) {
      client->OnPostureChanged(posture);
    }
    last_dispatched_posture_ = posture;
  }
}

void DevicePostureProviderImpl::OnVisibilityChanged(Visibility visibility) {
  if (visibility != Visibility::VISIBLE || posture_clients_.empty()) {
    return;
  }

  // When the tab regains visibility, dispatch the latest posture to the
  // clients if it changed while the tab was hidden.
  blink::mojom::DevicePostureType current_posture = GetCurrentPosture();

  if (!last_dispatched_posture_ ||
      *last_dispatched_posture_ != current_posture) {
    for (auto& client : posture_clients_) {
      client->OnPostureChanged(current_posture);
    }
    last_dispatched_posture_ = current_posture;
  }
}

void DevicePostureProviderImpl::OnRemoteDisconnect(
    mojo::RemoteSetElementId id) {
  if (posture_clients_.empty()) {
    // We're not interested in receiving posture changes.
    platform_provider_->RemoveObserver(this);
    last_dispatched_posture_.reset();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DevicePostureProviderImpl);

}  // namespace content
