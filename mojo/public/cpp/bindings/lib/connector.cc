// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/connector.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/c/system/quota.h"
#include "mojo/public/cpp/bindings/features.h"
#include "mojo/public/cpp/bindings/lib/may_auto_lock.h"
#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"
#include "mojo/public/cpp/bindings/lib/tracing_helper.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"
#include "mojo/public/cpp/bindings/sync_handle_watcher.h"
#include "mojo/public/cpp/system/wait.h"

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
  ~ActiveDispatchTracker();

  void NotifyBeginNesting();

 private:
  const base::WeakPtr<Connector> connector_;
  RunLoopNestingObserver* const nesting_observer_;
  ActiveDispatchTracker* outer_tracker_ = nullptr;
  ActiveDispatchTracker* inner_tracker_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ActiveDispatchTracker);
};

// Watches the MessageLoop on the current thread. Notifies the current chain of
// ActiveDispatchTrackers when a nested run loop is started.
class Connector::RunLoopNestingObserver
    : public base::RunLoop::NestingObserver {
 public:
  RunLoopNestingObserver() {
    base::RunLoop::AddNestingObserverOnCurrentThread(this);
  }

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
    static base::NoDestructor<
        base::SequenceLocalStorageSlot<RunLoopNestingObserver>>
        sls_nesting_observer;
    return &sls_nesting_observer->GetOrCreateValue();
  }

 private:
  friend class ActiveDispatchTracker;

  ActiveDispatchTracker* top_tracker_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RunLoopNestingObserver);
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
                     scoped_refptr<base::SequencedTaskRunner> runner,
                     const char* heap_profiler_tag)
    : message_pipe_(std::move(message_pipe)),
      task_runner_(std::move(runner)),
      error_(false),
      force_immediate_dispatch_(!EnableTaskPerMessage()),
      outgoing_serialization_mode_(g_default_outgoing_serialization_mode),
      incoming_serialization_mode_(g_default_incoming_serialization_mode),
      heap_profiler_tag_(heap_profiler_tag),
      nesting_observer_(RunLoopNestingObserver::GetForThread()) {
  if (config == MULTI_THREADED_SEND)
    lock_.emplace();

#if defined(ENABLE_IPC_FUZZER)
  if (!MessageDumper::GetMessageDumpDirectory().empty())
    message_dumper_ = std::make_unique<MessageDumper>();
#endif

  weak_self_ = weak_factory_.GetWeakPtr();
  // Even though we don't have an incoming receiver, we still want to monitor
  // the message pipe to know if is closed or encounters an error.
  WaitToReadMore();
}

Connector::~Connector() {
  if (quota_checker_) {
    // Clear the message pipe handle in the checker.
    quota_checker_->SetMessagePipe(MessagePipeHandle());
    UMA_HISTOGRAM_COUNTS_1M("Mojo.Connector.MaxUnreadMessageQuotaUsed",
                            quota_checker_->GetMaxQuotaUsage());
  }

  {
    // Allow for quick destruction on any sequence if the pipe is already
    // closed.
    base::AutoLock lock(connected_lock_);
    if (!connected_)
      return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelWait();
}

void Connector::SetOutgoingSerializationMode(OutgoingSerializationMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  outgoing_serialization_mode_ = mode;
}

void Connector::SetIncomingSerializationMode(IncomingSerializationMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  incoming_serialization_mode_ = mode;
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

  base::AutoLock lock(connected_lock_);
  connected_ = false;
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

  Message message;
  if ((rv = ReadMessage(&message)) != MOJO_RESULT_OK) {
    HandleError(rv != MOJO_RESULT_FAILED_PRECONDITION /* force_pipe_reset */,
                false /* force_async_handler */);
    return false;
  }

  DCHECK(!message.IsNull());
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
  if (outgoing_serialization_mode_ == OutgoingSerializationMode::kEager)
    return true;
  DCHECK_EQ(OutgoingSerializationMode::kLazy, outgoing_serialization_mode_);
  return peer_remoteness_tracker_ &&
         peer_remoteness_tracker_->last_known_state().peer_remote();
}

bool Connector::Accept(Message* message) {
  if (!lock_)
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error_)
    return false;

  internal::MayAutoLock locker(&lock_);

  if (!message_pipe_.is_valid() || drop_writes_)
    return true;

#if defined(ENABLE_IPC_FUZZER)
  if (message_dumper_ && message->is_serialized()) {
    bool dump_result = message_dumper_->Accept(message);
    DCHECK(dump_result);
  }
#endif

  if (quota_checker_)
    quota_checker_->BeforeWrite();

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
      return false;
    default:
      // This particular write was rejected, presumably because of bad input.
      // The pipe is not necessarily in a bad state.
      return false;
  }
  return true;
}

