// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_bootstrap.h"

#include <inttypes.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/common/task_annotator.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/pipe_control_message_handler.h"
#include "mojo/public/cpp/bindings/pipe_control_message_handler_delegate.h"
#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"
#include "mojo/public/cpp/bindings/sequence_local_sync_event_watcher.h"

namespace IPC {

namespace {

class ChannelAssociatedGroupController;

// Used to track some internal Channel state in pursuit of message leaks.
//
// TODO(https://crbug.com/813045): Remove this.
class ControllerMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  ControllerMemoryDumpProvider() {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "IPCChannel", nullptr);
  }

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
  std::set<ChannelAssociatedGroupController*> controllers_;

  DISALLOW_COPY_AND_ASSIGN(ControllerMemoryDumpProvider);
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

class ChannelAssociatedGroupController
    : public mojo::AssociatedGroupController,
      public mojo::MessageReceiver,
      public mojo::PipeControlMessageHandlerDelegate {
 public:
  ChannelAssociatedGroupController(
      bool set_interface_id_namespace_bit,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner,
      const scoped_refptr<mojo::internal::MessageQuotaChecker>& quota_checker)
      : task_runner_(ipc_task_runner),
        proxy_task_runner_(proxy_task_runner),
        quota_checker_(quota_checker),
        set_interface_id_namespace_bit_(set_interface_id_namespace_bit),
        dispatcher_(this),
        control_message_handler_(this),
        control_message_proxy_thunk_(this),
        control_message_proxy_(&control_message_proxy_thunk_) {
    thread_checker_.DetachFromThread();
    control_message_handler_.SetDescription(
        "IPC::mojom::Bootstrap [primary] PipeControlMessageHandler");
    dispatcher_.SetValidator(std::make_unique<mojo::MessageHeaderValidator>(
        "IPC::mojom::Bootstrap [primary] MessageHeaderValidator"));

    GetMemoryDumpProvider().AddController(this);
  }

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

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(task_runner_->BelongsToCurrentThread());

    connector_ = std::make_unique<mojo::Connector>(
        std::move(handle), mojo::Connector::SINGLE_THREADED_SEND, task_runner_,
        "IPC Channel");
    connector_->set_incoming_receiver(&dispatcher_);
    connector_->set_connection_error_handler(
        base::BindOnce(&ChannelAssociatedGroupController::OnPipeError,
                       base::Unretained(this)));
    connector_->set_enforce_errors_from_incoming_receiver(false);
    if (quota_checker_)
      connector_->SetMessageQuotaChecker(quota_checker_);

    // Don't let the Connector do any sort of queuing on our behalf. Individual
    // messages bound for the IPC::ChannelProxy thread (i.e. that vast majority
    // of messages received by this Connector) are already individually
    // scheduled for dispatch by ChannelProxy, so Connector's normal mode of
    // operation would only introduce a redundant scheduling step for most
    // messages.
    connector_->set_force_immediate_dispatch(true);
  }

  void Pause() {
    DCHECK(!paused_);
    paused_ = true;
  }

  void Unpause() {
    DCHECK(paused_);
    paused_ = false;
  }

  void FlushOutgoingMessages() {
    std::vector<mojo::Message> outgoing_messages;
    {
      base::AutoLock lock(outgoing_messages_lock_);
      std::swap(outgoing_messages, outgoing_messages_);
    }
    if (quota_checker_ && outgoing_messages.size())
      quota_checker_->AfterMessagesDequeued(outgoing_messages.size());

    for (auto& message : outgoing_messages)
      SendMessage(&message);
  }

  void CreateChannelEndpoints(
      mojo::AssociatedRemote<mojom::Channel>* sender,
      mojo::PendingAssociatedReceiver<mojom::Channel>* receiver) {
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

    sender->Bind(mojo::PendingAssociatedRemote<mojom::Channel>(
        std::move(sender_handle), 0));
    *receiver = mojo::PendingAssociatedReceiver<mojom::Channel>(
        std::move(receiver_handle));
  }

