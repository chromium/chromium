// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_mojo.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "ipc/ipc_mojo_handle_attachment.h"
#include "ipc/trace_ipc_message.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/thread_safe_proxy.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace IPC {

namespace {

class ThreadSafeChannelProxy : public mojo::ThreadSafeProxy {
 public:
  using Forwarder = base::RepeatingCallback<void(mojo::Message)>;

  ThreadSafeChannelProxy(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      Forwarder forwarder,
      mojo::AssociatedGroupController& group_controller)
      : task_runner_(std::move(task_runner)),
        forwarder_(std::move(forwarder)),
        group_controller_(group_controller) {}

  // mojo::ThreadSafeProxy:
  void SendMessage(mojo::Message& message) override {
    message.SerializeHandles(&*group_controller_);
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(forwarder_, std::move(message)));
  }

  void SendMessageWithResponder(
      mojo::Message& message,
      std::unique_ptr<mojo::MessageReceiver> responder) override {
    // We don't bother supporting this because it's not used in practice.
    NOTREACHED();
  }

 private:
  ~ThreadSafeChannelProxy() override = default;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const Forwarder forwarder_;
  const raw_ref<mojo::AssociatedGroupController, AcrossTasksDanglingUntriaged>
      group_controller_;
};

base::ProcessId GetSelfPID() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (int global_pid = Channel::GetGlobalPid())
    return global_pid;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return base::GetCurrentProcId();
}

}  // namespace

//------------------------------------------------------------------------------

ChannelMojo::ChannelMojo(
    mojo::ScopedMessagePipeHandle handle,
    Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner)
    : task_runner_(ipc_task_runner), pipe_(handle.get()), listener_(listener) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  bootstrap_ = MojoBootstrap::Create(std::move(handle), mode, ipc_task_runner,
                                     proxy_task_runner);
}

void ChannelMojo::ForwardMessage(mojo::Message message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!message_reader_ || !message_reader_->sender().is_bound())
    return;
  message_reader_->sender().internal_state()->ForwardMessage(
      std::move(message));
}

ChannelMojo::~ChannelMojo() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Close();
}

bool ChannelMojo::Connect() {
  WillConnect();

  mojo::PendingAssociatedRemote<mojom::Channel> sender;
  mojo::PendingAssociatedReceiver<mojom::Channel> receiver;
  bootstrap_->Connect(&sender, &receiver);

  DCHECK(!message_reader_);
  message_reader_ = std::make_unique<internal::MessagePipeReader>(
      pipe_, std::move(sender), std::move(receiver), task_runner_, this);

  if (task_runner_->RunsTasksInCurrentSequence()) {
    FinishConnectOnIOThread();
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChannelMojo::FinishConnectOnIOThread, weak_ptr_));
  }
  return true;
}

void ChannelMojo::FinishConnectOnIOThread() {
  DCHECK(message_reader_);
  message_reader_->FinishInitializationOnIOThread(GetSelfPID());
  bootstrap_->StartReceiving();
}

void ChannelMojo::Pause() {
  bootstrap_->Pause();
}

void ChannelMojo::Unpause(bool flush) {
  bootstrap_->Unpause();
  if (flush)
    Flush();
}

void ChannelMojo::Flush() {
  bootstrap_->Flush();
}

void ChannelMojo::Close() {
  // NOTE: The MessagePipeReader's destructor may re-enter this function. Use
  // caution when changing this method.
  std::unique_ptr<internal::MessagePipeReader> reader =
      std::move(message_reader_);
  reader.reset();

  base::AutoLock lock(associated_interface_lock_);
  associated_interfaces_.clear();
}

void ChannelMojo::OnPipeError() {
  DCHECK(task_runner_);
  if (task_runner_->RunsTasksInCurrentSequence()) {
    listener_->OnChannelError();
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelMojo::OnPipeError, weak_ptr_));
  }
}

void ChannelMojo::OnAssociatedInterfaceRequest(
    mojo::GenericPendingAssociatedReceiver receiver) {
  GenericAssociatedInterfaceFactory factory;
  {
    base::AutoLock locker(associated_interface_lock_);
    auto iter = associated_interfaces_.find(*receiver.interface_name());
    if (iter != associated_interfaces_.end())
      factory = iter->second;
  }

  if (!factory.is_null()) {
    factory.Run(receiver.PassHandle());
  } else {
    const std::string interface_name = *receiver.interface_name();
    listener_->OnAssociatedInterfaceRequest(interface_name,
                                            receiver.PassHandle());
  }
}

bool ChannelMojo::Send(Message* message) {
  DVLOG(2) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type();

  std::unique_ptr<Message> scoped_message = base::WrapUnique(message);
  if (!message_reader_)
    return false;

  // Comment copied from ipc_channel_posix.cc:
  // We can't close the pipe here, because calling OnChannelError may destroy
  // this object, and that would be bad if we are called from Send(). Instead,
  // we return false and hope the caller will close the pipe. If they do not,
  // the pipe will still be closed next time OnFileCanReadWithoutBlocking is
  // called.
  //
  // With Mojo, there's no OnFileCanReadWithoutBlocking, but we expect the
  // pipe's connection error handler will be invoked in its place.
  return message_reader_->Send(std::move(scoped_message));
}

Channel::AssociatedInterfaceSupport*
ChannelMojo::GetAssociatedInterfaceSupport() { return this; }

std::unique_ptr<mojo::ThreadSafeForwarder<mojom::Channel>>
ChannelMojo::CreateThreadSafeChannel() {
  return std::make_unique<mojo::ThreadSafeForwarder<mojom::Channel>>(
      base::MakeRefCounted<ThreadSafeChannelProxy>(
          task_runner_,
          base::BindRepeating(&ChannelMojo::ForwardMessage, weak_ptr_),
          *bootstrap_->GetAssociatedGroup()->GetController()));
}

void ChannelMojo::OnPeerPidReceived(int32_t peer_pid) {
  listener_->OnChannelConnected(peer_pid);
}

void ChannelMojo::OnMessageReceived(const Message& message) {
  const Message* message_ptr = &message;
  TRACE_IPC_MESSAGE_SEND("ipc,toplevel", "ChannelMojo::OnMessageReceived",
                         message_ptr);
  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

void ChannelMojo::OnBrokenDataReceived() {
  listener_->OnBadMessageReceived(Message());
}

void ChannelMojo::AddGenericAssociatedInterface(
    const std::string& name,
    const GenericAssociatedInterfaceFactory& factory) {
  base::AutoLock locker(associated_interface_lock_);
  auto result = associated_interfaces_.insert({ name, factory });
  DCHECK(result.second);
}

void ChannelMojo::GetRemoteAssociatedInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  if (message_reader_) {
    if (!task_runner_->RunsTasksInCurrentSequence()) {
      message_reader_->thread_safe_sender().GetAssociatedInterface(
          std::move(receiver));
      return;
    }
    message_reader_->GetRemoteInterface(std::move(receiver));
  } else {
    // Attach the associated interface to a disconnected pipe, so that the
    // associated interface pointer can be used to make calls (which are
    // dropped).
    mojo::AssociateWithDisconnectedPipe(receiver.PassHandle());
  }
}

void ChannelMojo::SetUrgentMessageObserver(UrgentMessageObserver* observer) {
  bootstrap_->SetUrgentMessageObserver(observer);
}

}  // namespace IPC
