// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/connector.h"

#include <stdint.h>

#include <memory>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "mojo/public/c/system/quota.h"
#include "mojo/public/cpp/bindings/features.h"
#include "mojo/public/cpp/bindings/lib/may_auto_lock.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"
#include "mojo/public/cpp/bindings/sync_handle_watcher.h"
#include "mojo/public/cpp/bindings/tracing_helpers.h"
#include "mojo/public/cpp/system/wait.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_mojo_event_info.pbzero.h"

#if defined(ENABLE_IPC_FUZZER)
#include "mojo/public/cpp/bindings/message_dumper.h"
#endif

namespace mojo {

namespace {

// The default outgoing serialization mode for new Connectors.
Connector::OutgoingSerializationMode g_default_outgoing_serialization_mode =
    Connector::OutgoingSerializationMode::kLazy;

// The default incoming serialization mode for new Connectors.
Connector::IncomingSerializationMode g_default_incoming_serialization_mode =
    Connector::IncomingSerializationMode::kDispatchAsIs;

bool EnableTaskPerMessage() {
  // Const since this may be called from any thread. Initialization is
  // thread-safe. This is a workaround since some consumers of Mojo (e.g. many
  // browser tests) use base::FeatureList incorrectly and thus cause data races
  // when features are queried from arbitrary threads.
  static const bool enable =
      base::FeatureList::IsEnabled(features::kTaskPerMessage);
  return enable;
}

}  // namespace

// Used to efficiently maintain a doubly-linked list of all Connectors
// currently dispatching on any given thread.
class Connector::ActiveDispatchTracker {
 public:
  explicit ActiveDispatchTracker(const base::WeakPtr<Connector>& connector);

  ActiveDispatchTracker(const ActiveDispatchTracker&) = delete;
  ActiveDispatchTracker& operator=(const ActiveDispatchTracker&) = delete;

  ~ActiveDispatchTracker();

  void NotifyBeginNesting();

 private:
  const base::WeakPtr<Connector> connector_;
  const raw_ptr<RunLoopNestingObserver> nesting_observer_;
  raw_ptr<ActiveDispatchTracker> outer_tracker_ = nullptr;
  raw_ptr<ActiveDispatchTracker> inner_tracker_ = nullptr;
};

// Watches the MessageLoop on the current thread. Notifies the current chain of
// ActiveDispatchTrackers when a nested run loop is started.
class Connector::RunLoopNestingObserver
    : public base::RunLoop::NestingObserver {
 public:
  RunLoopNestingObserver() {
    base::RunLoop::AddNestingObserverOnCurrentThread(this);
  }

  RunLoopNestingObserver(const RunLoopNestingObserver&) = delete;
  RunLoopNestingObserver& operator=(const RunLoopNestingObserver&) = delete;

  ~RunLoopNestingObserver() override {
    base::RunLoop::RemoveNestingObserverOnCurrentThread(this);
  }

  // base::RunLoop::NestingObserver:
  void OnBeginNestedRunLoop() override {
    if (top_tracker_)
      top_tracker_->NotifyBeginNesting();
  }

  static RunLoopNestingObserver* GetForThread() {
    if (!base::CurrentThread::Get())
      return nullptr;
    // The NestingObserver for each thread. Note that this is always a
    // Connector::RunLoopNestingObserver; we use the base type here because that
    // subclass is private to Connector.
    static base::SequenceLocalStorageSlot<RunLoopNestingObserver>
        sls_nesting_observer;
    return &sls_nesting_observer.GetOrCreateValue();
  }

 private:
  friend class ActiveDispatchTracker;

  raw_ptr<ActiveDispatchTracker> top_tracker_ = nullptr;
};

Connector::ActiveDispatchTracker::ActiveDispatchTracker(
    const base::WeakPtr<Connector>& connector)
    : connector_(connector), nesting_observer_(connector_->nesting_observer_) {
  DCHECK(nesting_observer_);
  if (nesting_observer_->top_tracker_) {
    outer_tracker_ = nesting_observer_->top_tracker_;
    outer_tracker_->inner_tracker_ = this;
  }
  nesting_observer_->top_tracker_ = this;
}

Connector::ActiveDispatchTracker::~ActiveDispatchTracker() {
  if (nesting_observer_->top_tracker_ == this)
    nesting_observer_->top_tracker_ = outer_tracker_;
  else if (inner_tracker_)
    inner_tracker_->outer_tracker_ = outer_tracker_;
  if (outer_tracker_)
    outer_tracker_->inner_tracker_ = inner_tracker_;
}

void Connector::ActiveDispatchTracker::NotifyBeginNesting() {
  if (connector_ && connector_->handle_watcher_)
    connector_->handle_watcher_->ArmOrNotify();
  if (outer_tracker_)
    outer_tracker_->NotifyBeginNesting();
}

Connector::Connector(ScopedMessagePipeHandle message_pipe,
                     ConnectorConfig config,
                     const char* interface_name)
    : message_pipe_(std::move(message_pipe)),
      error_(false),
      force_immediate_dispatch_(!EnableTaskPerMessage()),
      outgoing_serialization_mode_(g_default_outgoing_serialization_mode),
      incoming_serialization_mode_(g_default_incoming_serialization_mode),
      interface_name_(interface_name),
      header_validator_(
          base::JoinString({interface_name ? interface_name : "Generic",
                            "MessageHeaderValidator"},
                           "")) {
  if (config == MULTI_THREADED_SEND)
    lock_.emplace();

#if defined(ENABLE_IPC_FUZZER)
  if (!MessageDumper::GetMessageDumpDirectory().empty())
    message_dumper_ = std::make_unique<MessageDumper>();
#endif

  weak_self_ = weak_factory_.GetWeakPtr();
}

Connector::Connector(ScopedMessagePipeHandle message_pipe,
                     ConnectorConfig config,
                     scoped_refptr<base::SequencedTaskRunner> runner,
                     const char* interface_name)
    : Connector(std::move(message_pipe), config, interface_name) {
  StartReceiving(std::move(runner));
}

Connector::~Connector() {
  if (is_receiving_) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CancelWait();
  }
}

