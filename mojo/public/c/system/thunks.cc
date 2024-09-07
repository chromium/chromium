// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/c/system/thunks.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/message_pipe.h"

namespace {

typedef void (*MojoGetSystemThunksFunction)(MojoSystemThunks2* thunks);

MojoSystemThunks2 g_thunks;

MojoResult NotImplemented(const char* name) {
  if (g_thunks.size > 0) {
    DLOG(ERROR) << "Function 'Mojo" << name
                << "()' not supported in this version of Mojo Core.";
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  LOG(FATAL) << "Mojo has not been initialized in this process. You must call "
             << "either mojo::core::Init() as an embedder.";
}

}  // namespace

#define INVOKE_THUNK(name, ...)                     \
  offsetof(MojoSystemThunks2, name) < g_thunks.size \
      ? g_thunks.name(__VA_ARGS__)                  \
      : NotImplemented(#name)

extern "C" {

MojoResult MojoInitialize(const struct MojoInitializeOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoTimeTicks MojoGetTimeTicksNow() {
  return INVOKE_THUNK(GetTimeTicksNow);
}

MojoResult MojoClose(MojoHandle handle) {
  return INVOKE_THUNK(Close, handle);
}

MojoResult MojoQueryHandleSignalsState(
    MojoHandle handle,
    struct MojoHandleSignalsState* signals_state) {
  return INVOKE_THUNK(QueryHandleSignalsState, handle, signals_state);
}

MojoResult MojoCreateMessagePipe(const MojoCreateMessagePipeOptions* options,
                                 MojoHandle* message_pipe_handle0,
                                 MojoHandle* message_pipe_handle1) {
  return INVOKE_THUNK(CreateMessagePipe, options, message_pipe_handle0,
                      message_pipe_handle1);
}

MojoResult MojoWriteMessage(MojoHandle message_pipe_handle,
                            MojoMessageHandle message_handle,
                            const MojoWriteMessageOptions* options) {
  return INVOKE_THUNK(WriteMessage, message_pipe_handle, message_handle,
                      options);
}

MojoResult MojoReadMessage(MojoHandle message_pipe_handle,
                           const MojoReadMessageOptions* options,
                           MojoMessageHandle* message_handle) {
  return INVOKE_THUNK(ReadMessage, message_pipe_handle, options,
                      message_handle);
}

MojoResult MojoFuseMessagePipes(MojoHandle handle0,
                                MojoHandle handle1,
                                const MojoFuseMessagePipesOptions* options) {
  return INVOKE_THUNK(FuseMessagePipes, handle0, handle1, options);
}

MojoResult MojoCreateDataPipe(const MojoCreateDataPipeOptions* options,
                              MojoHandle* data_pipe_producer_handle,
                              MojoHandle* data_pipe_consumer_handle) {
  return INVOKE_THUNK(CreateDataPipe, options, data_pipe_producer_handle,
                      data_pipe_consumer_handle);
}

MojoResult MojoWriteData(MojoHandle data_pipe_producer_handle,
                         const void* elements,
                         uint32_t* num_elements,
                         const MojoWriteDataOptions* options) {
  return INVOKE_THUNK(WriteData, data_pipe_producer_handle, elements,
                      num_elements, options);
}

MojoResult MojoBeginWriteData(MojoHandle data_pipe_producer_handle,
                              const MojoBeginWriteDataOptions* options,
                              void** buffer,
                              uint32_t* buffer_num_elements) {
  return INVOKE_THUNK(BeginWriteData, data_pipe_producer_handle, options,
                      buffer, buffer_num_elements);
}

MojoResult MojoEndWriteData(MojoHandle data_pipe_producer_handle,
                            uint32_t num_elements_written,
                            const MojoEndWriteDataOptions* options) {
  return INVOKE_THUNK(EndWriteData, data_pipe_producer_handle,
                      num_elements_written, options);
}

MojoResult MojoReadData(MojoHandle data_pipe_consumer_handle,
                        const MojoReadDataOptions* options,
                        void* elements,
                        uint32_t* num_elements) {
  return INVOKE_THUNK(ReadData, data_pipe_consumer_handle, options, elements,
                      num_elements);
}

MojoResult MojoBeginReadData(MojoHandle data_pipe_consumer_handle,
                             const MojoBeginReadDataOptions* options,
                             const void** buffer,
                             uint32_t* buffer_num_elements) {
  return INVOKE_THUNK(BeginReadData, data_pipe_consumer_handle, options, buffer,
                      buffer_num_elements);
}

MojoResult MojoEndReadData(MojoHandle data_pipe_consumer_handle,
                           uint32_t num_elements_read,
                           const MojoEndReadDataOptions* options) {
  return INVOKE_THUNK(EndReadData, data_pipe_consumer_handle, num_elements_read,
                      options);
}

MojoResult MojoCreateSharedBuffer(uint64_t num_bytes,
                                  const MojoCreateSharedBufferOptions* options,
                                  MojoHandle* shared_buffer_handle) {
  return INVOKE_THUNK(CreateSharedBuffer, num_bytes, options,
                      shared_buffer_handle);
}

MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  return INVOKE_THUNK(DuplicateBufferHandle, buffer_handle, options,
                      new_buffer_handle);
}

MojoResult MojoMapBuffer(MojoHandle buffer_handle,
                         uint64_t offset,
                         uint64_t num_bytes,
                         const MojoMapBufferOptions* options,
                         void** buffer) {
  return INVOKE_THUNK(MapBuffer, buffer_handle, offset, num_bytes, options,
                      buffer);
}

MojoResult MojoUnmapBuffer(void* buffer) {
  return INVOKE_THUNK(UnmapBuffer, buffer);
}

MojoResult MojoGetBufferInfo(MojoHandle buffer_handle,
                             const MojoGetBufferInfoOptions* options,
                             MojoSharedBufferInfo* info) {
  return INVOKE_THUNK(GetBufferInfo, buffer_handle, options, info);
}

MojoResult MojoCreateTrap(MojoTrapEventHandler handler,
                          const MojoCreateTrapOptions* options,
                          MojoHandle* trap_handle) {
  return INVOKE_THUNK(CreateTrap, handler, options, trap_handle);
}

MojoResult MojoAddTrigger(MojoHandle trap_handle,
                          MojoHandle handle,
                          MojoHandleSignals signals,
                          MojoTriggerCondition condition,
                          uintptr_t context,
                          const MojoAddTriggerOptions* options) {
  return INVOKE_THUNK(AddTrigger, trap_handle, handle, signals, condition,
                      context, options);
}

MojoResult MojoRemoveTrigger(MojoHandle trap_handle,
                             uintptr_t context,
                             const MojoRemoveTriggerOptions* options) {
  return INVOKE_THUNK(RemoveTrigger, trap_handle, context, options);
}

MojoResult MojoArmTrap(MojoHandle trap_handle,
                       const MojoArmTrapOptions* options,
                       uint32_t* num_blocking_events,
                       MojoTrapEvent* blocking_events) {
  return INVOKE_THUNK(ArmTrap, trap_handle, options, num_blocking_events,
                      blocking_events);
}

MojoResult MojoCreateMessage(const MojoCreateMessageOptions* options,
                             MojoMessageHandle* message) {
  return INVOKE_THUNK(CreateMessage, options, message);
}

MojoResult MojoDestroyMessage(MojoMessageHandle message) {
  return INVOKE_THUNK(DestroyMessage, message);
}

MojoResult MojoSerializeMessage(MojoMessageHandle message,
                                const MojoSerializeMessageOptions* options) {
  return INVOKE_THUNK(SerializeMessage, message, options);
}

MojoResult MojoReserveMessageCapacity(MojoMessageHandle message,
                                      uint32_t payload_buffer_size,
                                      uint32_t* buffer_size) {
  return INVOKE_THUNK(ReserveMessageCapacity, message, payload_buffer_size,
                      buffer_size);
}

MojoResult MojoAppendMessageData(MojoMessageHandle message,
                                 uint32_t payload_size,
                                 const MojoHandle* handles,
                                 uint32_t num_handles,
                                 const MojoAppendMessageDataOptions* options,
                                 void** buffer,
                                 uint32_t* buffer_size) {
  return INVOKE_THUNK(AppendMessageData, message, payload_size, handles,
                      num_handles, options, buffer, buffer_size);
}

MojoResult MojoGetMessageData(MojoMessageHandle message,
                              const MojoGetMessageDataOptions* options,
                              void** buffer,
                              uint32_t* num_bytes,
                              MojoHandle* handles,
                              uint32_t* num_handles) {
  return INVOKE_THUNK(GetMessageData, message, options, buffer, num_bytes,
                      handles, num_handles);
}

MojoResult MojoSetMessageContext(MojoMessageHandle message,
                                 uintptr_t context,
                                 MojoMessageContextSerializer serializer,
                                 MojoMessageContextDestructor destructor,
                                 const MojoSetMessageContextOptions* options) {
  return INVOKE_THUNK(SetMessageContext, message, context, serializer,
                      destructor, options);
}

MojoResult MojoGetMessageContext(MojoMessageHandle message,
                                 const MojoGetMessageContextOptions* options,
                                 uintptr_t* context) {
  return INVOKE_THUNK(GetMessageContext, message, options, context);
}

MojoResult MojoNotifyBadMessage(MojoMessageHandle message,
                                const char* error,
                                uint32_t error_num_bytes,
                                const MojoNotifyBadMessageOptions* options) {
  return INVOKE_THUNK(NotifyBadMessage, message, error, error_num_bytes,
                      options);
}

MojoResult MojoWrapPlatformHandle(const MojoPlatformHandle* platform_handle,
                                  const MojoWrapPlatformHandleOptions* options,
                                  MojoHandle* mojo_handle) {
  return INVOKE_THUNK(WrapPlatformHandle, platform_handle, options,
                      mojo_handle);
}

MojoResult MojoUnwrapPlatformHandle(
    MojoHandle mojo_handle,
    const MojoUnwrapPlatformHandleOptions* options,
    MojoPlatformHandle* platform_handle) {
  return INVOKE_THUNK(UnwrapPlatformHandle, mojo_handle, options,
                      platform_handle);
}

MojoResult MojoWrapPlatformSharedMemoryRegion(
    const struct MojoPlatformHandle* platform_handles,
    uint32_t num_platform_handles,
    uint64_t num_bytes,
    const MojoSharedBufferGuid* guid,
    MojoPlatformSharedMemoryRegionAccessMode access_mode,
    const MojoWrapPlatformSharedMemoryRegionOptions* options,
    MojoHandle* mojo_handle) {
  return INVOKE_THUNK(WrapPlatformSharedMemoryRegion, platform_handles,
                      num_platform_handles, num_bytes, guid, access_mode,
                      options, mojo_handle);
}

MojoResult MojoUnwrapPlatformSharedMemoryRegion(
    MojoHandle mojo_handle,
    const MojoUnwrapPlatformSharedMemoryRegionOptions* options,
    struct MojoPlatformHandle* platform_handles,
    uint32_t* num_platform_handles,
    uint64_t* num_bytes,
    struct MojoSharedBufferGuid* guid,
    MojoPlatformSharedMemoryRegionAccessMode* access_mode) {
  return INVOKE_THUNK(UnwrapPlatformSharedMemoryRegion, mojo_handle, options,
                      platform_handles, num_platform_handles, num_bytes, guid,
                      access_mode);
}

MojoResult MojoCreateInvitation(const MojoCreateInvitationOptions* options,
                                MojoHandle* invitation_handle) {
  return INVOKE_THUNK(CreateInvitation, options, invitation_handle);
}

MojoResult MojoAttachMessagePipeToInvitation(
    MojoHandle invitation_handle,
    const void* name,
    uint32_t name_num_bytes,
    const MojoAttachMessagePipeToInvitationOptions* options,
    MojoHandle* message_pipe_handle) {
  return INVOKE_THUNK(AttachMessagePipeToInvitation, invitation_handle, name,
                      name_num_bytes, options, message_pipe_handle);
}

MojoResult MojoExtractMessagePipeFromInvitation(
    MojoHandle invitation_handle,
    const void* name,
    uint32_t name_num_bytes,
    const MojoExtractMessagePipeFromInvitationOptions* options,
    MojoHandle* message_pipe_handle) {
  return INVOKE_THUNK(ExtractMessagePipeFromInvitation, invitation_handle, name,
                      name_num_bytes, options, message_pipe_handle);
}

MojoResult MojoSendInvitation(
    MojoHandle invitation_handle,
    const MojoPlatformProcessHandle* process_handle,
    const MojoInvitationTransportEndpoint* transport_endpoint,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    const MojoSendInvitationOptions* options) {
  return INVOKE_THUNK(SendInvitation, invitation_handle, process_handle,
                      transport_endpoint, error_handler, error_handler_context,
                      options);
}

MojoResult MojoAcceptInvitation(
    const MojoInvitationTransportEndpoint* transport_endpoint,
    const MojoAcceptInvitationOptions* options,
    MojoHandle* invitation_handle) {
  return INVOKE_THUNK(AcceptInvitation, transport_endpoint, options,
                      invitation_handle);
}

MojoResult MojoSetQuota(MojoHandle handle,
                        MojoQuotaType type,
                        uint64_t limit,
                        const MojoSetQuotaOptions* options) {
  return INVOKE_THUNK(SetQuota, handle, type, limit, options);
}

MojoResult MojoQueryQuota(MojoHandle handle,
                          MojoQuotaType type,
                          const MojoQueryQuotaOptions* options,
                          uint64_t* limit,
                          uint64_t* usage) {
  return INVOKE_THUNK(QueryQuota, handle, type, options, limit, usage);
}

MojoResult MojoShutdown(const MojoShutdownOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoSetDefaultProcessErrorHandler(
    MojoDefaultProcessErrorHandler handler,
    const struct MojoSetDefaultProcessErrorHandlerOptions* options) {
  return INVOKE_THUNK(SetDefaultProcessErrorHandler, handler, options);
}

MojoResult MojoClose32(MojoHandle32 handle) {
  return MojoClose(handle);
}

MojoResult MojoQueryHandleSignalsState32(
    MojoHandle32 handle,
    struct MojoHandleSignalsState* signals_state) {
  return MojoQueryHandleSignalsState(handle, signals_state);
}

MojoResult MojoCreateMessagePipe32(const MojoCreateMessagePipeOptions* options,
                                   MojoHandle32* message_pipe_handle0,
                                   MojoHandle32* message_pipe_handle1) {
  MojoHandle handle0, handle1;
  MojoResult result = MojoCreateMessagePipe(options, &handle0, &handle1);
  *message_pipe_handle0 = static_cast<MojoHandle32>(handle0);
  *message_pipe_handle1 = static_cast<MojoHandle32>(handle1);
  return result;
}

MojoResult MojoWriteMessage32(MojoHandle32 message_pipe_handle,
                              MojoMessageHandle message_handle,
                              const MojoWriteMessageOptions* options) {
  return MojoWriteMessage(message_pipe_handle, message_handle, options);
}

MojoResult MojoReadMessage32(MojoHandle32 message_pipe_handle,
                             const MojoReadMessageOptions* options,
                             MojoMessageHandle* message_handle) {
  return MojoReadMessage(message_pipe_handle, options, message_handle);
}

MojoResult MojoFuseMessagePipes32(MojoHandle32 handle0,
                                  MojoHandle32 handle1,
                                  const MojoFuseMessagePipesOptions* options) {
  return MojoFuseMessagePipes(handle0, handle1, options);
}

MojoResult MojoCreateDataPipe32(const MojoCreateDataPipeOptions* options,
                                MojoHandle32* data_pipe_producer_handle,
                                MojoHandle32* data_pipe_consumer_handle) {
  MojoHandle producer, consumer;
  MojoResult result = MojoCreateDataPipe(options, &producer, &consumer);
  *data_pipe_producer_handle = static_cast<MojoHandle32>(producer);
  *data_pipe_consumer_handle = static_cast<MojoHandle32>(consumer);
  return result;
}

MojoResult MojoWriteData32(MojoHandle32 data_pipe_producer_handle,
                           const void* elements,
                           uint32_t* num_elements,
                           const MojoWriteDataOptions* options) {
  return MojoWriteData(data_pipe_producer_handle, elements, num_elements,
                       options);
}

MojoResult MojoBeginWriteData32(MojoHandle32 data_pipe_producer_handle,
                                const MojoBeginWriteDataOptions* options,
                                void** buffer,
                                uint32_t* buffer_num_elements) {
  return MojoBeginWriteData(data_pipe_producer_handle, options, buffer,
                            buffer_num_elements);
}

MojoResult MojoEndWriteData32(MojoHandle32 data_pipe_producer_handle,
                              uint32_t num_elements_written,
                              const MojoEndWriteDataOptions* options) {
  return MojoEndWriteData(data_pipe_producer_handle, num_elements_written,
                          options);
}

MojoResult MojoReadData32(MojoHandle32 data_pipe_consumer_handle,
                          const MojoReadDataOptions* options,
                          void* elements,
                          uint32_t* num_elements) {
  return MojoReadData(data_pipe_consumer_handle, options, elements,
                      num_elements);
}

MojoResult MojoBeginReadData32(MojoHandle32 data_pipe_consumer_handle,
                               const MojoBeginReadDataOptions* options,
                               const void** buffer,
                               uint32_t* buffer_num_elements) {
  return MojoBeginReadData(data_pipe_consumer_handle, options, buffer,
                           buffer_num_elements);
}

MojoResult MojoEndReadData32(MojoHandle32 data_pipe_consumer_handle,
                             uint32_t num_elements_read,
                             const MojoEndReadDataOptions* options) {
  return MojoEndReadData(data_pipe_consumer_handle, num_elements_read, options);
}

MojoResult MojoCreateSharedBuffer32(
    uint64_t num_bytes,
    const MojoCreateSharedBufferOptions* options,
    MojoHandle32* shared_buffer_handle) {
  MojoHandle handle;
  MojoResult result = MojoCreateSharedBuffer(num_bytes, options, &handle);
  *shared_buffer_handle = static_cast<MojoHandle32>(handle);
  return result;
}

MojoResult MojoDuplicateBufferHandle32(
    MojoHandle32 buffer_handle,
    const MojoDuplicateBufferHandleOptions* options,
    MojoHandle32* new_buffer_handle) {
  MojoHandle new_handle;
  MojoResult result =
      MojoDuplicateBufferHandle(buffer_handle, options, &new_handle);
  *new_buffer_handle = static_cast<MojoHandle32>(new_handle);
  return result;
}

MojoResult MojoMapBuffer32(MojoHandle32 buffer_handle,
                           uint64_t offset,
                           uint64_t num_bytes,
                           const MojoMapBufferOptions* options,
                           void** buffer) {
  return MojoMapBuffer(buffer_handle, offset, num_bytes, options, buffer);
}

MojoResult MojoGetBufferInfo32(MojoHandle32 buffer_handle,
                               const MojoGetBufferInfoOptions* options,
                               MojoSharedBufferInfo* info) {
  return MojoGetBufferInfo(buffer_handle, options, info);
}

MojoResult MojoCreateTrap32(MojoTrapEventHandler handler,
                            const MojoCreateTrapOptions* options,
                            MojoHandle32* trap_handle) {
  MojoHandle handle;
  MojoResult result = MojoCreateTrap(handler, options, &handle);
  *trap_handle = static_cast<MojoHandle32>(handle);
  return result;
}

MojoResult MojoAddTrigger32(MojoHandle32 trap_handle,
                            MojoHandle32 handle,
                            MojoHandleSignals signals,
                            MojoTriggerCondition condition,
                            uintptr_t context,
                            const MojoAddTriggerOptions* options) {
  return MojoAddTrigger(trap_handle, handle, signals, condition, context,
                        options);
}

MojoResult MojoRemoveTrigger32(MojoHandle32 trap_handle,
                               uintptr_t context,
                               const MojoRemoveTriggerOptions* options) {
  return MojoRemoveTrigger(trap_handle, context, options);
}

MojoResult MojoArmTrap32(MojoHandle32 trap_handle,
                         const MojoArmTrapOptions* options,
                         uint32_t* num_blocking_events,
                         MojoTrapEvent* blocking_events) {
  return MojoArmTrap(trap_handle, options, num_blocking_events,
                     blocking_events);
}

MojoResult MojoAppendMessageData32(MojoMessageHandle message,
                                   uint32_t payload_size,
                                   const MojoHandle32* handles,
                                   uint32_t num_handles,
                                   const MojoAppendMessageDataOptions* options,
                                   void** buffer,
                                   uint32_t* buffer_size) {
  std::vector<MojoHandle> handles64(num_handles);
  for (size_t i = 0; i < num_handles; ++i) {
    handles64[i] = handles[i];
  }
  return MojoAppendMessageData(message, payload_size, handles64.data(),
                               num_handles, options, buffer, buffer_size);
}

MojoResult MojoGetMessageData32(MojoMessageHandle message,
                                const MojoGetMessageDataOptions* options,
                                void** buffer,
                                uint32_t* num_bytes,
                                MojoHandle32* handles,
                                uint32_t* num_handles) {
  std::vector<MojoHandle> handles64(num_handles ? *num_handles : 0);
  MojoResult result = MojoGetMessageData(message, options, buffer, num_bytes,
                                         handles64.data(), num_handles);
  if (result == MOJO_RESULT_OK && num_handles) {
    for (size_t i = 0; i < *num_handles; ++i) {
      handles[i] = static_cast<MojoHandle32>(handles64[i]);
    }
  }
  return result;
}

MojoResult MojoWrapPlatformHandle32(
    const MojoPlatformHandle* platform_handle,
    const MojoWrapPlatformHandleOptions* options,
    MojoHandle32* mojo_handle) {
  MojoHandle handle;
  MojoResult result = MojoWrapPlatformHandle(platform_handle, options, &handle);
  *mojo_handle = static_cast<MojoHandle32>(handle);
  return result;
}

MojoResult MojoUnwrapPlatformHandle32(
    MojoHandle32 mojo_handle,
    const MojoUnwrapPlatformHandleOptions* options,
    MojoPlatformHandle* platform_handle) {
  return MojoUnwrapPlatformHandle(mojo_handle, options, platform_handle);
}

MojoResult MojoWrapPlatformSharedMemoryRegion32(
    const struct MojoPlatformHandle* platform_handles,
    uint32_t num_platform_handles,
    uint64_t num_bytes,
    const MojoSharedBufferGuid* guid,
    MojoPlatformSharedMemoryRegionAccessMode access_mode,
    const MojoWrapPlatformSharedMemoryRegionOptions* options,
    MojoHandle32* mojo_handle) {
  MojoHandle handle;
  MojoResult result = MojoWrapPlatformSharedMemoryRegion(
      platform_handles, num_platform_handles, num_bytes, guid, access_mode,
      options, &handle);
  *mojo_handle = static_cast<MojoHandle32>(handle);
  return result;
}

MojoResult MojoUnwrapPlatformSharedMemoryRegion32(
    MojoHandle32 mojo_handle,
    const MojoUnwrapPlatformSharedMemoryRegionOptions* options,
    struct MojoPlatformHandle* platform_handles,
    uint32_t* num_platform_handles,
    uint64_t* num_bytes,
    struct MojoSharedBufferGuid* guid,
    MojoPlatformSharedMemoryRegionAccessMode* access_mode) {
  return MojoUnwrapPlatformSharedMemoryRegion(
      mojo_handle, options, platform_handles, num_platform_handles, num_bytes,
      guid, access_mode);
}

MojoResult MojoCreateInvitation32(const MojoCreateInvitationOptions* options,
                                  MojoHandle32* invitation_handle) {
  MojoHandle handle;
  MojoResult result = MojoCreateInvitation(options, &handle);
  *invitation_handle = static_cast<MojoHandle32>(handle);
  return result;
}

MojoResult MojoAttachMessagePipeToInvitation32(
    MojoHandle32 invitation_handle,
    const void* name,
    uint32_t name_num_bytes,
    const MojoAttachMessagePipeToInvitationOptions* options,
    MojoHandle32* message_pipe_handle) {
  MojoHandle handle;
  MojoResult result = MojoAttachMessagePipeToInvitation(
      invitation_handle, name, name_num_bytes, options, &handle);
  *message_pipe_handle = static_cast<MojoHandle32>(handle);
  return result;
}

MojoResult MojoExtractMessagePipeFromInvitation32(
    MojoHandle32 invitation_handle,
    const void* name,
    uint32_t name_num_bytes,
    const MojoExtractMessagePipeFromInvitationOptions* options,
    MojoHandle32* message_pipe_handle) {
  MojoHandle handle;
  MojoResult result = MojoExtractMessagePipeFromInvitation(
      invitation_handle, name, name_num_bytes, options, &handle);
  *message_pipe_handle = static_cast<MojoHandle32>(handle);
  return result;
}

MojoResult MojoSendInvitation32(
    MojoHandle32 invitation_handle,
    const MojoPlatformProcessHandle* process_handle,
    const MojoInvitationTransportEndpoint* transport_endpoint,
    MojoProcessErrorHandler error_handler,
    uintptr_t error_handler_context,
    const MojoSendInvitationOptions* options) {
  return MojoSendInvitation(invitation_handle, process_handle,
                            transport_endpoint, error_handler,
                            error_handler_context, options);
}

MojoResult MojoAcceptInvitation32(
    const MojoInvitationTransportEndpoint* transport_endpoint,
    const MojoAcceptInvitationOptions* options,
    MojoHandle32* invitation_handle) {
  MojoHandle handle;
  MojoResult result =
      MojoAcceptInvitation(transport_endpoint, options, &handle);
  *invitation_handle = static_cast<MojoHandle32>(handle);
  return result;
}

MojoResult MojoSetQuota32(MojoHandle32 handle,
                          MojoQuotaType type,
                          uint64_t limit,
                          const MojoSetQuotaOptions* options) {
  return MojoSetQuota(handle, type, limit, options);
}

MojoResult MojoQueryQuota32(MojoHandle32 handle,
                            MojoQuotaType type,
                            const MojoQueryQuotaOptions* options,
                            uint64_t* limit,
                            uint64_t* usage) {
  return MojoQueryQuota(handle, type, options, limit, usage);
}

MojoSystemThunks32 g_thunks_32 = {
    sizeof(g_thunks_32),
    MojoInitialize,
    MojoGetTimeTicksNow,
    MojoClose32,
    MojoQueryHandleSignalsState32,
    MojoCreateMessagePipe32,
    MojoWriteMessage32,
    MojoReadMessage32,
    MojoFuseMessagePipes32,
    MojoCreateMessage,
    MojoDestroyMessage,
    MojoSerializeMessage,
    MojoAppendMessageData32,
    MojoGetMessageData32,
    MojoSetMessageContext,
    MojoGetMessageContext,
    MojoNotifyBadMessage,
    MojoCreateDataPipe32,
    MojoWriteData32,
    MojoBeginWriteData32,
    MojoEndWriteData32,
    MojoReadData32,
    MojoBeginReadData32,
    MojoEndReadData32,
    MojoCreateSharedBuffer32,
    MojoDuplicateBufferHandle32,
    MojoMapBuffer32,
    MojoUnmapBuffer,
    MojoGetBufferInfo32,
    MojoCreateTrap32,
    MojoAddTrigger32,
    MojoRemoveTrigger32,
    MojoArmTrap32,
    MojoWrapPlatformHandle32,
    MojoUnwrapPlatformHandle32,
    MojoWrapPlatformSharedMemoryRegion32,
    MojoUnwrapPlatformSharedMemoryRegion32,
    MojoCreateInvitation32,
    MojoAttachMessagePipeToInvitation32,
    MojoExtractMessagePipeFromInvitation32,
    MojoSendInvitation32,
    MojoAcceptInvitation32,
    MojoSetQuota32,
    MojoQueryQuota32,
    MojoShutdown,
    MojoSetDefaultProcessErrorHandler,
    MojoReserveMessageCapacity,
};

const MojoSystemThunks2* MojoEmbedderGetSystemThunks2() {
  return &g_thunks;
}

const MojoSystemThunks32* MojoEmbedderGetSystemThunks32() {
  return &g_thunks_32;
}

void MojoEmbedderSetSystemThunks(const MojoSystemThunks2* thunks) {
  // Assume embedders will always use matching versions of the Mojo Core and
  // public APIs.
  DCHECK_EQ(thunks->size, sizeof(g_thunks));

  // This should only have to check that the |g_thunks->size| is zero, but we
  // have multiple Mojo Core initializations in some test suites still. For now
  // we allow double calls as long as they're the same thunks as before.
  DCHECK(g_thunks.size == 0 || !memcmp(&g_thunks, thunks, sizeof(g_thunks)))
      << "Cannot set embedder thunks after Mojo API calls have been made.";

  g_thunks = *thunks;
}

}  // extern "C"
