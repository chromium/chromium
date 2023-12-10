// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_posture/device_posture_provider_impl.h"

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/device_posture/device_posture_platform_provider.h"

namespace device {

DevicePostureProviderImpl::DevicePostureClientInformation::
    DevicePostureClientInformation() = default;
DevicePostureProviderImpl::DevicePostureClientInformation::
    ~DevicePostureClientInformation() = default;

DevicePostureProviderImpl::DevicePostureProviderImpl(
    std::unique_ptr<DevicePosturePlatformProvider> platform_provider)
    : platform_provider_(std::move(platform_provider)) {
  DCHECK(platform_provider_);
  platform_provider_->SetPostureProvider(this);
  // We need to  listen to disconnections so that if there is nobody interested
  // in posture changes we can shutdown the native backends.
  receivers_.set_disconnect_handler(
      base::BindRepeating(&DevicePostureProviderImpl::OnReceiverConnectionError,
                          weak_ptr_factory_.GetWeakPtr()));
}

DevicePostureProviderImpl::~DevicePostureProviderImpl() = default;

void DevicePostureProviderImpl::AddListenerAndGetCurrentPosture(
    mojo::PendingRemote<mojom::DevicePostureClient> client,
    AddListenerAndGetCurrentPostureCallback callback) {
  posture_clients_[receivers_.current_receiver()].clients.Add(
      std::move(client));
  mojom::DevicePostureType posture = platform_provider_->GetDevicePosture();
  std::move(callback).Run(posture);
}

void DevicePostureProviderImpl::AddListenerAndGetCurrentViewportSegments(
    mojo::PendingRemote<mojom::DeviceViewportSegmentsClient> client,
    AddListenerAndGetCurrentViewportSegmentsCallback callback) {
  viewport_segments_clients_.Add(std::move(client));
  std::move(callback).Run(platform_provider_->GetViewportSegments());
}

void DevicePostureProviderImpl::OverrideDevicePostureForEmulation(
    mojom::DevicePostureType emulated_posture) {
  // Notify the related clients about the new posture.
  const auto receiver_id = receivers_.current_receiver();
  DevicePostureClientInformation& client_info = posture_clients_[receiver_id];
  client_info.is_emulated = true;
  for (auto& client : client_info.clients) {
    client->OnPostureChanged(emulated_posture);
  }
}

void DevicePostureProviderImpl::DisableDevicePostureOverrideForEmulation() {
  // Restore the original posture from the platform.
  const auto receiver_id = receivers_.current_receiver();
  DevicePostureClientInformation& client_info = posture_clients_[receiver_id];
  client_info.is_emulated = false;
  for (auto& client : client_info.clients) {
    client->OnPostureChanged(platform_provider_->GetDevicePosture());
  }
}

void DevicePostureProviderImpl::Bind(
    mojo::PendingReceiver<mojom::DevicePostureProvider> receiver) {
  if (receivers_.empty())
    platform_provider_->StartListening();
  receivers_.Add(this, std::move(receiver));
}

void DevicePostureProviderImpl::OnDevicePostureChanged(
    const mojom::DevicePostureType& posture) {
  for (const auto& [receiver_id, posture_client_information] :
       posture_clients_) {
    // If we receive a posture change from the platform but we're emulating it
    // we shouldn't notify the clients.
    if (posture_client_information.is_emulated) {
      continue;
    }

    for (auto& client : posture_client_information.clients) {
      client->OnPostureChanged(posture);
    }
  }
}

void DevicePostureProviderImpl::OnViewportSegmentsChanged(
    const std::vector<gfx::Rect>& segments) {
  for (auto& client : viewport_segments_clients_) {
    client->OnViewportSegmentsChanged(segments);
  }
}

void DevicePostureProviderImpl::OnReceiverConnectionError() {
  if (receivers_.empty())
    platform_provider_->StopListening();
}

}  // namespace device
