// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/broadcasting_receiver.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace video_capture {

// A mojom::VideoFrameAccessHandler implementation that forwards buffer release
// calls to the BroadcastingReceiver.
class BroadcastingReceiver::ClientVideoFrameAccessHandler
    : public mojom::VideoFrameAccessHandler {
 public:
  explicit ClientVideoFrameAccessHandler(
      base::WeakPtr<BroadcastingReceiver> broadcasting_receiver)
      : broadcasting_receiver_(std::move(broadcasting_receiver)) {}

  // mojom::VideoFrameAccessHandler implementation.
  void OnFinishedConsumingBuffer(int32_t buffer_id) override {
    if (!broadcasting_receiver_) {
      return;
    }
    broadcasting_receiver_->OnClientFinishedConsumingFrame(buffer_id);
  }

 private:
  base::WeakPtr<BroadcastingReceiver> broadcasting_receiver_;
};

BroadcastingReceiver::ClientContext::ClientContext(
    mojo::PendingRemote<mojom::VideoFrameHandler> client,
    media::VideoCaptureBufferType target_buffer_type)
    : client_(std::move(client)),
      target_buffer_type_(target_buffer_type),
      is_suspended_(false),
      on_started_has_been_called_(false),
      on_started_using_gpu_decode_has_been_called_(false),
      has_client_frame_access_handler_remote_(false) {}

BroadcastingReceiver::ClientContext::~ClientContext() = default;

BroadcastingReceiver::ClientContext::ClientContext(
    BroadcastingReceiver::ClientContext&& other) = default;

BroadcastingReceiver::ClientContext& BroadcastingReceiver::ClientContext::
operator=(BroadcastingReceiver::ClientContext&& other) = default;

void BroadcastingReceiver::ClientContext::OnStarted() {
  if (on_started_has_been_called_)
    return;
  on_started_has_been_called_ = true;
  client_->OnStarted();
}

void BroadcastingReceiver::ClientContext::OnStartedUsingGpuDecode() {
  if (on_started_using_gpu_decode_has_been_called_)
    return;
  on_started_using_gpu_decode_has_been_called_ = true;
  client_->OnStartedUsingGpuDecode();
}

BroadcastingReceiver::BufferContext::BufferContext(
    int buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle)
    : buffer_id_(buffer_id),
      buffer_handle_(std::move(buffer_handle)),
      consumer_hold_count_(0),
      is_retired_(false) {
  static int next_buffer_context_id = 0;
  buffer_context_id_ = next_buffer_context_id++;
}

BroadcastingReceiver::BufferContext::~BufferContext() = default;

BroadcastingReceiver::BufferContext::BufferContext(
    BroadcastingReceiver::BufferContext&& other)
    : buffer_context_id_(other.buffer_context_id_),
      buffer_id_(other.buffer_id_),
      buffer_handle_(std::move(other.buffer_handle_)),
      consumer_hold_count_(other.consumer_hold_count_),
      is_retired_(other.is_retired_) {
  // The consumer hold was moved from |other|.
  other.consumer_hold_count_ = 0;
}

BroadcastingReceiver::BufferContext&
BroadcastingReceiver::BufferContext::operator=(
    BroadcastingReceiver::BufferContext&& other) {
  buffer_context_id_ = other.buffer_context_id_;
  buffer_id_ = other.buffer_id_;
  buffer_handle_ = std::move(other.buffer_handle_);
  consumer_hold_count_ = other.consumer_hold_count_;
  is_retired_ = other.is_retired_;
  // The consumer hold was moved from |other|.
  other.consumer_hold_count_ = 0;
  return *this;
}

void BroadcastingReceiver::BufferContext::IncreaseConsumerCount() {
  consumer_hold_count_++;
}

void BroadcastingReceiver::BufferContext::DecreaseConsumerCount() {
  consumer_hold_count_--;
}

bool BroadcastingReceiver::BufferContext::IsStillBeingConsumed() const {
  return consumer_hold_count_ > 0;
}

media::mojom::VideoBufferHandlePtr
BroadcastingReceiver::BufferContext::CloneBufferHandle(
    media::VideoCaptureBufferType target_buffer_type) {
  // If the source uses a shared image handle, i.e. texture, we pass it
  // through without conversion, no matter what clients requested.
  if (buffer_handle_->is_shared_image_handle()) {
    return media::mojom::VideoBufferHandle::NewSharedImageHandle(
        buffer_handle_->get_shared_image_handle()->Clone());
  }

  // If the source uses a GpuMemoryBuffer handle, we pass it through without
  // conversion, no matter what clients requested.
  if (buffer_handle_->is_gpu_memory_buffer_handle()) {
    return media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
        buffer_handle_->get_gpu_memory_buffer_handle().Clone());
  }

  switch (target_buffer_type) {
    case media::VideoCaptureBufferType::kMailboxHolder:
      NOTREACHED_IN_MIGRATION()
          << "Cannot convert buffer type to kMailboxHolder from "
             "handle type other than mailbox handles.";
      break;
    case media::VideoCaptureBufferType::kSharedMemory:
      if (buffer_handle_->is_unsafe_shmem_region()) {
        return media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
            buffer_handle_->get_unsafe_shmem_region().Duplicate());
      } else {
        NOTREACHED_IN_MIGRATION() << "Unexpected video buffer handle type";
      }
      break;
    case media::VideoCaptureBufferType::kGpuMemoryBuffer:
