// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/core_ipcz.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/mojo_trap.h"
#include "mojo/core/ipcz_driver/wrapped_platform_handle.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core {

namespace {

// The ipcz-based implementation of Mojo message objects. ipcz API exposes no
// notion of message objects, so this is merely heap storage for data and ipcz
// handles.
class MojoMessage {
 public:
  // Even with an input size of 0, MojoAppendMessageData is expected to allocate
  // *some* storage for message data. This constant therefore sets a lower bound
  // on payload allocation size. 32 bytes is chosen since it's the smallest
  // possible Mojo bindings message size (v0 header + 8 byte payload)
  static constexpr size_t kMinBufferSize = 32;

  MojoMessage() = default;
  MojoMessage(std::vector<uint8_t> data, std::vector<IpczHandle> handles) {
    SetContents(std::move(data), std::move(handles));
  }

  ~MojoMessage() {
    for (IpczHandle handle : handles_) {
      if (handle != IPCZ_INVALID_HANDLE) {
        GetIpczAPI().Close(handle, IPCZ_NO_FLAGS, nullptr);
      }
    }
  }

  static MojoMessage* FromHandle(MojoMessageHandle handle) {
    return reinterpret_cast<MojoMessage*>(handle);
  }

  static std::unique_ptr<MojoMessage> TakeFromHandle(MojoMessageHandle handle) {
    return base::WrapUnique(FromHandle(handle));
  }

  MojoMessageHandle handle() const {
    return reinterpret_cast<MojoMessageHandle>(this);
  }

  base::span<uint8_t> data() { return data_; }
  std::vector<IpczHandle>& handles() { return handles_; }

  void SetContents(std::vector<uint8_t> data, std::vector<IpczHandle> handles) {
    const size_t size = data.size();
    if (size >= kMinBufferSize) {
      data_storage_ = std::move(data);
    } else {
      data_storage_.resize(kMinBufferSize);
      std::copy(data.begin(), data.end(), data_storage_.begin());
    }
    data_ = base::make_span(data_storage_).first(size);
    handles_ = std::move(handles);
  }

  MojoResult AppendData(uint32_t additional_num_bytes,
                        const MojoHandle* handles,
                        uint32_t num_handles,
                        void** buffer,
                        uint32_t* buffer_size) {
    const size_t new_data_size = data_.size() + additional_num_bytes;
    const size_t required_storage_size =
        std::max(new_data_size, kMinBufferSize);
    if (required_storage_size > data_storage_.size()) {
      data_storage_.resize(std::max(data_.size() * 2, required_storage_size));
    }
    data_ = base::make_span(data_storage_).first(new_data_size);

    handles_.reserve(handles_.size() + num_handles);
    for (MojoHandle handle : base::make_span(handles, num_handles)) {
      handles_.push_back(handle);
    }
    if (buffer) {
      *buffer = data_storage_.data();
    }
    if (buffer_size) {
      *buffer_size = base::checked_cast<uint32_t>(data_storage_.size());
    }
    return MOJO_RESULT_OK;
  }

  IpczResult GetData(void** buffer,
                     uint32_t* num_bytes,
                     MojoHandle* handles,
                     uint32_t* num_handles,
                     bool consume_handles) {
    if (buffer) {
      *buffer = data_storage_.data();
    }
    if (num_bytes) {
      *num_bytes = base::checked_cast<uint32_t>(data_.size());
    }

    if (!consume_handles || handles_.empty()) {
      return MOJO_RESULT_OK;
    }

    uint32_t capacity = num_handles ? *num_handles : 0;
    uint32_t required_capacity = base::checked_cast<uint32_t>(handles_.size());
    if (num_handles) {
      *num_handles = required_capacity;
    }
    if (!handles || capacity < required_capacity) {
      return MOJO_RESULT_RESOURCE_EXHAUSTED;
    }

    std::copy(handles_.begin(), handles_.end(), handles);
    handles_.clear();
    return MOJO_RESULT_OK;
  }

