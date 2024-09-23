// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_THUNKS_H_
#define MOJO_PUBLIC_C_SYSTEM_THUNKS_H_

#include <stddef.h>
#include <stdint.h>

#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/system_export.h"

// This defines the *stable*, foward-compatible ABI for the Mojo Core C library.
// As such, the following types of changes are DISALLOWED:
//
//   - DO NOT delete or re-order any of the fields in this structure
//   - DO NOT modify any function signatures defined here
//   - DO NOT alter the alignment of the stucture
//
// Some changes are of course permissible:
//
//   - DO feel free to rename existing fields if there's a good reason to do so,
//     e.g. deprecation of a function for all future applications.
//   - DO add new functions to the end of this structure, but ensure that they
//     have a signature which lends itself to reasonably extensible behavior
//     (e.g. an optional "Options" structure as many functions here have).
//
#pragma pack(push, 8)
struct MojoSystemThunks2 {
  uint32_t size;  // Should be set to sizeof(MojoSystemThunks).

  MojoResult (*Initialize)(const struct MojoInitializeOptions* options);

  MojoTimeTicks (*GetTimeTicksNow)();

  // Generic handle API.
  MojoResult (*Close)(MojoHandle handle);
  MojoResult (*QueryHandleSignalsState)(
      MojoHandle handle,
      struct MojoHandleSignalsState* signals_state);

  // Message pipe API.
  MojoResult (*CreateMessagePipe)(
      const struct MojoCreateMessagePipeOptions* options,
      MojoHandle* message_pipe_handle0,
      MojoHandle* message_pipe_handle1);
  MojoResult (*WriteMessage)(MojoHandle message_pipe_handle,
                             MojoMessageHandle message_handle,
                             const struct MojoWriteMessageOptions* options);
  MojoResult (*ReadMessage)(MojoHandle message_pipe_handle,
                            const struct MojoReadMessageOptions* options,
                            MojoMessageHandle* message_handle);
  MojoResult (*FuseMessagePipes)(
      MojoHandle handle0,
      MojoHandle handle1,
      const struct MojoFuseMessagePipesOptions* options);

  // Message object API.
  MojoResult (*CreateMessage)(const struct MojoCreateMessageOptions* options,
                              MojoMessageHandle* message);
  MojoResult (*DestroyMessage)(MojoMessageHandle message);
  MojoResult (*SerializeMessage)(
      MojoMessageHandle message,
      const struct MojoSerializeMessageOptions* options);
  MojoResult (*AppendMessageData)(
      MojoMessageHandle message,
      uint32_t additional_payload_size,
      const MojoHandle* handles,
      uint32_t num_handles,
      const struct MojoAppendMessageDataOptions* options,
      void** buffer,
      uint32_t* buffer_size);
  MojoResult (*GetMessageData)(MojoMessageHandle message,
                               const struct MojoGetMessageDataOptions* options,
                               void** buffer,
                               uint32_t* num_bytes,
                               MojoHandle* handles,
                               uint32_t* num_handles);
  MojoResult (*SetMessageContext)(
      MojoMessageHandle message,
      uintptr_t context,
      MojoMessageContextSerializer serializer,
      MojoMessageContextDestructor destructor,
      const struct MojoSetMessageContextOptions* options);
  MojoResult (*GetMessageContext)(
      MojoMessageHandle message,
      const struct MojoGetMessageContextOptions* options,
      uintptr_t* context);
  MojoResult (*NotifyBadMessage)(
      MojoMessageHandle message,
      const char* error,
      uint32_t error_num_bytes,
      const struct MojoNotifyBadMessageOptions* options);

