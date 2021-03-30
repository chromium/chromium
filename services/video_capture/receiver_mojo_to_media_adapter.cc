// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/receiver_mojo_to_media_adapter.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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
    media::ReadyFrameInBuffer frame,
    std::vector<media::ReadyFrameInBuffer> scaled_frames) {
  mojo::PendingRemote<mojom::ScopedAccessPermission> frame_permission_proxy;
  mojo::MakeSelfOwnedReceiver<mojom::ScopedAccessPermission>(
      std::make_unique<ScopedAccessPermissionMediaToMojoAdapter>(
          std::move(frame.buffer_read_permission)),
      frame_permission_proxy.InitWithNewPipeAndPassReceiver());
  mojom::ReadyFrameInBufferPtr mojom_frame = mojom::ReadyFrameInBuffer::New(
      frame.buffer_id, frame.frame_feedback_id,
      std::move(frame_permission_proxy), std::move(frame.frame_info));

  std::vector<mojom::ReadyFrameInBufferPtr> mojom_scaled_frames;
  mojom_scaled_frames.reserve(scaled_frames.size());
  for (auto& scaled_frame : scaled_frames) {
    mojo::PendingRemote<mojom::ScopedAccessPermission>
        scaled_frame_permission_proxy;
    mojo::MakeSelfOwnedReceiver<mojom::ScopedAccessPermission>(
        std::make_unique<ScopedAccessPermissionMediaToMojoAdapter>(
            std::move(scaled_frame.buffer_read_permission)),
        scaled_frame_permission_proxy.InitWithNewPipeAndPassReceiver());
    mojom_scaled_frames.push_back(mojom::ReadyFrameInBuffer::New(
        scaled_frame.buffer_id, scaled_frame.frame_feedback_id,
        std::move(scaled_frame_permission_proxy),
        std::move(scaled_frame.frame_info)));
  }

  video_frame_handler_->OnFrameReadyInBuffer(std::move(mojom_frame),
                                             std::move(mojom_scaled_frames));
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
