// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/broadcasting_receiver.h"

#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace video_capture {

namespace {

media::mojom::VideoBufferHandlePtr
CloneUnsafeShmemRegionToRawFileDescriptorHandle(
    const base::UnsafeSharedMemoryRegion& region) {
  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          region.Duplicate());
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  // On these platforms, the PlatformHandle is a single scoper object.
  auto platform_handle = platform_region.PassPlatformHandle();
#else
  // On Linux, a valid platform handle is a pair of FDs, and at least one of
  // those FDs must be valid: `fd`, if present, is readable and writable and
  // `readonly_fd`, if present, is only readable.
  //
  // For an `UnsafeSharedMemoryRegion` which is always writable, only `fd` is
  // present.
  base::ScopedFD platform_handle =
      std::move(platform_region.PassPlatformHandle().fd);
#endif
  return media::mojom::VideoBufferHandle::NewSharedMemoryViaRawFileDescriptor(
      media::mojom::SharedMemoryViaRawFileDescriptor::New(
          mojo::PlatformHandle(std::move(platform_handle)), region.GetSize()));
}

}  // namespace

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

BroadcastingReceiver::BufferContext::~BufferContext() {
  // Signal that the buffer is no longer in use, if we haven't already.
  if (consumer_hold_count_ != 0) {
    DCHECK(frame_access_handler_remote_);
    (*frame_access_handler_remote_)->OnFinishedConsumingBuffer(buffer_id_);
  }
}

BroadcastingReceiver::BufferContext::BufferContext(
    BroadcastingReceiver::BufferContext&& other)
    : buffer_context_id_(other.buffer_context_id_),
      buffer_id_(other.buffer_id_),
      frame_access_handler_remote_(other.frame_access_handler_remote_),
      buffer_handle_(std::move(other.buffer_handle_)),
      consumer_hold_count_(other.consumer_hold_count_),
      is_retired_(other.is_retired_) {
  // The consumer hold was moved from |other|.
  other.consumer_hold_count_ = 0;
  other.frame_access_handler_remote_ = nullptr;
}

BroadcastingReceiver::BufferContext&
BroadcastingReceiver::BufferContext::operator=(
    BroadcastingReceiver::BufferContext&& other) {
  buffer_context_id_ = other.buffer_context_id_;
  buffer_id_ = other.buffer_id_;
  frame_access_handler_remote_ = other.frame_access_handler_remote_;
  buffer_handle_ = std::move(other.buffer_handle_);
  consumer_hold_count_ = other.consumer_hold_count_;
  is_retired_ = other.is_retired_;
  // The consumer hold was moved from |other|.
  other.consumer_hold_count_ = 0;
  other.frame_access_handler_remote_ = nullptr;
  return *this;
}

void BroadcastingReceiver::BufferContext::SetFrameAccessHandlerRemote(
    scoped_refptr<VideoFrameAccessHandlerRemote> frame_access_handler_remote) {
  frame_access_handler_remote_ = frame_access_handler_remote;
}

void BroadcastingReceiver::BufferContext::IncreaseConsumerCount() {
  // The access handler should be ready if we have a consumer since it is needed
  // when the consumer decreases the consumer count.
  DCHECK(frame_access_handler_remote_);
  consumer_hold_count_++;
}

void BroadcastingReceiver::BufferContext::DecreaseConsumerCount() {
  DCHECK(frame_access_handler_remote_);
  consumer_hold_count_--;
  if (consumer_hold_count_ == 0) {
    (*frame_access_handler_remote_)->OnFinishedConsumingBuffer(buffer_id_);
  }
}

bool BroadcastingReceiver::BufferContext::IsStillBeingConsumed() const {
  return consumer_hold_count_ > 0;
}

media::mojom::VideoBufferHandlePtr
BroadcastingReceiver::BufferContext::CloneBufferHandle(
    media::VideoCaptureBufferType target_buffer_type) {
  // If the source uses mailbox handles, i.e. textures, we pass those through
  // without conversion, no matter what clients requested.
  if (buffer_handle_->is_mailbox_handles()) {
    return media::mojom::VideoBufferHandle::NewMailboxHandles(
        buffer_handle_->get_mailbox_handles()->Clone());
  }

  // If the source uses GpuMemoryBuffer handles, we pass those through without
  // conversion, no matter what clients requested.
  if (buffer_handle_->is_gpu_memory_buffer_handle()) {
    return media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
        buffer_handle_->get_gpu_memory_buffer_handle().Clone());
  }

  switch (target_buffer_type) {
    case media::VideoCaptureBufferType::kMailboxHolder:
      NOTREACHED() << "Cannot convert buffer type to kMailboxHolder from "
                      "handle type other than mailbox handles.";
      break;
    case media::VideoCaptureBufferType::kSharedMemory:
      if (buffer_handle_->is_unsafe_shmem_region()) {
        return media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
            buffer_handle_->get_unsafe_shmem_region().Duplicate());
      } else if (buffer_handle_->is_shared_memory_via_raw_file_descriptor()) {
        ConvertRawFileDescriptorToUnsafeShmemRegion();
        return media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
            buffer_handle_->get_unsafe_shmem_region().Duplicate());
      } else {
        NOTREACHED() << "Unexpected video buffer handle type";
      }
      break;
    case media::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor:
      if (buffer_handle_->is_unsafe_shmem_region()) {
        return CloneUnsafeShmemRegionToRawFileDescriptorHandle(
            buffer_handle_->get_unsafe_shmem_region());
      } else if (buffer_handle_->is_shared_memory_via_raw_file_descriptor()) {
        ConvertRawFileDescriptorToUnsafeShmemRegion();
        return CloneUnsafeShmemRegionToRawFileDescriptorHandle(
            buffer_handle_->get_unsafe_shmem_region().Duplicate());
      } else {
        NOTREACHED() << "Unexpected video buffer handle type";
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
      NOTREACHED() << "Unexpected GpuMemoryBuffer handle type";
      break;
#endif
  }
  return media::mojom::VideoBufferHandlePtr();
}

