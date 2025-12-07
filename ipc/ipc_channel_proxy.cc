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
#include "ipc/param_traits_macros.h"
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
  thread_safe_channel_ = channel_->CreateThreadSafeChannel();

  base::AutoLock filter_lock(pending_filters_lock_);
  for (auto& entry : pending_io_thread_interfaces_) {
    channel_->AddGenericAssociatedInterface(entry.first, entry.second);
  }

  pending_io_thread_interfaces_.clear();
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
void ChannelProxy::Context::OnChannelConnected(int32_t peer_pid) {
  // We cache off the peer_pid so it can be safely accessed from both threads.
  {
    base::AutoLock l(peer_pid_lock_);
    peer_pid_ = peer_pid;
  }

  // See above comment about using default_listener_task_runner_ here.
  default_listener_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Context::OnDispatchConnected, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelError() {
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
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelClosed() {
  // It's okay for IPC::ChannelProxy::Close to be called more than once, which
  // would result in this branch being taken.
  if (!channel_)
    return;

  ClearChannel();

  // Balance with the reference taken during startup.  This may result in
  // self-destruction.
  Release();
}

void ChannelProxy::Context::Clear() {
  listener_ = nullptr;
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
  channel_->AddGenericAssociatedInterface(name, factory);
}

// Called on the listener's thread.
void ChannelProxy::Context::SetUrgentMessageObserver(
    UrgentMessageObserver* observer) {
  urgent_message_observer_ = observer;
}

//-----------------------------------------------------------------------------

// static
std::unique_ptr<ChannelProxy> ChannelProxy::Create(
    const mojo::MessagePipeHandle& channel_handle,
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

void ChannelProxy::Init(const mojo::MessagePipeHandle& channel_handle,
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

void ChannelProxy::SetUrgentMessageObserver(UrgentMessageObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!did_init_);
  context_->SetUrgentMessageObserver(observer);
}

//-----------------------------------------------------------------------------

}  // namespace IPC