 private:
  std::vector<uint8_t> data_storage_;
  base::span<uint8_t> data_;
  std::vector<IpczHandle> handles_;
};

// ipcz get and put operations differ slightly in their return code semantics as
// compared to Mojo read and write operations. These helpers perform the
// translation.
MojoResult GetMojoReadResultForIpczGet(IpczResult result) {
  if (result == IPCZ_RESULT_UNAVAILABLE) {
    // The peer is still open but there are not currently any parcels to read.
    return MOJO_RESULT_SHOULD_WAIT;
  }
  if (result == IPCZ_RESULT_NOT_FOUND) {
    // There are no more parcels to read and the peer is closed.
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  return result;
}

MojoResult GetMojoWriteResultForIpczPut(IpczResult result) {
  if (result == IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    // For put operations with limits, which are used to emulate data pipe
    // producer writes, this indicates that the caller needs to try again later
    // due to the pipe being at capacity.
    return MOJO_RESULT_SHOULD_WAIT;
  }
  if (result == IPCZ_RESULT_NOT_FOUND) {
    // The peer is closed.
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  return result;
}

extern "C" {

MojoResult MojoInitializeIpcz(const struct MojoInitializeOptions* options) {
  NOTREACHED();
  return MOJO_RESULT_OK;
}

MojoTimeTicks MojoGetTimeTicksNowIpcz() {
  return base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
}

MojoResult MojoCloseIpcz(MojoHandle handle) {
  return GetIpczAPI().Close(handle, IPCZ_NO_FLAGS, nullptr);
}

MojoResult MojoQueryHandleSignalsStateIpcz(
    MojoHandle handle,
    MojoHandleSignalsState* signals_state) {
  IpczPortalStatus status = {sizeof(status)};
  IpczResult result =
      GetIpczAPI().QueryPortalStatus(handle, IPCZ_NO_FLAGS, nullptr, &status);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  // TODO: These signals aren't quite accurate for data pipe handles.
  signals_state->satisfiable_signals = MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  signals_state->satisfied_signals = 0;
  if (status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) {
    signals_state->satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  } else {
    signals_state->satisfiable_signals |= MOJO_HANDLE_SIGNAL_WRITABLE |
                                          MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED |
                                          MOJO_HANDLE_SIGNAL_PEER_REMOTE;
    signals_state->satisfied_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
  }
  if ((status.flags & IPCZ_PORTAL_STATUS_DEAD) == 0) {
    signals_state->satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  }
  if (status.num_local_parcels > 0) {
    signals_state->satisfied_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  }
  return MOJO_RESULT_OK;
}

MojoResult MojoCreateMessagePipeIpcz(
    const MojoCreateMessagePipeOptions* options,
    MojoHandle* message_pipe_handle0,
    MojoHandle* message_pipe_handle1) {
  return GetIpczAPI().OpenPortals(GetIpczNode(), IPCZ_NO_FLAGS, nullptr,
                                  message_pipe_handle0, message_pipe_handle1);
}

MojoResult MojoWriteMessageIpcz(MojoHandle message_pipe_handle,
                                MojoMessageHandle message,
                                const MojoWriteMessageOptions* options) {
  auto m = MojoMessage::TakeFromHandle(message);
  if (!m || !message_pipe_handle) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  IpczResult result = GetIpczAPI().Put(
      message_pipe_handle, m->data().data(), m->data().size(),
      m->handles().data(), m->handles().size(), IPCZ_NO_FLAGS, nullptr);
  if (result == IPCZ_RESULT_NOT_FOUND) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  if (result == IPCZ_RESULT_OK) {
    // Ensure the hadles don't get freed on MojoMessage destruction, as their
    // ownership was relinquished in Put() above.
    m->handles().clear();
  }

  return GetMojoWriteResultForIpczPut(result);
}

MojoResult MojoReadMessageIpcz(MojoHandle message_pipe_handle,
                               const MojoReadMessageOptions* options,
                               MojoMessageHandle* message) {
  std::vector<uint8_t> data;
  std::vector<MojoHandle> handles;
  size_t num_bytes = 0;
  size_t num_handles = 0;
  IpczResult result =
      GetIpczAPI().Get(message_pipe_handle, IPCZ_NO_FLAGS, nullptr, nullptr,
                       &num_bytes, nullptr, &num_handles);
  if (result == IPCZ_RESULT_OK) {
    *message = std::make_unique<MojoMessage>().release()->handle();
    return MOJO_RESULT_OK;
  }

  if (result != IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    return GetMojoReadResultForIpczGet(result);
  }

  data.resize(num_bytes);
  handles.resize(num_handles);
  result =
      GetIpczAPI().Get(message_pipe_handle, IPCZ_NO_FLAGS, nullptr, data.data(),
                       &num_bytes, handles.data(), &num_handles);
  if (result != IPCZ_RESULT_OK) {
    return GetMojoReadResultForIpczGet(result);
  }

  auto m = std::make_unique<MojoMessage>();
  m->SetContents(std::move(data), std::move(handles));
  *message = m.release()->handle();
  return MOJO_RESULT_OK;
}

MojoResult MojoFuseMessagePipesIpcz(
    MojoHandle handle0,
    MojoHandle handle1,
    const MojoFuseMessagePipesOptions* options) {
  return GetIpczAPI().MergePortals(handle0, handle1, IPCZ_NO_FLAGS, nullptr);
}

MojoResult MojoCreateMessageIpcz(const MojoCreateMessageOptions* options,
                                 MojoMessageHandle* message) {
  auto new_message = std::make_unique<MojoMessage>();
  *message = new_message.release()->handle();
  return MOJO_RESULT_OK;
}

MojoResult MojoDestroyMessageIpcz(MojoMessageHandle message) {
  std::unique_ptr<MojoMessage> scoped_message(MojoMessage::FromHandle(message));
  return scoped_message ? MOJO_RESULT_OK : MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult MojoSerializeMessageIpcz(
    MojoMessageHandle message,
    const MojoSerializeMessageOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoAppendMessageDataIpcz(
    MojoMessageHandle message,
    uint32_t additional_payload_size,
    const MojoHandle* handles,
    uint32_t num_handles,
    const MojoAppendMessageDataOptions* options,
    void** buffer,
    uint32_t* buffer_size) {
  if (auto* m = MojoMessage::FromHandle(message)) {
    return m->AppendData(additional_payload_size, handles, num_handles, buffer,
                         buffer_size);
  }
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult MojoGetMessageDataIpcz(MojoMessageHandle message,
                                  const MojoGetMessageDataOptions* options,
                                  void** buffer,
                                  uint32_t* num_bytes,
                                  MojoHandle* handles,
                                  uint32_t* num_handles) {
  if (auto* m = MojoMessage::FromHandle(message)) {
    const bool consume_handles =
        !options ||
        ((options->flags & MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES) == 0);
    return m->GetData(buffer, num_bytes, handles, num_handles, consume_handles);
  }
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult MojoSetMessageContextIpcz(
    MojoMessageHandle message,
    uintptr_t context,
    MojoMessageContextSerializer serializer,
    MojoMessageContextDestructor destructor,
    const MojoSetMessageContextOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoGetMessageContextIpcz(
    MojoMessageHandle message,
    const MojoGetMessageContextOptions* options,
    uintptr_t* context) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoNotifyBadMessageIpcz(
    MojoMessageHandle message,
    const char* error,
    uint32_t error_num_bytes,
    const MojoNotifyBadMessageOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoCreateDataPipeIpcz(const MojoCreateDataPipeOptions* options,
                                  MojoHandle* data_pipe_producer_handle,
                                  MojoHandle* data_pipe_consumer_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoWriteDataIpcz(MojoHandle data_pipe_producer_handle,
                             const void* elements,
                             uint32_t* num_elements,
                             const MojoWriteDataOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoBeginWriteDataIpcz(MojoHandle data_pipe_producer_handle,
                                  const MojoBeginWriteDataOptions* options,
                                  void** buffer,
                                  uint32_t* buffer_num_elements) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoEndWriteDataIpcz(MojoHandle data_pipe_producer_handle,
                                uint32_t num_elements_written,
                                const MojoEndWriteDataOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoReadDataIpcz(MojoHandle data_pipe_consumer_handle,
                            const MojoReadDataOptions* options,
                            void* elements,
                            uint32_t* num_elements) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoBeginReadDataIpcz(MojoHandle data_pipe_consumer_handle,
                                 const MojoBeginReadDataOptions* options,
                                 const void** buffer,
                                 uint32_t* buffer_num_elements) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoEndReadDataIpcz(MojoHandle data_pipe_consumer_handle,
                               uint32_t num_elements_read,
                               const MojoEndReadDataOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoCreateSharedBufferIpcz(
    uint64_t num_bytes,
    const MojoCreateSharedBufferOptions* options,
    MojoHandle* shared_buffer_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoDuplicateBufferHandleIpcz(
    MojoHandle buffer_handle,
    const MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoMapBufferIpcz(MojoHandle buffer_handle,
                             uint64_t offset,
                             uint64_t num_bytes,
                             const MojoMapBufferOptions* options,
                             void** address) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoUnmapBufferIpcz(void* address) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoGetBufferInfoIpcz(MojoHandle buffer_handle,
                                 const MojoGetBufferInfoOptions* options,
                                 MojoSharedBufferInfo* info) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoCreateTrapIpcz(MojoTrapEventHandler handler,
                              const MojoCreateTrapOptions* options,
                              MojoHandle* trap_handle) {
  if (!handler || !trap_handle) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  *trap_handle = ipcz_driver::MojoTrap::MakeBoxed(handler);
  return MOJO_RESULT_OK;
}

MojoResult MojoAddTriggerIpcz(MojoHandle trap_handle,
                              MojoHandle handle,
                              MojoHandleSignals signals,
                              MojoTriggerCondition condition,
                              uintptr_t context,
                              const MojoAddTriggerOptions* options) {
  auto* trap = ipcz_driver::MojoTrap::FromBox(trap_handle);
  if (!trap) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return trap->AddTrigger(handle, signals, condition, context);
}

MojoResult MojoRemoveTriggerIpcz(MojoHandle trap_handle,
                                 uintptr_t context,
                                 const MojoRemoveTriggerOptions* options) {
  auto* trap = ipcz_driver::MojoTrap::FromBox(trap_handle);
  if (!trap) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return trap->RemoveTrigger(context);
}

MojoResult MojoArmTrapIpcz(MojoHandle trap_handle,
                           const MojoArmTrapOptions* options,
                           uint32_t* num_blocking_events,
                           MojoTrapEvent* blocking_events) {
  auto* trap = ipcz_driver::MojoTrap::FromBox(trap_handle);
  if (!trap) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  return trap->Arm(blocking_events, num_blocking_events);
}

MojoResult MojoWrapPlatformHandleIpcz(
    const MojoPlatformHandle* platform_handle,
    const MojoWrapPlatformHandleOptions* options,
    MojoHandle* mojo_handle) {
  if (!platform_handle || !mojo_handle) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  auto handle = PlatformHandle::FromMojoPlatformHandle(platform_handle);
  *mojo_handle =
      ipcz_driver::WrappedPlatformHandle::MakeBoxed(std::move(handle));
  return MOJO_RESULT_OK;
}

MojoResult MojoUnwrapPlatformHandleIpcz(
    MojoHandle mojo_handle,
    const MojoUnwrapPlatformHandleOptions* options,
    MojoPlatformHandle* platform_handle) {
  if (!mojo_handle || !platform_handle ||
      platform_handle->struct_size < sizeof(*platform_handle)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  auto wrapper = ipcz_driver::WrappedPlatformHandle::Unbox(mojo_handle);
  if (!wrapper) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  PlatformHandle::ToMojoPlatformHandle(std::move(wrapper->handle()),
                                       platform_handle);
  return MOJO_RESULT_OK;
}

MojoResult MojoWrapPlatformSharedMemoryRegionIpcz(
    const MojoPlatformHandle* platform_handles,
    uint32_t num_platform_handles,
    uint64_t num_bytes,
    const MojoSharedBufferGuid* guid,
    MojoPlatformSharedMemoryRegionAccessMode access_mode,
    const MojoWrapPlatformSharedMemoryRegionOptions* options,
    MojoHandle* mojo_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoUnwrapPlatformSharedMemoryRegionIpcz(
    MojoHandle mojo_handle,
    const MojoUnwrapPlatformSharedMemoryRegionOptions* options,
    MojoPlatformHandle* platform_handles,
    uint32_t* num_platform_handles,
    uint64_t* num_bytes,
    MojoSharedBufferGuid* mojo_guid,
    MojoPlatformSharedMemoryRegionAccessMode* access_mode) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoCreateInvitationIpcz(const MojoCreateInvitationOptions* options,
                                    MojoHandle* invitation_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoAttachMessagePipeToInvitationIpcz(
    MojoHandle invitation_handle,
    const void* name,
    uint32_t name_num_bytes,
    const MojoAttachMessagePipeToInvitationOptions* options,
    MojoHandle* message_pipe_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoExtractMessagePipeFromInvitationIpcz(
    MojoHandle invitation_handle,
    const void* name,
    uint32_t name_num_bytes,
    const MojoExtractMessagePipeFromInvitationOptions* options,
    MojoHandle* message_pipe_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoSendInvitationIpcz(
    MojoHandle invitation_handle,
    const MojoPlatformProcessHandle* process_handle,
    const MojoInvitationTransportEndpoint* transport_endpoint,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    const MojoSendInvitationOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoAcceptInvitationIpcz(
    const MojoInvitationTransportEndpoint* transport_endpoint,
    const MojoAcceptInvitationOptions* options,
    MojoHandle* invitation_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoSetQuotaIpcz(MojoHandle handle,
                            MojoQuotaType type,
                            uint64_t limit,
                            const MojoSetQuotaOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoQueryQuotaIpcz(MojoHandle handle,
                              MojoQuotaType type,
                              const MojoQueryQuotaOptions* options,
                              uint64_t* current_limit,
                              uint64_t* current_usage) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoShutdownIpcz(const MojoShutdownOptions* options) {
  NOTREACHED();
  return MOJO_RESULT_OK;
}

MojoResult MojoSetDefaultProcessErrorHandlerIpcz(
    MojoDefaultProcessErrorHandler handler,
    const MojoSetDefaultProcessErrorHandlerOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

}  // extern "C"

MojoSystemThunks2 g_mojo_ipcz_thunks = {
    sizeof(MojoSystemThunks2),
    MojoInitializeIpcz,
    MojoGetTimeTicksNowIpcz,
    MojoCloseIpcz,
    MojoQueryHandleSignalsStateIpcz,
    MojoCreateMessagePipeIpcz,
    MojoWriteMessageIpcz,
    MojoReadMessageIpcz,
    MojoFuseMessagePipesIpcz,
    MojoCreateMessageIpcz,
    MojoDestroyMessageIpcz,
    MojoSerializeMessageIpcz,
    MojoAppendMessageDataIpcz,
    MojoGetMessageDataIpcz,
    MojoSetMessageContextIpcz,
    MojoGetMessageContextIpcz,
    MojoNotifyBadMessageIpcz,
    MojoCreateDataPipeIpcz,
    MojoWriteDataIpcz,
    MojoBeginWriteDataIpcz,
    MojoEndWriteDataIpcz,
    MojoReadDataIpcz,
    MojoBeginReadDataIpcz,
    MojoEndReadDataIpcz,
    MojoCreateSharedBufferIpcz,
    MojoDuplicateBufferHandleIpcz,
    MojoMapBufferIpcz,
    MojoUnmapBufferIpcz,
    MojoGetBufferInfoIpcz,
    MojoCreateTrapIpcz,
    MojoAddTriggerIpcz,
    MojoRemoveTriggerIpcz,
    MojoArmTrapIpcz,
    MojoWrapPlatformHandleIpcz,
    MojoUnwrapPlatformHandleIpcz,
    MojoWrapPlatformSharedMemoryRegionIpcz,
    MojoUnwrapPlatformSharedMemoryRegionIpcz,
    MojoCreateInvitationIpcz,
    MojoAttachMessagePipeToInvitationIpcz,
    MojoExtractMessagePipeFromInvitationIpcz,
    MojoSendInvitationIpcz,
    MojoAcceptInvitationIpcz,
    MojoSetQuotaIpcz,
    MojoQueryQuotaIpcz,
    MojoShutdownIpcz,
    MojoSetDefaultProcessErrorHandlerIpcz};

}  // namespace

const MojoSystemThunks2* GetMojoIpczImpl() {
  return &g_mojo_ipcz_thunks;
}

}  // namespace mojo::core
