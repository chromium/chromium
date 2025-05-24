// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/message_filter.h"
#include "ipc/message_filter_router.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace IPC {

//------------------------------------------------------------------------------

ChannelProxy::Context::Context(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner)
    : default_listener_task_runner_(listener_task_runner),
      listener_(listener),
      ipc_task_runner_(ipc_task_runner),
      channel_connected_called_(false),
      message_filter_router_(new MessageFilterRouter()),
      peer_pid_(base::kNullProcessId) {
  DCHECK(ipc_task_runner_.get());
  // The Listener thread where Messages are handled must be a separate thread
  // to avoid oversubscribing the IO thread. If you trigger this error, you
  // need to either:
  // 1) Create the ChannelProxy on a different thread, or
  // 2) Just use Channel
  // We make an exception for NULL listeners.
  DCHECK(!listener ||
         (ipc_task_runner_.get() != default_listener_task_runner_.get()));
}

ChannelProxy::Context::~Context() = default;

void ChannelProxy::Context::ClearIPCTaskRunner() {
  ipc_task_runner_.reset();
}

void ChannelProxy::Context::CreateChannel(
    std::unique_ptr<ChannelFactory> factory) {
  base::AutoLock channel_lock(channel_lifetime_lock_);
  DCHECK(!channel_);
  DCHECK_EQ(factory->GetIPCTaskRunner(), ipc_task_runner_);
  channel_ = factory->BuildChannel(this);
  channel_->SetUrgentMessageObserver(urgent_message_observer_);

  Channel::AssociatedInterfaceSupport* support =
      channel_->GetAssociatedInterfaceSupport();
  if (support) {
    thread_safe_channel_ = support->CreateThreadSafeChannel();

    base::AutoLock filter_lock(pending_filters_lock_);
    for (auto& entry : pending_io_thread_interfaces_)
      support->AddGenericAssociatedInterface(entry.first, entry.second);
    pending_io_thread_interfaces_.clear();
  }
}