  void ShutDown() {
    DCHECK(thread_checker_.CalledOnValidThread());
    shut_down_ = true;
    if (connector_)
      connector_->CloseMessagePipe();
    OnPipeError();
    connector_.reset();

    base::AutoLock lock(outgoing_messages_lock_);
    if (quota_checker_ && outgoing_messages_.size())
      quota_checker_->AfterMessagesDequeued(outgoing_messages_.size());

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
      const base::Optional<mojo::DisconnectReason>& reason) override {
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

    mojo::Message& value() { return value_; }

   private:
    ChannelAssociatedGroupController* controller_ = nullptr;
    mojo::Message value_;

    DISALLOW_COPY_AND_ASSIGN(MessageWrapper);
  };

  class Endpoint : public base::RefCountedThreadSafe<Endpoint>,
                   public mojo::InterfaceEndpointController {
   public:
    Endpoint(ChannelAssociatedGroupController* controller, mojo::InterfaceId id)
        : controller_(controller), id_(id) {}

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

    const base::Optional<mojo::DisconnectReason>& disconnect_reason() const {
      return disconnect_reason_;
    }

    void set_disconnect_reason(
        const base::Optional<mojo::DisconnectReason>& disconnect_reason) {
      disconnect_reason_ = disconnect_reason;
    }

    base::SequencedTaskRunner* task_runner() const {
      return task_runner_.get();
    }

    mojo::InterfaceEndpointClient* client() const {
      controller_->lock_.AssertAcquired();
      return client_;
    }

    void AttachClient(mojo::InterfaceEndpointClient* client,
                      scoped_refptr<base::SequencedTaskRunner> runner) {
      controller_->lock_.AssertAcquired();
      DCHECK(!client_);
      DCHECK(!closed_);
      DCHECK(runner->RunsTasksInCurrentSequence());

      task_runner_ = std::move(runner);
      client_ = client;
    }

    void DetachClient() {
      controller_->lock_.AssertAcquired();
      DCHECK(client_);
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      DCHECK(!closed_);

      task_runner_ = nullptr;
      client_ = nullptr;
      sync_watcher_.reset();
    }

    uint32_t EnqueueSyncMessage(MessageWrapper message) {
      controller_->lock_.AssertAcquired();
      uint32_t id = GenerateSyncMessageId();
      sync_messages_.emplace(id, std::move(message));
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
      sync_messages_.pop();
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

    bool SyncWatch(const bool* should_stop) override {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());

      // It's not legal to make sync calls from the primary endpoint's thread,
      // and in fact they must only happen from the proxy task runner.
      DCHECK(!controller_->task_runner_->BelongsToCurrentThread());
      DCHECK(controller_->proxy_task_runner_->BelongsToCurrentThread());

      EnsureSyncWatcherExists();
      return sync_watcher_->SyncWatch(should_stop);
    }

   private:
    friend class base::RefCountedThreadSafe<Endpoint>;

    ~Endpoint() override {
      controller_->lock_.AssertAcquired();
      DCHECK(!client_);
      DCHECK(closed_);
      DCHECK(peer_closed_);
      DCHECK(!sync_watcher_);
    }

    void OnSyncMessageEventReady() {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());

      scoped_refptr<Endpoint> keepalive(this);
      scoped_refptr<AssociatedGroupController> controller_keepalive(
          controller_);
      base::AutoLock locker(controller_->lock_);
      bool more_to_process = false;
      if (!sync_messages_.empty()) {
        MessageWrapper message_wrapper =
            std::move(sync_messages_.front().second);
        sync_messages_.pop();

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

    ChannelAssociatedGroupController* const controller_;
    const mojo::InterfaceId id_;

    bool closed_ = false;
    bool peer_closed_ = false;
    bool handle_created_ = false;
    base::Optional<mojo::DisconnectReason> disconnect_reason_;
    mojo::InterfaceEndpointClient* client_ = nullptr;
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    std::unique_ptr<mojo::SequenceLocalSyncEventWatcher> sync_watcher_;
    base::queue<std::pair<uint32_t, MessageWrapper>> sync_messages_;
    uint32_t next_sync_message_id_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Endpoint);
  };

