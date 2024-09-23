// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_DISPATCHER_H_
#define MOJO_CORE_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <ostream>
#include <string_view>
#include <vector>

#include "base/memory/ref_counted.h"
#include "mojo/core/handle_signals_state.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/core/watch.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/quota.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"

struct MojoDuplicateBufferHandleOptions;
struct MojoReadDataOptions;
struct MojoSharedBufferInfo;
struct MojoWriteDataOptions;

namespace mojo {

class PlatformHandle;

namespace core {

namespace ports {
class PortRef;
class UserMessageEvent;
struct PortName;
}

class Dispatcher;
class PlatformSharedMemoryMapping;

using DispatcherVector = std::vector<scoped_refptr<Dispatcher>>;

// A |Dispatcher| implements Mojo core API calls that are associated with a
// particular MojoHandle.
//
// Every MojoHandle in the system is an opaque reference to some implementation
// of this class. See MessagePipeDispatcher, SharedBufferDispatcher,
// DataPipeConsumerDispatcher, DataPipeProducerDispatcher, WatcherDispatcher
// (which should really be renamed to TrapDispatcher now), and
// InvitationDispatcher.
class MOJO_SYSTEM_IMPL_EXPORT Dispatcher
    : public base::RefCountedThreadSafe<Dispatcher> {
 public:
  struct MOJO_SYSTEM_IMPL_EXPORT DispatcherInTransit {
    DispatcherInTransit();
    DispatcherInTransit(const DispatcherInTransit& other);
    ~DispatcherInTransit();

    scoped_refptr<Dispatcher> dispatcher;
    MojoHandle local_handle;
  };

  enum class Type {
    UNKNOWN = 0,
    MESSAGE_PIPE,
    DATA_PIPE_PRODUCER,
    DATA_PIPE_CONSUMER,
    SHARED_BUFFER,
    WATCHER,
    INVITATION,

    // "Private" types (not exposed via the public interface):
    PLATFORM_HANDLE = -1,
  };

  Dispatcher(const Dispatcher&) = delete;
  Dispatcher& operator=(const Dispatcher&) = delete;

  // TODO(crbug.com/40778522): Remove these and all callers.
  //
  // The assert is invoked at various points of handle deserialization failure.
  // Such failures are expected and innocuous when destroying unread or unsent,
  // discarded messages with attachments that may no longer be valid; but they
  // are problematic when hit during normal message deserialization for messages
  // the application expects to read and dispatch. Both this setter and the
  // assertion are concerned only with their calling thread.
  static void SetExtractingHandlesFromMessage(bool extracting);
  static void AssertNotExtractingHandlesFromMessage();

  // All Dispatchers must minimally implement these methods.

  virtual Type GetType() const = 0;
  virtual MojoResult Close() = 0;

  ///////////// Watcher API ////////////////////

  // Supports the |MojoAddTrigger()| API if implemented by this Dispatcher.
  // |dispatcher| is the resolved Dispatcher implementation from the given
  // MojoHandle to watch. The remaining arguments correspond directly to
  // arguments on the original |MojoAddTrigger()| API call. See
  // |MojoAddTrigger()| documentation.
  virtual MojoResult WatchDispatcher(scoped_refptr<Dispatcher> dispatcher,
                                     MojoHandleSignals signals,
                                     MojoTriggerCondition condition,
                                     uintptr_t context);

  // Supports the |MojoRemoveTrigger()| API if implemented by this Dispatcher.
  // Arguments correspond directly to arguments on the original
  // |MojoRemoveTrigger()| API call. See |MojoRemoveTrigger()| documentation.
  virtual MojoResult CancelWatch(uintptr_t context);

  // Supports the |MojoArmTrap()| API if implemented by this Dispatcher.
  // Arguments correspond directly to arguments on the original |MojoArmTrap()|
  // API call. See |MojoArmTrap()| documentation.
  virtual MojoResult Arm(uint32_t* num_blocking_events,
                         MojoTrapEvent* blocking_events);

  ///////////// Message pipe API /////////////

  // Supports the |MojoWriteMessage()| API if implemented by this Dispatcher.
  // |message| is the message object referenced by the MojoMessageHandle passed
  // to the original API call. See |MojoWriteMessage()| documentation.
  virtual MojoResult WriteMessage(
      std::unique_ptr<ports::UserMessageEvent> message);

  // Supports the |MojoReadMessage()| API if implemented by this Dispatcher.
  // If successful, |*message| contains a newly read message object, which will
  // be yielded to the API caller as an opaque MojoMessageHandle value. See
  // |MojoReadMessage()| documentation.
  virtual MojoResult ReadMessage(
      std::unique_ptr<ports::UserMessageEvent>* message);

  ///////////// Shared buffer API /////////////

  // Supports the |MojoDuplicateBufferHandle()| API if implemented by this
  // Dispatcher.
  //
  // |options| may be null. |new_dispatcher| must not be null, but
  // |*new_dispatcher| should be null (and will contain the dispatcher for the
  // new handle on success).
  //
  // See |MojoDuplicateBufferHandle()| documentation.
  virtual MojoResult DuplicateBufferHandle(
      const MojoDuplicateBufferHandleOptions* options,
      scoped_refptr<Dispatcher>* new_dispatcher);

  // Supports the |MojoMapBuffer()| API if implemented by this Dispatcher.
  // |offset| and |num_bytes| correspond to arguments given to the original API
  // call. On success, |*mapping| will contain a memory mapping that Mojo Core
  // will internally retain until the buffer is unmapped by |MojoUnmapBuffer()|.
  // See |MojoMapBuffer()| documentation.
  virtual MojoResult MapBuffer(
      uint64_t offset,
      uint64_t num_bytes,
      std::unique_ptr<PlatformSharedMemoryMapping>* mapping);

  // Supports the |MojoGetBufferInfo()| API if implemented by this Dispatcher.
  // Arguments correspond to the ones given to the original API call. See
  // |MojoGetBufferInfo()| documentation.
  virtual MojoResult GetBufferInfo(MojoSharedBufferInfo* info);

  ///////////// Data pipe consumer API /////////////

  // Supports the the |MojoReadData()| API if implemented by this Dispatcher.
  // Arguments correspond to the ones given to the original API call. See
  // |MojoReadData()| documentation.
  virtual MojoResult ReadData(const MojoReadDataOptions& options,
                              void* elements,
                              uint32_t* num_bytes);

  // Supports the the |MojoBeginReadData()| API if implemented by this
  // Dispatcher. Arguments correspond to the ones given to the original API
  // call. See |MojoBeginReadData()| documentation.
  virtual MojoResult BeginReadData(const void** buffer,
                                   uint32_t* buffer_num_bytes);

  // Supports the the |MojoEndReadData()| API if implemented by this Dispatcher.
  // Arguments correspond to the ones given to the original API call. See
  // |MojoEndReadData()| documentation.
  virtual MojoResult EndReadData(uint32_t num_bytes_read);

  ///////////// Data pipe producer API /////////////

  // Supports the the |MojoWriteData()| API if implemented by this Dispatcher.
  // Arguments correspond to the ones given to the original API call. See
  // |MojoWriteData()| documentation.
  virtual MojoResult WriteData(const void* elements,
                               uint32_t* num_bytes,
                               const MojoWriteDataOptions& options);

  // Supports the the |MojoBeginWriteData()| API if implemented by this
  // Dispatcher. Arguments correspond to the ones given to the original API
  // call. See |MojoBeginWriteData()| documentation.
  virtual MojoResult BeginWriteData(void** buffer,
                                    uint32_t* buffer_num_bytes,
                                    MojoBeginWriteDataFlags flags);

  // Supports the the |MojoEndWriteData()| API if implemented by this
  // Dispatcher. Arguments correspond to the ones given to the original API
  // call. See |MojoEndWriteData()| documentation.
  virtual MojoResult EndWriteData(uint32_t num_bytes_written);

  // Supports the |MojoAttachMessagePipeToInvitation()| API if implemented by
  // this Dispatcher. Arguments correspond to the ones given to the original API
  // call. See |MojoAttachMessagePipeToInvitation()| documentation.
  virtual MojoResult AttachMessagePipe(std::string_view name,
                                       ports::PortRef remote_peer_port);

  // Supports the |MojoExtractMessagePipeFromInvitation()| API if implemented by
  // this Dispatcher. Arguments correspond to the ones given to the original API
  // call. See |MojoExtractMessagePipeFromInvitation()| documentation.
  virtual MojoResult ExtractMessagePipe(std::string_view name,
                                        MojoHandle* message_pipe_handle);

  // Supports the |MojoSetQuota()| API if implemented by this Dispatcher.
  // Arguments correspond to the ones given to the original API call. See
  // |MojoSetQuota()| documentation.
  virtual MojoResult SetQuota(MojoQuotaType type, uint64_t limit);

  // Supports the |MojoQueryQuota()| API if implemented by this Dispatcher.
  // Arguments correspond to the ones given to the original API call. See
  // |MojoQueryQuota()| documentation.
  virtual MojoResult QueryQuota(MojoQuotaType type,
                                uint64_t* limit,
                                uint64_t* usage);

  ///////////// General-purpose API for all handle types /////////

  // Gets the current handle signals state. (The default implementation simply
  // returns a default-constructed |HandleSignalsState|, i.e., no signals
  // satisfied or satisfiable.) Note: The state is subject to change from other
  // threads.
  virtual HandleSignalsState GetHandleSignalsState() const;

  // Adds a WatcherDispatcher reference to this dispatcher, to be notified of
  // all subsequent changes to handle state including signal changes or closure.
  // The reference is associated with a |context| for disambiguation of
  // removals.
  virtual MojoResult AddWatcherRef(
      const scoped_refptr<WatcherDispatcher>& watcher,
      uintptr_t context);

  // Removes a WatcherDispatcher reference from this dispatcher.
  virtual MojoResult RemoveWatcherRef(WatcherDispatcher* watcher,
                                      uintptr_t context);

  // Informs the caller of the total serialized size (in bytes) and the total
  // number of platform handles and ports needed to transfer this dispatcher
  // across a message pipe.
  //
  // Must eventually be followed by a call to EndSerializeAndClose(). Note that
  // StartSerialize() and EndSerialize() are always called in sequence, and
  // only between calls to BeginTransit() and either (but not both)
  // CompleteTransitAndClose() or CancelTransit().
  //
  // For this reason it is IMPERATIVE that the implementation ensure a
  // consistent serializable state between BeginTransit() and
  // CompleteTransitAndClose()/CancelTransit().
  virtual void StartSerialize(uint32_t* num_bytes,
                              uint32_t* num_ports,
                              uint32_t* num_platform_handles);

  // Serializes this dispatcher into |destination|, |ports|, and |handles|.
  // Returns true iff successful, false otherwise. In either case the dispatcher
  // will close.
  //
  // NOTE: Transit MAY still fail after this call returns. Implementations
  // should not assume PlatformHandle ownership has transferred until
  // CompleteTransitAndClose() is called. In other words, if CancelTransit() is
  // called, the implementation should retain its PlatformHandles in working
  // condition.
  virtual bool EndSerialize(void* destination,
                            ports::PortName* ports,
                            PlatformHandle* handles);

  // Does whatever is necessary to begin transit of the dispatcher.  This
  // should return |true| if transit is OK, or false if the underlying resource
  // is deemed busy by the implementation.
  virtual bool BeginTransit();

  // Does whatever is necessary to complete transit of the dispatcher, including
  // closure. This is only called upon successfully transmitting an outgoing
  // message containing this serialized dispatcher.
  virtual void CompleteTransitAndClose();

  // Does whatever is necessary to cancel transit of the dispatcher. The
  // dispatcher should remain in a working state and resume normal operation.
  virtual void CancelTransit();

  // Deserializes a specific dispatcher type from an incoming message.
  static scoped_refptr<Dispatcher> Deserialize(Type type,
                                               const void* bytes,
                                               size_t num_bytes,
                                               const ports::PortName* ports,
                                               size_t num_ports,
                                               PlatformHandle* platform_handles,
                                               size_t platform_handle_count);

 protected:
  friend class base::RefCountedThreadSafe<Dispatcher>;

  Dispatcher();
  virtual ~Dispatcher();
};

// So logging macros and |DCHECK_EQ()|, etc. work.
MOJO_SYSTEM_IMPL_EXPORT inline std::ostream& operator<<(std::ostream& out,
                                                        Dispatcher::Type type) {
  return out << static_cast<int>(type);
}

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_DISPATCHER_H_