void BroadcastingReceiver::BufferContext::
    ConvertRawFileDescriptorToUnsafeShmemRegion() {
  DCHECK(buffer_handle_->is_shared_memory_via_raw_file_descriptor());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  const size_t handle_size =
      buffer_handle_->get_shared_memory_via_raw_file_descriptor()
          ->shared_memory_size_in_bytes;
  base::ScopedFD platform_file =
      buffer_handle_->get_shared_memory_via_raw_file_descriptor()
          ->file_descriptor_handle.TakeFD();
  base::UnguessableToken guid = base::UnguessableToken::Create();
  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          std::move(platform_file),
          base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe, handle_size,
          guid);
  if (!platform_region.IsValid()) {
    NOTREACHED();
    return;
  }
  buffer_handle_->set_unsafe_shmem_region(
      base::UnsafeSharedMemoryRegion::Deserialize(std::move(platform_region)));
#else
  NOTREACHED() << "Unable to consume buffer handle of type "
                  "kSharedMemoryViaRawFileDescriptor on non-Linux platform.";
#endif
}

BroadcastingReceiver::BroadcastingReceiver()
    : status_(Status::kOnStartedHasNotYetBeenCalled),
      error_(media::VideoCaptureError::kNone),
      next_client_id_(0) {}

BroadcastingReceiver::~BroadcastingReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void BroadcastingReceiver::OnFrameAccessHandlerReady(
    mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
        pending_frame_access_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!frame_access_handler_remote_);
  frame_access_handler_remote_ =
      base::MakeRefCounted<VideoFrameAccessHandlerRemote>(
          mojo::Remote<video_capture::mojom::VideoFrameAccessHandler>(
              std::move(pending_frame_access_handler)));
}

void BroadcastingReceiver::OnFrameReadyInBuffer(
    mojom::ReadyFrameInBufferPtr buffer,
    std::vector<mojom::ReadyFrameInBufferPtr> scaled_buffers) {
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
  if (!has_consumers) {
    if (frame_access_handler_remote_) {
      (*frame_access_handler_remote_)
          ->OnFinishedConsumingBuffer(buffer->buffer_id);
      for (const auto& scaled_buffer : scaled_buffers) {
        (*frame_access_handler_remote_)
            ->OnFinishedConsumingBuffer(scaled_buffer->buffer_id);
      }
    }
    return;
  }

  // Obtain buffer contexts for all frame representations.
  auto it = FindUnretiredBufferContextFromBufferId(buffer->buffer_id);
  CHECK(it != buffer_contexts_.end());
  BufferContext* buffer_context = &(*it);
  std::vector<BufferContext*> scaled_buffer_contexts;
  scaled_buffer_contexts.reserve(scaled_buffers.size());
  for (const auto& scaled_buffer : scaled_buffers) {
    it = FindUnretiredBufferContextFromBufferId(scaled_buffer->buffer_id);
    CHECK(it != buffer_contexts_.end());
    scaled_buffer_contexts.push_back(&(*it));
  }
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

    buffer_context->SetFrameAccessHandlerRemote(frame_access_handler_remote_);
    buffer_context->IncreaseConsumerCount();
    mojom::ReadyFrameInBufferPtr ready_buffer = mojom::ReadyFrameInBuffer::New(
        buffer_context->buffer_context_id(), buffer->frame_feedback_id,
        buffer->frame_info.Clone());

    std::vector<mojom::ReadyFrameInBufferPtr> scaled_ready_buffers;
    scaled_ready_buffers.reserve(scaled_buffers.size());
    for (size_t i = 0; i < scaled_buffers.size(); ++i) {
      scaled_buffer_contexts[i]->SetFrameAccessHandlerRemote(
          frame_access_handler_remote_);
      scaled_buffer_contexts[i]->IncreaseConsumerCount();
      scaled_ready_buffers.push_back(mojom::ReadyFrameInBuffer::New(
          scaled_buffer_contexts[i]->buffer_context_id(),
          scaled_buffers[i]->frame_feedback_id,
          scaled_buffers[i]->frame_info.Clone()));
    }
    client.second.client()->OnFrameReadyInBuffer(
        std::move(ready_buffer), std::move(scaled_ready_buffers));
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

void BroadcastingReceiver::OnNewCropVersion(uint32_t crop_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second.client()->OnNewCropVersion(crop_version);
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
  auto buffer_context_iter =
      std::find_if(buffer_contexts_.begin(), buffer_contexts_.end(),
                   [buffer_context_id](const BufferContext& entry) {
                     return entry.buffer_context_id() == buffer_context_id;
                   });
  CHECK(buffer_context_iter != buffer_contexts_.end());
  buffer_context_iter->DecreaseConsumerCount();
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
  return std::find_if(buffer_contexts_.begin(), buffer_contexts_.end(),
                      [buffer_id](const BufferContext& entry) {
                        return !entry.is_retired() &&
                               entry.buffer_id() == buffer_id;
                      });
}

}  // namespace video_capture
