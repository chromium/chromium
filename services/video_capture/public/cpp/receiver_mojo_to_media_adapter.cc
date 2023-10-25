// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/receiver_mojo_to_media_adapter.h"

#include "base/memory/scoped_refptr.h"

namespace video_capture {

ReceiverMojoToMediaAdapter::ReceiverMojoToMediaAdapter(
    mojo::Remote<mojom::VideoFrameHandler> handler)
    : video_frame_handler_(std::move(handler)) {}

ReceiverMojoToMediaAdapter::~ReceiverMojoToMediaAdapter() = default;

base::WeakPtr<media::VideoFrameReceiver>
ReceiverMojoToMediaAdapter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ReceiverMojoToMediaAdapter::OnCaptureConfigurationChanged() {
  video_frame_handler_->OnCaptureConfigurationChanged();
}

void ReceiverMojoToMediaAdapter::OnNewBuffer(
    int buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  video_frame_handler_->OnNewBuffer(buffer_id, std::move(buffer_handle));
}

void ReceiverMojoToMediaAdapter::OnFrameReadyInBuffer(
    media::ReadyFrameInBuffer frame) {
  if (!scoped_access_permission_map_) {
    scoped_access_permission_map_ =
        ScopedAccessPermissionMap::CreateMapAndSendVideoFrameAccessHandlerReady(
            video_frame_handler_);
  }
  scoped_access_permission_map_->InsertAccessPermission(
      frame.buffer_id, std::move(frame.buffer_read_permission));
  mojom::ReadyFrameInBufferPtr mojom_frame = mojom::ReadyFrameInBuffer::New(
      frame.buffer_id, frame.frame_feedback_id, std::move(frame.frame_info));

  video_frame_handler_->OnFrameReadyInBuffer(std::move(mojom_frame));
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

void ReceiverMojoToMediaAdapter::OnFrameWithEmptyRegionCapture() {
  video_frame_handler_->OnFrameWithEmptyRegionCapture();
}

void ReceiverMojoToMediaAdapter::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  video_frame_handler_->OnNewSubCaptureTargetVersion(
      sub_capture_target_version);
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
