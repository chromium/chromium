// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_bootstrap.h"

#include <inttypes.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/common/task_annotator.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/typed_macros.h"
#include "ipc/ipc_channel.h"
#include "ipc/urgent_message_observer.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/features.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"
#include "mojo/public/cpp/bindings/pipe_control_message_handler.h"
#include "mojo/public/cpp/bindings/pipe_control_message_handler_delegate.h"
#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "mojo/public/cpp/bindings/sequence_local_sync_event_watcher.h"
#include "mojo/public/cpp/bindings/tracing_helpers.h"

namespace IPC {

class ChannelAssociatedGroupController;

namespace {

constinit thread_local bool off_sequence_binding_allowed = false;

BASE_FEATURE(kMojoChannelAssociatedSendUsesRunOrPostTask,
             "MojoChannelAssociatedSendUsesRunOrPostTask",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMojoChannelAssociatedCrashesOnSendError,
             "MojoChannelAssociatedCrashesOnSendError",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to track some internal Channel state in pursuit of message leaks.
//
// TODO(crbug.com/40563310): Remove this.
class ControllerMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  ControllerMemoryDumpProvider() {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "IPCChannel", nullptr);
  }

  ControllerMemoryDumpProvider(const ControllerMemoryDumpProvider&) = delete;
  ControllerMemoryDumpProvider& operator=(const ControllerMemoryDumpProvider&) =
      delete;

  ~ControllerMemoryDumpProvider() override {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        this);
  }

  void AddController(ChannelAssociatedGroupController* controller) {
    base::AutoLock lock(lock_);
    controllers_.insert(controller);
  }

  void RemoveController(ChannelAssociatedGroupController* controller) {
    base::AutoLock lock(lock_);
    controllers_.erase(controller);
  }

  // base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  base::Lock lock_;
  std::set<raw_ptr<ChannelAssociatedGroupController, SetExperimental>>
      controllers_;
};

ControllerMemoryDumpProvider& GetMemoryDumpProvider() {
  static base::NoDestructor<ControllerMemoryDumpProvider> provider;
  return *provider;
}

// Messages are grouped by this info when recording memory metrics.
struct MessageMemoryDumpInfo {
  MessageMemoryDumpInfo(const mojo::Message& message)
      : id(message.name()), profiler_tag(message.heap_profiler_tag()) {}
  MessageMemoryDumpInfo() = default;

  bool operator==(const MessageMemoryDumpInfo& other) const {
    return other.id == id && other.profiler_tag == profiler_tag;
  }

  uint32_t id = 0;
  const char* profiler_tag = nullptr;
};

struct MessageMemoryDumpInfoHash {
  size_t operator()(const MessageMemoryDumpInfo& info) const {
    return base::HashInts(
        info.id, info.profiler_tag ? base::FastHash(info.profiler_tag) : 0);
  }
};

class ScopedUrgentMessageNotification {
 public:
  explicit ScopedUrgentMessageNotification(
      UrgentMessageObserver* observer = nullptr)
      : observer_(observer) {
    if (observer_) {
      observer_->OnUrgentMessageReceived();
    }
  }

  ~ScopedUrgentMessageNotification() {
    if (observer_) {
      observer_->OnUrgentMessageProcessed();
    }
  }

  ScopedUrgentMessageNotification(ScopedUrgentMessageNotification&& other)
      : observer_(std::exchange(other.observer_, nullptr)) {}

  ScopedUrgentMessageNotification& operator=(
      ScopedUrgentMessageNotification&& other) {
    observer_ = std::exchange(other.observer_, nullptr);
    return *this;
  }

 private:
  raw_ptr<UrgentMessageObserver> observer_;
};

}  // namespace

