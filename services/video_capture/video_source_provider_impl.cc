// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_source_provider_impl.h"

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/video_source_impl.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"

namespace video_capture {

VideoSourceProviderImpl::VideoSourceProviderImpl(
    DeviceFactory* device_factory,
    base::RepeatingClosure on_last_client_disconnected_cb)
    : device_factory_(device_factory),
      on_last_client_disconnected_cb_(
          std::move(on_last_client_disconnected_cb)) {
  // Unretained |this| is safe because |receivers_| is owned by |this|.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &VideoSourceProviderImpl::OnClientDisconnected, base::Unretained(this)));
}

VideoSourceProviderImpl::~VideoSourceProviderImpl() = default;

void VideoSourceProviderImpl::AddClient(
    mojo::PendingReceiver<mojom::VideoSourceProvider> receiver) {
  receivers_.Add(this, std::move(receiver));
  client_count_++;
}

void VideoSourceProviderImpl::GetSourceInfos(GetSourceInfosCallback callback) {
  // The service might be shut down before the callback has a chance to be
  // executed. This triggers the CHECK in mojo code, which assumes that
  // callbacks are either executed or the underlying channel is closed. Wrap
  // the callback to ensure it will be executed on destruction.
  GetSourceInfosCallback callback_with_default =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback),
          VideoSourceProvider::GetSourceInfosResult::kErrorDroppedRequest,
          std::vector<media::VideoCaptureDeviceInfo>());
  device_factory_->GetDeviceInfos(
      base::BindOnce(std::move(callback_with_default),
                     VideoSourceProvider::GetSourceInfosResult::kSuccess));
}

void VideoSourceProviderImpl::GetVideoSource(
    const std::string& device_id,
    mojo::PendingReceiver<mojom::VideoSource> source_receiver) {
  auto source_iter = sources_.find(device_id);
  if (source_iter == sources_.end()) {
    auto video_source = std::make_unique<VideoSourceImpl>(
        device_factory_, device_id,
        base::BindRepeating(
            &VideoSourceProviderImpl::OnVideoSourceLastClientDisconnected,
            base::Unretained(this), device_id));
    source_iter =
        sources_.insert(std::make_pair(device_id, std::move(video_source)))
            .first;
  }
  source_iter->second->AddToReceiverSet(std::move(source_receiver));
}

void VideoSourceProviderImpl::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<mojom::Producer> producer,
    mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  device_factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer), std::move(virtual_device_receiver));
}

void VideoSourceProviderImpl::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  device_factory_->AddTextureVirtualDevice(device_info,
                                           std::move(virtual_device_receiver));
}

void VideoSourceProviderImpl::RegisterVirtualDevicesChangedObserver(
    mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
    bool raise_event_if_virtual_devices_already_present) {
  device_factory_->RegisterVirtualDevicesChangedObserver(
      std::move(observer), raise_event_if_virtual_devices_already_present);
}

void VideoSourceProviderImpl::RegisterDevicesChangedObserver(
    mojo::PendingRemote<mojom::DevicesChangedObserver> observer) {
  if (!devices_changed_notifier_) {
    devices_changed_notifier_.emplace();
  }
  devices_changed_notifier_->RegisterObserver(std::move(observer));
}

void VideoSourceProviderImpl::Close(CloseCallback callback) {
  closed_but_not_yet_disconnected_client_count_++;
  // |callback must be run before OnClientDisconnectedOrClosed(), because if the
  // latter leads to the destruction of |this|, the message pipe to the client
  // gets severed, and the callback never makes it through.
  std::move(callback).Run();
  OnClientDisconnectedOrClosed();
}

void VideoSourceProviderImpl::OnClientDisconnected() {
  if (closed_but_not_yet_disconnected_client_count_ > 0) {
    closed_but_not_yet_disconnected_client_count_--;
    return;
  }
  OnClientDisconnectedOrClosed();
}

void VideoSourceProviderImpl::OnClientDisconnectedOrClosed() {
  client_count_--;
  if (client_count_ == 0) {
    // No member access allowed after this call, because it may lead to the
    // destruction of |this|.
    on_last_client_disconnected_cb_.Run();
  }
}

void VideoSourceProviderImpl::OnVideoSourceLastClientDisconnected(
    const std::string& device_id) {
  sources_.erase(device_id);
}

}  // namespace video_capture
