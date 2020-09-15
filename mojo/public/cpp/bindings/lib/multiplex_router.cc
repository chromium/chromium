// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/lib/may_auto_lock.h"
#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"
#include "mojo/public/cpp/bindings/sequence_local_sync_event_watcher.h"

namespace mojo {
namespace internal {

// InterfaceEndpoint stores the information of an interface endpoint registered
// with the router.
// No one other than the router's |endpoints_| and |tasks_| should hold refs to
// this object.
class MultiplexRouter::InterfaceEndpoint
    : public base::RefCountedThreadSafe<InterfaceEndpoint>,
      public InterfaceEndpointController {
 public:
  InterfaceEndpoint(MultiplexRouter* router, InterfaceId id)
      : router_(router),
        id_(id),
        closed_(false),
        peer_closed_(false),
        handle_created_(false),
        client_(nullptr) {}

  // ---------------------------------------------------------------------------
  // The following public methods are safe to call from any sequence without
  // locking.

  InterfaceId id() const { return id_; }

  // ---------------------------------------------------------------------------
  // The following public methods are called under the router's lock.

  bool closed() const { return closed_; }
  void set_closed() {
    router_->AssertLockAcquired();
    closed_ = true;
  }

  bool peer_closed() const { return peer_closed_; }
  void set_peer_closed() {
    router_->AssertLockAcquired();
    peer_closed_ = true;
  }

  bool handle_created() const { return handle_created_; }
  void set_handle_created() {
    router_->AssertLockAcquired();
    handle_created_ = true;
  }

  const base::Optional<DisconnectReason>& disconnect_reason() const {
    return disconnect_reason_;
  }
  void set_disconnect_reason(
      const base::Optional<DisconnectReason>& disconnect_reason) {
    router_->AssertLockAcquired();
    disconnect_reason_ = disconnect_reason;
  }

  base::SequencedTaskRunner* task_runner() const { return task_runner_.get(); }

  InterfaceEndpointClient* client() const { return client_; }

  void AttachClient(InterfaceEndpointClient* client,
                    scoped_refptr<base::SequencedTaskRunner> runner) {
    router_->AssertLockAcquired();
    DCHECK(!client_);
    DCHECK(!closed_);
    DCHECK(runner->RunsTasksInCurrentSequence());

    task_runner_ = std::move(runner);
    client_ = client;
  }

  // This method must be called on the same sequence as the corresponding
  // AttachClient() call.
  void DetachClient() {
    router_->AssertLockAcquired();
    DCHECK(client_);
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!closed_);

    task_runner_ = nullptr;
    client_ = nullptr;
    sync_watcher_.reset();
  }

  void SignalSyncMessageEvent() {
    router_->AssertLockAcquired();
    if (sync_message_event_signaled_)
      return;
    sync_message_event_signaled_ = true;
    if (sync_watcher_)
      sync_watcher_->SignalEvent();
  }

  void ResetSyncMessageSignal() {
    router_->AssertLockAcquired();
    if (!sync_message_event_signaled_)
      return;
    sync_message_event_signaled_ = false;
    if (sync_watcher_)
      sync_watcher_->ResetEvent();
  }

  // ---------------------------------------------------------------------------
  // The following public methods (i.e., InterfaceEndpointController
  // implementation) are called by the client on the same sequence as the
  // AttachClient() call. They are called outside of the router's lock.

  bool SendMessage(Message* message) override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    message->set_interface_id(id_);
    return router_->connector_.Accept(message);
  }