bool ChannelProxy::Context::TryFilters(const Message& message) {
  DCHECK(message_filter_router_);
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  Logging* logger = Logging::GetInstance();
  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  if (message_filter_router_->TryFilters(message)) {
    if (message.dispatch_error()) {
      GetTaskRunner(message.routing_id())
          ->PostTask(FROM_HERE, base::BindOnce(&Context::OnDispatchBadMessage,
                                               this, message));
    }
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
    if (logger->Enabled())
      logger->OnPostDispatchMessage(message);
#endif
    return true;
  }
  return false;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::PauseChannel() {
  DCHECK(channel_);
  channel_->Pause();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::UnpauseChannel(bool flush) {
  DCHECK(channel_);
  channel_->Unpause(flush);
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::FlushChannel() {
  DCHECK(channel_);
  channel_->Flush();
}

// Called on the IPC::Channel thread
bool ChannelProxy::Context::OnMessageReceived(const Message& message) {
  // First give a chance to the filters to process this message.
  if (!TryFilters(message))
    OnMessageReceivedNoFilter(message);
  return true;
}

// Called on the IPC::Channel thread
bool ChannelProxy::Context::OnMessageReceivedNoFilter(const Message& message) {
  GetTaskRunner(message.routing_id())
      ->PostTask(FROM_HERE,
                 base::BindOnce(&Context::OnDispatchMessage, this, message));
  return true;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelConnected(int32_t peer_pid) {
  // We cache off the peer_pid so it can be safely accessed from both threads.
  {
    base::AutoLock l(peer_pid_lock_);
    peer_pid_ = peer_pid;
  }

  // Add any pending filters.  This avoids a race condition where someone
  // creates a ChannelProxy, calls AddFilter, and then right after starts the
  // peer process.  The IO thread could receive a message before the task to add
  // the filter is run on the IO thread.
  OnAddFilter();

  // See above comment about using default_listener_task_runner_ here.
  default_listener_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Context::OnDispatchConnected, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelError() {
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelError();

  // See above comment about using default_listener_task_runner_ here.
  default_listener_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Context::OnDispatchError, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  default_listener_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Context::OnDispatchAssociatedInterfaceRequest,
                                this, interface_name, std::move(handle)));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelOpened() {
  DCHECK(channel_);

  // Assume a reference to ourselves on behalf of this thread.  This reference
  // will be released when we are closed.
  AddRef();

  if (!channel_->Connect()) {
    OnChannelError();
    return;
  }

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_.get());
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelClosed() {
  // It's okay for IPC::ChannelProxy::Close to be called more than once, which
  // would result in this branch being taken.
  if (!channel_)
    return;

  for (auto& filter : pending_filters_) {
    filter->OnChannelClosing();
    filter->OnFilterRemoved();
  }
  for (auto& filter : filters_) {
    filter->OnChannelClosing();
    filter->OnFilterRemoved();
  }

  // We don't need the filters anymore.
  message_filter_router_->Clear();
  filters_.clear();
  // We don't need the lock, because at this point, the listener thread can't
  // access it any more.
  pending_filters_.clear();

  ClearChannel();

  // Balance with the reference taken during startup.  This may result in
  // self-destruction.
  Release();
}

void ChannelProxy::Context::Clear() {
  listener_ = nullptr;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnSendMessage(std::unique_ptr<Message> message) {
  if (!channel_) {
    OnChannelClosed();
    return;
  }

  if (!channel_->Send(message.release()))
    OnChannelError();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnAddFilter() {
  // Our OnChannelConnected method has not yet been called, so we can't be
  // sure that channel_ is valid yet. When OnChannelConnected *is* called,
  // it invokes OnAddFilter, so any pending filter(s) will be added at that
  // time.
  // No lock necessary for |peer_pid_| because it is only modified on this
  // thread.
  if (peer_pid_ == base::kNullProcessId)
    return;

  std::vector<scoped_refptr<MessageFilter> > new_filters;
  {
    base::AutoLock auto_lock(pending_filters_lock_);
    new_filters.swap(pending_filters_);
  }

  for (size_t i = 0; i < new_filters.size(); ++i) {
    filters_.push_back(new_filters[i]);

    message_filter_router_->AddFilter(new_filters[i].get());

    // The channel has already been created and connected, so we need to
    // inform the filters right now.
    new_filters[i]->OnFilterAdded(channel_.get());
    new_filters[i]->OnChannelConnected(peer_pid_);
  }
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnRemoveFilter(MessageFilter* filter) {
  // No lock necessary for |peer_pid_| because it is only modified on this
  // thread.
  if (peer_pid_ == base::kNullProcessId) {
    // The channel is not yet connected, so any filters are still pending.
    base::AutoLock auto_lock(pending_filters_lock_);
    for (size_t i = 0; i < pending_filters_.size(); ++i) {
      if (pending_filters_[i].get() == filter) {
        filter->OnFilterRemoved();
        pending_filters_.erase(pending_filters_.begin() + i);
        return;
      }
    }
    return;
  }
  if (!channel_)
    return;  // The filters have already been deleted.

  message_filter_router_->RemoveFilter(filter);

  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i].get() == filter) {
      filter->OnFilterRemoved();
      filters_.erase(filters_.begin() + i);
      return;
    }
  }

  NOTREACHED() << "filter to be removed not found";
}

// Called on the listener's thread
void ChannelProxy::Context::AddFilter(MessageFilter* filter) {
  base::AutoLock auto_lock(pending_filters_lock_);
  pending_filters_.push_back(base::WrapRefCounted(filter));
  ipc_task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&Context::OnAddFilter, this));
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchMessage(const Message& message) {
  if (!listener_)
    return;

  OnDispatchConnected();

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  Logging* logger = Logging::GetInstance();
  if (message.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(message);
    return;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  if (logger->Enabled())
    logger->OnPostDispatchMessage(message);
#endif
}

// Called on the listener's thread.
void ChannelProxy::Context::AddListenerTaskRunner(
    int32_t routing_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(default_listener_task_runner_->BelongsToCurrentThread());
  DCHECK(task_runner);
  base::AutoLock lock(listener_thread_task_runners_lock_);
  if (!base::Contains(listener_thread_task_runners_, routing_id))
    listener_thread_task_runners_.insert({routing_id, std::move(task_runner)});
}

// Called on the listener's thread.
void ChannelProxy::Context::RemoveListenerTaskRunner(int32_t routing_id) {
  DCHECK(default_listener_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(listener_thread_task_runners_lock_);
  listener_thread_task_runners_.erase(routing_id);
}

// Called on the IPC::Channel thread.
scoped_refptr<base::SingleThreadTaskRunner>
ChannelProxy::Context::GetTaskRunner(int32_t routing_id) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (routing_id == MSG_ROUTING_NONE)
    return default_listener_task_runner_;

  base::AutoLock lock(listener_thread_task_runners_lock_);
  auto task_runner = listener_thread_task_runners_.find(routing_id);
  if (task_runner == listener_thread_task_runners_.end())
    return default_listener_task_runner_;
  DCHECK(task_runner->second);
  return task_runner->second;
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchConnected() {
  if (channel_connected_called_)
    return;

  base::ProcessId peer_pid;
  {
    base::AutoLock l(peer_pid_lock_);
    peer_pid = peer_pid_;
  }
  channel_connected_called_ = true;
  if (listener_)
    listener_->OnChannelConnected(peer_pid);
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchError() {
  if (listener_)
    listener_->OnChannelError();
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchBadMessage(const Message& message) {
  if (listener_)
    listener_->OnBadMessageReceived(message);
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (listener_)
    listener_->OnAssociatedInterfaceRequest(interface_name, std::move(handle));
}

void ChannelProxy::Context::ClearChannel() {
  base::AutoLock l(channel_lifetime_lock_);
  channel_.reset();
}

void ChannelProxy::Context::AddGenericAssociatedInterfaceForIOThread(
    const std::string& name,
    const GenericAssociatedInterfaceFactory& factory) {
  base::AutoLock channel_lock(channel_lifetime_lock_);
  if (!channel_) {
    base::AutoLock filter_lock(pending_filters_lock_);
    pending_io_thread_interfaces_.emplace_back(name, factory);
    return;
  }
  Channel::AssociatedInterfaceSupport* support =
      channel_->GetAssociatedInterfaceSupport();
  if (support)
    support->AddGenericAssociatedInterface(name, factory);
}

void ChannelProxy::Context::Send(Message* message) {
  ipc_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ChannelProxy::Context::OnSendMessage, this,
                                base::WrapUnique(message)));
}

// Called on the listener's thread.
void ChannelProxy::Context::SetUrgentMessageObserver(
    UrgentMessageObserver* observer) {
  urgent_message_observer_ = observer;
}

//-----------------------------------------------------------------------------

// static
std::unique_ptr<ChannelProxy> ChannelProxy::Create(
    const IPC::ChannelHandle& channel_handle,
    Channel::Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner) {
  std::unique_ptr<ChannelProxy> channel(
      new ChannelProxy(listener, ipc_task_runner, listener_task_runner));
  channel->Init(channel_handle, mode, true);
  return channel;
}

// static
std::unique_ptr<ChannelProxy> ChannelProxy::Create(
    std::unique_ptr<ChannelFactory> factory,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner) {
  std::unique_ptr<ChannelProxy> channel(
      new ChannelProxy(listener, ipc_task_runner, listener_task_runner));
  channel->Init(std::move(factory), true);
  return channel;
}

ChannelProxy::ChannelProxy(Context* context) : context_(context) {}

ChannelProxy::ChannelProxy(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner)
    : context_(new Context(listener, ipc_task_runner, listener_task_runner)) {}

ChannelProxy::~ChannelProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Close();
}

void ChannelProxy::Init(const IPC::ChannelHandle& channel_handle,
                        Channel::Mode mode,
                        bool create_pipe_now) {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // When we are creating a server on POSIX, we need its file descriptor
  // to be created immediately so that it can be accessed and passed
  // to other processes. Forcing it to be created immediately avoids
  // race conditions that may otherwise arise.
  if (mode & Channel::MODE_SERVER_FLAG) {
    create_pipe_now = true;
  }
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  Init(
      ChannelFactory::Create(channel_handle, mode, context_->ipc_task_runner()),
      create_pipe_now);
}

void ChannelProxy::Init(std::unique_ptr<ChannelFactory> factory,
                        bool create_pipe_now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!did_init_);

  if (create_pipe_now) {
    // Create the channel immediately.  This effectively sets up the
    // low-level pipe so that the client can connect.  Without creating
    // the pipe immediately, it is possible for a listener to attempt
    // to connect and get an error since the pipe doesn't exist yet.
    context_->CreateChannel(std::move(factory));
  } else {
    context_->ipc_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&Context::CreateChannel, context_, std::move(factory)));
  }

  // complete initialization on the background thread
  context_->ipc_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Context::OnChannelOpened, context_));

  did_init_ = true;
  OnChannelInit();
}

