// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/broadcasting_receiver.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/video_capture/public/mojom/scoped_access_permission.mojom.h"

namespace video_capture {

namespace {

class ConsumerAccessPermission : public mojom::ScopedAccessPermission {
 public:
  ConsumerAccessPermission(base::OnceClosure destruction_cb)
      : destruction_cb_(std::move(destruction_cb)) {}
  ~ConsumerAccessPermission() override { std::move(destruction_cb_).Run(); }

 private:
  base::OnceClosure destruction_cb_;
};

void CloneSharedBufferHandle(const mojo::ScopedSharedBufferHandle& source,
                             media::mojom::VideoBufferHandlePtr* target) {
  // Buffers are always cloned read-write, as they can be used as output
  // buffers for the cross-process MojoMjpegDecodeAccelerator.
  //
  // TODO(crbug.com/793446): VideoBufferHandle.shared_buffer_handle is also
  // managed in VideoCaptureController, which makes it hard to keep shared
  // memory permissions consistent. Permissions should be coordinated better
  // between these two classes.
  (*target)->set_shared_buffer_handle(
      source->Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE));
}

void CloneSharedBufferToRawFileDescriptorHandle(
    const mojo::ScopedSharedBufferHandle& source,
    media::mojom::VideoBufferHandlePtr* target) {
#if defined(OS_LINUX)
  // |source| is unwrapped to a |PlatformSharedMemoryRegion|, from whence a file
  // descriptor can be extracted which is then mojo-wrapped.
  base::subtle::PlatformSharedMemoryRegion platform_region =
      mojo::UnwrapPlatformSharedMemoryRegion(
          source->Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE));
  auto sub_struct = media::mojom::SharedMemoryViaRawFileDescriptor::New();
  sub_struct->shared_memory_size_in_bytes = platform_region.GetSize();
  base::subtle::ScopedFDPair fds = platform_region.PassPlatformHandle();
  sub_struct->file_descriptor_handle = mojo::WrapPlatformFile(fds.fd.release());
  (*target)->set_shared_memory_via_raw_file_descriptor(std::move(sub_struct));
#else
  NOTREACHED() << "Cannot convert buffer handle to "
                  "kSharedMemoryViaRawFileDescriptor on non-Linux platform.";
#endif
}

void CloneGpuMemoryBufferHandle(const gfx::GpuMemoryBufferHandle& source,
                                media::mojom::VideoBufferHandlePtr* target) {
  (*target)->set_gpu_memory_buffer_handle(source.Clone());
}

}  // anonymous namespace

BroadcastingReceiver::ClientContext::ClientContext(
    mojo::PendingRemote<mojom::VideoFrameHandler> client,
    media::VideoCaptureBufferType target_buffer_type)
    : client_(std::move(client)),
      target_buffer_type_(target_buffer_type),
      is_suspended_(false),
      on_started_has_been_called_(false),
      on_started_using_gpu_decode_has_been_called_(false) {}

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
    BroadcastingReceiver::BufferContext&& other) = default;

BroadcastingReceiver::BufferContext& BroadcastingReceiver::BufferContext::
operator=(BroadcastingReceiver::BufferContext&& other) = default;

void BroadcastingReceiver::BufferContext::IncreaseConsumerCount() {
  consumer_hold_count_++;
}

void BroadcastingReceiver::BufferContext::DecreaseConsumerCount() {
  consumer_hold_count_--;
  if (consumer_hold_count_ == 0) {
    access_permission_.reset();
  }
}

bool BroadcastingReceiver::BufferContext::IsStillBeingConsumed() const {
  return consumer_hold_count_ > 0;
}