void Connector::SetOutgoingSerializationMode(OutgoingSerializationMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  outgoing_serialization_mode_ = mode;
}

void Connector::SetIncomingSerializationMode(IncomingSerializationMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  incoming_serialization_mode_ = mode;
}

void Connector::StartReceiving(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool allow_woken_up_by_others) {
  DCHECK(!task_runner_);
  task_runner_ = std::move(task_runner);
  allow_woken_up_by_others_ = allow_woken_up_by_others;

  DETACH_FROM_SEQUENCE(sequence_checker_);
  if (task_runner_->RunsTasksInCurrentSequence()) {
    WaitToReadMore();
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Connector::WaitToReadMore, weak_factory_.GetWeakPtr()));
  }
}

void Connector::CloseMessagePipe() {
  // Throw away the returned message pipe.
  PassMessagePipe();
}

ScopedMessagePipeHandle Connector::PassMessagePipe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CancelWait();
  internal::MayAutoLock locker(&lock_);
  ScopedMessagePipeHandle message_pipe = std::move(message_pipe_);
  weak_factory_.InvalidateWeakPtrs();
  sync_handle_watcher_callback_count_ = 0;

  return message_pipe;
}

void Connector::RaiseError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HandleError(true, true);
}

void Connector::SetConnectionGroup(ConnectionGroup::Ref ref) {
  // If this Connector already belonged to a group, parent the new group to that
  // one so that the reference is not lost.
  if (connection_group_)
    ref.SetParentGroup(std::move(connection_group_));
  connection_group_ = std::move(ref);
}

bool Connector::WaitForIncomingMessage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error_)
    return false;

  ResumeIncomingMethodCallProcessing();

  MojoResult rv = Wait(message_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE);
  if (rv != MOJO_RESULT_OK) {
    // Users that call WaitForIncomingMessage() should expect their code to be
    // re-entered, so we call the error handler synchronously.
    HandleError(rv != MOJO_RESULT_FAILED_PRECONDITION /* force_pipe_reset */,
                false /* force_async_handler */);
    return false;
  }

  ScopedMessageHandle message;
  if ((rv = ReadMessage(message)) != MOJO_RESULT_OK) {
    HandleError(rv != MOJO_RESULT_FAILED_PRECONDITION /* force_pipe_reset */,
                false /* force_async_handler */);
    return false;
  }

  return DispatchMessage(std::move(message));
}

void Connector::PauseIncomingMethodCallProcessing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (paused_)
    return;

  paused_ = true;
  CancelWait();
}

void Connector::ResumeIncomingMethodCallProcessing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!paused_)
    return;

  paused_ = false;
  WaitToReadMore();
}

