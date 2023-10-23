// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/receiver_media_to_mojo_adapter.h"

#include "media/capture/video/video_frame_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace video_capture {

namespace {
void OnFramePropagationComplete(
    scoped_refptr<VideoFrameAccessHandlerRemote> frame_access_handler_remote,
    int32_t buffer_id) {
  // Notify the VideoFrameAccessHandler that the buffer is no longer valid.
  (*frame_access_handler_remote)->OnFinishedConsumingBuffer(buffer_id);
}
}  // namespace

ReceiverMediaToMojoAdapter::ReceiverMediaToMojoAdapter(
    std::unique_ptr<media::VideoFrameReceiver> receiver)
    : receiver_(std::move(receiver)) {}

ReceiverMediaToMojoAdapter::~ReceiverMediaToMojoAdapter() = default;

void ReceiverMediaToMojoAdapter::OnCaptureConfigurationChanged() {
  receiver_->OnCaptureConfigurationChanged();
}

void ReceiverMediaToMojoAdapter::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  receiver_->OnNewBuffer(buffer_id, std::move(buffer_handle));
}

void ReceiverMediaToMojoAdapter::OnFrameAccessHandlerReady(
    mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
        pending_frame_access_handler) {
  DCHECK(!frame_access_handler_);
  frame_access_handler_ = base::MakeRefCounted<VideoFrameAccessHandlerRemote>(
      mojo::Remote<video_capture::mojom::VideoFrameAccessHandler>(
          std::move(pending_frame_access_handler)));
}

void ReceiverMediaToMojoAdapter::OnFrameReadyInBuffer(
    mojom::ReadyFrameInBufferPtr buffer) {
  DCHECK(frame_access_handler_);

  media::ReadyFrameInBuffer media_buffer(
      buffer->buffer_id, buffer->frame_feedback_id,
      std::make_unique<media::ScopedFrameDoneHelper>(
          base::BindOnce(&OnFramePropagationComplete, frame_access_handler_,
                         buffer->buffer_id)),
      std::move(buffer->frame_info));

  receiver_->OnFrameReadyInBuffer(std::move(media_buffer));
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

void ReceiverMediaToMojoAdapter::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  receiver_->OnNewSubCaptureTargetVersion(sub_capture_target_version);
}

void ReceiverMediaToMojoAdapter::OnFrameWithEmptyRegionCapture() {
  receiver_->OnFrameWithEmptyRegionCapture();
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
