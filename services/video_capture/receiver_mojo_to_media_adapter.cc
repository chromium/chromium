// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/receiver_mojo_to_media_adapter.h"

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/video_capture/scoped_access_permission_media_to_mojo_adapter.h"

namespace video_capture {

ReceiverMojoToMediaAdapter::ReceiverMojoToMediaAdapter(
    mojo::Remote<mojom::VideoFrameHandler> handler)
    : video_frame_handler_(std::move(handler)) {}

ReceiverMojoToMediaAdapter::~ReceiverMojoToMediaAdapter() = default;

base::WeakPtr<media::VideoFrameReceiver>
ReceiverMojoToMediaAdapter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ReceiverMojoToMediaAdapter::OnNewBuffer(
    int buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  video_frame_handler_->OnNewBuffer(buffer_id, std::move(buffer_handle));
}

void ReceiverMojoToMediaAdapter::OnFrameReadyInBuffer(
    int buffer_id,
    int frame_feedback_id,
    std::unique_ptr<
        media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        access_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission_proxy;
  mojo::MakeStrongBinding<mojom::ScopedAccessPermission>(
      std::make_unique<ScopedAccessPermissionMediaToMojoAdapter>(
          std::move(access_permission)),
      access_permission_proxy.InitWithNewPipeAndPassReceiver());
  video_frame_handler_->OnFrameReadyInBuffer(buffer_id, frame_feedback_id,
                                             std::move(access_permission_proxy),
                                             std::move(frame_info));
}

void ReceiverMojoToMediaAdapter::OnBufferRetired(int buffer_id) {
  video_frame_handler_->OnBufferRetired(buffer_id);
}

void ReceiverMojoToMediaAdapter::OnError(media::VideoCaptureError error) {
  video_frame_handler_->OnError(error);
}

void ReceiverMojoToMediaAdapter::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  video_frame_handler_->OnFrameDropped(reason);
}

void ReceiverMojoToMediaAdapter::OnLog(const std::string& message) {
  video_frame_handler_->OnLog(message);
}

void ReceiverMojoToMediaAdapter::OnStarted() {
  video_frame_handler_->OnStarted();
}

void ReceiverMojoToMediaAdapter::OnStartedUsingGpuDecode() {
  video_frame_handler_->OnStartedUsingGpuDecode();
}

void ReceiverMojoToMediaAdapter::OnStopped() {
  video_frame_handler_->OnStopped();
}

}  // namespace video_capture
