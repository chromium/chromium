// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/texture_virtual_device_mojo_adapter.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/video_capture/public/mojom/constants.mojom.h"

namespace video_capture {

namespace {
class ScopedBufferPoolReservation
    : public media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  ScopedBufferPoolReservation(
      scoped_refptr<VideoFrameAccessHandlerRemote> video_frame_handler_remote,
      int buffer_id)
      : video_frame_handler_remote_(std::move(video_frame_handler_remote)),
        buffer_id_(buffer_id) {}

  ~ScopedBufferPoolReservation() override {
    (*video_frame_handler_remote_)->OnFinishedConsumingBuffer(buffer_id_);
  }

 private:
  const scoped_refptr<VideoFrameAccessHandlerRemote>
      video_frame_handler_remote_;
  const int buffer_id_;
};
}  // anonymous namespace

TextureVirtualDeviceMojoAdapter::TextureVirtualDeviceMojoAdapter() = default;

TextureVirtualDeviceMojoAdapter::~TextureVirtualDeviceMojoAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TextureVirtualDeviceMojoAdapter::SetReceiverDisconnectedCallback(
    base::OnceClosure callback) {
  optional_receiver_disconnected_callback_ = std::move(callback);
}

void TextureVirtualDeviceMojoAdapter::OnNewSharedImageBufferHandle(
    int32_t buffer_id,
    media::mojom::SharedImageBufferHandleSetPtr shared_image_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Keep track of the buffer handle in order to be able to forward it to
  // the Receiver when it connects. This includes cases where a new Receiver
  // connects after a previous one has disconnected.
  known_shared_image_buffer_handles_.insert(
      std::make_pair(buffer_id, shared_image_handle->Clone()));

  media::mojom::VideoBufferHandlePtr buffer_handle =
      media::mojom::VideoBufferHandle::NewSharedImageHandle(
          std::move(shared_image_handle));
  if (video_frame_handler_.is_bound()) {
    video_frame_handler_->OnNewBuffer(buffer_id, std::move(buffer_handle));
  } else if (video_frame_handler_in_process_) {
    video_frame_handler_in_process_->OnNewBuffer(buffer_id,
                                                 std::move(buffer_handle));
  }
}

void TextureVirtualDeviceMojoAdapter::OnFrameAccessHandlerReady(
    mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
        pending_frame_access_handler) {
  DCHECK(!frame_access_handler_remote_);
  frame_access_handler_remote_ =
      base::MakeRefCounted<VideoFrameAccessHandlerRemote>(
          mojo::Remote<video_capture::mojom::VideoFrameAccessHandler>(
              std::move(pending_frame_access_handler)));
}

void TextureVirtualDeviceMojoAdapter::OnFrameReadyInBuffer(
    int32_t buffer_id,
    media::mojom::VideoFrameInfoPtr frame_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_access_handler_remote_);
  if (!video_frame_handler_.is_bound() && !video_frame_handler_in_process_) {
    (*frame_access_handler_remote_)->OnFinishedConsumingBuffer(buffer_id);
    return;
  }

  if (video_frame_handler_.is_bound()) {
    if (!video_frame_handler_has_forwarder_) {
      VideoFrameAccessHandlerForwarder::
          CreateForwarderAndSendVideoFrameAccessHandlerReady(
              video_frame_handler_, frame_access_handler_remote_);
      video_frame_handler_has_forwarder_ = true;
    }

    video_frame_handler_->OnFrameReadyInBuffer(mojom::ReadyFrameInBuffer::New(
        buffer_id, 0 /* frame_feedback_id */, std::move(frame_info)));
  } else if (video_frame_handler_in_process_) {
    video_frame_handler_has_forwarder_ = true;
    video_frame_handler_in_process_->OnFrameReadyInBuffer(
        media::ReadyFrameInBuffer(buffer_id, 0 /* frame_feedback_id */,
                                  std::make_unique<ScopedBufferPoolReservation>(
                                      frame_access_handler_remote_, buffer_id),
                                  std::move(frame_info)));
  }
}

void TextureVirtualDeviceMojoAdapter::OnBufferRetired(int buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  known_shared_image_buffer_handles_.erase(buffer_id);
  if (video_frame_handler_.is_bound()) {
    video_frame_handler_->OnBufferRetired(buffer_id);
  } else if (video_frame_handler_in_process_) {
    video_frame_handler_in_process_->OnBufferRetired(buffer_id);
  }
}

void TextureVirtualDeviceMojoAdapter::Start(
    const media::VideoCaptureParams& requested_settings,
    mojo::PendingRemote<mojom::VideoFrameHandler> handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_frame_handler_.Bind(std::move(handler));
  video_frame_handler_.set_disconnect_handler(base::BindOnce(
      &TextureVirtualDeviceMojoAdapter::OnReceiverConnectionErrorOrClose,
      base::Unretained(this)));
  video_frame_handler_->OnStarted();

  for (auto& entry : known_shared_image_buffer_handles_) {
    media::mojom::VideoBufferHandlePtr buffer_handle =
        media::mojom::VideoBufferHandle::NewSharedImageHandle(
            entry.second->Clone());
    video_frame_handler_->OnNewBuffer(entry.first, std::move(buffer_handle));
  }
}

void TextureVirtualDeviceMojoAdapter::StartInProcess(
    const media::VideoCaptureParams& requested_settings,
    const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
    media::VideoEffectsContext context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_frame_handler_in_process_ = std::move(frame_handler);
  video_frame_handler_in_process_->OnStarted();

  for (auto& entry : known_shared_image_buffer_handles_) {
    media::mojom::VideoBufferHandlePtr buffer_handle =
        media::mojom::VideoBufferHandle::NewSharedImageHandle(
            entry.second->Clone());
    video_frame_handler_in_process_->OnNewBuffer(entry.first,
                                                 std::move(buffer_handle));
  }
}

void TextureVirtualDeviceMojoAdapter::MaybeSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TextureVirtualDeviceMojoAdapter::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TextureVirtualDeviceMojoAdapter::GetPhotoState(
    GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(nullptr);
}

void TextureVirtualDeviceMojoAdapter::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TextureVirtualDeviceMojoAdapter::TakePhoto(TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TextureVirtualDeviceMojoAdapter::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TextureVirtualDeviceMojoAdapter::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TextureVirtualDeviceMojoAdapter::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_frame_handler_.is_bound()) {
    // Unsubscribe from connection error callbacks.
    video_frame_handler_.set_disconnect_handler(base::OnceClosure());
    // Send out OnBufferRetired events and OnStopped.
    for (const auto& entry : known_shared_image_buffer_handles_) {
      video_frame_handler_->OnBufferRetired(entry.first);
    }
    video_frame_handler_->OnStopped();
    video_frame_handler_.reset();
  } else if (video_frame_handler_in_process_) {
    // Send out OnBufferRetired events and OnStopped.
    for (const auto& entry : known_shared_image_buffer_handles_) {
      video_frame_handler_in_process_->OnBufferRetired(entry.first);
    }
    video_frame_handler_in_process_->OnStopped();
    video_frame_handler_in_process_ = nullptr;
  }
  video_frame_handler_has_forwarder_ = false;
}

void TextureVirtualDeviceMojoAdapter::OnReceiverConnectionErrorOrClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
  if (optional_receiver_disconnected_callback_)
    std::move(optional_receiver_disconnected_callback_).Run();
}

}  // namespace video_capture
