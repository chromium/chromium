// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_data_stream_adapter.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {
namespace protocol {

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
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(base::DoNothing::Once<
                           rtc::scoped_refptr<webrtc::DataChannelInterface>>(),
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
  // This shouldn't DCHECK in the CLOSED case, because the connection may be
  // abruptly closed at any time and the caller may not have been notified, yet.
  // The message will still be enqueued so that the outstanding done callbacks
  // are dropped at the expected time in the expected order.
  DCHECK(state_ != State::CONNECTING);

  rtc::CopyOnWriteBuffer buffer;
  buffer.SetSize(message->ByteSize());
  message->SerializeWithCachedSizesToArray(
      reinterpret_cast<uint8_t*>(buffer.data()));
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
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(message.done_callback));
    }
  }
}

void WebrtcDataStreamAdapter::OnStateChange() {
  switch (channel_->state()) {
    case webrtc::DataChannelInterface::kOpen:
      DCHECK(state_ == State::CONNECTING);
      state_ = State::OPEN;
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&WebrtcDataStreamAdapter::InvokeOpenEvent,
                                    weak_ptr_factory_.GetWeakPtr()));
      break;

    case webrtc::DataChannelInterface::kClosing:
      if (state_ != State::CLOSED) {
        state_ = State::CLOSED;
        base::SequencedTaskRunnerHandle::Get()->PostTask(
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
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcDataStreamAdapter::InvokeMessageEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(buffer)));
}

void WebrtcDataStreamAdapter::OnBufferedAmountChange(uint64_t previous_amount) {
  // WebRTC explicitly doesn't support sending from observer callbacks, so post
  // a task to let the stack unwind.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
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

}  // namespace protocol
}  // namespace remoting
