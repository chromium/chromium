// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CORE_H_
#define MOJO_CORE_CORE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/dispatcher.h"
#include "mojo/core/handle_signals_state.h"
#include "mojo/core/handle_table.h"
#include "mojo/core/node_controller.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/c/system/quota.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace core {

class PlatformSharedMemoryMapping;

// |Core| is an object that implements the Mojo system calls. All public methods
// are thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT Core {
 public:
  Core();

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  virtual ~Core();

  static Core* Get();

  // Called exactly once, shortly after construction, and before any other
  // methods are called on this object.
  void SetIOTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Retrieves the NodeController for the current process.
  NodeController* GetNodeController();

  scoped_refptr<Dispatcher> GetDispatcher(MojoHandle handle);
  scoped_refptr<Dispatcher> GetAndRemoveDispatcher(MojoHandle handle);

  // Creates a message pipe endpoint with an unbound peer port returned in
  // |*peer|. Useful for setting up cross-process bootstrap message pipes. The
  // returned message pipe handle is usable immediately by the caller.
  //
  // The value returned in |*peer| may be passed along with a broker client
  // invitation. See SendBrokerClientInvitation() below.
  MojoHandle CreatePartialMessagePipe(ports::PortRef* peer);

  // Like above but exchanges an existing ports::PortRef for a message pipe
  // handle which wraps it.
  MojoHandle CreatePartialMessagePipe(const ports::PortRef& port);

  // Sends a broker client invitation to |target_process| over the connection
  // medium in |connection_params|. The other end of the connection medium in
  // |connection_params| can be used within the target process to call
  // AcceptBrokerClientInvitation() and complete the process's admission into
  // this process graph.
  //
  // |attached_ports| is a list of named port references to be attached to the
  // invitation. An attached port can be claimed (as a message pipe handle) by
  // the invitee.
  void SendBrokerClientInvitation(
      base::Process target_process,
      ConnectionParams connection_params,
      const std::vector<std::pair<std::string, ports::PortRef>>& attached_ports,
      const ProcessErrorCallback& process_error_callback);

  // Extracts a named message pipe endpoint from the broker client invitation
  // accepted by this process. Must only be called after
  // AcceptBrokerClientInvitation.
  MojoHandle ExtractMessagePipeFromInvitation(const std::string& name);

  // Called to connect to a peer process. This should be called only if there
  // is no common ancestor for the processes involved within this mojo system.
  // Both processes must call this function, each passing one end of a platform
  // channel. |port| is a port to be merged with the remote peer's port, which
  // it will provide via the same API.
  //
  // |connection_name| if non-empty guarantees that no other isolated
  // connections exist in the calling process using the same name. This is
  // useful for invitation endpoints that use a named server accepting multiple
  // connections.
  void ConnectIsolated(ConnectionParams connection_params,
                       const ports::PortRef& port,
                       std::string_view connection_name);

  MojoHandle AddDispatcher(scoped_refptr<Dispatcher> dispatcher);

  // Adds new dispatchers for non-message-pipe handles received in a message.
  // |dispatchers| and |handles| should be the same size.
  bool AddDispatchersFromTransit(
      const std::vector<Dispatcher::DispatcherInTransit>& dispatchers,
      MojoHandle* handles);

  // Marks a set of handles as busy and acquires references to each of their
  // dispatchers. The caller MUST eventually call ReleaseDispatchersForTransit()
  // on the resulting |*dispatchers|. Note that |*dispatchers| contents are
  // extended, not replaced, by this call.
  MojoResult AcquireDispatchersForTransit(
      const MojoHandle* handles,
      size_t num_handles,
      std::vector<Dispatcher::DispatcherInTransit>* dispatchers);

  // Releases dispatchers previously acquired by
  // |AcquireDispatchersForTransit()|. |in_transit| should be |true| if the
  // caller has fully serialized every dispatcher in |dispatchers|, in which
  // case this will close and remove their handles from the handle table.
  //
  // If |in_transit| is false, this simply unmarks the dispatchers as busy,
  // making them available for general use once again.
  void ReleaseDispatchersForTransit(
      const std::vector<Dispatcher::DispatcherInTransit>& dispatchers,
      bool in_transit);

  // Requests that the EDK tear itself down. |callback| will be called once
  // the shutdown process is complete. Note that |callback| is always called
  // asynchronously on the calling thread if said thread is running a message
  // loop, and the calling thread must continue running a MessageLoop at least
  // until the callback is called. If there is no running loop, the |callback|
  // may be called from any thread. Beware!
  void RequestShutdown(base::OnceClosure callback);

  // ---------------------------------------------------------------------------

  // The following methods are essentially implementations of the Mojo Core
  // functions of the Mojo API, with the C interface translated to C++ by
  // "mojo/core/embedder/entrypoints.cc". The best way to understand the
  // contract of these methods is to look at the header files defining the
  // corresponding API functions, referenced below.

  // These methods correspond to the API functions defined in
  // "mojo/public/c/system/functions.h":
  MojoTimeTicks GetTimeTicksNow();
  MojoResult Close(MojoHandle handle);
  MojoResult QueryHandleSignalsState(MojoHandle handle,
                                     MojoHandleSignalsState* signals_state);
  MojoResult CreateTrap(MojoTrapEventHandler handler,
                        const MojoCreateTrapOptions* options,
                        MojoHandle* trap_handle);
  MojoResult AddTrigger(MojoHandle trap_handle,
                        MojoHandle handle,
                        MojoHandleSignals signals,
                        MojoTriggerCondition condition,
                        uintptr_t context,
                        const MojoAddTriggerOptions* options);
  MojoResult RemoveTrigger(MojoHandle trap_handle,
                           uintptr_t context,
                           const MojoRemoveTriggerOptions* options);
  MojoResult ArmTrap(MojoHandle trap_handle,
                     const MojoArmTrapOptions* options,
                     uint32_t* num_blocking_events,
                     MojoTrapEvent* blocking_events);
  MojoResult CreateMessage(const MojoCreateMessageOptions* options,
                           MojoMessageHandle* message_handle);
  MojoResult DestroyMessage(MojoMessageHandle message_handle);
  MojoResult SerializeMessage(MojoMessageHandle message_handle,
                              const MojoSerializeMessageOptions* options);
  MojoResult ReserveMessageCapacity(MojoMessageHandle message_handle,
                                    uint32_t payload_buffer_size,
                                    uint32_t* buffer_size);
  MojoResult AppendMessageData(MojoMessageHandle message_handle,
                               uint32_t additional_payload_size,
                               const MojoHandle* handles,
                               uint32_t num_handles,
                               const MojoAppendMessageDataOptions* options,
                               void** buffer,
                               uint32_t* buffer_size);
  MojoResult GetMessageData(MojoMessageHandle message_handle,
                            const MojoGetMessageDataOptions* options,
                            void** buffer,
                            uint32_t* num_bytes,
                            MojoHandle* handles,
                            uint32_t* num_handles);
  MojoResult SetMessageContext(MojoMessageHandle message_handle,
                               uintptr_t context,
                               MojoMessageContextSerializer serializer,
                               MojoMessageContextDestructor destructor,
                               const MojoSetMessageContextOptions* options);
  MojoResult GetMessageContext(MojoMessageHandle message_handle,
                               const MojoGetMessageContextOptions* options,
                               uintptr_t* context);

  // These methods correspond to the API functions defined in
  // "mojo/public/c/system/message_pipe.h":
  MojoResult CreateMessagePipe(const MojoCreateMessagePipeOptions* options,
                               MojoHandle* message_pipe_handle0,
                               MojoHandle* message_pipe_handle1);
  MojoResult WriteMessage(MojoHandle message_pipe_handle,
                          MojoMessageHandle message_handle,
                          const MojoWriteMessageOptions* options);
  MojoResult ReadMessage(MojoHandle message_pipe_handle,
                         const MojoReadMessageOptions* options,
                         MojoMessageHandle* message_handle);
  MojoResult FuseMessagePipes(MojoHandle handle0,
                              MojoHandle handle1,
                              const MojoFuseMessagePipesOptions* options);
  MojoResult NotifyBadMessage(MojoMessageHandle message_handle,
                              const char* error,
                              size_t error_num_bytes,
                              const MojoNotifyBadMessageOptions* options);

  // These methods correspond to the API functions defined in
  // "mojo/public/c/system/data_pipe.h":
  MojoResult CreateDataPipe(const MojoCreateDataPipeOptions* options,
                            MojoHandle* data_pipe_producer_handle,
                            MojoHandle* data_pipe_consumer_handle);
  MojoResult WriteData(MojoHandle data_pipe_producer_handle,
                       const void* elements,
                       uint32_t* num_bytes,
                       const MojoWriteDataOptions* options);
  MojoResult BeginWriteData(MojoHandle data_pipe_producer_handle,
                            const MojoBeginWriteDataOptions* options,
                            void** buffer,
                            uint32_t* buffer_num_bytes);
  MojoResult EndWriteData(MojoHandle data_pipe_producer_handle,
                          uint32_t num_bytes_written,
                          const MojoEndWriteDataOptions* options);
  MojoResult ReadData(MojoHandle data_pipe_consumer_handle,
                      const MojoReadDataOptions* options,
                      void* elements,
                      uint32_t* num_bytes);
  MojoResult BeginReadData(MojoHandle data_pipe_consumer_handle,
                           const MojoBeginReadDataOptions* options,
                           const void** buffer,
                           uint32_t* buffer_num_bytes);
  MojoResult EndReadData(MojoHandle data_pipe_consumer_handle,
                         uint32_t num_bytes_read,
                         const MojoEndReadDataOptions* options);

  // These methods correspond to the API functions defined in
  // "mojo/public/c/system/buffer.h":
  MojoResult CreateSharedBuffer(uint64_t num_bytes,
                                const MojoCreateSharedBufferOptions* options,
                                MojoHandle* shared_buffer_handle);
  MojoResult DuplicateBufferHandle(
      MojoHandle buffer_handle,
      const MojoDuplicateBufferHandleOptions* options,
      MojoHandle* new_buffer_handle);
  MojoResult MapBuffer(MojoHandle buffer_handle,
                       uint64_t offset,
                       uint64_t num_bytes,
                       const MojoMapBufferOptions* options,
                       void** buffer);
  MojoResult UnmapBuffer(void* buffer);
  MojoResult GetBufferInfo(MojoHandle buffer_handle,
                           const MojoGetBufferInfoOptions* options,
                           MojoSharedBufferInfo* info);

  // These methods correspond to the API functions defined in
  // "mojo/public/c/system/platform_handle.h".
  MojoResult WrapPlatformHandle(const MojoPlatformHandle* platform_handle,
                                const MojoWrapPlatformHandleOptions* options,
                                MojoHandle* mojo_handle);
  MojoResult UnwrapPlatformHandle(
      MojoHandle mojo_handle,
      const MojoUnwrapPlatformHandleOptions* options,
      MojoPlatformHandle* platform_handle);
  MojoResult WrapPlatformSharedMemoryRegion(
      const MojoPlatformHandle* platform_handles,
      uint32_t num_platform_handles,
      uint64_t size,
      const MojoSharedBufferGuid* guid,
      MojoPlatformSharedMemoryRegionAccessMode access_mode,
      const MojoWrapPlatformSharedMemoryRegionOptions* options,
      MojoHandle* mojo_handle);
  MojoResult UnwrapPlatformSharedMemoryRegion(
      MojoHandle mojo_handle,
      const MojoUnwrapPlatformSharedMemoryRegionOptions* options,
      MojoPlatformHandle* platform_handles,
      uint32_t* num_platform_handles,
      uint64_t* size,
      MojoSharedBufferGuid* guid,
      MojoPlatformSharedMemoryRegionAccessMode* access_mode);

  // Invitation API.
  MojoResult CreateInvitation(const MojoCreateInvitationOptions* options,
                              MojoHandle* invitation_handle);
  MojoResult AttachMessagePipeToInvitation(
      MojoHandle invitation_handle,
      const void* name,
      uint32_t name_num_bytes,
      const MojoAttachMessagePipeToInvitationOptions* options,
      MojoHandle* message_pipe_handle);
  MojoResult ExtractMessagePipeFromInvitation(
      MojoHandle invitation_handle,
      const void* name,
      uint32_t name_num_bytes,
      const MojoExtractMessagePipeFromInvitationOptions* options,
      MojoHandle* message_pipe_handle);
  MojoResult SendInvitation(
      MojoHandle invitation_handle,
      const MojoPlatformProcessHandle* process_handle,
      const MojoInvitationTransportEndpoint* transport_endpoint,
      MojoProcessErrorHandler error_handler,
      uintptr_t error_handler_context,
      const MojoSendInvitationOptions* options);
  MojoResult AcceptInvitation(
      const MojoInvitationTransportEndpoint* transport_endpoint,
      const MojoAcceptInvitationOptions* options,
      MojoHandle* invitation_handle);

  // Quota API.
  MojoResult SetQuota(MojoHandle handle,
                      MojoQuotaType type,
                      uint64_t limit,
                      const MojoSetQuotaOptions* options);
  MojoResult QueryQuota(MojoHandle handle,
                        MojoQuotaType type,
                        const MojoQueryQuotaOptions* options,
                        uint64_t* limit,
                        uint64_t* usage);

  MojoResult SetDefaultProcessErrorHandler(
      MojoDefaultProcessErrorHandler handler,
      const MojoSetDefaultProcessErrorHandlerOptions* options);

  void GetActiveHandlesForTest(std::vector<MojoHandle>* handles);

 private:
  // Used to pass ownership of our NodeController over to the IO thread in the
  // event that we're torn down before said thread.
  static void PassNodeControllerToIOThread(
      std::unique_ptr<NodeController> node_controller);

  // Guards node_controller_.
  //
  // TODO(rockot): Consider removing this. It's only needed because we
  // initialize node_controller_ lazily and that may happen on any thread.
  // Otherwise it's effectively const and shouldn't need to be guarded.
  //
  // We can get rid of lazy initialization if we defer Mojo initialization far
  // enough that zygotes don't do it. The zygote can't create a NodeController.
  base::Lock node_controller_lock_;

  // This is lazily initialized on first access. Always use GetNodeController()
  // to access it.
  std::unique_ptr<NodeController> node_controller_;

  // The default callback to invoke, if any, when a process error is reported
  // but cannot be associated with a specific process.
  ProcessErrorCallback default_process_error_callback_;

  std::unique_ptr<HandleTable> handles_;

  base::Lock mapping_table_lock_;  // Protects |mapping_table_|.

  using MappingTable =
      std::unordered_map<void*, std::unique_ptr<PlatformSharedMemoryMapping>>;
  MappingTable mapping_table_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_CORE_H_
