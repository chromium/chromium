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
    mojom::ReadyFrameInBufferPtr buffer,
    std::vector<mojom::ReadyFrameInBufferPtr> scaled_buffers) {
  media::ReadyFrameInBuffer media_buffer(
      buffer->buffer_id, buffer->frame_feedback_id,
      std::make_unique<ScopedAccessPermissionMojoToMediaAdapter>(
          std::move(buffer->access_permission)),
      std::move(buffer->frame_info));

  std::vector<media::ReadyFrameInBuffer> media_scaled_buffers;
  media_scaled_buffers.reserve(scaled_buffers.size());
  for (auto& scaled_buffer : scaled_buffers) {
    media_scaled_buffers.emplace_back(
        scaled_buffer->buffer_id, scaled_buffer->frame_feedback_id,
        std::make_unique<ScopedAccessPermissionMojoToMediaAdapter>(
            std::move(scaled_buffer->access_permission)),
        std::move(scaled_buffer->frame_info));
  }
  receiver_->OnFrameReadyInBuffer(std::move(media_buffer),
                                  std::move(media_scaled_buffers));
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