media::mojom::VideoBufferHandlePtr
BroadcastingReceiver::BufferContext::CloneBufferHandle(
    media::VideoCaptureBufferType target_buffer_type) {
  media::mojom::VideoBufferHandlePtr result =
      media::mojom::VideoBufferHandle::New();

  // If the source uses mailbox hanldes, i.e. textures, we pass those through
  // without conversion, no matter what clients requested.
  if (buffer_handle_->is_mailbox_handles()) {
    result->set_mailbox_handles(buffer_handle_->get_mailbox_handles()->Clone());
    return result;
  }

  switch (target_buffer_type) {
    case media::VideoCaptureBufferType::kMailboxHolder:
      NOTREACHED() << "Cannot convert buffer type to kMailboxHolder from "
                      "handle type other than mailbox handles.";
      break;
    case media::VideoCaptureBufferType::kSharedMemory:
      if (buffer_handle_->is_shared_buffer_handle()) {
        CloneSharedBufferHandle(buffer_handle_->get_shared_buffer_handle(),
                                &result);
      } else if (buffer_handle_->is_shared_memory_via_raw_file_descriptor()) {
        ConvertRawFileDescriptorToSharedBuffer();
        CloneSharedBufferHandle(buffer_handle_->get_shared_buffer_handle(),
                                &result);
      } else {
        NOTREACHED() << "Unexpected video buffer handle type";
      }
      break;
    case media::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor:
      if (buffer_handle_->is_shared_buffer_handle()) {
        CloneSharedBufferToRawFileDescriptorHandle(
            buffer_handle_->get_shared_buffer_handle(), &result);
      } else if (buffer_handle_->is_shared_memory_via_raw_file_descriptor()) {
        ConvertRawFileDescriptorToSharedBuffer();
        CloneSharedBufferToRawFileDescriptorHandle(
            buffer_handle_->get_shared_buffer_handle(), &result);
      } else {
        NOTREACHED() << "Unexpected video buffer handle type";
      }
      break;
    case media::VideoCaptureBufferType::kGpuMemoryBuffer:
      CloneGpuMemoryBufferHandle(buffer_handle_->get_gpu_memory_buffer_handle(),
                                 &result);
      break;
  }
  return result;
}

void BroadcastingReceiver::BufferContext::
    ConvertRawFileDescriptorToSharedBuffer() {
  DCHECK(buffer_handle_->is_shared_memory_via_raw_file_descriptor());

#if defined(OS_LINUX)
  // The conversion unwraps the descriptor from its mojo handle to the raw file
  // descriptor (ie, an int). This is used to create a
  // PlatformSharedMemoryRegion which is then wrapped as a
  // mojo::ScopedSharedBufferHandle.
  const size_t handle_size =
      buffer_handle_->get_shared_memory_via_raw_file_descriptor()
          ->shared_memory_size_in_bytes;
  base::PlatformFile raw_platform_file;
  MojoResult result = mojo::UnwrapPlatformFile(
      std::move(buffer_handle_->get_shared_memory_via_raw_file_descriptor()
                    ->file_descriptor_handle),
      &raw_platform_file);
  if (result != MOJO_RESULT_OK) {
    NOTREACHED();
    return;
  }
  base::ScopedFD platform_file(raw_platform_file);
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
  buffer_handle_->set_shared_buffer_handle(
      mojo::WrapPlatformSharedMemoryRegion(std::move(platform_region)));
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

void BroadcastingReceiver::OnFrameReadyInBuffer(
    int32_t buffer_id,
    int32_t frame_feedback_id,
    mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (clients_.empty())
    return;
  auto buffer_context_iter = FindUnretiredBufferContextFromBufferId(buffer_id);
  CHECK(buffer_context_iter != buffer_contexts_.end());
  auto& buffer_context = *buffer_context_iter;
  for (auto& client : clients_) {
    if (client.second.is_suspended())
      continue;
    if (access_permission)
      buffer_context.set_access_permission(std::move(access_permission));
    mojo::PendingRemote<mojom::ScopedAccessPermission>
        consumer_access_permission;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<ConsumerAccessPermission>(base::BindOnce(
            &BroadcastingReceiver::OnClientFinishedConsumingFrame,
            weak_factory_.GetWeakPtr(), buffer_context.buffer_context_id())),
        consumer_access_permission.InitWithNewPipeAndPassReceiver());
    client.second.client()->OnFrameReadyInBuffer(
        buffer_context.buffer_context_id(), frame_feedback_id,
        std::move(consumer_access_permission), frame_info.Clone());
    buffer_context.IncreaseConsumerCount();
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