void ChannelProxy::Pause() {
  context_->ipc_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Context::PauseChannel, context_));
}

void ChannelProxy::Unpause(bool flush) {
  context_->ipc_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Context::UnpauseChannel, context_, flush));
}

void ChannelProxy::Flush() {
  context_->ipc_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Context::FlushChannel, context_));
}

void ChannelProxy::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear the backpointer to the listener so that any pending calls to
  // Context::OnDispatchMessage or OnDispatchError will be ignored.  It is
  // possible that the channel could be closed while it is receiving messages!
  context_->Clear();

  if (context_->ipc_task_runner()) {
    context_->ipc_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Context::OnChannelClosed, context_));
  }
}

bool ChannelProxy::Send(Message* message) {
  DCHECK(!message->is_sync()) << "Need to use IPC::SyncChannel";
  SendInternal(message);
  return true;
}

void ChannelProxy::SendInternal(Message* message) {
  DCHECK(did_init_);

  // TODO(alexeypa): add DCHECK(CalledOnValidThread()) here. Currently there are
  // tests that call Send() from a wrong thread. See http://crbug.com/163523.

#ifdef ENABLE_IPC_FUZZER
  // In IPC fuzzing builds, it is possible to define a filter to apply to
  // outgoing messages. It will either rewrite the message and return a new
  // one, freeing the original, or return the message unchanged.
  if (outgoing_message_filter())
    message = outgoing_message_filter()->Rewrite(message);
#endif

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  Logging::GetInstance()->OnSendMessage(message);
#endif

  context_->Send(message);
}

void ChannelProxy::AddFilter(MessageFilter* filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  context_->AddFilter(filter);
}

void ChannelProxy::RemoveFilter(MessageFilter* filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  context_->ipc_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Context::OnRemoveFilter, context_,
                                base::RetainedRef(filter)));
}

void ChannelProxy::AddGenericAssociatedInterfaceForIOThread(
    const std::string& name,
    const GenericAssociatedInterfaceFactory& factory) {
  context()->AddGenericAssociatedInterfaceForIOThread(name, factory);
}

void ChannelProxy::GetRemoteAssociatedInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  DCHECK(did_init_);
  context()->thread_safe_channel().GetAssociatedInterface(std::move(receiver));
}

void ChannelProxy::ClearIPCTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  context()->ClearIPCTaskRunner();
}

void ChannelProxy::OnChannelInit() {
}

void ChannelProxy::SetUrgentMessageObserver(UrgentMessageObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!did_init_);
  context_->SetUrgentMessageObserver(observer);
}

//-----------------------------------------------------------------------------

}  // namespace IPC