  void AllowWokenUpBySyncWatchOnSameThread() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    EnsureSyncWatcherExists();
    sync_watcher_->AllowWokenUpBySyncWatchOnSameSequence();
  }

  bool SyncWatch(const bool* should_stop) override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    EnsureSyncWatcherExists();
    return sync_watcher_->SyncWatch(should_stop);
  }

 private:
  friend class base::RefCountedThreadSafe<InterfaceEndpoint>;

  ~InterfaceEndpoint() override {
    router_->AssertLockAcquired();

    DCHECK(!client_);
  }

  void OnSyncEventSignaled() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    scoped_refptr<MultiplexRouter> router_protector(router_);

    MayAutoLock locker(&router_->lock_);
    scoped_refptr<InterfaceEndpoint> self_protector(this);

    bool more_to_process = router_->ProcessFirstSyncMessageForEndpoint(id_);

    if (!more_to_process)
      ResetSyncMessageSignal();

    // Currently there are no queued sync messages and the peer has closed so
    // there won't be incoming sync messages in the future.
    if (!more_to_process && peer_closed_) {
      // If a SyncWatch() call (or multiple ones) of this interface endpoint is
      // on the call stack, resetting the sync watcher will allow it to exit
      // when the call stack unwinds to that frame.
      sync_watcher_.reset();
    }
  }

  void EnsureSyncWatcherExists() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (sync_watcher_)
      return;

    MayAutoLock locker(&router_->lock_);
    sync_watcher_ =
        std::make_unique<SequenceLocalSyncEventWatcher>(base::BindRepeating(
            &InterfaceEndpoint::OnSyncEventSignaled, base::Unretained(this)));
    if (sync_message_event_signaled_)
      sync_watcher_->SignalEvent();
  }

  // ---------------------------------------------------------------------------
  // The following members are safe to access from any sequence.

  MultiplexRouter* const router_;
  const InterfaceId id_;

  // ---------------------------------------------------------------------------
  // The following members are accessed under the router's lock.

  // Whether the endpoint has been closed.
  bool closed_;
  // Whether the peer endpoint has been closed.
  bool peer_closed_;

  // Whether there is already a ScopedInterfaceEndpointHandle created for this
  // endpoint.
  bool handle_created_;

  base::Optional<DisconnectReason> disconnect_reason_;

  // The task runner on which |client_|'s methods can be called.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Not owned. It is null if no client is attached to this endpoint.
  InterfaceEndpointClient* client_;

  // Indicates whether the sync watcher should be signaled for this endpoint.
  bool sync_message_event_signaled_ = false;

  // Guarded by the router's lock. Used to synchronously wait on replies.
  std::unique_ptr<SequenceLocalSyncEventWatcher> sync_watcher_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceEndpoint);
};

// MessageWrapper objects are always destroyed under the router's lock. On
// destruction, if the message it wrappers contains interface IDs, the wrapper
// closes the corresponding endpoints.
class MultiplexRouter::MessageWrapper {
 public:
  MessageWrapper() = default;

  MessageWrapper(MultiplexRouter* router, Message message)
      : router_(router), value_(std::move(message)) {}

  MessageWrapper(MessageWrapper&& other)
      : router_(other.router_), value_(std::move(other.value_)) {}

  ~MessageWrapper() {
    if (!router_ || value_.IsNull())
      return;

    router_->AssertLockAcquired();
    // Don't try to close the endpoints if at this point the router is already
    // half-destructed.
    if (!router_->being_destructed_)
      router_->CloseEndpointsForMessage(value_);
  }

  MessageWrapper& operator=(MessageWrapper&& other) {
    router_ = other.router_;
    value_ = std::move(other.value_);
    return *this;
  }

  const Message& value() const { return value_; }

  // Must be called outside of the router's lock.
  // Returns a null message if it fails to deseralize the associated endpoint
  // handles.
  Message DeserializeEndpointHandlesAndTake() {
    if (!value_.DeserializeAssociatedEndpointHandles(router_)) {
      // The previous call may have deserialized part of the associated
      // interface endpoint handles. They must be destroyed outside of the
      // router's lock, so we cannot wait until destruction of MessageWrapper.
      value_.Reset();
      return Message();
    }
    return std::move(value_);
  }

 private:
  MultiplexRouter* router_ = nullptr;
  Message value_;

  DISALLOW_COPY_AND_ASSIGN(MessageWrapper);
};

struct MultiplexRouter::Task {
 public:
  // Doesn't take ownership of |message| but takes its contents.
  static std::unique_ptr<Task> CreateMessageTask(
      MessageWrapper message_wrapper) {
    Task* task = new Task(MESSAGE);
    task->message_wrapper = std::move(message_wrapper);
    return base::WrapUnique(task);
  }
  static std::unique_ptr<Task> CreateNotifyErrorTask(
      InterfaceEndpoint* endpoint) {
    Task* task = new Task(NOTIFY_ERROR);
    task->endpoint_to_notify = endpoint;
    return base::WrapUnique(task);
  }

  ~Task() {}

  bool IsMessageTask() const { return type == MESSAGE; }
  bool IsNotifyErrorTask() const { return type == NOTIFY_ERROR; }

  MessageWrapper message_wrapper;
  scoped_refptr<InterfaceEndpoint> endpoint_to_notify;

  enum Type { MESSAGE, NOTIFY_ERROR };
  Type type;

 private:
  explicit Task(Type in_type) : type(in_type) {}

  DISALLOW_COPY_AND_ASSIGN(Task);
};

