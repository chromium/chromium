// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_data_stream_adapter.h"

#include <stdint.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting::protocol {

WebrtcDataStreamAdapter::WebrtcDataStreamAdapter(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
    : channel_(channel.get()) {
  channel_->RegisterObserver(this);
  DCHECK_EQ(channel_->state(), webrtc::DataChannelInterface::kConnecting);
}

WebrtcDataStreamAdapter::~WebrtcDataStreamAdapter() {
  if (channel_) {
    channel_->UnregisterObserver();
    channel_->Close();

    // Destroy |channel_| asynchronously as it may be on stack.
    // TODO(dcheng): This could probably be ReleaseSoon.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce([](rtc::scoped_refptr<webrtc::DataChannelInterface>) {},
                       std::move(channel_)));
  }
}

void WebrtcDataStreamAdapter::Start(EventHandler* event_handler) {
  DCHECK(!event_handler_);
  DCHECK(event_handler);

  event_handler_ = event_handler;
}

void WebrtcDataStreamAdapter::Send(google::protobuf::MessageLite* message,
                                   base::OnceClosure done) {
  rtc::CopyOnWriteBuffer buffer;
  buffer.SetSize(message->ByteSize());
  message->SerializeWithCachedSizesToArray(buffer.MutableData());
  pending_messages_.emplace(
      webrtc::DataBuffer(std::move(buffer), true /* binary */),
      std::move(done));

  SendMessagesIfReady();
}

void WebrtcDataStreamAdapter::SendMessagesIfReady() {
  // We use our own send queue instead of queuing multiple messages in the
  // data-channel queue so we can invoke the done callback as close to the
  // message actually being sent as possible and avoid overrunning the data-
  // channel queue. There is also lower-level buffering beneath the data-channel
  // queue, which we do want to keep full to ensure the link is fully utilized.

  // Send messages to the data channel until it has to add one to its own queue.
  // This ensures that the lower-level buffers remain full.
  while (state_ == State::OPEN && channel_->buffered_amount() == 0 &&
         !pending_messages_.empty()) {
    PendingMessage message = std::move(pending_messages_.front());
    pending_messages_.pop();
    if (!channel_->Send(std::move(message.buffer))) {
      LOG(ERROR) << "Send failed on data channel " << channel_->label();
      channel_->Close();
      return;
    }

    if (message.done_callback) {
      // Invoke callback asynchronously to avoid nested calls to Send.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(message.done_callback));
    }
  }
}

void WebrtcDataStreamAdapter::OnStateChange() {
  switch (channel_->state()) {
    case webrtc::DataChannelInterface::kOpen:
      DCHECK(state_ == State::CONNECTING);
      state_ = State::OPEN;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&WebrtcDataStreamAdapter::InvokeOpenEvent,
                                    weak_ptr_factory_.GetWeakPtr()));
      break;

    case webrtc::DataChannelInterface::kClosing:
      if (state_ != State::CLOSED) {
        state_ = State::CLOSED;
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&WebrtcDataStreamAdapter::InvokeClosedEvent,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      break;

    case webrtc::DataChannelInterface::kConnecting:
    case webrtc::DataChannelInterface::kClosed:
      break;
  }
}

void WebrtcDataStreamAdapter::OnMessage(const webrtc::DataBuffer& rtc_buffer) {
  if (state_ != State::OPEN) {
    LOG(ERROR) << "Dropping a message received when the channel is not open.";
    return;
  }

  std::unique_ptr<CompoundBuffer> buffer(new CompoundBuffer());
  buffer->AppendCopyOf(reinterpret_cast<const char*>(rtc_buffer.data.data()),
                       rtc_buffer.data.size());
  buffer->Lock();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcDataStreamAdapter::InvokeMessageEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(buffer)));
}

void WebrtcDataStreamAdapter::OnBufferedAmountChange(uint64_t previous_amount) {
  // WebRTC explicitly doesn't support sending from observer callbacks, so post
  // a task to let the stack unwind.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebrtcDataStreamAdapter::SendMessagesIfReady,
                                weak_ptr_factory_.GetWeakPtr()));
}

void WebrtcDataStreamAdapter::InvokeOpenEvent() {
  event_handler_->OnMessagePipeOpen();
}

void WebrtcDataStreamAdapter::InvokeClosedEvent() {
  event_handler_->OnMessagePipeClosed();
}

void WebrtcDataStreamAdapter::InvokeMessageEvent(
    std::unique_ptr<CompoundBuffer> buffer) {
  event_handler_->OnMessageReceived(std::move(buffer));
}

WebrtcDataStreamAdapter::PendingMessage::PendingMessage(
    webrtc::DataBuffer buffer,
    base::OnceClosure done_callback)
    : buffer(std::move(buffer)), done_callback(std::move(done_callback)) {}

WebrtcDataStreamAdapter::PendingMessage&
WebrtcDataStreamAdapter::PendingMessage::operator=(PendingMessage&&) = default;

WebrtcDataStreamAdapter::PendingMessage::PendingMessage(PendingMessage&&) =
    default;

WebrtcDataStreamAdapter::PendingMessage::~PendingMessage() = default;

}  // namespace remoting::protocol
