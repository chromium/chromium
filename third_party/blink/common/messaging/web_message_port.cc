// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/web_message_port.h"

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/messaging/transferable_message_mojom_traits.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"

namespace blink {

WebMessagePort::Message::Message() = default;
WebMessagePort::Message::Message(Message&&) = default;
WebMessagePort::Message& WebMessagePort::Message::operator=(Message&&) =
    default;
WebMessagePort::Message::~Message() = default;

WebMessagePort::Message::Message(const std::u16string& data) : data(data) {}

WebMessagePort::Message::Message(std::vector<WebMessagePort> ports)
    : ports(std::move(ports)) {}

WebMessagePort::Message::Message(WebMessagePort&& port) {
  ports.emplace_back(std::move(port));
}

WebMessagePort::Message::Message(const std::u16string& data,
                                 std::vector<WebMessagePort> ports)
    : data(data), ports(std::move(ports)) {}

WebMessagePort::Message::Message(const std::u16string& data,
                                 WebMessagePort port)
    : data(data) {
  ports.emplace_back(std::move(port));
}

WebMessagePort::MessageReceiver::MessageReceiver() = default;
WebMessagePort::MessageReceiver::~MessageReceiver() = default;

bool WebMessagePort::MessageReceiver::OnMessage(Message) {
  return false;
}

WebMessagePort::WebMessagePort() = default;

WebMessagePort::WebMessagePort(WebMessagePort&& other) {
  Take(std::move(other));
}

WebMessagePort& WebMessagePort::operator=(WebMessagePort&& other) {
  CloseIfNecessary();
  Take(std::move(other));
  return *this;
}

WebMessagePort::~WebMessagePort() {
  CloseIfNecessary();
}

// static
std::pair<WebMessagePort, WebMessagePort> WebMessagePort::CreatePair() {
  MessagePortDescriptorPair port_pair;
  return std::make_pair(WebMessagePort(port_pair.TakePort0()),
                        WebMessagePort(port_pair.TakePort1()));
}

// static
WebMessagePort WebMessagePort::Create(MessagePortDescriptor port) {
  DCHECK(port.IsValid());
  DCHECK(!port.IsEntangled());

  return WebMessagePort(std::move(port));
}

void WebMessagePort::SetReceiver(
    MessageReceiver* receiver,
    scoped_refptr<base::SequencedTaskRunner> runner) {
  DCHECK(receiver);
  DCHECK(runner.get());

  DCHECK(port_.IsValid());
  DCHECK(!connector_);
  DCHECK(!is_closed_);
  DCHECK(!is_errored_);
  DCHECK(is_transferable_);

  is_transferable_ = false;
  receiver_ = receiver;
  connector_ = std::make_unique<mojo::Connector>(
      port_.TakeHandleToEntangleWithEmbedder(),
      mojo::Connector::SINGLE_THREADED_SEND, std::move(runner));
  connector_->set_incoming_receiver(this);
  connector_->set_connection_error_handler(
      base::BindOnce(&WebMessagePort::OnPipeError, base::Unretained(this)));
}

void WebMessagePort::ClearReceiver() {
  if (!connector_)
    return;
  port_.GiveDisentangledHandle(connector_->PassMessagePipe());
  connector_.reset();
  receiver_ = nullptr;
}

base::SequencedTaskRunner* WebMessagePort::GetTaskRunner() const {
  if (!connector_)
    return nullptr;
  return connector_->task_runner();
}

MessagePortDescriptor WebMessagePort::PassPort() {
  DCHECK(is_transferable_);

  // Clear the receiver, which takes the handle out of the connector if it
  // exists, and puts it back in |port_|.
  ClearReceiver();
  MessagePortDescriptor port = std::move(port_);
  Reset();
  return port;
}

const base::UnguessableToken& WebMessagePort::GetEmbedderAgentClusterID() {
  // This is creating a single agent cluster ID that would represent the
  // embedder in MessagePort IPCs. While we could create a new ID on each call,
  // providing a consistent one saves RNG work and could be useful in the future
  // if we'd want to consistently identify messages from the embedder.
  static const auto agent_cluster_id = base::UnguessableToken::Create();
  return agent_cluster_id;
}

WebMessagePort::WebMessagePort(MessagePortDescriptor&& port)
    : port_(std::move(port)), is_closed_(false), is_transferable_(true) {
  DCHECK(port_.IsValid());
}

bool WebMessagePort::CanPostMessage() const {
  return connector_ && connector_->is_valid() && !is_closed_ && !is_errored_ &&
         receiver_;
}

bool WebMessagePort::PostMessage(Message&& message) {
  if (!CanPostMessage())
    return false;

  // Extract the underlying handles for transport in a
  // blink::TransferableMessage.
  std::vector<MessagePortDescriptor> ports;
  for (auto& port : message.ports) {
    // We should not be trying to send ourselves in a message. Mojo prevents
    // this at a deeper level, but we can also check here.
    DCHECK_NE(this, &port);

    ports.emplace_back(port.PassPort());
  }

  // Build the message.
  // TODO(chrisha): Finally kill off MessagePortChannel, once
  // MessagePortDescriptor more thoroughly plays that role.
  blink::TransferableMessage transferable_message =
      blink::EncodeWebMessagePayload(std::move(message.data));
  transferable_message.ports =
      blink::MessagePortChannel::CreateFromHandles(std::move(ports));

  // Get the embedder assigned cluster ID, as these messages originate from the
  // embedder.
  transferable_message.sender_agent_cluster_id = GetEmbedderAgentClusterID();

  // TODO(chrisha): Notify the instrumentation delegate of a message being sent!

  // Send via Mojo. The message should never be malformed so should always be
  // accepted.
  mojo::Message mojo_message =
      blink::mojom::TransferableMessage::SerializeAsMessage(
          &transferable_message);
  CHECK(connector_->Accept(&mojo_message));

  return true;
}

bool WebMessagePort::IsValid() const {
  if (connector_)
    return connector_->is_valid();
  return port_.IsValid();
}

void WebMessagePort::Close() {
  CloseIfNecessary();
}

void WebMessagePort::Reset() {
  CloseIfNecessary();
  is_closed_ = true;
  is_errored_ = false;
  is_transferable_ = false;
}

void WebMessagePort::Take(WebMessagePort&& other) {
  port_ = std::move(other.port_);
  connector_ = std::move(other.connector_);
  is_closed_ = std::exchange(other.is_closed_, true);
  is_errored_ = std::exchange(other.is_errored_, false);
  is_transferable_ = std::exchange(other.is_transferable_, false);
  receiver_ = std::exchange(other.receiver_, nullptr);
}

void WebMessagePort::OnPipeError() {
  DCHECK(!is_transferable_);
  if (is_errored_)
    return;
  is_errored_ = true;
  if (receiver_)
    receiver_->OnPipeError();
}

void WebMessagePort::CloseIfNecessary() {
  if (is_closed_)
    return;
  is_closed_ = true;
  ClearReceiver();
  port_.Reset();
}

bool WebMessagePort::Accept(mojo::Message* mojo_message) {
  DCHECK(receiver_);
  DCHECK(!is_transferable_);

  // Deserialize the message.
  blink::TransferableMessage transferable_message;
  if (!blink::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(*mojo_message), &transferable_message)) {
    return false;
  }
  auto ports = std::move(transferable_message.ports);
  // Decode the string portion of the message.
  Message message;
  std::optional<WebMessagePayload> optional_payload =
      blink::DecodeToWebMessagePayload(std::move(transferable_message));
  if (!optional_payload)
    return false;
  auto& payload = optional_payload.value();
  if (auto* str = absl::get_if<std::u16string>(&payload)) {
    message.data = std::move(*str);
  } else {
    return false;
  }

  // Convert raw handles to MessagePorts.
  // TODO(chrisha): Kill off MessagePortChannel entirely!
  auto handles = blink::MessagePortChannel::ReleaseHandles(ports);
  for (auto& handle : handles) {
    message.ports.emplace_back(WebMessagePort(std::move(handle)));
  }

  // Pass the message on to the receiver.
  return receiver_->OnMessage(std::move(message));
}

}  // namespace blink