#if BUILDFLAG(IS_WIN)
      // On windows with MediaFoundationD3D11VideoCapture if the
      // texture capture path fails, a ShMem buffer might be produced instead.
      DCHECK(buffer_handle_->is_unsafe_shmem_region());
      return media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
          buffer_handle_->get_unsafe_shmem_region().Duplicate());
#else
      NOTREACHED_IN_MIGRATION() << "Unexpected GpuMemoryBuffer handle type";
      break;
#endif
  }
  return media::mojom::VideoBufferHandlePtr();
}

BroadcastingReceiver::BroadcastingReceiver()
    : status_(Status::kOnStartedHasNotYetBeenCalled),
      error_(media::VideoCaptureError::kNone),
      next_client_id_(0) {}

BroadcastingReceiver::~BroadcastingReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::WeakPtr<media::VideoFrameReceiver> BroadcastingReceiver::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void BroadcastingReceiver::HideSourceRestartFromClients(
    base::OnceClosure on_stopped_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_stopped_handler_ = std::move(on_stopped_handler);
  status_ = Status::kDeviceIsRestarting;
}

void BroadcastingReceiver::SetOnStoppedHandler(
    base::OnceClosure on_stopped_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_stopped_handler_ = std::move(on_stopped_handler);
}

int32_t BroadcastingReceiver::AddClient(
    mojo::PendingRemote<mojom::VideoFrameHandler> client,
    media::VideoCaptureBufferType target_buffer_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto client_id = next_client_id_++;
  ClientContext context(std::move(client), target_buffer_type);
  auto& added_client_context =
      clients_.insert(std::make_pair(client_id, std::move(context)))
          .first->second;
  added_client_context.client().set_disconnect_handler(
      base::BindOnce(&BroadcastingReceiver::OnClientDisconnected,
                     weak_factory_.GetWeakPtr(), client_id));
  if (status_ == Status::kOnErrorHasBeenCalled) {
    added_client_context.client()->OnError(error_);
    return client_id;
  }
  if (status_ == Status::kOnStartedHasBeenCalled) {
    added_client_context.OnStarted();
  }
  if (status_ == Status::kOnStartedUsingGpuDecodeHasBeenCalled) {
    added_client_context.OnStarted();
    added_client_context.OnStartedUsingGpuDecode();
  }

  for (auto& buffer_context : buffer_contexts_) {
    if (buffer_context.is_retired())
      continue;
    added_client_context.client()->OnNewBuffer(
        buffer_context.buffer_context_id(),
        buffer_context.CloneBufferHandle(
            added_client_context.target_buffer_type()));
  }
  return client_id;
}

void BroadcastingReceiver::SuspendClient(int32_t client_id) {
  clients_.at(client_id).set_is_suspended(true);
}

void BroadcastingReceiver::ResumeClient(int32_t client_id) {
  clients_.at(client_id).set_is_suspended(false);
}

mojo::Remote<mojom::VideoFrameHandler> BroadcastingReceiver::RemoveClient(
    int32_t client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto client = std::move(clients_.at(client_id));
  clients_.erase(client_id);
  return std::move(client.client());
}

void BroadcastingReceiver::OnCaptureConfigurationChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second.client()->OnCaptureConfigurationChanged();
  }
}

void BroadcastingReceiver::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(FindUnretiredBufferContextFromBufferId(buffer_id) ==
        buffer_contexts_.end());
  buffer_contexts_.emplace_back(buffer_id, std::move(buffer_handle));
  auto& buffer_context = buffer_contexts_.back();
  for (auto& client : clients_) {
    client.second.client()->OnNewBuffer(
        buffer_context.buffer_context_id(),
        buffer_context.CloneBufferHandle(client.second.target_buffer_type()));
  }
}