  class ControlMessageProxyThunk : public MessageReceiver {
   public:
    explicit ControlMessageProxyThunk(
        ChannelAssociatedGroupController* controller)
        : controller_(controller) {}

   private:
    // MessageReceiver:
    bool Accept(mojo::Message* message) override {
      return controller_->SendMessage(message);
    }

    ChannelAssociatedGroupController* controller_;

    DISALLOW_COPY_AND_ASSIGN(ControlMessageProxyThunk);
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
        MarkClosedAndMaybeRemove(endpoint);
      } else {
        MarkPeerClosedAndMaybeRemove(endpoint);
      }
    }

    DCHECK(endpoints_.empty());

    GetMemoryDumpProvider().RemoveController(this);
  }

  bool SendMessage(mojo::Message* message) {
    DCHECK(message->heap_profiler_tag());
    if (task_runner_->BelongsToCurrentThread()) {
      DCHECK(thread_checker_.CalledOnValidThread());
      if (!connector_ || paused_) {
        if (!shut_down_) {
          base::AutoLock lock(outgoing_messages_lock_);
          if (quota_checker_)
            quota_checker_->BeforeMessagesEnqueued(1);
          outgoing_messages_.emplace_back(std::move(*message));
        }
        return true;
      }
      return connector_->Accept(message);
    } else {
      // We always post tasks to the primary endpoint thread when called from
      // other threads in order to simulate IPC::ChannelProxy::Send behavior.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ChannelAssociatedGroupController::SendMessageOnPrimaryThread,
              this, std::move(*message)));
      return true;
    }
  }

  void SendMessageOnPrimaryThread(mojo::Message message) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!SendMessage(&message))
      RaiseError();
  }

  void OnPipeError() {
    DCHECK(thread_checker_.CalledOnValidThread());

    // We keep |this| alive here because it's possible for the notifications
    // below to release all other references.
    scoped_refptr<ChannelAssociatedGroupController> keepalive(this);

    base::AutoLock locker(lock_);
    encountered_error_ = true;

    std::vector<scoped_refptr<Endpoint>> endpoints_to_notify;
    for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
      Endpoint* endpoint = iter->second.get();
      ++iter;

      if (endpoint->client())
        endpoints_to_notify.push_back(endpoint);

      MarkPeerClosedAndMaybeRemove(endpoint);
    }

    for (auto& endpoint : endpoints_to_notify) {
      // Because a notification may in turn detach any endpoint, we have to
      // check each client again here.
      if (endpoint->client())
        NotifyEndpointOfError(endpoint.get(), false /* force_async */);
    }
  }

  void NotifyEndpointOfError(Endpoint* endpoint, bool force_async) {
    lock_.AssertAcquired();
    DCHECK(endpoint->task_runner() && endpoint->client());
    if (endpoint->task_runner()->RunsTasksInCurrentSequence() && !force_async) {
      mojo::InterfaceEndpointClient* client = endpoint->client();
      base::Optional<mojo::DisconnectReason> reason(
          endpoint->disconnect_reason());

      base::AutoUnlock unlocker(lock_);
      client->NotifyError(reason);
    } else {
      endpoint->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&ChannelAssociatedGroupController::
                             NotifyEndpointOfErrorOnEndpointThread,
                         this, endpoint->id(), base::Unretained(endpoint)));
    }
  }

  void NotifyEndpointOfErrorOnEndpointThread(mojo::InterfaceId id,
                                             Endpoint* endpoint) {
    base::AutoLock locker(lock_);
    auto iter = endpoints_.find(id);
    if (iter == endpoints_.end() || iter->second.get() != endpoint)
      return;
    if (!endpoint->client())
      return;

    DCHECK(endpoint->task_runner()->RunsTasksInCurrentSequence());
    NotifyEndpointOfError(endpoint, false /* force_async */);
  }

  void MarkClosedAndMaybeRemove(Endpoint* endpoint) {
    lock_.AssertAcquired();
    endpoint->set_closed();
    if (endpoint->closed() && endpoint->peer_closed())
      endpoints_.erase(endpoint->id());
  }

  void MarkPeerClosedAndMaybeRemove(Endpoint* endpoint) {
    lock_.AssertAcquired();
    endpoint->set_peer_closed();
    endpoint->SignalSyncMessageEvent();
    if (endpoint->closed() && endpoint->peer_closed())
      endpoints_.erase(endpoint->id());
  }

  Endpoint* FindOrInsertEndpoint(mojo::InterfaceId id, bool* inserted) {
    lock_.AssertAcquired();
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

  Endpoint* FindEndpoint(mojo::InterfaceId id) {
    lock_.AssertAcquired();
    auto iter = endpoints_.find(id);
    return iter != endpoints_.end() ? iter->second.get() : nullptr;
  }

  // mojo::MessageReceiver:
  bool Accept(mojo::Message* message) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (!message->DeserializeAssociatedEndpointHandles(this))
      return false;

    if (mojo::PipeControlMessageHandler::IsPipeControlMessage(message))
      return control_message_handler_.Accept(message);

    mojo::InterfaceId id = message->interface_id();
    DCHECK(mojo::IsValidInterfaceId(id));

    base::ReleasableAutoLock locker(&lock_);
    Endpoint* endpoint = FindEndpoint(id);
    if (!endpoint)
      return true;

    mojo::InterfaceEndpointClient* client = endpoint->client();
    if (!client || !endpoint->task_runner()->RunsTasksInCurrentSequence()) {
      // No client has been bound yet or the client runs tasks on another
      // thread. We assume the other thread must always be the one on which
      // |proxy_task_runner_| runs tasks, since that's the only valid scenario.
      //
      // If the client is not yet bound, it must be bound by the time this task
      // runs or else it's programmer error.
      DCHECK(proxy_task_runner_);

      if (message->has_flag(mojo::Message::kFlagIsSync)) {
        MessageWrapper message_wrapper(this, std::move(*message));
        // Sync messages may need to be handled by the endpoint if it's blocking
        // on a sync reply. We pass ownership of the message to the endpoint's
        // sync message queue. If the endpoint was blocking, it will dequeue the
        // message and dispatch it. Otherwise the posted |AcceptSyncMessage()|
        // call will dequeue the message and dispatch it.
        uint32_t message_id =
            endpoint->EnqueueSyncMessage(std::move(message_wrapper));
        proxy_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&ChannelAssociatedGroupController::AcceptSyncMessage,
                           this, id, message_id));
        return true;
      }

      // If |proxy_task_runner_| has been torn down already, this PostTask will
      // fail and destroy |message|. That operation may need to in turn destroy
      // in-transit associated endpoints and thus acquire |lock_|. We no longer
      // need the lock to be held now since |proxy_task_runner_| is safe to
      // access unguarded.
      {
        // Grab interface name from |client| before releasing the lock to ensure
        // that |client| is safe to access.
        base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(
            client ? client->interface_name() : "unknown interface");
        locker.Release();
        proxy_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ChannelAssociatedGroupController::AcceptOnProxyThread, this,
                std::move(*message)));
      }
      return true;
    }

    // We do not expect to receive sync responses on the primary endpoint
    // thread. If it's happening, it's a bug.
    DCHECK(!message->has_flag(mojo::Message::kFlagIsSync) ||
           !message->has_flag(mojo::Message::kFlagIsResponse));

    locker.Release();
    // It's safe to access |client| here without holding a lock, because this
    // code runs on a proxy thread and |client| can't be destroyed from any
    // thread.
    return client->HandleIncomingMessage(message);
  }

  void AcceptOnProxyThread(mojo::Message message) {
    DCHECK(proxy_task_runner_->BelongsToCurrentThread());
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mojom"),
                 "ChannelAssociatedGroupController::AcceptOnProxyThread");

    mojo::InterfaceId id = message.interface_id();
    DCHECK(mojo::IsValidInterfaceId(id) && !mojo::IsPrimaryInterfaceId(id));

    base::AutoLock locker(lock_);
    Endpoint* endpoint = FindEndpoint(id);
    if (!endpoint)
      return;

    mojo::InterfaceEndpointClient* client = endpoint->client();
    if (!client)
      return;

    // Using client->interface_name() is safe here because this is a static
    // string defined for each mojo interface.
    TRACE_EVENT0("mojom", client->interface_name());
    DCHECK(endpoint->task_runner()->RunsTasksInCurrentSequence());

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

  void AcceptSyncMessage(mojo::InterfaceId interface_id, uint32_t message_id) {
    DCHECK(proxy_task_runner_->BelongsToCurrentThread());
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

    // Using client->interface_name() is safe here because this is a static
    // string defined for each mojo interface.
    TRACE_EVENT0("mojom", client->interface_name());
    DCHECK(endpoint->task_runner()->RunsTasksInCurrentSequence());
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
      const base::Optional<mojo::DisconnectReason>& reason) override {
    DCHECK(thread_checker_.CalledOnValidThread());

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

  // Checked in places which must be run on the primary endpoint's thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const scoped_refptr<base::SingleThreadTaskRunner> proxy_task_runner_;
  const scoped_refptr<mojo::internal::MessageQuotaChecker> quota_checker_;
  const bool set_interface_id_namespace_bit_;
  bool paused_ = false;
  std::unique_ptr<mojo::Connector> connector_;
  mojo::MessageDispatcher dispatcher_;
  mojo::PipeControlMessageHandler control_message_handler_;
  ControlMessageProxyThunk control_message_proxy_thunk_;

  // NOTE: It is unsafe to call into this object while holding |lock_|.
  mojo::PipeControlMessageProxy control_message_proxy_;

  // Guards access to |outgoing_messages_| only. Used to support memory dumps
  // which may be triggered from any thread.
  base::Lock outgoing_messages_lock_;

  // Outgoing messages that were sent before this controller was bound to a
  // real message pipe.
  std::vector<mojo::Message> outgoing_messages_;

  // Guards the fields below for thread-safe access.
  base::Lock lock_;

  bool encountered_error_ = false;
  bool shut_down_ = false;

  // ID #1 is reserved for the mojom::Channel interface.
  uint32_t next_interface_id_ = 2;

  std::map<uint32_t, scoped_refptr<Endpoint>> endpoints_;

  DISALLOW_COPY_AND_ASSIGN(ChannelAssociatedGroupController);
};

bool ControllerMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);
  for (auto* controller : controllers_) {
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

  ~MojoBootstrapImpl() override {
    controller_->ShutDown();
  }

 private:
  void Connect(
      mojo::AssociatedRemote<mojom::Channel>* sender,
      mojo::PendingAssociatedReceiver<mojom::Channel>* receiver) override {
    controller_->Bind(std::move(handle_));
    controller_->CreateChannelEndpoints(sender, receiver);
  }

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

  scoped_refptr<ChannelAssociatedGroupController> controller_;
  mojo::AssociatedGroup associated_group_;

  mojo::ScopedMessagePipeHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(MojoBootstrapImpl);
};

}  // namespace

// static
std::unique_ptr<MojoBootstrap> MojoBootstrap::Create(
    mojo::ScopedMessagePipeHandle handle,
    Channel::Mode mode,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner,
    const scoped_refptr<mojo::internal::MessageQuotaChecker>& quota_checker) {
  return std::make_unique<MojoBootstrapImpl>(
      std::move(handle), new ChannelAssociatedGroupController(
                             mode == Channel::MODE_SERVER, ipc_task_runner,
                             proxy_task_runner, quota_checker));
}

}  // namespace IPC
