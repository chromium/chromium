// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "ipc/ipc_mojo_handle_attachment.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/thread_safe_proxy.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace IPC {

namespace {

// Global atomic used to guarantee channel IDs are unique.
base::AtomicSequenceNumber g_last_id;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

int g_global_pid = 0;

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

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
  if (int global_pid = Channel::GetGlobalPid()) {
    return global_pid;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return base::GetCurrentProcId();
}

}  // namespace

// static
std::unique_ptr<Channel> Channel::Create(
    mojo::ScopedMessagePipeHandle handle,
    Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner) {
  return base::WrapUnique(new Channel(std::move(handle), mode, listener,
                                      ipc_task_runner, proxy_task_runner));
}

// static
std::string Channel::GenerateUniqueRandomChannelID() {
  // Note: the string must start with the current process id, this is how
  // some child processes determine the pid of the parent.
  //
  // This is composed of a unique incremental identifier, the process ID of
  // the creator, an identifier for the child instance, and a strong random
  // component. The strong random component prevents other processes from
  // hijacking or squatting on predictable channel names.
  int process_id = base::GetCurrentProcId();
  return base::StringPrintf("%d.%u.%d",
      process_id,
      g_last_id.GetNext(),
      base::RandInt(0, std::numeric_limits<int32_t>::max()));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// static
void Channel::SetGlobalPid(int pid) {
  g_global_pid = pid;
}

// static
int Channel::GetGlobalPid() {
  return g_global_pid;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

void Channel::WillConnect() {
  did_start_connect_ = true;
}

//------------------------------------------------------------------------------

Channel::Channel(
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

void Channel::ForwardMessage(mojo::Message message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!message_reader_ || !message_reader_->sender().is_bound()) {
    return;
  }
  message_reader_->sender().internal_state()->ForwardMessage(
      std::move(message));
}

Channel::~Channel() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Close();
}

bool Channel::Connect() {
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
        base::BindOnce(&Channel::FinishConnectOnIOThread, weak_ptr_));
  }
  return true;
}

void Channel::FinishConnectOnIOThread() {
  DCHECK(message_reader_);
  message_reader_->FinishInitializationOnIOThread(GetSelfPID());
  bootstrap_->StartReceiving();
}

void Channel::Pause() {
  bootstrap_->Pause();
}

void Channel::Unpause(bool flush) {
  bootstrap_->Unpause();
  if (flush) {
    Flush();
  }
}

void Channel::Flush() {
  bootstrap_->Flush();
}

void Channel::Close() {
  // NOTE: The MessagePipeReader's destructor may re-enter this function. Use
  // caution when changing this method.
  std::unique_ptr<internal::MessagePipeReader> reader =
      std::move(message_reader_);
  reader.reset();

  base::AutoLock lock(associated_interface_lock_);
  associated_interfaces_.clear();
}

void Channel::OnPipeError() {
  DCHECK(task_runner_);
  if (task_runner_->RunsTasksInCurrentSequence()) {
    listener_->OnChannelError();
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&Channel::OnPipeError, weak_ptr_));
  }
}

void Channel::OnAssociatedInterfaceRequest(
    mojo::GenericPendingAssociatedReceiver receiver) {
  GenericAssociatedInterfaceFactory factory;
  {
    base::AutoLock locker(associated_interface_lock_);
    auto iter = associated_interfaces_.find(*receiver.interface_name());
    if (iter != associated_interfaces_.end()) {
      factory = iter->second;
    }
  }

  if (!factory.is_null()) {
    factory.Run(receiver.PassHandle());
  } else {
    const std::string interface_name = *receiver.interface_name();
    listener_->OnAssociatedInterfaceRequest(interface_name,
                                            receiver.PassHandle());
  }
}

std::unique_ptr<mojo::ThreadSafeForwarder<mojom::Channel>>
Channel::CreateThreadSafeChannel() {
  return std::make_unique<mojo::ThreadSafeForwarder<mojom::Channel>>(
      base::MakeRefCounted<ThreadSafeChannelProxy>(
          task_runner_,
          base::BindRepeating(&Channel::ForwardMessage, weak_ptr_),
          *bootstrap_->GetAssociatedGroup()->GetController()));
}

void Channel::OnPeerPidReceived(int32_t peer_pid) {
  listener_->OnChannelConnected(peer_pid);
}

void Channel::AddGenericAssociatedInterface(
    const std::string& name,
    const GenericAssociatedInterfaceFactory& factory) {
  base::AutoLock locker(associated_interface_lock_);
  auto result = associated_interfaces_.insert({name, factory});
  DCHECK(result.second);
}

void Channel::GetRemoteAssociatedInterface(
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

void Channel::SetUrgentMessageObserver(UrgentMessageObserver* observer) {
  bootstrap_->SetUrgentMessageObserver(observer);
}

}  // namespace IPC