void BroadcastingReceiver::OnFrameReadyInBuffer(
    media::ReadyFrameInBuffer frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool has_consumers = false;
  for (auto& client : clients_) {
    if (!client.second.is_suspended()) {
      has_consumers = true;
      break;
    }
  }
  // If we don't have any consumers to forward the frame to, signal to finish
  // consuming the buffers immediately.
  if (!has_consumers)
    return;

  // Obtain buffer contexts for all frame representations.
  auto it = FindUnretiredBufferContextFromBufferId(frame.buffer_id);
  CHECK(it != buffer_contexts_.end());
  BufferContext* buffer_context = &(*it);
  auto result = scoped_access_permissions_by_buffer_context_id_.insert(
      std::make_pair(buffer_context->buffer_context_id(),
                     std::move(frame.buffer_read_permission)));
  DCHECK(result.second);
  // Broadcast to all clients.
  for (auto& client : clients_) {
    if (client.second.is_suspended())
      continue;
    // Set up a frame access handler for this client, if we haven't already. The
    // frame access handler mojo pipe is open for the lifetime of the
    // ClientContext.
    if (!client.second.has_client_frame_access_handler_remote()) {
      mojo::PendingRemote<mojom::VideoFrameAccessHandler>
          pending_frame_access_handler;
      mojo::MakeSelfOwnedReceiver<mojom::VideoFrameAccessHandler>(
          std::make_unique<ClientVideoFrameAccessHandler>(
              weak_factory_.GetWeakPtr()),
          pending_frame_access_handler.InitWithNewPipeAndPassReceiver());
      client.second.client()->OnFrameAccessHandlerReady(
          std::move(pending_frame_access_handler));
      client.second.set_has_client_frame_access_handler_remote();
    }

    buffer_context->IncreaseConsumerCount();
    mojom::ReadyFrameInBufferPtr ready_buffer = mojom::ReadyFrameInBuffer::New(
        buffer_context->buffer_context_id(), frame.frame_feedback_id,
        frame.frame_info.Clone());

    client.second.client()->OnFrameReadyInBuffer(std::move(ready_buffer));
  }
}

void BroadcastingReceiver::OnBufferRetired(int32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto buffer_context_iter = FindUnretiredBufferContextFromBufferId(buffer_id);
  CHECK(buffer_context_iter != buffer_contexts_.end());
  const auto context_id = buffer_context_iter->buffer_context_id();
  if (buffer_context_iter->IsStillBeingConsumed())
    // Mark the buffer context as retired but keep holding on to it until the
    // last client finished consuming it, because it contains the
    // |access_permission| required during consumption.
    buffer_context_iter->set_retired();
  else
    buffer_contexts_.erase(buffer_context_iter);
  for (auto& client : clients_) {
    client.second.client()->OnBufferRetired(context_id);
  }
}

void BroadcastingReceiver::OnError(media::VideoCaptureError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second.client()->OnError(error);
  }
  status_ = Status::kOnErrorHasBeenCalled;
  error_ = error;
}

void BroadcastingReceiver::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    if (client.second.is_suspended())
      continue;
    client.second.client()->OnFrameDropped(reason);
  }
}

void BroadcastingReceiver::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second.client()->OnNewSubCaptureTargetVersion(
        sub_capture_target_version);
  }
}

void BroadcastingReceiver::OnFrameWithEmptyRegionCapture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second.client()->OnFrameWithEmptyRegionCapture();
  }
}

void BroadcastingReceiver::OnLog(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second.client()->OnLog(message);
  }
}

void BroadcastingReceiver::OnStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second.OnStarted();
  }
  status_ = Status::kOnStartedHasBeenCalled;
}

void BroadcastingReceiver::OnStartedUsingGpuDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second.OnStartedUsingGpuDecode();
  }
  status_ = Status::kOnStartedUsingGpuDecodeHasBeenCalled;
}

void BroadcastingReceiver::OnStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ == Status::kDeviceIsRestarting) {
    status_ = Status::kOnStartedHasNotYetBeenCalled;
    std::move(on_stopped_handler_).Run();
  } else {
    for (auto& client : clients_) {
      client.second.client()->OnStopped();
    }
    status_ = Status::kOnStoppedHasBeenCalled;
    if (on_stopped_handler_)
      std::move(on_stopped_handler_).Run();
  }
  // Reset the frame access handler so that it is possible to bind a new one if
  // BroadcastingReceiver is started again in the future.
  frame_access_handler_remote_.reset();
}

void BroadcastingReceiver::OnClientFinishedConsumingFrame(
    int32_t buffer_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto buffer_context_iter = base::ranges::find(
      buffer_contexts_, buffer_context_id, &BufferContext::buffer_context_id);
  CHECK(buffer_context_iter != buffer_contexts_.end());
  buffer_context_iter->DecreaseConsumerCount();

  if (!buffer_context_iter->IsStillBeingConsumed()) {
    auto it =
        scoped_access_permissions_by_buffer_context_id_.find(buffer_context_id);
    if (it == scoped_access_permissions_by_buffer_context_id_.end()) {
      NOTREACHED_IN_MIGRATION();
      return;
    }
    scoped_access_permissions_by_buffer_context_id_.erase(it);
  }

  if (buffer_context_iter->is_retired() &&
      !buffer_context_iter->IsStillBeingConsumed()) {
    buffer_contexts_.erase(buffer_context_iter);
  }
}

void BroadcastingReceiver::OnClientDisconnected(int32_t client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clients_.erase(client_id);
}

std::vector<BroadcastingReceiver::BufferContext>::iterator
BroadcastingReceiver::FindUnretiredBufferContextFromBufferId(
    int32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::ranges::find_if(
      buffer_contexts_, [buffer_id](const BufferContext& entry) {
        return !entry.is_retired() && entry.buffer_id() == buffer_id;
      });
}

}  // namespace video_capture
