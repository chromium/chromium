// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/rpc_broker.h"

#include <utility>

#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"

namespace media {
namespace remoting {

namespace {

std::ostream& operator<<(std::ostream& out, const pb::RpcMessage& message) {
  out << "handle=" << message.handle() << ", proc=" << message.proc();
  switch (message.rpc_oneof_case()) {
    case pb::RpcMessage::kIntegerValue:
      out << ", integer_value=" << message.integer_value();
      break;
    case pb::RpcMessage::kInteger64Value:
      out << ", integer64_value=" << message.integer64_value();
      break;
    case pb::RpcMessage::kDoubleValue:
      out << ", double_value=" << message.double_value();
      break;
    case pb::RpcMessage::kBooleanValue:
      out << ", boolean_value=" << message.boolean_value();
      break;
    case pb::RpcMessage::kStringValue:
      out << ", string_value=" << message.string_value();
      break;
    default:
      out << ", rpc_oneof=" << message.rpc_oneof_case();
      break;
  }
  return out;
}

}  // namespace

RpcBroker::RpcBroker(const SendMessageCallback& send_message_cb)
    : next_handle_(kFirstHandle), send_message_cb_(send_message_cb) {}

RpcBroker::~RpcBroker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  receive_callbacks_.clear();
}

int RpcBroker::GetUniqueHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return next_handle_++;
}

void RpcBroker::RegisterMessageReceiverCallback(
    int handle,
    const ReceiveMessageCallback& callback) {
  VLOG(2) << __func__ << "handle=" << handle;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(receive_callbacks_.find(handle) == receive_callbacks_.end());
  receive_callbacks_[handle] = callback;
}

void RpcBroker::UnregisterMessageReceiverCallback(int handle) {
  VLOG(2) << __func__ << " handle=" << handle;
  DCHECK(thread_checker_.CalledOnValidThread());
  receive_callbacks_.erase(handle);
}

void RpcBroker::ProcessMessageFromRemote(
    std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(message);
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(3) << __func__ << ": " << *message;
  const auto entry = receive_callbacks_.find(message->handle());
  if (entry == receive_callbacks_.end()) {
    VLOG(1) << "unregistered handle: " << message->handle();
    return;
  }
  entry->second.Run(std::move(message));
}

void RpcBroker::SendMessageToRemote(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(message);
  VLOG(3) << __func__ << ": " << *message;
  std::unique_ptr<std::vector<uint8_t>> serialized_message(
      new std::vector<uint8_t>(message->ByteSize()));
  CHECK(message->SerializeToArray(serialized_message->data(),
                                  serialized_message->size()));
  send_message_cb_.Run(std::move(serialized_message));
}

base::WeakPtr<RpcBroker> RpcBroker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RpcBroker::SetMessageCallbackForTesting(
    const SendMessageCallback& send_message_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  send_message_cb_ = send_message_cb;
}

}  // namespace remoting
}  // namespace media