  // Data pipe API.
  MojoResult (*CreateDataPipe)(const struct MojoCreateDataPipeOptions* options,
                               MojoHandle* data_pipe_producer_handle,
                               MojoHandle* data_pipe_consumer_handle);
  MojoResult (*WriteData)(MojoHandle data_pipe_producer_handle,
                          const void* elements,
                          uint32_t* num_elements,
                          const struct MojoWriteDataOptions* options);
  MojoResult (*BeginWriteData)(MojoHandle data_pipe_producer_handle,
                               const struct MojoBeginWriteDataOptions* options,
                               void** buffer,
                               uint32_t* buffer_num_elements);
  MojoResult (*EndWriteData)(MojoHandle data_pipe_producer_handle,
                             uint32_t num_elements_written,
                             const struct MojoEndWriteDataOptions* options);
  MojoResult (*ReadData)(MojoHandle data_pipe_consumer_handle,
                         const struct MojoReadDataOptions* options,
                         void* elements,
                         uint32_t* num_elements);
  MojoResult (*BeginReadData)(MojoHandle data_pipe_consumer_handle,
                              const struct MojoBeginReadDataOptions* options,
                              const void** buffer,
                              uint32_t* buffer_num_elements);
  MojoResult (*EndReadData)(MojoHandle data_pipe_consumer_handle,
                            uint32_t num_elements_read,
                            const struct MojoEndReadDataOptions* options);

  // Shared buffer API.
  MojoResult (*CreateSharedBuffer)(
      uint64_t num_bytes,
      const struct MojoCreateSharedBufferOptions* options,
      MojoHandle* shared_buffer_handle);
  MojoResult (*DuplicateBufferHandle)(
      MojoHandle buffer_handle,
      const struct MojoDuplicateBufferHandleOptions* options,
      MojoHandle* new_buffer_handle);
  MojoResult (*MapBuffer)(MojoHandle buffer_handle,
                          uint64_t offset,
                          uint64_t num_bytes,
                          const struct MojoMapBufferOptions* options,
                          void** buffer);
  MojoResult (*UnmapBuffer)(void* buffer);
  MojoResult (*GetBufferInfo)(MojoHandle buffer_handle,
                              const struct MojoGetBufferInfoOptions* options,
                              struct MojoSharedBufferInfo* info);

  // Traps API.
  MojoResult (*CreateTrap)(MojoTrapEventHandler handler,
                           const struct MojoCreateTrapOptions* options,
                           MojoHandle* trap_handle);
  MojoResult (*AddTrigger)(MojoHandle trap_handle,
                           MojoHandle handle,
                           MojoHandleSignals signals,
                           MojoTriggerCondition condition,
                           uintptr_t context,
                           const struct MojoAddTriggerOptions* options);
  MojoResult (*RemoveTrigger)(MojoHandle trap_handle,
                              uintptr_t context,
                              const struct MojoRemoveTriggerOptions* options);
  MojoResult (*ArmTrap)(MojoHandle trap_handle,
                        const struct MojoArmTrapOptions* options,
                        uint32_t* num_blocking_events,
                        struct MojoTrapEvent* blocking_events);

  // Platform handle API.
  MojoResult (*WrapPlatformHandle)(
      const struct MojoPlatformHandle* platform_handle,
      const struct MojoWrapPlatformHandleOptions* options,
      MojoHandle* mojo_handle);
  MojoResult (*UnwrapPlatformHandle)(
      MojoHandle mojo_handle,
      const struct MojoUnwrapPlatformHandleOptions* options,
      struct MojoPlatformHandle* platform_handle);
  MojoResult (*WrapPlatformSharedMemoryRegion)(
      const struct MojoPlatformHandle* platform_handles,
      uint32_t num_platform_handles,
      uint64_t num_bytes,
      const struct MojoSharedBufferGuid* guid,
      MojoPlatformSharedMemoryRegionAccessMode access_mode,
      const struct MojoWrapPlatformSharedMemoryRegionOptions* options,
      MojoHandle* mojo_handle);
  MojoResult (*UnwrapPlatformSharedMemoryRegion)(
      MojoHandle mojo_handle,
      const struct MojoUnwrapPlatformSharedMemoryRegionOptions* options,
      struct MojoPlatformHandle* platform_handles,
      uint32_t* num_platform_handles,
      uint64_t* num_bytes,
      struct MojoSharedBufferGuid* guid,
      MojoPlatformSharedMemoryRegionAccessMode* access_mode);

