// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ipc/ipc_message_pipe_reader.h"

#include <stdint.h>

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/thread_safe_proxy.h"

namespace IPC {
namespace internal {

namespace {

class ThreadSafeProxy : public mojo::ThreadSafeProxy {
 public:
  using Forwarder = base::RepeatingCallback<void(mojo::Message)>;

  ThreadSafeProxy(scoped_refptr<base::SequencedTaskRunner> task_runner,
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
  ~ThreadSafeProxy() override = default;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const Forwarder forwarder_;
  const raw_ref<mojo::AssociatedGroupController> group_controller_;
};

}  // namespace

MessagePipeReader::MessagePipeReader(
    mojo::MessagePipeHandle pipe,
    mojo::PendingAssociatedRemote<mojom::Channel> sender,
    mojo::PendingAssociatedReceiver<mojom::Channel> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MessagePipeReader::Delegate* delegate)
    : delegate_(delegate),
      sender_(std::move(sender), task_runner),
      receiver_(this, std::move(receiver), task_runner) {
  thread_safe_sender_ =
      std::make_unique<mojo::ThreadSafeForwarder<mojom::Channel>>(
          base::MakeRefCounted<ThreadSafeProxy>(
              task_runner,
              base::BindRepeating(&MessagePipeReader::ForwardMessage,
                                  weak_ptr_factory_.GetWeakPtr()),
              *sender_.internal_state()->associated_group()->GetController()));

  thread_checker_.DetachFromThread();
}

MessagePipeReader::~MessagePipeReader() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // The pipe should be closed before deletion.
}

void MessagePipeReader::FinishInitializationOnIOThread(
    base::ProcessId self_pid) {
  sender_.set_disconnect_handler(
      base::BindOnce(&MessagePipeReader::OnPipeError, base::Unretained(this),
                     MOJO_RESULT_FAILED_PRECONDITION));
  receiver_.set_disconnect_handler(
      base::BindOnce(&MessagePipeReader::OnPipeError, base::Unretained(this),
                     MOJO_RESULT_FAILED_PRECONDITION));

  sender_->SetPeerPid(self_pid);
}

void MessagePipeReader::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sender_.reset();
  if (receiver_.is_bound())
    receiver_.reset();
}

bool MessagePipeReader::Send(std::unique_ptr<Message> message) {
  CHECK(message->IsValid());
  TRACE_EVENT_WITH_FLOW0("toplevel.flow", "MessagePipeReader::Send",
                         message->flags(), TRACE_EVENT_FLAG_FLOW_OUT);
  std::optional<std::vector<mojo::native::SerializedHandlePtr>> handles;
  MojoResult result = MOJO_RESULT_OK;
  result = ChannelMojo::ReadFromMessageAttachmentSet(message.get(), &handles);
  if (result != MOJO_RESULT_OK)
    return false;

  if (!sender_)
    return false;

  base::span<const uint8_t> bytes(static_cast<const uint8_t*>(message->data()),
                                  message->size());
  sender_->Receive(MessageView(bytes, std::move(handles)));
  DVLOG(4) << "Send " << message->type() << ": " << message->size();
  return true;
}

void MessagePipeReader::GetRemoteInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  if (!sender_.is_bound())
    return;
  sender_->GetAssociatedInterface(std::move(receiver));
}

void MessagePipeReader::SetPeerPid(int32_t peer_pid) {
  delegate_->OnPeerPidReceived(peer_pid);
}

void MessagePipeReader::Receive(MessageView message_view) {
  if (message_view.bytes().empty()) {
    delegate_->OnBrokenDataReceived();
    return;
  }
  Message message(reinterpret_cast<const char*>(message_view.bytes().data()),
                  message_view.bytes().size());
  if (!message.IsValid()) {
    delegate_->OnBrokenDataReceived();
    return;
  }

  DVLOG(4) << "Receive " << message.type() << ": " << message.size();
  MojoResult write_result = ChannelMojo::WriteToMessageAttachmentSet(
      message_view.TakeHandles(), &message);
  if (write_result != MOJO_RESULT_OK) {
    OnPipeError(write_result);
    return;
  }

  TRACE_EVENT_WITH_FLOW0("toplevel.flow", "MessagePipeReader::Receive",
                         message.flags(), TRACE_EVENT_FLAG_FLOW_IN);
  delegate_->OnMessageReceived(message);
}

void MessagePipeReader::GetAssociatedInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_)
    delegate_->OnAssociatedInterfaceRequest(std::move(receiver));
}

void MessagePipeReader::OnPipeError(MojoResult error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Close();

  // NOTE: The delegate call below may delete |this|.
  if (delegate_)
    delegate_->OnPipeError();
}

void MessagePipeReader::ForwardMessage(mojo::Message message) {
  sender_.internal_state()->ForwardMessage(std::move(message));
}

}  // namespace internal
}  // namespace IPC