MultiplexRouter::MultiplexRouter(
    ScopedMessagePipeHandle message_pipe,
    Config config,
    bool set_interface_id_namespace_bit,
    scoped_refptr<base::SequencedTaskRunner> runner,
    const char* primary_interface_name)
    : set_interface_id_namespace_bit_(set_interface_id_namespace_bit),
      task_runner_(runner),
      dispatcher_(this),
      connector_(std::move(message_pipe),
                 config == MULTI_INTERFACE ? Connector::MULTI_THREADED_SEND
                                           : Connector::SINGLE_THREADED_SEND,
                 std::move(runner),
                 primary_interface_name),
      control_message_handler_(this),
      control_message_proxy_(&connector_) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (config == MULTI_INTERFACE)
    lock_.emplace();

  if (config == SINGLE_INTERFACE_WITH_SYNC_METHODS ||
      config == MULTI_INTERFACE) {
    // Always participate in sync handle watching in multi-interface mode,
    // because even if it doesn't expect sync requests during sync handle
    // watching, it may still need to dispatch messages to associated endpoints
    // on a different sequence.
    connector_.AllowWokenUpBySyncWatchOnSameThread();
  }
  connector_.set_incoming_receiver(&dispatcher_);
  connector_.set_connection_error_handler(
      base::BindOnce(&MultiplexRouter::OnPipeConnectionError,
                     base::Unretained(this), false /* force_async_dispatch */));

  scoped_refptr<internal::MessageQuotaChecker> quota_checker =
      internal::MessageQuotaChecker::MaybeCreate();
  if (quota_checker)
    connector_.SetMessageQuotaChecker(std::move(quota_checker));

  std::unique_ptr<MessageHeaderValidator> header_validator =
      std::make_unique<MessageHeaderValidator>();
  header_validator_ = header_validator.get();
  dispatcher_.SetValidator(std::move(header_validator));

  if (primary_interface_name) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    header_validator_->SetDescription(base::JoinString(
        {primary_interface_name, "[primary] MessageHeaderValidator"}, " "));
    control_message_handler_.SetDescription(base::JoinString(
        {primary_interface_name, "[primary] PipeControlMessageHandler"}, " "));
  }
}

MultiplexRouter::~MultiplexRouter() {
  MayAutoLock locker(&lock_);

  being_destructed_ = true;

  sync_message_tasks_.clear();
  tasks_.clear();
  endpoints_.clear();
}

void MultiplexRouter::SetIncomingMessageFilter(
    std::unique_ptr<MessageFilter> filter) {
  dispatcher_.SetFilter(std::move(filter));
}

void MultiplexRouter::SetConnectionGroup(ConnectionGroup::Ref ref) {
  connector_.SetConnectionGroup(std::move(ref));
}

InterfaceId MultiplexRouter::AssociateInterface(
    ScopedInterfaceEndpointHandle handle_to_send) {
  if (!handle_to_send.pending_association())
    return kInvalidInterfaceId;

  uint32_t id = 0;
  {
    MayAutoLock locker(&lock_);
    do {
      if (next_interface_id_value_ >= kInterfaceIdNamespaceMask)
        next_interface_id_value_ = 1;
      id = next_interface_id_value_++;
      if (set_interface_id_namespace_bit_)
        id |= kInterfaceIdNamespaceMask;
    } while (base::Contains(endpoints_, id));

    InterfaceEndpoint* endpoint = new InterfaceEndpoint(this, id);
    endpoints_[id] = endpoint;
    if (encountered_error_)
      UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
    endpoint->set_handle_created();
  }

  if (!NotifyAssociation(&handle_to_send, id)) {
    // The peer handle of |handle_to_send|, which is supposed to join this
    // associated group, has been closed.
    {
      MayAutoLock locker(&lock_);
      InterfaceEndpoint* endpoint = FindEndpoint(id);
      if (endpoint)
        UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);
    }

    control_message_proxy_.NotifyPeerEndpointClosed(
        id, handle_to_send.disconnect_reason());
  }
  return id;
}

ScopedInterfaceEndpointHandle MultiplexRouter::CreateLocalEndpointHandle(
    InterfaceId id) {
  if (!IsValidInterfaceId(id))
    return ScopedInterfaceEndpointHandle();

  MayAutoLock locker(&lock_);
  bool inserted = false;
  InterfaceEndpoint* endpoint = FindOrInsertEndpoint(id, &inserted);
  if (inserted) {
    if (encountered_error_)
      UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  } else {
    if (endpoint->handle_created() || endpoint->closed())
      return ScopedInterfaceEndpointHandle();
  }

  endpoint->set_handle_created();
  return CreateScopedInterfaceEndpointHandle(id);
}

