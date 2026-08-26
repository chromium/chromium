// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_manager.h"

#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace content {

MediaDevicesManager::VideoCaptureDevicesChangedObserver::
    VideoCaptureDevicesChangedObserver(
        base::RepeatingClosure invalidate_cache_cb,
        base::RepeatingClosure enumerate_system_devices_cb,
        base::RepeatingClosure listener_cb)
    : invalidate_cache_cb_(std::move(invalidate_cache_cb)),
      enumerate_system_devices_cb_(std::move(enumerate_system_devices_cb)),
      listener_cb_(std::move(listener_cb)) {}

MediaDevicesManager::VideoCaptureDevicesChangedObserver::
    ~VideoCaptureDevicesChangedObserver() = default;

void MediaDevicesManager::VideoCaptureDevicesChangedObserver::
    OnDevicesChanged() {
  listener_cb_.Run();
}

void MediaDevicesManager::VideoCaptureDevicesChangedObserver::
    EnsureConnectedToService(ConnectType connect_type) {
  if (mojo_device_notifier_ && receiver_.is_bound()) {
    return;
  }
  CHECK(!mojo_device_notifier_);
  CHECK(!receiver_.is_bound());
  GetVideoCaptureService().ConnectToVideoSourceProvider(
      mojo_device_notifier_.BindNewPipeAndPassReceiver());
  mojo_device_notifier_.set_disconnect_handler(
      base::BindOnce(&MediaDevicesManager::VideoCaptureDevicesChangedObserver::
                         OnConnectionError,
                     base::Unretained(this)));
  mojo_device_notifier_->RegisterDevicesChangedObserver(
      receiver_.BindNewPipeAndPassRemote());

  if (connect_type == ConnectType::kReconnect && enumerate_system_devices_cb_) {
    enumerate_system_devices_cb_.Run();
  }
}

void MediaDevicesManager::VideoCaptureDevicesChangedObserver::
    OnConnectionError() {
  mojo_device_notifier_.reset();
  receiver_.reset();

  if (invalidate_cache_cb_) {
    invalidate_cache_cb_.Run();
  }
  EnsureConnectedToService(ConnectType::kReconnect);
}

void MediaDevicesManager::VideoCaptureDevicesChangedObserver::
    DisconnectVideoSourceProvider() {
  mojo_device_notifier_.reset();
  receiver_.reset();
}

}  // namespace content