  // Invitation API.
  MojoResult (*CreateInvitation)(
      const struct MojoCreateInvitationOptions* options,
      MojoHandle* invitation_handle);
  MojoResult (*AttachMessagePipeToInvitation)(
      MojoHandle invitation_handle,
      const void* name,
      uint32_t name_num_bytes,
      const struct MojoAttachMessagePipeToInvitationOptions* options,
      MojoHandle* message_pipe_handle);
  MojoResult (*ExtractMessagePipeFromInvitation)(
      MojoHandle invitation_handle,
      const void* name,
      uint32_t name_num_bytes,
      const struct MojoExtractMessagePipeFromInvitationOptions* options,
      MojoHandle* message_pipe_handle);
  MojoResult (*SendInvitation)(
      MojoHandle invitation_handle,
      const struct MojoPlatformProcessHandle* process_handle,
      const struct MojoInvitationTransportEndpoint* transport_endpoint,
      MojoProcessErrorHandler error_handler,
      uintptr_t error_handler_context,
      const struct MojoSendInvitationOptions* options);
  MojoResult (*AcceptInvitation)(
      const struct MojoInvitationTransportEndpoint* transport_endpoint,
      const struct MojoAcceptInvitationOptions* options,
      MojoHandle* invitation_handle);

  // Core ABI version 1 additions begin here.

  MojoResult (*SetQuota)(MojoHandle handle,
                         MojoQuotaType type,
                         uint64_t limit,
                         const struct MojoSetQuotaOptions* options);
  MojoResult (*QueryQuota)(MojoHandle handle,
                           MojoQuotaType type,
                           const struct MojoQueryQuotaOptions* options,
                           uint64_t* limit,
                           uint64_t* usage);

  // Core ABI version 2 additions begin here.

  MojoResult (*Shutdown)(const struct MojoShutdownOptions* options);

  // Core ABI version 3 additions begin here.
  MojoResult (*SetDefaultProcessErrorHandler)(
      MojoDefaultProcessErrorHandler handler,
      const struct MojoSetDefaultProcessErrorHandlerOptions* options);

  // Core ABI version 4 additions begin here.
  MojoResult (*ReserveMessageCapacity)(MojoMessageHandle message,
                                       uint32_t payload_buffer_size,
                                       uint32_t* buffer_size);
};

// Hacks: This is a copy of the ABI from before it was switched to pointer-sized
// MojoHandle values. It can be removed once the Chrome OS IME service is
// longer consuming it.
typedef uint32_t MojoHandle32;

struct MojoSystemThunks {
  uint32_t size;  // Should be set to sizeof(MojoSystemThunks32).

  MojoResult (*Initialize)(const struct MojoInitializeOptions* options);

  MojoTimeTicks (*GetTimeTicksNow)();

  // Generic handle API.
  MojoResult (*Close)(MojoHandle32 handle);
  MojoResult (*QueryHandleSignalsState)(
      MojoHandle32 handle,
      struct MojoHandleSignalsState* signals_state);

  // Message pipe API.
  MojoResult (*CreateMessagePipe)(
      const struct MojoCreateMessagePipeOptions* options,
      MojoHandle32* message_pipe_handle0,
      MojoHandle32* message_pipe_handle1);
  MojoResult (*WriteMessage)(MojoHandle32 message_pipe_handle,
                             MojoMessageHandle message_handle,
                             const struct MojoWriteMessageOptions* options);
  MojoResult (*ReadMessage)(MojoHandle32 message_pipe_handle,
                            const struct MojoReadMessageOptions* options,
                            MojoMessageHandle* message_handle);
  MojoResult (*FuseMessagePipes)(
      MojoHandle32 handle0,
      MojoHandle32 handle1,
      const struct MojoFuseMessagePipesOptions* options);