void MultiplexRouter::CloseEndpointHandle(
    InterfaceId id,
    const base::Optional<DisconnectReason>& reason) {
  if (!IsValidInterfaceId(id))
    return;

  MayAutoLock locker(&lock_);
  DCHECK(base::Contains(endpoints_, id));
  InterfaceEndpoint* endpoint = endpoints_[id].get();
  DCHECK(!endpoint->client());
  DCHECK(!endpoint->closed());
  UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);

  if (!IsPrimaryInterfaceId(id) || reason) {
    MayAutoUnlock unlocker(&lock_);
    control_message_proxy_.NotifyPeerEndpointClosed(id, reason);
  }

  ProcessTasks(NO_DIRECT_CLIENT_CALLS, nullptr);
}

InterfaceEndpointController* MultiplexRouter::AttachEndpointClient(
    const ScopedInterfaceEndpointHandle& handle,
    InterfaceEndpointClient* client,
    scoped_refptr<base::SequencedTaskRunner> runner) {
  const InterfaceId id = handle.id();

  DCHECK(IsValidInterfaceId(id));
  DCHECK(client);

  MayAutoLock locker(&lock_);
  DCHECK(base::Contains(endpoints_, id));

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  endpoint->AttachClient(client, std::move(runner));

  if (endpoint->peer_closed())
    tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));
  ProcessTasks(NO_DIRECT_CLIENT_CALLS, nullptr);

  return endpoint;
}

void MultiplexRouter::DetachEndpointClient(
    const ScopedInterfaceEndpointHandle& handle) {
  const InterfaceId id = handle.id();

  DCHECK(IsValidInterfaceId(id));

  MayAutoLock locker(&lock_);
  DCHECK(base::Contains(endpoints_, id));

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  endpoint->DetachClient();
}

void MultiplexRouter::RaiseError() {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    connector_.RaiseError();
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&MultiplexRouter::RaiseError, this));
  }
}

bool MultiplexRouter::PrefersSerializedMessages() {
  MayAutoLock locker(&lock_);
  return connector_.PrefersSerializedMessages();
}

void MultiplexRouter::CloseMessagePipe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connector_.CloseMessagePipe();
  flush_pipe_watcher_.reset();
  active_flush_pipe_.reset();
  // CloseMessagePipe() above won't trigger connection error handler.
  // Explicitly call OnPipeConnectionError() so that associated endpoints will
  // get notified.
  OnPipeConnectionError(true /* force_async_dispatch */);
}

void MultiplexRouter::PauseIncomingMethodCallProcessing() {
  PauseInternal(/*must_resume_manually=*/true);
}

void MultiplexRouter::ResumeIncomingMethodCallProcessing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the owner is manually resuming from a previous pause request, the
  // interface may also still be paused due to waiting on a pending async flush
  // in the system.
  //
  // In that case we ignore the caller, except to subsequently allow implicit
  // resume once the pending flush operation is finished.
  if (active_flush_pipe_) {
    MayAutoLock locker(&lock_);
    must_resume_manually_ = false;
    return;
  }

  connector_.ResumeIncomingMethodCallProcessing();

  MayAutoLock locker(&lock_);
  paused_ = false;
  must_resume_manually_ = false;

  for (auto iter = endpoints_.begin(); iter != endpoints_.end(); ++iter) {
    auto sync_iter = sync_message_tasks_.find(iter->first);
    if (iter->second->peer_closed() ||
        (sync_iter != sync_message_tasks_.end() &&
         !sync_iter->second.empty())) {
      iter->second->SignalSyncMessageEvent();
    }
  }

  ProcessTasks(NO_DIRECT_CLIENT_CALLS, nullptr);
}

void MultiplexRouter::FlushAsync(AsyncFlusher flusher) {
  control_message_proxy_.FlushAsync(std::move(flusher));
}

void MultiplexRouter::PausePeerUntilFlushCompletes(PendingFlush flush) {
  control_message_proxy_.PausePeerUntilFlushCompletes(std::move(flush));
}

bool MultiplexRouter::HasAssociatedEndpoints() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MayAutoLock locker(&lock_);

  if (endpoints_.size() > 1)
    return true;
  if (endpoints_.size() == 0)
    return false;

  return !base::Contains(endpoints_, kPrimaryInterfaceId);
}

