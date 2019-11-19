// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/thunks.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "mojo/public/c/system/core.h"

#if defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_WIN)
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/scoped_native_library.h"
#include "base/threading/thread_restrictions.h"
#endif

namespace {

typedef void (*MojoGetSystemThunksFunction)(MojoSystemThunks* thunks);

MojoSystemThunks g_thunks;

MojoResult NotImplemented(const char* name) {
  if (g_thunks.size > 0) {
    DLOG(ERROR) << "Function 'Mojo" << name
                << "()' not supported in this version of Mojo Core.";
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  LOG(FATAL)
      << "Mojo has not been initialized in this process. You must call "
      << "either mojo::core::Init() as an embedder, or |MojoInitialize()| if "
      << "using the mojo_core shared library.";
  return MOJO_RESULT_UNIMPLEMENTED;
}

}  // namespace

#define INVOKE_THUNK(name, ...)                    \
  offsetof(MojoSystemThunks, name) < g_thunks.size \
      ? g_thunks.name(__VA_ARGS__)                 \
      : NotImplemented(#name)

namespace mojo {

// NOTE: This is defined within the global mojo namespace so that it can be
// referenced as a friend to base::ScopedAllowBlocking when library support is
// enabled.
class CoreLibraryInitializer {
 public:
  CoreLibraryInitializer(const MojoInitializeOptions* options) {
#if defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_WIN)
    bool application_provided_path = false;
    base::Optional<base::FilePath> library_path;
    if (options && options->struct_size >= sizeof(*options) &&
        options->mojo_core_path) {
      base::StringPiece utf8_path(options->mojo_core_path,
                                  options->mojo_core_path_length);
      library_path.emplace(base::FilePath::FromUTF8Unsafe(utf8_path));
      application_provided_path = true;
    } else {
      auto environment = base::Environment::Create();
      std::string library_path_value;
      const char kLibraryPathEnvironmentVar[] = "MOJO_CORE_LIBRARY_PATH";
      if (environment->GetVar(kLibraryPathEnvironmentVar, &library_path_value))
        library_path = base::FilePath::FromUTF8Unsafe(library_path_value);
    }

    if (!library_path) {
      // Default to looking for the library in the current working directory.
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
      const base::FilePath::CharType kDefaultLibraryPathValue[] =
          FILE_PATH_LITERAL("./libmojo_core.so");
#elif defined(OS_WIN)
      const base::FilePath::CharType kDefaultLibraryPathValue[] =
          FILE_PATH_LITERAL("mojo_core.dll");
#endif
      library_path.emplace(kDefaultLibraryPathValue);
    }

    // NOTE: |prefer_own_symbols| on POSIX implies that the library is loaded
    // with RTLD_DEEPBIND, which is critical given that libmojo_core.so links
    // against base's allocator shim. Essentially, this ensures that mojo_core
    // internals get their own heap, and this is OK since heap pointer ownership
    // is never passed across the ABI boundary.
    base::ScopedAllowBlocking allow_blocking;
    base::NativeLibraryOptions library_options;
#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER) && !defined(LEAK_SANITIZER)
    // Sanitizer builds cannnot support RTLD_DEEPBIND, but they also disable
    // allocator shims, so it's unnecessary there.
    library_options.prefer_own_symbols = true;
#endif
    library_.emplace(base::LoadNativeLibraryWithOptions(
        *library_path, library_options, nullptr));
    if (!application_provided_path) {
      CHECK(library_->is_valid())
          << "Unable to load the mojo_core library. Make sure the library is "
          << "in the working directory or is correctly pointed to by the "
          << "MOJO_CORE_LIBRARY_PATH environment variable.";
    } else {
      CHECK(library_->is_valid())
          << "Unable to locate mojo_core library. This application expects to "
          << "find it at " << library_path->value();
    }

    const char kGetThunksFunctionName[] = "MojoGetSystemThunks";

    MojoGetSystemThunksFunction g_get_thunks =
        reinterpret_cast<MojoGetSystemThunksFunction>(
            library_->GetFunctionPointer(kGetThunksFunctionName));
    CHECK(g_get_thunks) << "Invalid mojo_core library: "
                        << library_path->value();

    DCHECK_EQ(g_thunks.size, 0u);
    g_thunks.size = sizeof(g_thunks);
    g_get_thunks(&g_thunks);

    CHECK_GT(g_thunks.size, 0u)
        << "Invalid mojo_core library: " << library_path->value();
#else   // defined(OS_CHROMEOS) || defined(OS_LINUX)
    NOTREACHED()
        << "Dynamic mojo_core loading is not supported on this platform.";
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)
  }

  ~CoreLibraryInitializer() = default;

 private:
#if defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_WIN)
  base::Optional<base::ScopedNativeLibrary> library_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CoreLibraryInitializer);
};

}  // namespace mojo

extern "C" {

MojoResult MojoInitialize(const struct MojoInitializeOptions* options) {
  static base::NoDestructor<mojo::CoreLibraryInitializer> initializer(options);
  ALLOW_UNUSED_LOCAL(initializer);
  DCHECK(g_thunks.Initialize);

  return INVOKE_THUNK(Initialize, options);
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
  return INVOKE_THUNK(Shutdown, options);
}

}  // extern "C"

void MojoEmbedderSetSystemThunks(const MojoSystemThunks* thunks) {
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