  // Message object API.
  MojoResult (*CreateMessage)(const struct MojoCreateMessageOptions* options,
                              MojoMessageHandle* message);
  MojoResult (*DestroyMessage)(MojoMessageHandle message);
  MojoResult (*SerializeMessage)(
      MojoMessageHandle message,
      const struct MojoSerializeMessageOptions* options);
  MojoResult (*AppendMessageData)(
      MojoMessageHandle message,
      uint32_t additional_payload_size,
      const MojoHandle32* handles,
      uint32_t num_handles,
      const struct MojoAppendMessageDataOptions* options,
      void** buffer,
      uint32_t* buffer_size);
  MojoResult (*GetMessageData)(MojoMessageHandle message,
                               const struct MojoGetMessageDataOptions* options,
                               void** buffer,
                               uint32_t* num_bytes,
                               MojoHandle32* handles,
                               uint32_t* num_handles);
  MojoResult (*SetMessageContext)(
      MojoMessageHandle message,
      uintptr_t context,
      MojoMessageContextSerializer serializer,
      MojoMessageContextDestructor destructor,
      const struct MojoSetMessageContextOptions* options);
  MojoResult (*GetMessageContext)(
      MojoMessageHandle message,
      const struct MojoGetMessageContextOptions* options,
      uintptr_t* context);
  MojoResult (*NotifyBadMessage)(
      MojoMessageHandle message,
      const char* error,
      uint32_t error_num_bytes,
      const struct MojoNotifyBadMessageOptions* options);

  // Data pipe API.
  MojoResult (*CreateDataPipe)(const struct MojoCreateDataPipeOptions* options,
                               MojoHandle32* data_pipe_producer_handle,
                               MojoHandle32* data_pipe_consumer_handle);
  MojoResult (*WriteData)(MojoHandle32 data_pipe_producer_handle,
                          const void* elements,
                          uint32_t* num_elements,
                          const struct MojoWriteDataOptions* options);
  MojoResult (*BeginWriteData)(MojoHandle32 data_pipe_producer_handle,
                               const struct MojoBeginWriteDataOptions* options,
                               void** buffer,
                               uint32_t* buffer_num_elements);
  MojoResult (*EndWriteData)(MojoHandle32 data_pipe_producer_handle,
                             uint32_t num_elements_written,
                             const struct MojoEndWriteDataOptions* options);
  MojoResult (*ReadData)(MojoHandle32 data_pipe_consumer_handle,
                         const struct MojoReadDataOptions* options,
                         void* elements,
                         uint32_t* num_elements);
  MojoResult (*BeginReadData)(MojoHandle32 data_pipe_consumer_handle,
                              const struct MojoBeginReadDataOptions* options,
                              const void** buffer,
                              uint32_t* buffer_num_elements);
  MojoResult (*EndReadData)(MojoHandle32 data_pipe_consumer_handle,
                            uint32_t num_elements_read,
                            const struct MojoEndReadDataOptions* options);

  // Shared buffer API.
  MojoResult (*CreateSharedBuffer)(
      uint64_t num_bytes,
      const struct MojoCreateSharedBufferOptions* options,
      MojoHandle32* shared_buffer_handle);
  MojoResult (*DuplicateBufferHandle)(
      MojoHandle32 buffer_handle,
      const struct MojoDuplicateBufferHandleOptions* options,
      MojoHandle32* new_buffer_handle);
  MojoResult (*MapBuffer)(MojoHandle32 buffer_handle,
                          uint64_t offset,
                          uint64_t num_bytes,
                          const struct MojoMapBufferOptions* options,
                          void** buffer);
  MojoResult (*UnmapBuffer)(void* buffer);
  MojoResult (*GetBufferInfo)(MojoHandle32 buffer_handle,
                              const struct MojoGetBufferInfoOptions* options,
                              struct MojoSharedBufferInfo* info);

  // Traps API.
  MojoResult (*CreateTrap)(MojoTrapEventHandler handler,
                           const struct MojoCreateTrapOptions* options,
                           MojoHandle32* trap_handle);
  MojoResult (*AddTrigger)(MojoHandle32 trap_handle,
                           MojoHandle32 handle,
                           MojoHandleSignals signals,
                           MojoTriggerCondition condition,
                           uintptr_t context,
                           const struct MojoAddTriggerOptions* options);
  MojoResult (*RemoveTrigger)(MojoHandle32 trap_handle,
                              uintptr_t context,
                              const struct MojoRemoveTriggerOptions* options);
  MojoResult (*ArmTrap)(MojoHandle32 trap_handle,
                        const struct MojoArmTrapOptions* options,
                        uint32_t* num_blocking_events,
                        struct MojoTrapEvent* blocking_events);