bool Connector::PrefersSerializedMessages() {
  if (outgoing_serialization_mode_ != OutgoingSerializationMode::kLazy)
    return true;

  // In lazy serialization mode (the default) we prefer to send unserialized
  // messages. Note that most interfaces don't support lazy serialization
  // though, so they'll still only send serialized messages. As such, in most
  // cases this return value is ignored.
  if (!peer_remoteness_tracker_)
    return false;

  // If we have set up a remoteness tracker however, that means we've actually
  // seen at least one unserialized message (see Accept()). In that case we
  // consult the tracker. The point of this is to avoid the redundant work of
  // deferred serialization if we're reasonably certain the message is going to
  // end up serialized anyway.
  return peer_remoteness_tracker_->last_known_state().peer_remote();
}

bool Connector::Accept(Message* message) {
  MojoResult result = AcceptAndGetResult(message);
  return result == MOJO_RESULT_OK;
}

MojoResult Connector::AcceptAndGetResult(Message* message) {
  if (!lock_ && task_runner_)
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (TS_UNCHECKED_READ(error_)) {
    return MOJO_RESULT_UNKNOWN;
  }

  internal::MayAutoLock locker(&lock_);

  if (!message_pipe_.is_valid() || drop_writes_)
    return MOJO_RESULT_OK;

#if defined(ENABLE_IPC_FUZZER)
  if (message_dumper_ && message->is_serialized()) {
    bool dump_result = message_dumper_->Accept(message);
    DCHECK(dump_result);
  }
#endif

  if (!message->is_serialized()) {
    // The caller is sending an unserialized message. If we haven't set up a
    // remoteness tracker yet, do so now. See PrefersSerializedMessages() above
    // for more details. Note that if the Connector is not yet bound to a
    // TaskRunner and activaly reading the pipe, we don't bother setting this up
    // yet.
    DCHECK_EQ(outgoing_serialization_mode_, OutgoingSerializationMode::kLazy);
    if (!peer_remoteness_tracker_ && task_runner_) {
      peer_remoteness_tracker_.emplace(
          message_pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_REMOTE, task_runner_);
    }
  }

  MojoResult rv =
      WriteMessageNew(message_pipe_.get(), message->TakeMojoMessage(),
                      MOJO_WRITE_MESSAGE_FLAG_NONE);

  switch (rv) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // There's no point in continuing to write to this pipe since the other
      // end is gone. Avoid writing any future messages. Hide write failures
      // from the caller since we'd like them to continue consuming any backlog
      // of incoming messages before regarding the message pipe as closed.
      drop_writes_ = true;
      rv = MOJO_RESULT_OK;
      break;
    case MOJO_RESULT_BUSY:
      // We'd get a "busy" result if one of the message's handles is:
      //   - |message_pipe_|'s own handle;
      //   - simultaneously being used on another sequence; or
      //   - in a "busy" state that prohibits it from being transferred (e.g.,
      //     a data pipe handle in the middle of a two-phase read/write,
      //     regardless of which sequence that two-phase read/write is happening
      //     on).
      // TODO(vtl): I wonder if this should be a |DCHECK()|. (But, until
      // crbug.com/389666, etc. are resolved, this will make tests fail quickly
      // rather than hanging.)
      CHECK(false) << "Race condition or other bug detected";
      break;
    default:
      // This particular write was rejected, presumably because of bad input.
      // The pipe is not necessarily in a bad state.
      break;
  }
  return rv;
}

void Connector::AllowWokenUpBySyncWatchOnSameThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  allow_woken_up_by_others_ = true;

  EnsureSyncWatcherExists();
  sync_watcher_->AllowWokenUpBySyncWatchOnSameThread();
}

// static
void Connector::OverrideDefaultSerializationBehaviorForTesting(
    OutgoingSerializationMode outgoing_mode,
    IncomingSerializationMode incoming_mode) {
  g_default_outgoing_serialization_mode = outgoing_mode;
  g_default_incoming_serialization_mode = incoming_mode;
}

bool Connector::SimulateReadMessage(ScopedMessageHandle message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DispatchMessage(std::move(message));
}

void Connector::OnWatcherHandleReady(const char* interface_name,
                                     MojoResult result) {
  // NOTE: `interface_name` always points to static string data, so it's useful
  // to alias without copying to the stack.
  base::debug::Alias(&interface_name);

  OnHandleReadyInternal(result);
}

