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
  mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission_proxy;
  mojo::MakeSelfOwnedReceiver<mojom::ScopedAccessPermission>(
      std::make_unique<ScopedAccessPermissionMediaToMojoAdapter>(
          std::move(frame.buffer_read_permission)),
      access_permission_proxy.InitWithNewPipeAndPassReceiver());
  // Scaled frames are currently not passed along.
  // TODO(https://crbug.com/1157072): When video_frame_handler.mojom is updated
  // to support scaled frames, update this code to pass along scaled frames.
  // This will achieve passing frames from Capture Process to Browser Process.
  video_frame_handler_->OnFrameReadyInBuffer(
      frame.buffer_id, frame.frame_feedback_id,
      std::move(access_permission_proxy), std::move(frame.frame_info));
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
