// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/receiver_media_to_mojo_adapter.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace video_capture {

namespace {

// Lets the mojo handler know that the buffer is no longer used upon going out
// of scope.
class ScopedVideoAccessHandlerNotifier
    : public media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  ScopedVideoAccessHandlerNotifier(
      scoped_refptr<VideoFrameAccessHandlerRemote> frame_access_handler_remote,
      int32_t buffer_id)
      : frame_access_handler_remote_(std::move(frame_access_handler_remote)),
        buffer_id_(buffer_id) {}
  ~ScopedVideoAccessHandlerNotifier() override {
    (*frame_access_handler_remote_)->OnFinishedConsumingBuffer(buffer_id_);
  }

 private:
  scoped_refptr<VideoFrameAccessHandlerRemote> frame_access_handler_remote_;
  int32_t buffer_id_;
};

}  // anonymous namespace

ReceiverMediaToMojoAdapter::ReceiverMediaToMojoAdapter(
    std::unique_ptr<media::VideoFrameReceiver> receiver)
    : receiver_(std::move(receiver)) {}

ReceiverMediaToMojoAdapter::~ReceiverMediaToMojoAdapter() = default;

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
    mojom::ReadyFrameInBufferPtr buffer,
    std::vector<mojom::ReadyFrameInBufferPtr> scaled_buffers) {
  DCHECK(frame_access_handler_);

  media::ReadyFrameInBuffer media_buffer(
      buffer->buffer_id, buffer->frame_feedback_id,
      std::make_unique<ScopedVideoAccessHandlerNotifier>(frame_access_handler_,
                                                         buffer->buffer_id),
      std::move(buffer->frame_info));

  std::vector<media::ReadyFrameInBuffer> media_scaled_buffers;
  media_scaled_buffers.reserve(scaled_buffers.size());
  for (auto& scaled_buffer : scaled_buffers) {
    media_scaled_buffers.emplace_back(
        scaled_buffer->buffer_id, scaled_buffer->frame_feedback_id,
        std::make_unique<ScopedVideoAccessHandlerNotifier>(
            frame_access_handler_, scaled_buffer->buffer_id),
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