void MultiplexRouter::EnableBatchDispatch() {
  connector_.set_force_immediate_dispatch(true);
}

void MultiplexRouter::EnableTestingMode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MayAutoLock locker(&lock_);

  testing_mode_ = true;
  connector_.set_enforce_errors_from_incoming_receiver(false);
}

bool MultiplexRouter::Accept(Message* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Insert endpoints for the payload interface IDs as soon as the message
  // arrives, instead of waiting till the message is dispatched. Consider the
  // following sequence:
  // 1) Async message msg1 arrives, containing interface ID x. Msg1 is not
  //    dispatched because a sync call is blocking the thread.
  // 2) Sync message msg2 arrives targeting interface ID x.
  //
  // If we don't insert endpoint for interface ID x, when trying to dispatch
  // msg2 we don't know whether it is an unexpected message or it is just
  // because the message containing x hasn't been dispatched.
  if (!InsertEndpointsForMessage(*message))
    return false;

  scoped_refptr<MultiplexRouter> protector(this);
  MayAutoLock locker(&lock_);

  DCHECK(!paused_);

  ClientCallBehavior client_call_behavior =
      connector_.during_sync_handle_watcher_callback()
          ? ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES
          : ALLOW_DIRECT_CLIENT_CALLS;

  MessageWrapper message_wrapper(this, std::move(*message));
  bool processed = tasks_.empty() && ProcessIncomingMessage(
                                         &message_wrapper, client_call_behavior,
                                         connector_.task_runner());

  if (!processed) {
    // Either the task queue is not empty or we cannot process the message
    // directly. In both cases, there is no need to call ProcessTasks().
    tasks_.push_back(Task::CreateMessageTask(std::move(message_wrapper)));
    Task* task = tasks_.back().get();

    if (task->message_wrapper.value().has_flag(Message::kFlagIsSync)) {
      InterfaceId id = task->message_wrapper.value().interface_id();
      sync_message_tasks_[id].push_back(task);
      InterfaceEndpoint* endpoint = FindEndpoint(id);
      if (endpoint)
        endpoint->SignalSyncMessageEvent();
    }
  } else if (!tasks_.empty()) {
    // Processing the message may result in new tasks (for error notification)
    // being added to the queue. In this case, we have to attempt to process the
    // tasks.
    ProcessTasks(client_call_behavior, connector_.task_runner());
  }

  // Always return true. If we see errors during message processing, we will
  // explicitly call Connector::RaiseError() to disconnect the message pipe.
  return true;
}

bool MultiplexRouter::OnPeerAssociatedEndpointClosed(
    InterfaceId id,
    const base::Optional<DisconnectReason>& reason) {
  MayAutoLock locker(&lock_);
  InterfaceEndpoint* endpoint = FindOrInsertEndpoint(id, nullptr);

  if (reason)
    endpoint->set_disconnect_reason(reason);

  // It is possible that this endpoint has been set as peer closed. That is
  // because when the message pipe is closed, all the endpoints are updated with
  // PEER_ENDPOINT_CLOSED. We continue to process remaining tasks in the queue,
  // as long as there are refs keeping the router alive. If there is a
  // PeerAssociatedEndpointClosedEvent control message in the queue, we will get
  // here and see that the endpoint has been marked as peer closed.
  if (!endpoint->peer_closed()) {
    if (endpoint->client())
      tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));
    UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  }

  // No need to trigger a ProcessTasks() because it is already on the stack.

  return true;
}

bool MultiplexRouter::WaitForFlushToComplete(ScopedMessagePipeHandle pipe) {
  // If this MultiplexRouter has an associated interface on some task runner
  // other than the primary interface's task runner, it is possible to process
  // incoming control messages on that task runner. We don't support this
  // control message on anything but the main interface though.
  if (!task_runner_->RunsTasksInCurrentSequence())
    return false;

  flush_pipe_watcher_.emplace(FROM_HERE, SimpleWatcher::ArmingPolicy::MANUAL,
                              task_runner_);
  flush_pipe_watcher_->Watch(
      pipe.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MultiplexRouter::OnFlushPipeSignaled, this));
  if (flush_pipe_watcher_->Arm() != MOJO_RESULT_OK) {
    // The peer must already be closed, so consider the flush to be complete.
    flush_pipe_watcher_.reset();
    return true;
  }

  active_flush_pipe_ = std::move(pipe);
  PauseInternal(/*must_resume_manually=*/false);
  return true;
}

