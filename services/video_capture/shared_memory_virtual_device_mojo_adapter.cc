// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/shared_memory_virtual_device_mojo_adapter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/scoped_buffer_pool_reservation.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/scoped_access_permission_media_to_mojo_adapter.h"

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
    mojo::Remote<mojom::Producer> producer,
    bool send_buffer_handles_to_producer_as_raw_file_descriptors)
    : producer_(std::move(producer)),
      send_buffer_handles_to_producer_as_raw_file_descriptors_(
          send_buffer_handles_to_producer_as_raw_file_descriptors),
      buffer_pool_(new media::VideoCaptureBufferPoolImpl(
          std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>(),
          media::VideoCaptureBufferType::kSharedMemory,
          max_buffer_pool_buffer_count())) {}

SharedMemoryVirtualDeviceMojoAdapter::~SharedMemoryVirtualDeviceMojoAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count() {
  // The maximum number of video frame buffers in-flight at any one time
  // If all buffers are still in use by consumers when new frames are produced
  // those frames get dropped.
  static const int kMaxBufferCount = 3;

  return kMaxBufferCount;
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
    auto entry_iter = std::find(known_buffer_ids_.begin(),
                                known_buffer_ids_.end(), buffer_id_to_drop);
    if (entry_iter != known_buffer_ids_.end()) {
      known_buffer_ids_.erase(entry_iter);
      if (producer_.is_bound())
        producer_->OnBufferRetired(buffer_id_to_drop);
      if (video_frame_handler_.is_bound()) {
        video_frame_handler_->OnBufferRetired(buffer_id_to_drop);
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
          media::mojom::VideoBufferHandle::New();
      buffer_handle->set_shared_buffer_handle(
          buffer_pool_->DuplicateAsMojoBuffer(buffer_id));
      video_frame_handler_->OnNewBuffer(buffer_id, std::move(buffer_handle));
    }
    known_buffer_ids_.push_back(buffer_id);

    // Share buffer handle with producer.
    media::mojom::VideoBufferHandlePtr buffer_handle =
        media::mojom::VideoBufferHandle::New();
    if (send_buffer_handles_to_producer_as_raw_file_descriptors_) {
      buffer_handle->set_shared_memory_via_raw_file_descriptor(
          buffer_pool_->CreateSharedMemoryViaRawFileDescriptorStruct(
              buffer_id));
    } else {
      buffer_handle->set_shared_buffer_handle(
          buffer_pool_->DuplicateAsMojoBuffer(buffer_id));
    }
    // Invoke the response back only after the producer have acked
    // that it has received the newly created buffer. This is need
    // because the |producer_| and the |callback| are bound to different
    // message pipes, so the order for calls to |producer_| and |callback|
    // is not guaranteed.
    if (producer_.is_bound())
      producer_->OnNewBuffer(
          buffer_id, std::move(buffer_handle),
          base::BindOnce(&OnNewBufferAcknowleged, base::Passed(&callback),
                         buffer_id));
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
    buffer_pool_->HoldForConsumers(buffer_id, 1 /* num_clients */);
    auto access_permission = std::make_unique<
        media::ScopedBufferPoolReservation<media::ConsumerReleaseTraits>>(
        buffer_pool_, buffer_id);
    mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission_proxy;
    mojo::MakeSelfOwnedReceiver<mojom::ScopedAccessPermission>(
        std::make_unique<ScopedAccessPermissionMediaToMojoAdapter>(
            std::move(access_permission)),
        access_permission_proxy.InitWithNewPipeAndPassReceiver());
    video_frame_handler_->OnFrameReadyInBuffer(
        buffer_id, 0 /* frame_feedback_id */,
        std::move(access_permission_proxy), std::move(frame_info));
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
        media::mojom::VideoBufferHandle::New();
    buffer_handle->set_shared_buffer_handle(
        buffer_pool_->DuplicateAsMojoBuffer(buffer_id));
    video_frame_handler_->OnNewBuffer(buffer_id, std::move(buffer_handle));
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

void SharedMemoryVirtualDeviceMojoAdapter::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_frame_handler_.is_bound())
    return;
  // Unsubscribe from connection error callbacks.
  video_frame_handler_.set_disconnect_handler(base::OnceClosure());
  // Send out OnBufferRetired events and OnStopped.
  for (auto buffer_id : known_buffer_ids_)
    video_frame_handler_->OnBufferRetired(buffer_id);
  video_frame_handler_->OnStopped();
  video_frame_handler_.reset();
}

void SharedMemoryVirtualDeviceMojoAdapter::OnReceiverConnectionErrorOrClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

}  // namespace video_capture