void Connector::OnSyncHandleWatcherHandleReady(const char* interface_name,
                                               MojoResult result) {
  // NOTE: `interface_name` always points to static string data, so it's useful
  // to alias without copying to the stack.
  base::debug::Alias(&interface_name);

  base::WeakPtr<Connector> weak_self(weak_self_);

  sync_handle_watcher_callback_count_++;
  OnHandleReadyInternal(result);
  // At this point, this object might have been deleted.
  if (weak_self) {
    DCHECK_LT(0u, sync_handle_watcher_callback_count_);
    sync_handle_watcher_callback_count_--;
  }
}

void Connector::OnHandleReadyInternal(MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // No more messages on the pipe and the peer is closed.
    HandleError(false /* force_pipe_reset */, false /* force_async_handler */);
    return;
  } else if (result != MOJO_RESULT_OK) {
    // Some other fatal error condition was encountered. We can propagate this
    // immediately.
    HandleError(true /* force_pipe_reset */, false /* force_async_handler */);
    return;
  }

  ReadAllAvailableMessages();
  // At this point, this object might have been deleted. Return.
}

void Connector::WaitToReadMore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!paused_);
  DCHECK(!handle_watcher_);

  if (!nesting_observer_)
    nesting_observer_ = RunLoopNestingObserver::GetForThread();

  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  handle_watcher_ = std::make_unique<SimpleWatcher>(
      FROM_HERE, SimpleWatcher::ArmingPolicy::MANUAL, task_runner_,
      interface_name_);
  MojoResult rv = handle_watcher_->Watch(
      message_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&Connector::OnWatcherHandleReady,
                          base::Unretained(this), interface_name_));

  if (rv != MOJO_RESULT_OK) {
    // If the watch failed because the handle is invalid or its conditions can
    // no longer be met, we signal the error asynchronously to avoid reentry.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Connector::OnWatcherHandleReady, weak_self_,
                                  interface_name_, rv));
  } else {
    handle_watcher_->ArmOrNotify();
  }

  if (allow_woken_up_by_others_) {
    EnsureSyncWatcherExists();
    sync_watcher_->AllowWokenUpBySyncWatchOnSameThread();
  }

  is_receiving_ = true;
}

uint64_t Connector::QueryPendingMessageCount() const {
  uint64_t unused_current_limit = 0;
  uint64_t pending_message_count = 0;
  MojoQueryQuota(
      message_pipe_.get().value(), MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH,
      /*options=*/nullptr, &unused_current_limit, &pending_message_count);
  return pending_message_count;
}

MojoResult Connector::ReadMessage(ScopedMessageHandle& message) {
  return ReadMessageNew(message_pipe_.get(), &message,
                        MOJO_READ_MESSAGE_FLAG_NONE);
}

bool Connector::DispatchMessage(ScopedMessageHandle handle) {
  DCHECK(!paused_);

  Message message = Message::CreateFromMessageHandle(&handle);
  if (message.IsNull()) {
    // If the Message is null, there was a problem extracting handles from it.
    NotifyBadMessage(
        handle.get(),
        base::StrCat({interface_name_,
                      " One or more handle attachments were invalid."}));
    HandleError(/*force_pipe_reset=*/true, /*force_async_handler=*/false);
    return false;
  }

  if (!header_validator_.Accept(&message)) {
    HandleError(/*force_pipe_reset=*/true, /*force_async_handler=*/false);
    return false;
  }

  base::WeakPtr<Connector> weak_self = weak_self_;
  std::optional<ActiveDispatchTracker> dispatch_tracker;
  if (!is_dispatching_ && nesting_observer_) {
    is_dispatching_ = true;
    dispatch_tracker.emplace(weak_self);
  }

  if (incoming_serialization_mode_ ==
      IncomingSerializationMode::kSerializeBeforeDispatchForTesting) {
    message.SerializeIfNecessary();
  } else {
    DCHECK_EQ(IncomingSerializationMode::kDispatchAsIs,
              incoming_serialization_mode_);
  }

  // This emits just full class name, and is inferior to full mojo tracing, so
  // the category is "toplevel" if full tracing isn't available. If it's
  // available, it's emitted under "disabled-by-default-mojom" for debugging
  // purposes.
  // TODO(altimin): This event is temporarily kept as a debug fallback. Remove
  // it once the new implementation proves to be stable.
  TRACE_EVENT(
      TRACE_DISABLED_BY_DEFAULT("mojom"), "Connector::DispatchMessage",
      [&](perfetto::EventContext& ctx) {
        ctx.event()->set_chrome_mojo_event_info()->set_mojo_interface_tag(
            interface_name_);

        static const uint8_t* flow_enabled =
            TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("toplevel.flow");
        if (!*flow_enabled)
          return;

        perfetto::Flow::Global(message.GetTraceId())(ctx);
      });

  if (connection_group_)
    message.set_receiver_connection_group(&connection_group_);
  bool receiver_result =
      incoming_receiver_ && incoming_receiver_->Accept(&message);
  if (!weak_self)
    return receiver_result;

  if (dispatch_tracker) {
    is_dispatching_ = false;
    dispatch_tracker.reset();
  }

  if (enforce_errors_from_incoming_receiver_ && !receiver_result) {
    HandleError(true /* force_pipe_reset */, false /* force_async_handler */);
    return false;
  }

  return true;
}

