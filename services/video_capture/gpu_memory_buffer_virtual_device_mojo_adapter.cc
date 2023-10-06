// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/gpu_memory_buffer_virtual_device_mojo_adapter.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/video_capture/public/mojom/constants.mojom.h"

namespace video_capture {

GpuMemoryBufferVirtualDeviceMojoAdapter::
    GpuMemoryBufferVirtualDeviceMojoAdapter() = default;

GpuMemoryBufferVirtualDeviceMojoAdapter::
    ~GpuMemoryBufferVirtualDeviceMojoAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::SetReceiverDisconnectedCallback(
    base::OnceClosure callback) {
  optional_receiver_disconnected_callback_ = std::move(callback);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::OnNewGpuMemoryBufferHandle(
    int32_t buffer_id,
    gfx::GpuMemoryBufferHandle gmb_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Keep track of the buffer handles in order to be able to forward them to
  // the Receiver when it connects. This includes cases where a new Receiver
  // connects after a previous one has disconnected.
  known_buffer_handles_.insert(std::make_pair(buffer_id, gmb_handle.Clone()));

  if (!video_frame_handler_.is_bound())
    return;
  media::mojom::VideoBufferHandlePtr buffer_handle =
      media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
          std::move(gmb_handle));
  video_frame_handler_->OnNewBuffer(buffer_id, std::move(buffer_handle));
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::OnFrameAccessHandlerReady(
    mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
        pending_frame_access_handler) {
  DCHECK(!frame_access_handler_remote_);
  frame_access_handler_remote_ =
      base::MakeRefCounted<VideoFrameAccessHandlerRemote>(
          mojo::Remote<video_capture::mojom::VideoFrameAccessHandler>(
              std::move(pending_frame_access_handler)));
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::OnFrameReadyInBuffer(
    int32_t buffer_id,
    media::mojom::VideoFrameInfoPtr frame_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_access_handler_remote_);
  if (!video_frame_handler_.is_bound()) {
    (*frame_access_handler_remote_)->OnFinishedConsumingBuffer(buffer_id);
    return;
  }
  if (!video_frame_handler_has_forwarder_) {
    VideoFrameAccessHandlerForwarder::
        CreateForwarderAndSendVideoFrameAccessHandlerReady(
            video_frame_handler_, frame_access_handler_remote_);
    video_frame_handler_has_forwarder_ = true;
  }
  video_frame_handler_->OnFrameReadyInBuffer(mojom::ReadyFrameInBuffer::New(
      buffer_id, 0 /* frame_feedback_id */, std::move(frame_info)));
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::OnBufferRetired(int buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  known_buffer_handles_.erase(buffer_id);
  if (!video_frame_handler_.is_bound())
    return;
  video_frame_handler_->OnBufferRetired(buffer_id);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::Start(
    const media::VideoCaptureParams& requested_settings,
    mojo::PendingRemote<mojom::VideoFrameHandler> handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_frame_handler_.Bind(std::move(handler));
  video_frame_handler_.set_disconnect_handler(
      base::BindOnce(&GpuMemoryBufferVirtualDeviceMojoAdapter::
                         OnReceiverConnectionErrorOrClose,
                     base::Unretained(this)));
  video_frame_handler_->OnStarted();

  // Notify receiver of known buffer handles */
  for (auto& entry : known_buffer_handles_) {
    media::mojom::VideoBufferHandlePtr buffer_handle =
        media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
            entry.second.Clone());
    video_frame_handler_->OnNewBuffer(entry.first, std::move(buffer_handle));
  }
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::MaybeSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::GetPhotoState(
    GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(nullptr);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::TakePhoto(
    TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_frame_handler_.is_bound())
    return;
  // Unsubscribe from connection error callbacks.
  video_frame_handler_.set_disconnect_handler(base::OnceClosure());
  // Send out OnBufferRetired events and OnStopped.
  for (const auto& entry : known_buffer_handles_)
    video_frame_handler_->OnBufferRetired(entry.first);
  video_frame_handler_->OnStopped();
  video_frame_handler_.reset();
  video_frame_handler_has_forwarder_ = false;
}

void GpuMemoryBufferVirtualDeviceMojoAdapter::
    OnReceiverConnectionErrorOrClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
  if (optional_receiver_disconnected_callback_)
    std::move(optional_receiver_disconnected_callback_).Run();
}

}  // namespace video_capture
