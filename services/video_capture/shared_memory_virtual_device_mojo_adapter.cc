// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/shared_memory_virtual_device_mojo_adapter.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "media/capture/video/scoped_buffer_pool_reservation.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_pool_util.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/mojom/constants.mojom.h"

namespace {

void OnNewBufferAcknowleged(
    video_capture::mojom::SharedMemoryVirtualDevice::RequestFrameBufferCallback
        callback,
    int32_t buffer_id) {
  std::move(callback).Run(buffer_id);
}

}  // anonymous namespace

namespace video_capture {

SharedMemoryVirtualDeviceMojoAdapter::SharedMemoryVirtualDeviceMojoAdapter(
    mojo::Remote<mojom::Producer> producer)
    : producer_(std::move(producer)),
      buffer_pool_(new media::VideoCaptureBufferPoolImpl(
          media::VideoCaptureBufferType::kSharedMemory,
          max_buffer_pool_buffer_count())) {}

SharedMemoryVirtualDeviceMojoAdapter::~SharedMemoryVirtualDeviceMojoAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count() {
  return media::kVideoCaptureDefaultMaxBufferPoolSize;
}

void SharedMemoryVirtualDeviceMojoAdapter::RequestFrameBuffer(
    const gfx::Size& dimension,
    media::VideoPixelFormat pixel_format,
    media::mojom::PlaneStridesPtr strides,
    RequestFrameBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int buffer_id_to_drop = media::VideoCaptureBufferPool::kInvalidId;
  int buffer_id = media::VideoCaptureBufferPool::kInvalidId;
  const auto reserve_result = buffer_pool_->ReserveForProducer(
      dimension, pixel_format, strides, 0 /* frame_feedback_id */, &buffer_id,
      &buffer_id_to_drop);

  // Remove dropped buffer if there is one.
  if (buffer_id_to_drop != media::VideoCaptureBufferPool::kInvalidId) {
    auto entry_iter = base::ranges::find(known_buffer_ids_, buffer_id_to_drop);
    if (entry_iter != known_buffer_ids_.end()) {
      known_buffer_ids_.erase(entry_iter);
      if (producer_.is_bound())
        producer_->OnBufferRetired(buffer_id_to_drop);
      if (video_frame_handler_.is_bound()) {
        video_frame_handler_->OnBufferRetired(buffer_id_to_drop);
      } else if (video_frame_handler_in_process_) {
        video_frame_handler_in_process_->OnBufferRetired(buffer_id_to_drop);
      }
    }
  }

  if (reserve_result !=
      media::VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
    std::move(callback).Run(mojom::kInvalidBufferId);
    return;
  }

  if (!base::Contains(known_buffer_ids_, buffer_id)) {
    if (video_frame_handler_.is_bound()) {
      media::mojom::VideoBufferHandlePtr buffer_handle =
          media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
              buffer_pool_->DuplicateAsUnsafeRegion(buffer_id));
      video_frame_handler_->OnNewBuffer(buffer_id, std::move(buffer_handle));
    } else if (video_frame_handler_in_process_) {
      media::mojom::VideoBufferHandlePtr buffer_handle =
          media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
              buffer_pool_->DuplicateAsUnsafeRegion(buffer_id));
      video_frame_handler_in_process_->OnNewBuffer(buffer_id,
                                                   std::move(buffer_handle));
    }
    known_buffer_ids_.push_back(buffer_id);

    // Share buffer handle with producer.
    media::mojom::VideoBufferHandlePtr buffer_handle;
    buffer_handle = media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
        buffer_pool_->DuplicateAsUnsafeRegion(buffer_id));
    // Invoke the response back only after the producer have acked
    // that it has received the newly created buffer. This is need
    // because the |producer_| and the |callback| are bound to different
    // message pipes, so the order for calls to |producer_| and |callback|
    // is not guaranteed.
    if (producer_.is_bound())
      producer_->OnNewBuffer(buffer_id, std::move(buffer_handle),
                             base::BindOnce(&OnNewBufferAcknowleged,
                                            std::move(callback), buffer_id));
    return;
  }
  std::move(callback).Run(buffer_id);
}

