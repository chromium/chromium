// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/core_ipcz.h"

#include "base/notreached.h"
#include "base/time/time.h"
#include "mojo/core/ipcz_api.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core {

namespace {

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
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoCreateMessagePipeIpcz(
    const MojoCreateMessagePipeOptions* options,
    MojoHandle* message_pipe_handle0,
    MojoHandle* message_pipe_handle1) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoWriteMessageIpcz(MojoHandle message_pipe_handle,
                                MojoMessageHandle message,
                                const MojoWriteMessageOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoReadMessageIpcz(MojoHandle message_pipe_handle,
                               const MojoReadMessageOptions* options,
                               MojoMessageHandle* message) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoFuseMessagePipesIpcz(
    MojoHandle handle0,
    MojoHandle handle1,
    const MojoFuseMessagePipesOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoCreateMessageIpcz(const MojoCreateMessageOptions* options,
                                 MojoMessageHandle* message) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoDestroyMessageIpcz(MojoMessageHandle message) {
  return MOJO_RESULT_UNIMPLEMENTED;
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
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoGetMessageDataIpcz(MojoMessageHandle message,
                                  const MojoGetMessageDataOptions* options,
                                  void** buffer,
                                  uint32_t* num_bytes,
                                  MojoHandle* handles,
                                  uint32_t* num_handles) {
  return MOJO_RESULT_UNIMPLEMENTED;
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
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoAddTriggerIpcz(MojoHandle trap_handle,
                              MojoHandle handle,
                              MojoHandleSignals signals,
                              MojoTriggerCondition condition,
                              uintptr_t context,
                              const MojoAddTriggerOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoRemoveTriggerIpcz(MojoHandle trap_handle,
                                 uintptr_t context,
                                 const MojoRemoveTriggerOptions* options) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoArmTrapIpcz(MojoHandle trap_handle,
                           const MojoArmTrapOptions* options,
                           uint32_t* num_blocking_events,
                           MojoTrapEvent* blocking_events) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoWrapPlatformHandleIpcz(
    const MojoPlatformHandle* platform_handle,
    const MojoWrapPlatformHandleOptions* options,
    MojoHandle* mojo_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult MojoUnwrapPlatformHandleIpcz(
    MojoHandle mojo_handle,
    const MojoUnwrapPlatformHandleOptions* options,
    MojoPlatformHandle* platform_handle) {
  return MOJO_RESULT_UNIMPLEMENTED;
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
