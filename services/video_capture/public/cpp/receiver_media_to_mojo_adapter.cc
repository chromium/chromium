// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/receiver_media_to_mojo_adapter.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/video_capture/public/mojom/scoped_access_permission.mojom.h"

namespace {

class ScopedAccessPermissionMojoToMediaAdapter
    : public media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  ScopedAccessPermissionMojoToMediaAdapter(
      mojo::PendingRemote<video_capture::mojom::ScopedAccessPermission>
          access_permission)
      : access_permission_(std::move(access_permission)) {}

 private:
  mojo::Remote<video_capture::mojom::ScopedAccessPermission> access_permission_;
};

}  // anonymous namespace

namespace video_capture {

ReceiverMediaToMojoAdapter::ReceiverMediaToMojoAdapter(
    std::unique_ptr<media::VideoFrameReceiver> receiver)
    : receiver_(std::move(receiver)) {}

ReceiverMediaToMojoAdapter::~ReceiverMediaToMojoAdapter() = default;

void ReceiverMediaToMojoAdapter::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  receiver_->OnNewBuffer(buffer_id, std::move(buffer_handle));
}

void ReceiverMediaToMojoAdapter::OnFrameReadyInBuffer(
    int32_t buffer_id,
    int32_t frame_feedback_id,
    mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  receiver_->OnFrameReadyInBuffer(
      buffer_id, frame_feedback_id,
      std::make_unique<ScopedAccessPermissionMojoToMediaAdapter>(
          std::move(access_permission)),
      std::move(frame_info));
}

void ReceiverMediaToMojoAdapter::OnBufferRetired(int32_t buffer_id) {
  receiver_->OnBufferRetired(buffer_id);
}

void ReceiverMediaToMojoAdapter::OnError(media::VideoCaptureError error) {
  receiver_->OnError(error);
}

void ReceiverMediaToMojoAdapter::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  receiver_->OnFrameDropped(reason);
}

void ReceiverMediaToMojoAdapter::OnLog(const std::string& message) {
  receiver_->OnLog(message);
}

void ReceiverMediaToMojoAdapter::OnStarted() {
  receiver_->OnStarted();
}

void ReceiverMediaToMojoAdapter::OnStartedUsingGpuDecode() {
  receiver_->OnStartedUsingGpuDecode();
}

void ReceiverMediaToMojoAdapter::OnStopped() {
  receiver_->OnStopped();
}

}  // namespace video_capture