void MultiplexRouter::OnPipeConnectionError(bool force_async_dispatch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<MultiplexRouter> protector(this);
  MayAutoLock locker(&lock_);

  encountered_error_ = true;

  // Calling UpdateEndpointStateMayRemove() may remove the corresponding value
  // from |endpoints_| and invalidate any iterator of |endpoints_|. Therefore,
  // copy the endpoint pointers to a vector and iterate over it instead.
  std::vector<scoped_refptr<InterfaceEndpoint>> endpoint_vector;
  endpoint_vector.reserve(endpoints_.size());
  for (const auto& pair : endpoints_)
    endpoint_vector.push_back(pair.second);

  for (const auto& endpoint : endpoint_vector) {
    if (endpoint->client())
      tasks_.push_back(Task::CreateNotifyErrorTask(endpoint.get()));

    UpdateEndpointStateMayRemove(endpoint.get(), PEER_ENDPOINT_CLOSED);
  }

  ClientCallBehavior call_behavior = ALLOW_DIRECT_CLIENT_CALLS;
  if (force_async_dispatch)
    call_behavior = NO_DIRECT_CLIENT_CALLS;
  else if (connector_.during_sync_handle_watcher_callback())
    call_behavior = ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES;

  ProcessTasks(call_behavior, connector_.task_runner());
}

void MultiplexRouter::OnFlushPipeSignaled(MojoResult result,
                                          const HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  flush_pipe_watcher_.reset();
  active_flush_pipe_.reset();

  // If there is not an explicit Pause waiting for a Resume, we can unpause.
  if (!must_resume_manually_)
    ResumeIncomingMethodCallProcessing();
}

void MultiplexRouter::PauseInternal(bool must_resume_manually) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  connector_.PauseIncomingMethodCallProcessing();

  MayAutoLock locker(&lock_);

  paused_ = true;
  for (auto& entry : endpoints_)
    entry.second->ResetSyncMessageSignal();

  // We do not want to override this to |false| if it's already |true|. If it's
  // ever |true|, that means there's been at least one explicit Pause call since
  // the last Resume and we must never unpause until at least one call to Resume
  // is made.
  must_resume_manually_ = must_resume_manually_ || must_resume_manually;
}

void MultiplexRouter::ProcessTasks(
    ClientCallBehavior client_call_behavior,
    base::SequencedTaskRunner* current_task_runner) {
  AssertLockAcquired();

  if (posted_to_process_tasks_)
    return;

  while (!tasks_.empty() && !paused_) {
    std::unique_ptr<Task> task(std::move(tasks_.front()));
    tasks_.pop_front();

    InterfaceId id = kInvalidInterfaceId;
    bool sync_message =
        task->IsMessageTask() && !task->message_wrapper.value().IsNull() &&
        task->message_wrapper.value().has_flag(Message::kFlagIsSync);
    if (sync_message) {
      id = task->message_wrapper.value().interface_id();
      auto& sync_message_queue = sync_message_tasks_[id];
      DCHECK_EQ(task.get(), sync_message_queue.front());
      sync_message_queue.pop_front();
    }

    bool processed =
        task->IsNotifyErrorTask()
            ? ProcessNotifyErrorTask(task.get(), client_call_behavior,
                                     current_task_runner)
            : ProcessIncomingMessage(&task->message_wrapper,
                                     client_call_behavior, current_task_runner);

    if (!processed) {
      if (sync_message) {
        auto& sync_message_queue = sync_message_tasks_[id];
        sync_message_queue.push_front(task.get());
      }
      tasks_.push_front(std::move(task));
      break;
    } else {
      if (sync_message) {
        auto iter = sync_message_tasks_.find(id);
        if (iter != sync_message_tasks_.end() && iter->second.empty())
          sync_message_tasks_.erase(iter);
      }
    }
  }
}

bool MultiplexRouter::ProcessFirstSyncMessageForEndpoint(InterfaceId id) {
  AssertLockAcquired();

  auto iter = sync_message_tasks_.find(id);
  if (iter == sync_message_tasks_.end())
    return false;

  if (paused_)
    return true;

  MultiplexRouter::Task* task = iter->second.front();
  iter->second.pop_front();

  DCHECK(task->IsMessageTask());
  MessageWrapper message_wrapper = std::move(task->message_wrapper);

  // Note: after this call, |task| and |iter| may be invalidated.
  bool processed = ProcessIncomingMessage(
      &message_wrapper, ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES, nullptr);
  DCHECK(processed);

  iter = sync_message_tasks_.find(id);
  if (iter == sync_message_tasks_.end())
    return false;

  if (iter->second.empty()) {
    sync_message_tasks_.erase(iter);
    return false;
  }

  return true;
}