class ChannelAssociatedGroupController
    : public mojo::AssociatedGroupController,
      public mojo::MessageReceiver,
      public mojo::PipeControlMessageHandlerDelegate {
 public:
  ChannelAssociatedGroupController(
      bool set_interface_id_namespace_bit,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner)
      : task_runner_(ipc_task_runner),
        proxy_task_runner_(proxy_task_runner),
        set_interface_id_namespace_bit_(set_interface_id_namespace_bit),
        dispatcher_(this),
        control_message_handler_(this),
        control_message_proxy_thunk_(this),
        control_message_proxy_(&control_message_proxy_thunk_) {
    control_message_handler_.SetDescription(
        "IPC::mojom::Bootstrap [primary] PipeControlMessageHandler");
    dispatcher_.SetValidator(std::make_unique<mojo::MessageHeaderValidator>(
        "IPC::mojom::Bootstrap [primary] MessageHeaderValidator"));

    GetMemoryDumpProvider().AddController(this);

    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ChannelAssociatedGroupController(const ChannelAssociatedGroupController&) =
      delete;
  ChannelAssociatedGroupController& operator=(
      const ChannelAssociatedGroupController&) = delete;

  size_t GetQueuedMessageCount() {
    base::AutoLock lock(outgoing_messages_lock_);
    return outgoing_messages_.size();
  }

  void GetTopQueuedMessageMemoryDumpInfo(MessageMemoryDumpInfo* info,
                                         size_t* count) {
    std::unordered_map<MessageMemoryDumpInfo, size_t, MessageMemoryDumpInfoHash>
        counts;
    std::pair<MessageMemoryDumpInfo, size_t> top_message_info_and_count = {
        MessageMemoryDumpInfo(), 0};
    base::AutoLock lock(outgoing_messages_lock_);
    for (const auto& message : outgoing_messages_) {
      auto it_and_inserted = counts.emplace(MessageMemoryDumpInfo(message), 0);
      it_and_inserted.first->second++;
      if (it_and_inserted.first->second > top_message_info_and_count.second)
        top_message_info_and_count = *it_and_inserted.first;
    }
    *info = top_message_info_and_count.first;
    *count = top_message_info_and_count.second;
  }

  void Pause() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(was_bound_or_message_sent_);
    CHECK(!paused_);
    paused_ = true;
  }

  void Unpause() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(was_bound_or_message_sent_);
    CHECK(paused_);
    paused_ = false;
  }

  void FlushOutgoingMessages() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(was_bound_or_message_sent_);

    std::vector<mojo::Message> outgoing_messages;
    {
      base::AutoLock lock(outgoing_messages_lock_);
      std::swap(outgoing_messages, outgoing_messages_);
    }

    for (auto& message : outgoing_messages)
      SendMessage(&message);
  }

  void Bind(mojo::ScopedMessagePipeHandle handle,
            mojo::PendingAssociatedRemote<mojom::Channel>* sender,
            mojo::PendingAssociatedReceiver<mojom::Channel>* receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    connector_ = std::make_unique<mojo::Connector>(
        std::move(handle), mojo::Connector::SINGLE_THREADED_SEND,
        "IPC Channel");
    connector_->set_incoming_receiver(&dispatcher_);
    connector_->set_connection_error_handler(
        base::BindOnce(&ChannelAssociatedGroupController::OnPipeError,
                       base::Unretained(this)));
    connector_->set_enforce_errors_from_incoming_receiver(false);

    // Don't let the Connector do any sort of queuing on our behalf. Individual
    // messages bound for the IPC::ChannelProxy thread (i.e. that vast majority
    // of messages received by this Connector) are already individually
    // scheduled for dispatch by ChannelProxy, so Connector's normal mode of
    // operation would only introduce a redundant scheduling step for most
    // messages.
    connector_->set_force_immediate_dispatch(true);

    mojo::InterfaceId sender_id, receiver_id;
    if (set_interface_id_namespace_bit_) {
      sender_id = 1 | mojo::kInterfaceIdNamespaceMask;
      receiver_id = 1;
    } else {
      sender_id = 1;
      receiver_id = 1 | mojo::kInterfaceIdNamespaceMask;
    }

    {
      base::AutoLock locker(lock_);
      Endpoint* sender_endpoint = new Endpoint(this, sender_id);
      Endpoint* receiver_endpoint = new Endpoint(this, receiver_id);
      endpoints_.insert({ sender_id, sender_endpoint });
      endpoints_.insert({ receiver_id, receiver_endpoint });
      sender_endpoint->set_handle_created();
      receiver_endpoint->set_handle_created();
    }

    mojo::ScopedInterfaceEndpointHandle sender_handle =
        CreateScopedInterfaceEndpointHandle(sender_id);
    mojo::ScopedInterfaceEndpointHandle receiver_handle =
        CreateScopedInterfaceEndpointHandle(receiver_id);

    *sender = mojo::PendingAssociatedRemote<mojom::Channel>(
        std::move(sender_handle), 0);
    *receiver = mojo::PendingAssociatedReceiver<mojom::Channel>(
        std::move(receiver_handle));

    if (!was_bound_or_message_sent_) {
      was_bound_or_message_sent_ = true;
      DETACH_FROM_SEQUENCE(sequence_checker_);
    }
  }

  void StartReceiving() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(was_bound_or_message_sent_);
    connector_->StartReceiving(task_runner_);
  }

  void ShutDown() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    shut_down_ = true;
    if (connector_)
      connector_->CloseMessagePipe();
    OnPipeError();
    connector_.reset();

    base::AutoLock lock(outgoing_messages_lock_);
    outgoing_messages_.clear();
  }

  // mojo::AssociatedGroupController:
  mojo::InterfaceId AssociateInterface(
      mojo::ScopedInterfaceEndpointHandle handle_to_send) override {
    if (!handle_to_send.pending_association())
      return mojo::kInvalidInterfaceId;

    uint32_t id = 0;
    {
      base::AutoLock locker(lock_);
      do {
        if (next_interface_id_ >= mojo::kInterfaceIdNamespaceMask)
          next_interface_id_ = 2;
        id = next_interface_id_++;
        if (set_interface_id_namespace_bit_)
          id |= mojo::kInterfaceIdNamespaceMask;
      } while (base::Contains(endpoints_, id));

      Endpoint* endpoint = new Endpoint(this, id);
      if (encountered_error_)
        endpoint->set_peer_closed();
      endpoint->set_handle_created();
      endpoints_.insert({id, endpoint});
    }

    if (!NotifyAssociation(&handle_to_send, id)) {
      // The peer handle of |handle_to_send|, which is supposed to join this
      // associated group, has been closed.
      {
        base::AutoLock locker(lock_);
        Endpoint* endpoint = FindEndpoint(id);
        if (endpoint)
          MarkClosedAndMaybeRemove(endpoint);
      }

      control_message_proxy_.NotifyPeerEndpointClosed(
          id, handle_to_send.disconnect_reason());
    }
    return id;
  }

  mojo::ScopedInterfaceEndpointHandle CreateLocalEndpointHandle(
      mojo::InterfaceId id) override {
    if (!mojo::IsValidInterfaceId(id))
      return mojo::ScopedInterfaceEndpointHandle();

    // Unless it is the primary ID, |id| is from the remote side and therefore
    // its namespace bit is supposed to be different than the value that this
    // router would use.
    if (!mojo::IsPrimaryInterfaceId(id) &&
        set_interface_id_namespace_bit_ ==
            mojo::HasInterfaceIdNamespaceBitSet(id)) {
      return mojo::ScopedInterfaceEndpointHandle();
    }

    base::AutoLock locker(lock_);
    bool inserted = false;
    Endpoint* endpoint = FindOrInsertEndpoint(id, &inserted);
    if (inserted) {
      DCHECK(!endpoint->handle_created());
      if (encountered_error_)
        endpoint->set_peer_closed();
    } else {
      if (endpoint->handle_created())
        return mojo::ScopedInterfaceEndpointHandle();
    }

    endpoint->set_handle_created();
    return CreateScopedInterfaceEndpointHandle(id);
  }

  void CloseEndpointHandle(
      mojo::InterfaceId id,
      const std::optional<mojo::DisconnectReason>& reason) override {
    if (!mojo::IsValidInterfaceId(id))
      return;
    {
      base::AutoLock locker(lock_);
      DCHECK(base::Contains(endpoints_, id));
      Endpoint* endpoint = endpoints_[id].get();
      DCHECK(!endpoint->client());
      DCHECK(!endpoint->closed());
      MarkClosedAndMaybeRemove(endpoint);
    }

    if (!mojo::IsPrimaryInterfaceId(id) || reason)
      control_message_proxy_.NotifyPeerEndpointClosed(id, reason);
  }

  void NotifyLocalEndpointOfPeerClosure(mojo::InterfaceId id) override {
    if (!task_runner_->RunsTasksInCurrentSequence()) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ChannelAssociatedGroupController::
                                        NotifyLocalEndpointOfPeerClosure,
                                    base::WrapRefCounted(this), id));
      return;
    }
    OnPeerAssociatedEndpointClosed(id, std::nullopt);
  }

  mojo::InterfaceEndpointController* AttachEndpointClient(
      const mojo::ScopedInterfaceEndpointHandle& handle,
      mojo::InterfaceEndpointClient* client,
      scoped_refptr<base::SequencedTaskRunner> runner) override {
    const mojo::InterfaceId id = handle.id();

    DCHECK(mojo::IsValidInterfaceId(id));
    DCHECK(client);

    base::AutoLock locker(lock_);
    DCHECK(base::Contains(endpoints_, id));

    Endpoint* endpoint = endpoints_[id].get();
    endpoint->AttachClient(client, std::move(runner));

    if (endpoint->peer_closed())
      NotifyEndpointOfError(endpoint, true /* force_async */);

    return endpoint;
  }

  void DetachEndpointClient(
      const mojo::ScopedInterfaceEndpointHandle& handle) override {
    const mojo::InterfaceId id = handle.id();

    DCHECK(mojo::IsValidInterfaceId(id));

    base::AutoLock locker(lock_);
    DCHECK(base::Contains(endpoints_, id));

    Endpoint* endpoint = endpoints_[id].get();
    endpoint->DetachClient();
  }

  void RaiseError() override {
    // We ignore errors on channel endpoints, leaving the pipe open. There are
    // good reasons for this:
    //
    //   * We should never close a channel endpoint in either process as long as
    //     the child process is still alive. The child's endpoint should only be
    //     closed implicitly by process death, and the browser's endpoint should
    //     only be closed after the child process is confirmed to be dead. Crash
    //     reporting logic in Chrome relies on this behavior in order to do the
    //     right thing.
    //
    //   * There are two interesting conditions under which RaiseError() can be
    //     implicitly reached: an incoming message fails validation, or the
    //     local endpoint drops a response callback without calling it.
    //
    //   * In the validation case, we also report the message as bad, and this
    //     will imminently trigger the common bad-IPC path in the browser,
    //     causing the browser to kill the offending renderer.
    //
    //   * In the dropped response callback case, the net result of ignoring the
    //     issue is generally innocuous. While indicative of programmer error,
    //     it's not a severe failure and is already covered by separate DCHECKs.
    //
    // See https://crbug.com/861607 for additional discussion.
  }

  bool PrefersSerializedMessages() override { return true; }

  void SetUrgentMessageObserver(UrgentMessageObserver* observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!was_bound_or_message_sent_);
    urgent_message_observer_ = observer;
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

 private:
  class Endpoint;
  class ControlMessageProxyThunk;
  friend class Endpoint;
  friend class ControlMessageProxyThunk;

  // MessageWrapper objects are always destroyed under the controller's lock. On
  // destruction, if the message it wrappers contains
  // ScopedInterfaceEndpointHandles (which cannot be destructed under the
  // controller's lock), the wrapper unlocks to clean them up.
  class MessageWrapper {
   public:
    MessageWrapper() = default;

    MessageWrapper(ChannelAssociatedGroupController* controller,
                   mojo::Message message)
        : controller_(controller), value_(std::move(message)) {}

    MessageWrapper(MessageWrapper&& other)
        : controller_(other.controller_), value_(std::move(other.value_)) {}

    MessageWrapper(const MessageWrapper&) = delete;
    MessageWrapper& operator=(const MessageWrapper&) = delete;

    ~MessageWrapper() {
      if (value_.associated_endpoint_handles()->empty())
        return;

      controller_->lock_.AssertAcquired();
      {
        base::AutoUnlock unlocker(controller_->lock_);
        value_.mutable_associated_endpoint_handles()->clear();
      }
    }

    MessageWrapper& operator=(MessageWrapper&& other) {
      controller_ = other.controller_;
      value_ = std::move(other.value_);
      return *this;
    }

    bool HasRequestId(uint64_t request_id) {
      return !value_.IsNull() && value_.version() >= 1 &&
             value_.header_v1()->request_id == request_id;
    }

    mojo::Message& value() { return value_; }

   private:
    raw_ptr<ChannelAssociatedGroupController> controller_ = nullptr;
    mojo::Message value_;
  };

  class Endpoint : public base::RefCountedThreadSafe<Endpoint>,
                   public mojo::InterfaceEndpointController {
   public:
    Endpoint(ChannelAssociatedGroupController* controller, mojo::InterfaceId id)
        : controller_(controller), id_(id) {}

    Endpoint(const Endpoint&) = delete;
    Endpoint& operator=(const Endpoint&) = delete;

    mojo::InterfaceId id() const { return id_; }

    bool closed() const {
      controller_->lock_.AssertAcquired();
      return closed_;
    }

    void set_closed() {
      controller_->lock_.AssertAcquired();
      closed_ = true;
    }

    bool peer_closed() const {
      controller_->lock_.AssertAcquired();
      return peer_closed_;
    }

    void set_peer_closed() {
      controller_->lock_.AssertAcquired();
      peer_closed_ = true;
    }

    bool handle_created() const {
      controller_->lock_.AssertAcquired();
      return handle_created_;
    }

    void set_handle_created() {
      controller_->lock_.AssertAcquired();
      handle_created_ = true;
    }

    const std::optional<mojo::DisconnectReason>& disconnect_reason() const {
      return disconnect_reason_;
    }

    void set_disconnect_reason(
        const std::optional<mojo::DisconnectReason>& disconnect_reason) {
      disconnect_reason_ = disconnect_reason;
    }

    base::SequencedTaskRunner* task_runner() const {
      return task_runner_.get();
    }

    bool was_bound_off_sequence() const { return was_bound_off_sequence_; }

    mojo::InterfaceEndpointClient* client() const {
      controller_->lock_.AssertAcquired();
      return client_;
    }

    void AttachClient(mojo::InterfaceEndpointClient* client,
                      scoped_refptr<base::SequencedTaskRunner> runner) {
      controller_->lock_.AssertAcquired();
      DCHECK(!client_);
      DCHECK(!closed_);

      task_runner_ = std::move(runner);
      client_ = client;

      if (off_sequence_binding_allowed) {
        was_bound_off_sequence_ = true;
      }
    }

    void DetachClient() {
      controller_->lock_.AssertAcquired();
      DCHECK(client_);
      DCHECK(!closed_);

      task_runner_ = nullptr;
      client_ = nullptr;
      sync_watcher_.reset();
    }

    std::optional<uint32_t> EnqueueSyncMessage(MessageWrapper message) {
      controller_->lock_.AssertAcquired();
      if (exclusive_wait_ && exclusive_wait_->TryFulfillingWith(message)) {
        exclusive_wait_ = nullptr;
        return std::nullopt;
      }

      uint32_t id = GenerateSyncMessageId();
      sync_messages_.emplace_back(id, std::move(message));
      SignalSyncMessageEvent();
      return id;
    }

    void SignalSyncMessageEvent() {
      controller_->lock_.AssertAcquired();

      if (sync_watcher_)
        sync_watcher_->SignalEvent();
    }

    MessageWrapper PopSyncMessage(uint32_t id) {
      controller_->lock_.AssertAcquired();
      if (sync_messages_.empty() || sync_messages_.front().first != id)
        return MessageWrapper();
      MessageWrapper message = std::move(sync_messages_.front().second);
      sync_messages_.pop_front();
      return message;
    }

    // mojo::InterfaceEndpointController:
    bool SendMessage(mojo::Message* message) override {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      message->set_interface_id(id_);
      return controller_->SendMessage(message);
    }

    void AllowWokenUpBySyncWatchOnSameThread() override {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());

      EnsureSyncWatcherExists();
      sync_watcher_->AllowWokenUpBySyncWatchOnSameSequence();
    }

    bool SyncWatch(const bool& should_stop) override {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());

      // It's not legal to make sync calls from the primary endpoint's thread,
      // and in fact they must only happen from the proxy task runner.
      DCHECK(!controller_->task_runner_->BelongsToCurrentThread());
      DCHECK(controller_->proxy_task_runner_->BelongsToCurrentThread());

      EnsureSyncWatcherExists();
      {
        base::AutoLock locker(controller_->lock_);
        if (peer_closed_) {
          SignalSyncMessageEvent();
        }
      }
      return sync_watcher_->SyncWatch(&should_stop);
    }

    MessageWrapper WaitForIncomingSyncReply(uint64_t request_id) {
      std::optional<ExclusiveSyncWait> wait;
      {
        base::AutoLock lock(controller_->lock_);
        for (auto& [id, message] : sync_messages_) {
          if (message.HasRequestId(request_id)) {
            return std::move(message);
          }
        }

        DCHECK(!exclusive_wait_);
        wait.emplace(request_id);
        exclusive_wait_ = &wait.value();
      }

      wait->event.Wait();
      return std::move(wait->message);
    }

    bool SyncWatchExclusive(uint64_t request_id) override {
      MessageWrapper message = WaitForIncomingSyncReply(request_id);
      if (message.value().IsNull() || !client_) {
        return false;
      }

      if (!client_->HandleIncomingMessage(&message.value())) {
        base::AutoLock locker(controller_->lock_);
        controller_->RaiseError();
        return false;
      }

      return true;
    }

    void RegisterExternalSyncWaiter(uint64_t request_id) override {}

   private:
    friend class base::RefCountedThreadSafe<Endpoint>;

    ~Endpoint() override {
      controller_->lock_.AssertAcquired();
      DCHECK(!client_);
      DCHECK(closed_);
      DCHECK(peer_closed_);
      DCHECK(!sync_watcher_);
      if (exclusive_wait_) {
        exclusive_wait_->event.Signal();
      }
    }

    void OnSyncMessageEventReady() {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());

      // SUBTLE: The order of these scoped_refptrs matters.
      // `controller_keepalive` MUST outlive `keepalive` because the Endpoint
      // holds raw pointer to the AssociatedGroupController.
      scoped_refptr<AssociatedGroupController> controller_keepalive(
          controller_.get());
      scoped_refptr<Endpoint> keepalive(this);
      base::AutoLock locker(controller_->lock_);
      bool more_to_process = false;
      if (!sync_messages_.empty()) {
        MessageWrapper message_wrapper =
            std::move(sync_messages_.front().second);
        sync_messages_.pop_front();

        bool dispatch_succeeded;
        mojo::InterfaceEndpointClient* client = client_;
        {
          base::AutoUnlock unlocker(controller_->lock_);
          dispatch_succeeded =
              client->HandleIncomingMessage(&message_wrapper.value());
        }

        if (!sync_messages_.empty())
          more_to_process = true;

        if (!dispatch_succeeded)
          controller_->RaiseError();
      }

      if (!more_to_process)
        sync_watcher_->ResetEvent();

      // If there are no queued sync messages and the peer has closed, there
      // there won't be incoming sync messages in the future. If any
      // SyncWatch() calls are on the stack for this endpoint, resetting the
      // watcher will allow them to exit as the stack undwinds.
      if (!more_to_process && peer_closed_)
        sync_watcher_.reset();
    }

    void EnsureSyncWatcherExists() {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      if (sync_watcher_)
        return;

      base::AutoLock locker(controller_->lock_);
      sync_watcher_ = std::make_unique<mojo::SequenceLocalSyncEventWatcher>(
          base::BindRepeating(&Endpoint::OnSyncMessageEventReady,
                              base::Unretained(this)));
      if (peer_closed_ || !sync_messages_.empty())
        SignalSyncMessageEvent();
    }

    uint32_t GenerateSyncMessageId() {
      // Overflow is fine.
      uint32_t id = next_sync_message_id_++;
      DCHECK(sync_messages_.empty() || sync_messages_.front().first != id);
      return id;
    }

    // Tracks the state of a pending sync wait which excludes all other incoming
    // IPC on the waiting thread.
    struct ExclusiveSyncWait {
      explicit ExclusiveSyncWait(uint64_t request_id)
          : request_id(request_id) {}
      ~ExclusiveSyncWait() = default;

      bool TryFulfillingWith(MessageWrapper& wrapper) {
        if (!wrapper.HasRequestId(request_id)) {
          return false;
        }

        message = std::move(wrapper);
        event.Signal();
        return true;
      }

      uint64_t request_id;
      base::WaitableEvent event;
      MessageWrapper message;
    };

    const raw_ptr<ChannelAssociatedGroupController> controller_;
    const mojo::InterfaceId id_;

    bool closed_ = false;
    bool peer_closed_ = false;
    bool handle_created_ = false;
    bool was_bound_off_sequence_ = false;
    std::optional<mojo::DisconnectReason> disconnect_reason_;
    raw_ptr<mojo::InterfaceEndpointClient> client_ = nullptr;
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    std::unique_ptr<mojo::SequenceLocalSyncEventWatcher> sync_watcher_;
    base::circular_deque<std::pair<uint32_t, MessageWrapper>> sync_messages_;
    raw_ptr<ExclusiveSyncWait> exclusive_wait_ = nullptr;
    uint32_t next_sync_message_id_ = 0;
  };

  class ControlMessageProxyThunk : public MessageReceiver {
   public:
    explicit ControlMessageProxyThunk(
        ChannelAssociatedGroupController* controller)
        : controller_(controller) {}

    ControlMessageProxyThunk(const ControlMessageProxyThunk&) = delete;
    ControlMessageProxyThunk& operator=(const ControlMessageProxyThunk&) =
        delete;

   private:
    // MessageReceiver:
    bool Accept(mojo::Message* message) override {
      return controller_->SendMessage(message);
    }

    raw_ptr<ChannelAssociatedGroupController> controller_;
  };

  ~ChannelAssociatedGroupController() override {
    DCHECK(!connector_);

    base::AutoLock locker(lock_);
    for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
      Endpoint* endpoint = iter->second.get();
      ++iter;

      if (!endpoint->closed()) {
        // This happens when a NotifyPeerEndpointClosed message been received,
        // but the interface ID hasn't been used to create local endpoint
        // handle.
        DCHECK(!endpoint->client());
        DCHECK(endpoint->peer_closed());
        MarkClosed(endpoint);
      } else {
        MarkPeerClosed(endpoint);
      }
    }
    endpoints_.clear();

    GetMemoryDumpProvider().RemoveController(this);
  }

  bool SendMessage(mojo::Message* message) {
    DCHECK(message->heap_profiler_tag());
    if (task_runner_->BelongsToCurrentThread()) {
      return SendMessageOnSequence(message);
    }

    // PostTask (or RunOrPostTask) so that `message` is sent after messages from
    // tasks that are already queued (e.g. by `IPC::ChannelProxy::Send`).
    auto callback = base::BindOnce(
        &ChannelAssociatedGroupController::SendMessageOnSequenceViaTask, this,
        std::move(*message));
    if (base::FeatureList::IsEnabled(
            kMojoChannelAssociatedSendUsesRunOrPostTask)) {
      task_runner_->RunOrPostTask(base::subtle::RunOrPostTaskPassKey(),
                                  FROM_HERE, std::move(callback));
    } else {
      task_runner_->PostTask(FROM_HERE, std::move(callback));
    }

    return true;
  }

  bool SendMessageOnSequence(mojo::Message* message) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    was_bound_or_message_sent_ = true;

    if (!connector_ || paused_) {
      if (!shut_down_) {
        base::AutoLock lock(outgoing_messages_lock_);
        outgoing_messages_.emplace_back(std::move(*message));
      }
      return true;
    }
    MojoResult result = connector_->AcceptAndGetResult(message);

    // TODO(crbug.com/40944462): Remove this code when the cause of skipped
    // messages with MojoChannelAssociatedSendUsesRunOrPostTask is understood,
    // or no later than November 2024.
    if (result != MOJO_RESULT_OK && !connector_->encountered_error() &&
        base::FeatureList::IsEnabled(
            kMojoChannelAssociatedCrashesOnSendError)) {
      // Crash when sending a message fails and `connector_` can send more
      // messages, as that breaks the assumption that messages are received in
      // the order they were sent. Note: `connector_` cannot send more messages
      // when `encountered_error()` is true.
      mojo::debug::ScopedMessageErrorCrashKey crash_key(
          base::StringPrintf("SendMessage failed with error %d", result));
      CHECK(false);
    }

    return result == MOJO_RESULT_OK;
  }

  void SendMessageOnSequenceViaTask(mojo::Message message) {
    if (!SendMessageOnSequence(&message)) {
      RaiseError();
    }
  }

  void OnPipeError() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // We keep |this| alive here because it's possible for the notifications
    // below to release all other references.
    scoped_refptr<ChannelAssociatedGroupController> keepalive(this);

    base::AutoLock locker(lock_);
    encountered_error_ = true;

    std::vector<uint32_t> endpoints_to_remove;
    std::vector<scoped_refptr<Endpoint>> endpoints_to_notify;
    for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
      Endpoint* endpoint = iter->second.get();
      ++iter;

      if (endpoint->client()) {
        endpoints_to_notify.push_back(endpoint);
      }

      if (MarkPeerClosed(endpoint)) {
        endpoints_to_remove.push_back(endpoint->id());
      }
    }

    for (auto& endpoint : endpoints_to_notify) {
      // Because a notification may in turn detach any endpoint, we have to
      // check each client again here.
      if (endpoint->client())
        NotifyEndpointOfError(endpoint.get(), false /* force_async */);
    }

    for (uint32_t id : endpoints_to_remove) {
      endpoints_.erase(id);
    }
  }

  void NotifyEndpointOfError(Endpoint* endpoint, bool force_async)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    DCHECK(endpoint->task_runner() && endpoint->client());
    if (endpoint->task_runner()->RunsTasksInCurrentSequence() && !force_async) {
      mojo::InterfaceEndpointClient* client = endpoint->client();
      std::optional<mojo::DisconnectReason> reason(
          endpoint->disconnect_reason());

      base::AutoUnlock unlocker(lock_);
      client->NotifyError(reason);
    } else {
      endpoint->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&ChannelAssociatedGroupController::
                             NotifyEndpointOfErrorOnEndpointThread,
                         this, endpoint->id(),
                         // This is safe as `endpoint` is verified to be in
                         // `endpoints_` (a map with ownership) before use.
                         base::UnsafeDangling(endpoint)));
    }
  }

  // `endpoint` might be a dangling ptr and must be checked before dereference.
  void NotifyEndpointOfErrorOnEndpointThread(mojo::InterfaceId id,
                                             MayBeDangling<Endpoint> endpoint) {
    base::AutoLock locker(lock_);
    auto iter = endpoints_.find(id);
    if (iter == endpoints_.end() || iter->second.get() != endpoint)
      return;
    if (!endpoint->client())
      return;

    DCHECK(endpoint->task_runner()->RunsTasksInCurrentSequence());
    NotifyEndpointOfError(endpoint, false /* force_async */);
  }

  // Marks `endpoint` as closed and returns true if and only if its peer was
  // also already closed.
  bool MarkClosed(Endpoint* endpoint) EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    endpoint->set_closed();
    return endpoint->peer_closed();
  }

  // Marks `endpoint` as having a closed peer and returns true if and only if
  // `endpoint` itself was also already closed.
  bool MarkPeerClosed(Endpoint* endpoint) EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    endpoint->set_peer_closed();
    endpoint->SignalSyncMessageEvent();
    return endpoint->closed();
  }

  void MarkClosedAndMaybeRemove(Endpoint* endpoint)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    if (MarkClosed(endpoint)) {
      endpoints_.erase(endpoint->id());
    }
  }

  void MarkPeerClosedAndMaybeRemove(Endpoint* endpoint)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    if (MarkPeerClosed(endpoint)) {
      endpoints_.erase(endpoint->id());
    }
  }

  Endpoint* FindOrInsertEndpoint(mojo::InterfaceId id, bool* inserted)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    DCHECK(!inserted || !*inserted);

    Endpoint* endpoint = FindEndpoint(id);
    if (!endpoint) {
      endpoint = new Endpoint(this, id);
      endpoints_.insert({id, endpoint});
      if (inserted)
        *inserted = true;
    }
    return endpoint;
  }

  Endpoint* FindEndpoint(mojo::InterfaceId id) EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    auto iter = endpoints_.find(id);
    return iter != endpoints_.end() ? iter->second.get() : nullptr;
  }

  // mojo::MessageReceiver:
  bool Accept(mojo::Message* message) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!message->DeserializeAssociatedEndpointHandles(this))
      return false;

    if (mojo::PipeControlMessageHandler::IsPipeControlMessage(message))
      return control_message_handler_.Accept(message);

    mojo::InterfaceId id = message->interface_id();
    if (!mojo::IsValidInterfaceId(id))
      return false;

    base::ReleasableAutoLock locker(&lock_);
    Endpoint* endpoint = FindEndpoint(id);
    if (!endpoint)
      return true;

    mojo::InterfaceEndpointClient* client = endpoint->client();
    if (!client || !endpoint->task_runner()->RunsTasksInCurrentSequence()) {
      // The ChannelProxy for this channel is bound to `proxy_task_runner_` and
      // by default legacy IPCs must dispatch to either the IO thread or the
      // proxy task runner. We generally impose the same constraint on
      // associated interface endpoints so that FIFO can be guaranteed across
      // all interfaces without stalling any of them to wait for a pending
      // endpoint to be bound.
      //
      // This allows us to assume that if an endpoint is not yet bound when we
      // receive a message targeting it, it *will* be bound on the proxy task
      // runner by the time a newly posted task runs there. Hence we simply post
      // a hopeful dispatch task to that task runner.
      //
      // As it turns out, there are even some instances of endpoints binding to
      // alternative (non-IO-thread, non-proxy) task runners, but still
      // ultimately relying on the fact that we schedule their messages on the
      // proxy task runner. So even if the endpoint is already bound, we
      // default to scheduling it on the proxy task runner as long as it's not
      // bound specifically to the IO task runner.
      // TODO(rockot): Try to sort out these cases and maybe eliminate them.
      //
      // Finally, it's also possible that an endpoint was bound to an
      // alternative task runner and it really does want its messages to
      // dispatch there. In that case `was_bound_off_sequence()` will be true to
      // signal that we should really use that task runner.
      const scoped_refptr<base::SequencedTaskRunner> task_runner =
          client && endpoint->was_bound_off_sequence()
              ? endpoint->task_runner()
              : proxy_task_runner_.get();

      ScopedUrgentMessageNotification scoped_urgent_message_notification(
          message->has_flag(mojo::Message::kFlagIsUrgent)
              ? urgent_message_observer_
              : nullptr);

      if (message->has_flag(mojo::Message::kFlagIsSync)) {
        MessageWrapper message_wrapper(this, std::move(*message));
        // Sync messages may need to be handled by the endpoint if it's blocking
        // on a sync reply. We pass ownership of the message to the endpoint's
        // sync message queue. If the endpoint was blocking, it will dequeue the
        // message and dispatch it. Otherwise the posted |AcceptSyncMessage()|
        // call will dequeue the message and dispatch it.
        std::optional<uint32_t> message_id =
            endpoint->EnqueueSyncMessage(std::move(message_wrapper));
        if (message_id) {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &ChannelAssociatedGroupController::AcceptSyncMessage, this,
                  id, *message_id,
                  std::move(scoped_urgent_message_notification)));
        }
        return true;
      }

      // If |task_runner| has been torn down already, this PostTask will fail
      // and destroy |message|. That operation may need to in turn destroy
      // in-transit associated endpoints and thus acquire |lock_|. We no longer
      // need the lock to be held now, so we can release it before the PostTask.
      {
        // Grab interface name from |client| before releasing the lock to ensure
        // that |client| is safe to access.
        base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(
            client ? client->interface_name() : "unknown interface");
        locker.Release();
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ChannelAssociatedGroupController::AcceptOnEndpointThread, this,
                std::move(*message),
                std::move(scoped_urgent_message_notification)));
      }
      return true;
    }

    locker.Release();
    // It's safe to access |client| here without holding a lock, because this
    // code runs on a proxy thread and |client| can't be destroyed from any
    // thread.
    return client->HandleIncomingMessage(message);
  }

  void AcceptOnEndpointThread(
      mojo::Message message,
      ScopedUrgentMessageNotification scoped_urgent_message_notification) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mojom"),
                 "ChannelAssociatedGroupController::AcceptOnEndpointThread");

    mojo::InterfaceId id = message.interface_id();
    DCHECK(mojo::IsValidInterfaceId(id) && !mojo::IsPrimaryInterfaceId(id));

    base::AutoLock locker(lock_);
    Endpoint* endpoint = FindEndpoint(id);
    if (!endpoint)
      return;

    mojo::InterfaceEndpointClient* client = endpoint->client();
    if (!client)
      return;

    if (!endpoint->task_runner()->RunsTasksInCurrentSequence() &&
        !proxy_task_runner_->RunsTasksInCurrentSequence()) {
      return;
    }

    // TODO(altimin): This event is temporarily kept as a debug fallback. Remove
    // it once the new implementation proves to be stable.
    TRACE_EVENT(
        TRACE_DISABLED_BY_DEFAULT("mojom"),
        // Using client->interface_name() is safe here because this is a static
        // string defined for each mojo interface.
        perfetto::StaticString(client->interface_name()),
        [&](perfetto::EventContext& ctx) {
          static const uint8_t* toplevel_flow_enabled =
              TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("toplevel.flow");
          if (!*toplevel_flow_enabled)
            return;

          perfetto::Flow::Global(message.GetTraceId())(ctx);
        });

    // Sync messages should never make their way to this method.
    DCHECK(!message.has_flag(mojo::Message::kFlagIsSync));

    bool result = false;
    {
      base::AutoUnlock unlocker(lock_);
      result = client->HandleIncomingMessage(&message);
    }

    if (!result)
      RaiseError();
  }

  void AcceptSyncMessage(
      mojo::InterfaceId interface_id,
      uint32_t message_id,
      ScopedUrgentMessageNotification scoped_urgent_message_notification) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mojom"),
                 "ChannelAssociatedGroupController::AcceptSyncMessage");

    base::AutoLock locker(lock_);
    Endpoint* endpoint = FindEndpoint(interface_id);
    if (!endpoint)
      return;

    // Careful, if the endpoint is detached its members are cleared. Check for
    // that before dereferencing.
    mojo::InterfaceEndpointClient* client = endpoint->client();
    if (!client)
      return;

    if (!endpoint->task_runner()->RunsTasksInCurrentSequence() &&
        !proxy_task_runner_->RunsTasksInCurrentSequence()) {
      return;
    }

    // Using client->interface_name() is safe here because this is a static
    // string defined for each mojo interface.
    TRACE_EVENT0("mojom", client->interface_name());
    MessageWrapper message_wrapper = endpoint->PopSyncMessage(message_id);

    // The message must have already been dequeued by the endpoint waking up
    // from a sync wait. Nothing to do.
    if (message_wrapper.value().IsNull())
      return;

    bool result = false;
    {
      base::AutoUnlock unlocker(lock_);
      result = client->HandleIncomingMessage(&message_wrapper.value());
    }

    if (!result)
      RaiseError();
  }

  // mojo::PipeControlMessageHandlerDelegate:
  bool OnPeerAssociatedEndpointClosed(
      mojo::InterfaceId id,
      const std::optional<mojo::DisconnectReason>& reason) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    scoped_refptr<ChannelAssociatedGroupController> keepalive(this);
    base::AutoLock locker(lock_);
    scoped_refptr<Endpoint> endpoint = FindOrInsertEndpoint(id, nullptr);
    if (reason)
      endpoint->set_disconnect_reason(reason);
    if (!endpoint->peer_closed()) {
      if (endpoint->client())
        NotifyEndpointOfError(endpoint.get(), false /* force_async */);
      MarkPeerClosedAndMaybeRemove(endpoint.get());
    }

    return true;
  }

  bool WaitForFlushToComplete(
      mojo::ScopedMessagePipeHandle flush_pipe) override {
    // We don't support async flushing on the IPC Channel pipe.
    return false;
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> proxy_task_runner_;
  const bool set_interface_id_namespace_bit_;

  // Ensures sequenced access to members below.
  SEQUENCE_CHECKER(sequence_checker_);

  // Whether `Bind()` or `SendMessageOnSequence()` was called.
  // `sequence_checker_` can be detached when this is `false`.
  bool was_bound_or_message_sent_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  bool paused_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool shut_down_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::unique_ptr<mojo::Connector> connector_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::MessageDispatcher dispatcher_ GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::PipeControlMessageHandler control_message_handler_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ControlMessageProxyThunk control_message_proxy_thunk_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<UrgentMessageObserver> urgent_message_observer_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // NOTE: It is unsafe to call into this object while holding |lock_|.
  mojo::PipeControlMessageProxy control_message_proxy_;

  // Outgoing messages sent before this controller Bound() to a pipe or while it
  // was paused. Protected by a lock to support memory dumps from any thread.
  base::Lock outgoing_messages_lock_;
  std::vector<mojo::Message> outgoing_messages_
      GUARDED_BY(outgoing_messages_lock_);

  // Guards the fields below for thread-safe access.
  base::Lock lock_;

  bool encountered_error_ GUARDED_BY(lock_) = false;

  // ID #1 is reserved for the mojom::Channel interface.
  uint32_t next_interface_id_ GUARDED_BY(lock_) = 2;

  std::map<uint32_t, scoped_refptr<Endpoint>> endpoints_ GUARDED_BY(lock_);
};