  // Platform handle API.
  MojoResult (*WrapPlatformHandle)(
      const struct MojoPlatformHandle* platform_handle,
      const struct MojoWrapPlatformHandleOptions* options,
      MojoHandle32* mojo_handle);
  MojoResult (*UnwrapPlatformHandle)(
      MojoHandle32 mojo_handle,
      const struct MojoUnwrapPlatformHandleOptions* options,
      struct MojoPlatformHandle* platform_handle);
  MojoResult (*WrapPlatformSharedMemoryRegion)(
      const struct MojoPlatformHandle* platform_handles,
      uint32_t num_platform_handles,
      uint64_t num_bytes,
      const struct MojoSharedBufferGuid* guid,
      MojoPlatformSharedMemoryRegionAccessMode access_mode,
      const struct MojoWrapPlatformSharedMemoryRegionOptions* options,
      MojoHandle32* mojo_handle);
  MojoResult (*UnwrapPlatformSharedMemoryRegion)(
      MojoHandle32 mojo_handle,
      const struct MojoUnwrapPlatformSharedMemoryRegionOptions* options,
      struct MojoPlatformHandle* platform_handles,
      uint32_t* num_platform_handles,
      uint64_t* num_bytes,
      struct MojoSharedBufferGuid* guid,
      MojoPlatformSharedMemoryRegionAccessMode* access_mode);

  // Invitation API.
  MojoResult (*CreateInvitation)(
      const struct MojoCreateInvitationOptions* options,
      MojoHandle32* invitation_handle);
  MojoResult (*AttachMessagePipeToInvitation)(
      MojoHandle32 invitation_handle,
      const void* name,
      uint32_t name_num_bytes,
      const struct MojoAttachMessagePipeToInvitationOptions* options,
      MojoHandle32* message_pipe_handle);
  MojoResult (*ExtractMessagePipeFromInvitation)(
      MojoHandle32 invitation_handle,
      const void* name,
      uint32_t name_num_bytes,
      const struct MojoExtractMessagePipeFromInvitationOptions* options,
      MojoHandle32* message_pipe_handle);
  MojoResult (*SendInvitation)(
      MojoHandle32 invitation_handle,
      const struct MojoPlatformProcessHandle* process_handle,
      const struct MojoInvitationTransportEndpoint* transport_endpoint,
      MojoProcessErrorHandler error_handler,
      uintptr_t error_handler_context,
      const struct MojoSendInvitationOptions* options);
  MojoResult (*AcceptInvitation)(
      const struct MojoInvitationTransportEndpoint* transport_endpoint,
      const struct MojoAcceptInvitationOptions* options,
      MojoHandle32* invitation_handle);
  MojoResult (*SetQuota)(MojoHandle32 handle,
                         MojoQuotaType type,
                         uint64_t limit,
                         const struct MojoSetQuotaOptions* options);
  MojoResult (*QueryQuota)(MojoHandle32 handle,
                           MojoQuotaType type,
                           const struct MojoQueryQuotaOptions* options,
                           uint64_t* limit,
                           uint64_t* usage);
  MojoResult (*Shutdown)(const struct MojoShutdownOptions* options);
  MojoResult (*SetDefaultProcessErrorHandler)(
      MojoDefaultProcessErrorHandler handler,
      const struct MojoSetDefaultProcessErrorHandlerOptions* options);
  MojoResult (*ReserveMessageCapacity)(MojoMessageHandle message,
                                       uint32_t payload_buffer_size,
                                       uint32_t* buffer_size);
};
#pragma pack(pop)

typedef struct MojoSystemThunks MojoSystemThunks32;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

MOJO_SYSTEM_EXPORT const struct MojoSystemThunks2*
MojoEmbedderGetSystemThunks2();

MOJO_SYSTEM_EXPORT const MojoSystemThunks32* MojoEmbedderGetSystemThunks32();

MOJO_SYSTEM_EXPORT void MojoEmbedderSetSystemThunks(
    const struct MojoSystemThunks2* system_thunks);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // MOJO_PUBLIC_C_SYSTEM_THUNKS_H_