bool MultiplexRouter::ProcessNotifyErrorTask(
    Task* task,
    ClientCallBehavior client_call_behavior,
    base::SequencedTaskRunner* current_task_runner) {
  DCHECK(!current_task_runner ||
         current_task_runner->RunsTasksInCurrentSequence());
  DCHECK(!paused_);

  AssertLockAcquired();
  InterfaceEndpoint* endpoint = task->endpoint_to_notify.get();
  if (!endpoint->client())
    return true;

  if (client_call_behavior != ALLOW_DIRECT_CLIENT_CALLS ||
      endpoint->task_runner() != current_task_runner) {
    MaybePostToProcessTasks(endpoint->task_runner());
    return false;
  }

  DCHECK(endpoint->task_runner()->RunsTasksInCurrentSequence());

  InterfaceEndpointClient* client = endpoint->client();
  base::Optional<DisconnectReason> disconnect_reason(
      endpoint->disconnect_reason());

  {
    // We must unlock before calling into |client| because it may call this
    // object within NotifyError(). Holding the lock will lead to deadlock.
    //
    // It is safe to call into |client| without the lock. Because |client| is
    // always accessed on the same sequence, including DetachEndpointClient().
    MayAutoUnlock unlocker(&lock_);
    client->NotifyError(disconnect_reason);
  }
  return true;
}

bool MultiplexRouter::ProcessIncomingMessage(
    MessageWrapper* message_wrapper,
    ClientCallBehavior client_call_behavior,
    base::SequencedTaskRunner* current_task_runner) {
  DCHECK(!current_task_runner ||
         current_task_runner->RunsTasksInCurrentSequence());
  DCHECK(!paused_);
  DCHECK(message_wrapper);
  AssertLockAcquired();

  const Message* message = &message_wrapper->value();
  if (message->IsNull()) {
    // This is a sync message and has been processed during sync handle
    // watching.
    return true;
  }

  if (PipeControlMessageHandler::IsPipeControlMessage(message)) {
    bool result = false;

    {
      MayAutoUnlock unlocker(&lock_);
      Message tmp_message =
          message_wrapper->DeserializeEndpointHandlesAndTake();
      result = !tmp_message.IsNull() &&
               control_message_handler_.Accept(&tmp_message);
    }

    if (!result)
      RaiseErrorInNonTestingMode();

    return true;
  }

  InterfaceId id = message->interface_id();
  DCHECK(IsValidInterfaceId(id));

  InterfaceEndpoint* endpoint = FindEndpoint(id);
  if (!endpoint || endpoint->closed())
    return true;

  if (!endpoint->client()) {
    // We need to wait until a client is attached in order to dispatch further
    // messages.
    return false;
  }

  bool can_direct_call;
  if (message->has_flag(Message::kFlagIsSync)) {
    can_direct_call = client_call_behavior != NO_DIRECT_CLIENT_CALLS &&
                      endpoint->task_runner()->RunsTasksInCurrentSequence();
  } else {
    can_direct_call = client_call_behavior == ALLOW_DIRECT_CLIENT_CALLS &&
                      endpoint->task_runner() == current_task_runner;
  }

  if (!can_direct_call) {
    MaybePostToProcessTasks(endpoint->task_runner());
    return false;
  }

  DCHECK(endpoint->task_runner()->RunsTasksInCurrentSequence());

  InterfaceEndpointClient* client = endpoint->client();
  bool result = false;
  {
    // We must unlock before calling into |client| because it may call this
    // object within HandleIncomingMessage(). Holding the lock will lead to
    // deadlock.
    //
    // It is safe to call into |client| without the lock. Because |client| is
    // always accessed on the same sequence, including DetachEndpointClient().
    MayAutoUnlock unlocker(&lock_);
    Message tmp_message = message_wrapper->DeserializeEndpointHandlesAndTake();
    result =
        !tmp_message.IsNull() && client->HandleIncomingMessage(&tmp_message);
  }
  if (!result)
    RaiseErrorInNonTestingMode();

  return true;
}

void MultiplexRouter::MaybePostToProcessTasks(
    base::SequencedTaskRunner* task_runner) {
  AssertLockAcquired();
  if (posted_to_process_tasks_)
    return;

  posted_to_process_tasks_ = true;
  posted_to_task_runner_ = task_runner;
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&MultiplexRouter::LockAndCallProcessTasks, this));
}