namespace {

bool ControllerMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);
  for (ChannelAssociatedGroupController* controller : controllers_) {
    base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
        base::StringPrintf("mojo/queued_ipc_channel_message/0x%" PRIxPTR,
                           reinterpret_cast<uintptr_t>(controller)));
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                    base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                    controller->GetQueuedMessageCount());
    MessageMemoryDumpInfo info;
    size_t count = 0;
    controller->GetTopQueuedMessageMemoryDumpInfo(&info, &count);
    dump->AddScalar("top_message_name", "id", info.id);
    dump->AddScalar("top_message_count",
                    base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                    count);

    if (info.profiler_tag) {
      // TODO(ssid): Memory dumps currently do not support adding string
      // arguments in background dumps. So, add this value as a trace event for
      // now.
      TRACE_EVENT2(base::trace_event::MemoryDumpManager::kTraceCategory,
                   "ControllerMemoryDumpProvider::OnMemoryDump",
                   "top_queued_message_tag", info.profiler_tag,
                   "count", count);
    }
  }

  return true;
}

class MojoBootstrapImpl : public MojoBootstrap {
 public:
  MojoBootstrapImpl(
      mojo::ScopedMessagePipeHandle handle,
      const scoped_refptr<ChannelAssociatedGroupController> controller)
      : controller_(controller),
        associated_group_(controller),
        handle_(std::move(handle)) {}

