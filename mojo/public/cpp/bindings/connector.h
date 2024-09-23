// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CONNECTOR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CONNECTOR_H_

#include <atomic>
#include <memory>
#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/system/handle_signal_tracker.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace base {
class Lock;
}

namespace mojo {

class SyncHandleWatcher;

// The Connector class is responsible for performing read/write operations on a
// MessagePipe. It writes messages it receives through the MessageReceiver
// interface that it subclasses, and it forwards messages it reads through the
// MessageReceiver interface assigned as its incoming receiver.
//
// NOTE:
//   - MessagePipe I/O is non-blocking.
//   - Sending messages can be configured to be thread safe (please see comments
//     of the constructor). Other than that, the object should only be accessed
//     on the creating sequence.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) Connector : public MessageReceiver {
 public:
  enum ConnectorConfig {
    // Connector::Accept() is only called from a single sequence.
    SINGLE_THREADED_SEND,
    // Connector::Accept() is allowed to be called from multiple sequences.
    MULTI_THREADED_SEND
  };

  // Determines how this Connector should behave with respect to serialization
  // of outgoing messages.
  enum class OutgoingSerializationMode {
    // Lazy serialization. The Connector prefers to transmit serialized messages
    // only when it knows its peer endpoint is remote. This ensures outgoing
    // requests are unserialized by default (when possible, i.e. when generated
    // bindings support it) and serialized only if and when necessary.
    kLazy,

    // Eager serialization. The Connector always prefers serialized messages,
    // ensuring that interface calls will be serialized immediately before
    // sending on the Connector.
    kEager,
  };

  // Determines how this Connector should behave with respect to serialization
  // of incoming messages.
  enum class IncomingSerializationMode {
    // Accepts and dispatches either serialized or unserialized messages. This
    // is the only mode that should be used in production.
    kDispatchAsIs,

    // Accepts either serialized or unserialized messages, but always forces
    // serialization (if applicable) before dispatch. Should be used only in
    // test environments to coerce the lazy serialization of a message after
    // transmission.
    kSerializeBeforeDispatchForTesting,
  };

  // The Connector takes ownership of `message_pipe`. A Connector is essentially
  // inert upon construction, though it may be used to send messages
  // immediately. In order to receive incoming messages or error events,
  // StartReceiving() must be called.
  Connector(ScopedMessagePipeHandle message_pipe,
            ConnectorConfig config,
            const char* interface_name = "unknown interface");

  // Same as above but automatically calls StartReceiving() with `runner` before
  // returning.
  Connector(ScopedMessagePipeHandle message_pipe,
            ConnectorConfig config,
            scoped_refptr<base::SequencedTaskRunner> runner,
            const char* interface_name = "unknown interface");

  Connector(const Connector&) = delete;
  Connector& operator=(const Connector&) = delete;

  ~Connector() override;

  const char* interface_name() const { return interface_name_; }

  // Sets outgoing serialization mode.
  void SetOutgoingSerializationMode(OutgoingSerializationMode mode);
  void SetIncomingSerializationMode(IncomingSerializationMode mode);

  // Sets the receiver to handle messages read from the message pipe.  The
  // Connector will read messages from the pipe regardless of whether or not an
  // incoming receiver has been set.
  void set_incoming_receiver(MessageReceiver* receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    incoming_receiver_ = receiver;
  }

