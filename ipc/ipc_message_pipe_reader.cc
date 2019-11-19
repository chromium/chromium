// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_pipe_reader.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/public/cpp/bindings/message.h"

namespace IPC {
namespace internal {

MessagePipeReader::MessagePipeReader(
    mojo::MessagePipeHandle pipe,
    mojo::AssociatedRemote<mojom::Channel> sender,
    mojo::PendingAssociatedReceiver<mojom::Channel> receiver,
    MessagePipeReader::Delegate* delegate)
    : delegate_(delegate),
      sender_(std::move(sender)),
      receiver_(this, std::move(receiver)) {
  sender_.set_disconnect_handler(base::Bind(&MessagePipeReader::OnPipeError,
                                            base::Unretained(this),
                                            MOJO_RESULT_FAILED_PRECONDITION));
  receiver_.set_disconnect_handler(base::Bind(&MessagePipeReader::OnPipeError,
                                              base::Unretained(this),
                                              MOJO_RESULT_FAILED_PRECONDITION));
}

MessagePipeReader::~MessagePipeReader() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // The pipe should be closed before deletion.
}

void MessagePipeReader::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sender_.reset();
  if (receiver_.is_bound())
    receiver_.reset();
}

bool MessagePipeReader::Send(std::unique_ptr<Message> message) {
  CHECK(message->IsValid());
  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                         "MessagePipeReader::Send", message->flags(),
                         TRACE_EVENT_FLAG_FLOW_OUT);
  base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles;
  MojoResult result = MOJO_RESULT_OK;
  result = ChannelMojo::ReadFromMessageAttachmentSet(message.get(), &handles);
  if (result != MOJO_RESULT_OK)
    return false;

  if (!sender_)
    return false;

  sender_->Receive(MessageView(*message, std::move(handles)));
  DVLOG(4) << "Send " << message->type() << ": " << message->size();
  return true;
}

void MessagePipeReader::GetRemoteInterface(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (!sender_.is_bound())
    return;
  sender_->GetAssociatedInterface(
      name, mojom::GenericInterfaceAssociatedRequest(std::move(handle)));
}

void MessagePipeReader::SetPeerPid(int32_t peer_pid) {
  delegate_->OnPeerPidReceived(peer_pid);
}

void MessagePipeReader::Receive(MessageView message_view) {
  if (!message_view.size()) {
    delegate_->OnBrokenDataReceived();
    return;
  }
  Message message(message_view.data(), message_view.size());
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

  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                         "MessagePipeReader::Receive",
                         message.flags(),
                         TRACE_EVENT_FLAG_FLOW_IN);
  delegate_->OnMessageReceived(message);
}

void MessagePipeReader::GetAssociatedInterface(
    const std::string& name,
    mojom::GenericInterfaceAssociatedRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_)
    delegate_->OnAssociatedInterfaceRequest(name, request.PassHandle());
}

void MessagePipeReader::OnPipeError(MojoResult error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Close();

  // NOTE: The delegate call below may delete |this|.
  if (delegate_)
    delegate_->OnPipeError();
}

}  // namespace internal
}  // namespace IPC