void SharedMemoryVirtualDeviceMojoAdapter::OnFrameReadyInBuffer(
    int32_t buffer_id,
    ::media::mojom::VideoFrameInfoPtr frame_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unknown buffer ID.
  if (!base::Contains(known_buffer_ids_, buffer_id)) {
    return;
  }

  // Notify receiver if there is one.
  if (video_frame_handler_.is_bound()) {
    if (!scoped_access_permission_map_) {
      scoped_access_permission_map_ = ScopedAccessPermissionMap::
          CreateMapAndSendVideoFrameAccessHandlerReady(video_frame_handler_);
    }
    buffer_pool_->HoldForConsumers(buffer_id, 1 /* num_clients */);
    auto access_permission = std::make_unique<
        media::ScopedBufferPoolReservation<media::ConsumerReleaseTraits>>(
        buffer_pool_, buffer_id);
    scoped_access_permission_map_->InsertAccessPermission(
        buffer_id, std::move(access_permission));
    video_frame_handler_->OnFrameReadyInBuffer(mojom::ReadyFrameInBuffer::New(
        buffer_id, 0 /* frame_feedback_id */, std::move(frame_info)));
  } else if (video_frame_handler_in_process_) {
    buffer_pool_->HoldForConsumers(buffer_id, 1 /* num_clients */);
    video_frame_handler_in_process_->OnFrameReadyInBuffer(
        media::ReadyFrameInBuffer(
            buffer_id, 0 /* frame_feedback_id */,
            std::make_unique<media::ScopedBufferPoolReservation<
                media::ConsumerReleaseTraits>>(buffer_pool_, buffer_id),
            std::move(frame_info)));
  }
  buffer_pool_->RelinquishProducerReservation(buffer_id);
}

void SharedMemoryVirtualDeviceMojoAdapter::Start(
    const media::VideoCaptureParams& requested_settings,
    mojo::PendingRemote<mojom::VideoFrameHandler> handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_frame_handler_.Bind(std::move(handler));
  video_frame_handler_.set_disconnect_handler(base::BindOnce(
      &SharedMemoryVirtualDeviceMojoAdapter::OnReceiverConnectionErrorOrClose,
      base::Unretained(this)));
  video_frame_handler_->OnStarted();

  // Notify receiver of known buffers */
  for (auto buffer_id : known_buffer_ids_) {
    media::mojom::VideoBufferHandlePtr buffer_handle =
        media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
            buffer_pool_->DuplicateAsUnsafeRegion(buffer_id));
    video_frame_handler_->OnNewBuffer(buffer_id, std::move(buffer_handle));
  }
}

void SharedMemoryVirtualDeviceMojoAdapter::StartInProcess(
    const media::VideoCaptureParams& requested_settings,
    const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
    media::VideoEffectsContext context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_frame_handler_in_process_ = std::move(frame_handler);
  video_frame_handler_in_process_->OnStarted();

  // Notify receiver of known buffers */
  for (auto buffer_id : known_buffer_ids_) {
    media::mojom::VideoBufferHandlePtr buffer_handle =
        media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
            buffer_pool_->DuplicateAsUnsafeRegion(buffer_id));
    video_frame_handler_in_process_->OnNewBuffer(buffer_id,
                                                 std::move(buffer_handle));
  }
}

void SharedMemoryVirtualDeviceMojoAdapter::MaybeSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SharedMemoryVirtualDeviceMojoAdapter::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SharedMemoryVirtualDeviceMojoAdapter::GetPhotoState(
    GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(nullptr);
}

void SharedMemoryVirtualDeviceMojoAdapter::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SharedMemoryVirtualDeviceMojoAdapter::TakePhoto(
    TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SharedMemoryVirtualDeviceMojoAdapter::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SharedMemoryVirtualDeviceMojoAdapter::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SharedMemoryVirtualDeviceMojoAdapter::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_frame_handler_.is_bound()) {
    // Unsubscribe from connection error callbacks.
    video_frame_handler_.set_disconnect_handler(base::OnceClosure());
    // Send out OnBufferRetired events and OnStopped.
    for (auto buffer_id : known_buffer_ids_)
      video_frame_handler_->OnBufferRetired(buffer_id);
    video_frame_handler_->OnStopped();
    video_frame_handler_.reset();
  } else if (video_frame_handler_in_process_) {
    // Send out OnBufferRetired events and OnStopped.
    for (auto buffer_id : known_buffer_ids_)
      video_frame_handler_in_process_->OnBufferRetired(buffer_id);
    video_frame_handler_in_process_->OnStopped();
    video_frame_handler_in_process_ = nullptr;
  }
}

void SharedMemoryVirtualDeviceMojoAdapter::OnReceiverConnectionErrorOrClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

}  // namespace video_capture