void Connector::PostDispatchNextMessageFromPipe() {
  ++num_pending_dispatch_tasks_;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Connector::CallDispatchNextMessageFromPipe, weak_self_));
}

void Connector::CallDispatchNextMessageFromPipe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(num_pending_dispatch_tasks_, 0u);
  --num_pending_dispatch_tasks_;
  ReadAllAvailableMessages();
}

void Connector::ScheduleDispatchOfPendingMessagesOrWaitForMore(
    uint64_t pending_message_count) {
  if (pending_message_count == 0) {
    // We're done only because there are no more messages to read, so go back to
    // watching the pipe for more.
    if (handle_watcher_)
      handle_watcher_->ArmOrNotify();
    return;
  }

  while (pending_message_count > num_pending_dispatch_tasks_) {
    PostDispatchNextMessageFromPipe();
  }
}

void Connector::ReadAllAvailableMessages() {
  if (paused_ || error_)
    return;

  base::WeakPtr<Connector> weak_self = weak_self_;

  do {
    ScopedMessageHandle message;
    MojoResult rv = ReadMessage(message);

    switch (rv) {
      case MOJO_RESULT_OK:
        if (!DispatchMessage(std::move(message)) || !weak_self || paused_) {
          return;
        }
        break;

      case MOJO_RESULT_SHOULD_WAIT:
        // No more messages - we need to wait for new ones to arrive.
        ScheduleDispatchOfPendingMessagesOrWaitForMore(
            /*pending_message_count*/ 0u);
        return;

      case MOJO_RESULT_FAILED_PRECONDITION:
        // The peer endpoint was closed and there are no more messages to read.
        // We can signal an error right away.
        HandleError(false /* force_pipe_reset */,
                    false /* force_async_handler */);
        return;

      default:
        // A fatal error occurred on the pipe, handle it immediately.
        HandleError(true /* force_pipe_reset */,
                    false /* force_async_handler */);
        return;
    }
  } while (weak_self && should_dispatch_messages_immediately());

  if (weak_self) {
    const auto pending_message_count = QueryPendingMessageCount();
    ScheduleDispatchOfPendingMessagesOrWaitForMore(pending_message_count);
  }
}

void Connector::CancelWait() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_receiving_ = false;
  peer_remoteness_tracker_.reset();
  handle_watcher_.reset();
  sync_watcher_.reset();
}

void Connector::HandleError(bool force_pipe_reset, bool force_async_handler) {
  if (error_ || !message_pipe_.is_valid())
    return;

  if (paused_) {
    // Enforce calling the error handler asynchronously if the user has paused
    // receiving messages. We need to wait until the user starts receiving
    // messages again.
    force_async_handler = true;
  }

  if (!force_pipe_reset && force_async_handler)
    force_pipe_reset = true;

  if (force_pipe_reset) {
    CancelWait();
    internal::MayAutoLock locker(&lock_);
    message_pipe_.reset();
    MessagePipe dummy_pipe;
    message_pipe_ = std::move(dummy_pipe.handle0);
  } else {
    CancelWait();
  }

  if (force_async_handler) {
    if (!paused_)
      WaitToReadMore();
  } else {
    error_ = true;
    if (connection_error_handler_)
      std::move(connection_error_handler_).Run();
  }
}

void Connector::EnsureSyncWatcherExists() {
  if (sync_watcher_)
    return;
  sync_watcher_ = std::make_unique<SyncHandleWatcher>(
      message_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&Connector::OnSyncHandleWatcherHandleReady,
                          base::Unretained(this), interface_name_));
}

}  // namespace mojo