  MojoBootstrapImpl(const MojoBootstrapImpl&) = delete;
  MojoBootstrapImpl& operator=(const MojoBootstrapImpl&) = delete;

  ~MojoBootstrapImpl() override {
    controller_->ShutDown();
  }

 private:
  void Connect(
      mojo::PendingAssociatedRemote<mojom::Channel>* sender,
      mojo::PendingAssociatedReceiver<mojom::Channel>* receiver) override {
    controller_->Bind(std::move(handle_), sender, receiver);
  }

  void StartReceiving() override { controller_->StartReceiving(); }

  void Pause() override {
    controller_->Pause();
  }

  void Unpause() override {
    controller_->Unpause();
  }

  void Flush() override {
    controller_->FlushOutgoingMessages();
  }

  mojo::AssociatedGroup* GetAssociatedGroup() override {
    return &associated_group_;
  }

  void SetUrgentMessageObserver(UrgentMessageObserver* observer) override {
    controller_->SetUrgentMessageObserver(observer);
  }

  scoped_refptr<ChannelAssociatedGroupController> controller_;
  mojo::AssociatedGroup associated_group_;

  mojo::ScopedMessagePipeHandle handle_;
};

}  // namespace

ScopedAllowOffSequenceChannelAssociatedBindings::
    ScopedAllowOffSequenceChannelAssociatedBindings()
    : resetter_(&off_sequence_binding_allowed, true) {}

ScopedAllowOffSequenceChannelAssociatedBindings::
    ~ScopedAllowOffSequenceChannelAssociatedBindings() = default;

// static
std::unique_ptr<MojoBootstrap> MojoBootstrap::Create(
    mojo::ScopedMessagePipeHandle handle,
    Channel::Mode mode,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner) {
  return std::make_unique<MojoBootstrapImpl>(
      std::move(handle),
      base::MakeRefCounted<ChannelAssociatedGroupController>(
          mode == Channel::MODE_SERVER, ipc_task_runner, proxy_task_runner));
}

}  // namespace IPC