  // Errors from incoming receivers will force the connector into an error
  // state, where no more messages will be processed. This method is used
  // during testing to prevent that from happening.
  void set_enforce_errors_from_incoming_receiver(bool enforce) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    enforce_errors_from_incoming_receiver_ = enforce;
  }

  // If set to |true|, this Connector will always dispatch messages to its
  // receiver as soon as they're read off the pipe, rather than scheduling
  // individual dispatch tasks for each message.
  void set_force_immediate_dispatch(bool force) {
    force_immediate_dispatch_ = force;
  }

  // Sets the error handler to receive notifications when an error is
  // encountered while reading from the pipe or waiting to read from the pipe.
  void set_connection_error_handler(base::OnceClosure error_handler) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    connection_error_handler_ = std::move(error_handler);
  }

  // Returns true if an error was encountered while reading from the pipe or
  // waiting to read from the pipe.
  bool encountered_error() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return error_;
  }

  // Starts receiving on the Connector's message pipe, allowing incoming
  // messages and error events to be dispatched. Once called, the Connector is
  // effectively bound to `task_runner`. Initialization methods like
  // `set_incoming_receiver` may be called before this, but if called after they
  // must be called from the same sequence as `task_runner`.
  //
  // If `allow_woken_up_by_others` is true, the receiving sequence will allow
  // this connector to process incoming messages during any sync wait by any
  // Mojo object on the same sequence.
  void StartReceiving(scoped_refptr<base::SequencedTaskRunner> task_runner,
                      bool allow_woken_up_by_others = false);

  // Closes the pipe. The connector is put into a quiescent state.
  //
  // Please note that this method shouldn't be called unless it results from an
  // explicit request of the user of bindings (e.g., the user sets an
  // InterfacePtr to null or closes a Binding).
  void CloseMessagePipe();

  // Releases the pipe. Connector is put into a quiescent state.
  ScopedMessagePipeHandle PassMessagePipe();

  // Enters the error state. The upper layer may do this for unrecoverable
  // issues such as invalid messages are received. If a connection error handler
  // has been set, it will be called asynchronously.
  //
  // It is a no-op if the connector is already in the error state or there isn't
  // a bound message pipe. Otherwise, it closes the message pipe, which notifies
  // the other end and also prevents potential danger (say, the caller raises
  // an error because it believes the other end is malicious). In order to
  // appear to the user that the connector still binds to a message pipe, it
  // creates a new message pipe, closes one end and binds to the other.
  void RaiseError();

  // Is the connector bound to a MessagePipe handle?
  bool is_valid() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return message_pipe_.is_valid();
  }

  // Adds this object to a ConnectionGroup identified by |ref|. All receiving
  // pipe endpoints decoded from inbound messages on this MultiplexRouter will
  // be added to the same group.
  void SetConnectionGroup(ConnectionGroup::Ref ref);

  // Waits for the next message on the pipe, blocking until one arrives or an
  // error happens. Returns |true| if a message has been delivered, |false|
  // otherwise.
  bool WaitForIncomingMessage();

  // See Binding for details of pause/resume.
  void PauseIncomingMethodCallProcessing();
  void ResumeIncomingMethodCallProcessing();

  // MessageReceiver implementation:
  bool PrefersSerializedMessages() override;
  bool Accept(Message* message) override;

  MojoResult AcceptAndGetResult(Message* message);

  MessagePipeHandle handle() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return message_pipe_.get();
  }

  // Allows |message_pipe_| to be watched while others perform sync handle
  // watching on the same sequence. Please see comments of
  // SyncHandleWatcher::AllowWokenUpBySyncWatchOnSameThread().
  void AllowWokenUpBySyncWatchOnSameThread();

  // Whether currently the control flow is inside the sync handle watcher
  // callback.
  // It always returns false after CloseMessagePipe()/PassMessagePipe().
  bool during_sync_handle_watcher_callback() const {
    return sync_handle_watcher_callback_count_ > 0;
  }

  base::SequencedTaskRunner* task_runner() const { return task_runner_.get(); }

  // Allows testing environments to override the default serialization behavior
  // of newly constructed Connector instances. Must be called before any
  // Connector instances are constructed.
  static void OverrideDefaultSerializationBehaviorForTesting(
      OutgoingSerializationMode outgoing_mode,
      IncomingSerializationMode incoming_mode);

  // Feeds a message to the Connector as if the Connector read it from a pipe.
  // Used for testing and fuzzing.
  bool SimulateReadMessage(ScopedMessageHandle message);

 private:
  class ActiveDispatchTracker;
  class RunLoopNestingObserver;

  // Callback given to SimpleWatcher to dispatch events for pipe activity.
  //
  // We pass the Connector's static interface name here as a parameter, ensuring
  // that if Chrome crashes within this method, the crash dump will include the
  // address of the interface name string in some accessible place such as a
  // register or nearby stack location. We do this to help pinpoint application
  // bugs which destroy bindings endpoints from the wrong thread, as this can
  // result in Connector destruction racing with execution of a WeakPtr-bound
  // OnWatcherHandleReady task.
  void OnWatcherHandleReady(const char* interface_name, MojoResult result);

  // Callback of SyncHandleWatcher. See notes on OnWatcherHandleReady()
  // regarding the `interface_name` argument.
  void OnSyncHandleWatcherHandleReady(const char* interface_name,
                                      MojoResult result);

  void OnHandleReadyInternal(MojoResult result);

  void WaitToReadMore();

  uint64_t QueryPendingMessageCount() const;

  // Attempts to read a single Message from the pipe. Returns |MOJO_RESULT_OK|
  // and a valid message in |*message| iff a message was successfully read and
  // prepared for dispatch.
  MojoResult ReadMessage(ScopedMessageHandle& message);

  // Dispatches |message| to the receiver. Returns |true| if the message was
  // accepted by the receiver, and |false| otherwise (e.g. if it failed
  // validation).
  bool DispatchMessage(ScopedMessageHandle handle)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Posts a task to read the next message from the pipe. These two functions
  // keep |num_pending_read_tasks_| up to date to limit the number of posted
  // tasks when the Connector is e.g. paused and resumed repeatedly.
  void PostDispatchNextMessageFromPipe();
  void CallDispatchNextMessageFromPipe();

  // Ensures that enough tasks are posted to dispatch |pending_message_count|
  // messages based on current |num_pending_dispatch_tasks_| value. If there are
  // no more pending messages, it will call ArmOrNotify() on |handle_watcher_|.
  void ScheduleDispatchOfPendingMessagesOrWaitForMore(
      uint64_t pending_message_count);

  // Reads all available messages off of the pipe, possibly dispatching one or
  // more of them depending on the state of the Connector when this is called.
  void ReadAllAvailableMessages() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // If |force_pipe_reset| is true, this method replaces the existing
  // |message_pipe_| with a dummy message pipe handle (whose peer is closed).
  // If |force_async_handler| is true, |connection_error_handler_| is called
  // asynchronously.
  void HandleError(bool force_pipe_reset, bool force_async_handler)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Cancels any calls made to |handle_watcher_|.
  void CancelWait();

  void EnsureSyncWatcherExists();

  // Indicates whether this Connector should immediately dispatch any message
  // it reads off the pipe, rather than queuing and/or scheduling an
  // asynchronous dispatch operation per message.
  bool should_dispatch_messages_immediately() const {
    return force_immediate_dispatch_ || during_sync_handle_watcher_callback();
  }

  base::OnceClosure connection_error_handler_;

  ScopedMessagePipeHandle message_pipe_;
  // `incoming_receiver_` is not a raw_ptr<...> for performance reasons (based
  // on analysis of sampling profiler data).
  RAW_PTR_EXCLUSION MessageReceiver* incoming_receiver_ = nullptr;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<SimpleWatcher> handle_watcher_;
  std::optional<HandleSignalTracker> peer_remoteness_tracker_;

  std::atomic<bool> error_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool drop_writes_ = false;
  bool enforce_errors_from_incoming_receiver_ = true;

  bool paused_ = false;

  // See |set_force_immediate_dispatch()|.
  bool force_immediate_dispatch_;

  OutgoingSerializationMode outgoing_serialization_mode_;
  IncomingSerializationMode incoming_serialization_mode_;

  // If sending messages is allowed from multiple sequences, |lock_| is used to
  // protect modifications to |message_pipe_| and |drop_writes_|.
  std::optional<base::Lock> lock_;

  std::unique_ptr<SyncHandleWatcher> sync_watcher_;

  bool allow_woken_up_by_others_ = false;
  // If non-zero, currently the control flow is inside the sync handle watcher
  // callback.
  size_t sync_handle_watcher_callback_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  // Indicates whether the Connector is configured to actively read from its
  // message pipe. As long as this is true, the Connector is only safe to
  // destroy in sequence with `task_runner_` tasks.
  bool is_receiving_ = false;

  // The tag used to track heap allocations that originated from a Watcher
  // notification.
  const char* interface_name_ = "unknown interface";

  // A cached pointer to the RunLoopNestingObserver for the thread on which this
  // Connector was created.
  // `nesting_observer_` is not a raw_ptr<...> for performance reasons (based on
  // analysis of sampling profiler data).
  RAW_PTR_EXCLUSION RunLoopNestingObserver* nesting_observer_ = nullptr;

  // |true| iff the Connector is currently dispatching a message. Used to detect
  // nested dispatch operations.
  bool is_dispatching_ = false;

  // The number of pending tasks for |CallDispatchNextMessageFromPipe|.
  size_t num_pending_dispatch_tasks_ = 0;

  MessageHeaderValidator header_validator_;

#if defined(ENABLE_IPC_FUZZER)
  std::unique_ptr<MessageReceiver> message_dumper_;
#endif

  // A reference to the ConnectionGroup to which this Connector belongs, if any.
  ConnectionGroup::Ref connection_group_;

  // Create a single weak ptr and use it everywhere, to avoid the malloc/free
  // cost of creating a new weak ptr whenever it is needed.
  // NOTE: This weak pointer is invalidated when the message pipe is closed or
  // transferred (i.e., when |connected_| is set to false).
  base::WeakPtr<Connector> weak_self_;
  base::WeakPtrFactory<Connector> weak_factory_{this};
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CONNECTOR_H_