void Connector::AllowWokenUpBySyncWatchOnSameThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  allow_woken_up_by_others_ = true;

  EnsureSyncWatcherExists();
  sync_watcher_->AllowWokenUpBySyncWatchOnSameThread();
}

void Connector::SetMessageQuotaChecker(
    scoped_refptr<internal::MessageQuotaChecker> checker) {
  DCHECK(checker && !quota_checker_);

  quota_checker_ = std::move(checker);
  quota_checker_->SetMessagePipe(message_pipe_.get());
}

// static
void Connector::OverrideDefaultSerializationBehaviorForTesting(
    OutgoingSerializationMode outgoing_mode,
    IncomingSerializationMode incoming_mode) {
  g_default_outgoing_serialization_mode = outgoing_mode;
  g_default_incoming_serialization_mode = incoming_mode;
}

void Connector::OnWatcherHandleReady(MojoResult result) {
  OnHandleReadyInternal(result);
}

void Connector::OnSyncHandleWatcherHandleReady(MojoResult result) {
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
  CHECK(!paused_);
  DCHECK(!handle_watcher_);

  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  handle_watcher_ = std::make_unique<SimpleWatcher>(
      FROM_HERE, SimpleWatcher::ArmingPolicy::MANUAL, task_runner_,
      heap_profiler_tag_);
  MojoResult rv = handle_watcher_->Watch(
      message_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&Connector::OnWatcherHandleReady,
                          base::Unretained(this)));

  if (message_pipe_.is_valid()) {
    peer_remoteness_tracker_.emplace(
        message_pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_REMOTE, task_runner_);
  }

  if (rv != MOJO_RESULT_OK) {
    // If the watch failed because the handle is invalid or its conditions can
    // no longer be met, we signal the error asynchronously to avoid reentry.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Connector::OnWatcherHandleReady, weak_self_, rv));
  } else {
    handle_watcher_->ArmOrNotify();
  }

  if (allow_woken_up_by_others_) {
    EnsureSyncWatcherExists();
    sync_watcher_->AllowWokenUpBySyncWatchOnSameThread();
  }
}

uint64_t Connector::QueryPendingMessageCount() const {
  uint64_t unused_current_limit = 0;
  uint64_t pending_message_count = 0;
  MojoQueryQuota(
      message_pipe_.get().value(), MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH,
      /*options=*/nullptr, &unused_current_limit, &pending_message_count);
  return pending_message_count;
}

MojoResult Connector::ReadMessage(Message* message) {
  ScopedMessageHandle handle;
  MojoResult result =
      ReadMessageNew(message_pipe_.get(), &handle, MOJO_READ_MESSAGE_FLAG_NONE);
  if (result != MOJO_RESULT_OK)
    return result;

  *message = Message::CreateFromMessageHandle(&handle);
  if (message->IsNull()) {
    // Even if the read was successful, the Message may still be null if there
    // was a problem extracting handles from it. We treat this essentially as
    // a bad IPC because we don't really have a better option.
    //
    // We include |heap_profiler_tag_| in the error message since it usually
    // (via this Connector's owner) provides useful information about which
    // binding interface is using this Connector.
    NotifyBadMessage(handle.get(),
                     std::string(heap_profiler_tag_) +
                         "One or more handle attachments were invalid.");
    return MOJO_RESULT_ABORTED;
  }

  return MOJO_RESULT_OK;
}

bool Connector::DispatchMessage(Message message) {
  DCHECK(!paused_);

  base::WeakPtr<Connector> weak_self = weak_self_;
  base::Optional<ActiveDispatchTracker> dispatch_tracker;
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

  TRACE_EVENT_WITH_FLOW0("toplevel.flow", "mojo::Message Receive",
                         MANGLE_MESSAGE_ID(message.header()->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN);
#if !BUILDFLAG(MOJO_TRACE_ENABLED)
  // This emits just full class name, and is inferior to mojo tracing.
  TRACE_EVENT0("mojom", heap_profiler_tag_);
#endif

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
    Message message;
    MojoResult rv = ReadMessage(&message);

    switch (rv) {
      case MOJO_RESULT_OK:
        DCHECK(!message.IsNull());
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
  sync_watcher_.reset(new SyncHandleWatcher(
      message_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&Connector::OnSyncHandleWatcherHandleReady,
                          base::Unretained(this))));
}

}  // namespace mojo