void MultiplexRouter::LockAndCallProcessTasks() {
  // There is no need to hold a ref to this class in this case because this is
  // always called from a bound callback, which holds a ref.
  MayAutoLock locker(&lock_);
  posted_to_process_tasks_ = false;
  scoped_refptr<base::SequencedTaskRunner> runner(
      std::move(posted_to_task_runner_));
  ProcessTasks(ALLOW_DIRECT_CLIENT_CALLS, runner.get());
}

void MultiplexRouter::UpdateEndpointStateMayRemove(
    InterfaceEndpoint* endpoint,
    EndpointStateUpdateType type) {
  if (type == ENDPOINT_CLOSED) {
    endpoint->set_closed();
  } else {
    endpoint->set_peer_closed();
    // If the interface endpoint is performing a sync watch, this makes sure
    // it is notified and eventually exits the sync watch.
    endpoint->SignalSyncMessageEvent();
  }
  if (endpoint->closed() && endpoint->peer_closed())
    endpoints_.erase(endpoint->id());
}

void MultiplexRouter::RaiseErrorInNonTestingMode() {
  AssertLockAcquired();
  if (!testing_mode_)
    RaiseError();
}

MultiplexRouter::InterfaceEndpoint* MultiplexRouter::FindOrInsertEndpoint(
    InterfaceId id,
    bool* inserted) {
  AssertLockAcquired();
  // Either |inserted| is nullptr or it points to a boolean initialized as
  // false.
  DCHECK(!inserted || !*inserted);

  InterfaceEndpoint* endpoint = FindEndpoint(id);
  if (!endpoint) {
    endpoint = new InterfaceEndpoint(this, id);
    endpoints_[id] = endpoint;
    if (inserted)
      *inserted = true;
  }

  return endpoint;
}

MultiplexRouter::InterfaceEndpoint* MultiplexRouter::FindEndpoint(
    InterfaceId id) {
  AssertLockAcquired();
  auto iter = endpoints_.find(id);
  return iter != endpoints_.end() ? iter->second.get() : nullptr;
}

void MultiplexRouter::AssertLockAcquired() {
#if DCHECK_IS_ON()
  if (lock_)
    lock_->AssertAcquired();
#endif
}

bool MultiplexRouter::InsertEndpointsForMessage(const Message& message) {
  if (!message.is_serialized())
    return true;

  uint32_t num_ids = message.payload_num_interface_ids();
  if (num_ids == 0)
    return true;

  const uint32_t* ids = message.payload_interface_ids();

  MayAutoLock locker(&lock_);
  for (uint32_t i = 0; i < num_ids; ++i) {
    // Message header validation already ensures that the IDs are valid and not
    // the primary ID.
    // The IDs are from the remote side and therefore their namespace bit is
    // supposed to be different than the value that this router would use.
    if (set_interface_id_namespace_bit_ ==
        HasInterfaceIdNamespaceBitSet(ids[i])) {
      return false;
    }

    // It is possible that the endpoint already exists even when the remote side
    // is well-behaved: it might have notified us that the peer endpoint has
    // closed.
    bool inserted = false;
    InterfaceEndpoint* endpoint = FindOrInsertEndpoint(ids[i], &inserted);
    if (endpoint->closed() || endpoint->handle_created())
      return false;
  }

  return true;
}

void MultiplexRouter::CloseEndpointsForMessage(const Message& message) {
  AssertLockAcquired();

  if (!message.is_serialized())
    return;

  uint32_t num_ids = message.payload_num_interface_ids();
  if (num_ids == 0)
    return;

  const uint32_t* ids = message.payload_interface_ids();
  for (uint32_t i = 0; i < num_ids; ++i) {
    InterfaceEndpoint* endpoint = FindEndpoint(ids[i]);
    // If the remote side maliciously sends the same interface ID in another
    // message which has been dispatched, we could get here with no endpoint
    // for the ID, a closed endpoint, or an endpoint with handle created.
    if (!endpoint || endpoint->closed() || endpoint->handle_created()) {
      RaiseErrorInNonTestingMode();
      continue;
    }

    UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);
    MayAutoUnlock unlocker(&lock_);
    control_message_proxy_.NotifyPeerEndpointClosed(ids[i], base::nullopt);
  }

  ProcessTasks(NO_DIRECT_CLIENT_CALLS, nullptr);
}

}  // namespace internal
}  // namespace mojo
