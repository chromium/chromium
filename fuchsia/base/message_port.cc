// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/message_port.h"

#include <stdint.h>

#include <lib/fit/function.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message_mojom_traits.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"

namespace cr_fuchsia {
namespace {

base::Optional<fuchsia::web::FrameError> MojoMessageFromFidl(
    fuchsia::web::WebMessage fidl_message,
    mojo::Message* mojo_message) {
  if (!fidl_message.has_data()) {
    return fuchsia::web::FrameError::NO_DATA_IN_MESSAGE;
  }

  base::string16 data_utf16;
  if (!cr_fuchsia::ReadUTF8FromVMOAsUTF16(fidl_message.data(), &data_utf16)) {
    return fuchsia::web::FrameError::BUFFER_NOT_UTF8;
  }

  blink::TransferableMessage transferable_message;
  if (fidl_message.has_outgoing_transfer()) {
    for (fuchsia::web::OutgoingTransferable& outgoing :
         *fidl_message.mutable_outgoing_transfer()) {
      transferable_message.ports.emplace_back(
          MessagePortFromFidl(std::move(outgoing.message_port())));
    }
  }

  transferable_message.owned_encoded_message =
      blink::EncodeStringMessage(data_utf16);
  transferable_message.encoded_message =
      transferable_message.owned_encoded_message;
  *mojo_message = blink::mojom::TransferableMessage::SerializeAsMessage(
      &transferable_message);

  return {};
}

base::Optional<fuchsia::web::WebMessage> FidlWebMessageFromMojo(
    mojo::Message mojo_message) {
  blink::TransferableMessage transferable_message;
  if (!blink::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(mojo_message), &transferable_message)) {
    return {};
  }

  fuchsia::web::WebMessage fidl_message;
  if (!transferable_message.ports.empty()) {
    std::vector<fuchsia::web::IncomingTransferable> transferables;
    for (const blink::MessagePortChannel& port : transferable_message.ports) {
      fuchsia::web::IncomingTransferable incoming;
      incoming.set_message_port(MessagePortFromMojo(port.ReleaseHandle()));
      transferables.emplace_back(std::move(incoming));
    }
    fidl_message.set_incoming_transfer(std::move(transferables));
  }

  base::string16 data_utf16;
  if (!blink::DecodeStringMessage(transferable_message.encoded_message,
                                  &data_utf16)) {
    return {};
  }

  std::string data_utf8;
  if (!base::UTF16ToUTF8(data_utf16.data(), data_utf16.size(), &data_utf8))
    return {};

  base::STLClearObject(&data_utf16);

  fuchsia::mem::Buffer data =
      cr_fuchsia::MemBufferFromString(data_utf8, "cr-web-message-from-mojo");
  if (!data.vmo)
    return {};

  fidl_message.set_data(std::move(data));
  return fidl_message;
}

// Defines the implementation of a MessagePort which routes messages from
// FIDL clients to web content, or vice versa. Every MessagePort has a FIDL
// port and a Mojo port.
//
// MessagePort instances are self-managed; they destroy themselves when
// the connection is terminated from either the Mojo or FIDL side.
class MessagePort : public mojo::MessageReceiver {
 protected:
  explicit MessagePort(mojo::ScopedMessagePipeHandle mojo_port) {
    mojo_port_ = std::make_unique<mojo::Connector>(
        std::move(mojo_port), mojo::Connector::SINGLE_THREADED_SEND,
        base::ThreadTaskRunnerHandle::Get());
    mojo_port_->set_incoming_receiver(this);
    mojo_port_->set_connection_error_handler(
        base::BindOnce(&MessagePort::Destroy, base::Unretained(this)));
  }

  ~MessagePort() override = default;

  // Deletes |this|, implicitly disconnecting the FIDL and Mojo ports.
  void Destroy() {
    // |mojo_port_| and |binding_| are implicitly unbound.
    delete this;
  }

  // Sends a message to |mojo_port_|.
  void SendMojoMessage(mojo::Message* message) {
    CHECK(mojo_port_->Accept(message));
  }

  // Called by |mojo_port_| when a Mojo message was received.
  virtual void DeliverMessageToFidl() = 0;

  // Returns the next messagefrom Mojo, or an empty value if there
  // are no more messages in the incoming queue.
  base::Optional<fuchsia::web::WebMessage> GetNextMojoMessage() {
    if (message_queue_.empty())
      return {};

    return std::move(message_queue_.front());
  }

  void OnDeliverMessageToFidlComplete() {
    DCHECK(!message_queue_.empty());

    message_queue_.pop_front();
  }

 private:
  // mojo::MessageReceiver implementation.
  bool Accept(mojo::Message* message) override {
    base::Optional<fuchsia::web::WebMessage> message_converted =
        FidlWebMessageFromMojo(std::move(*message));
    if (!message_converted) {
      DLOG(ERROR) << "Couldn't decode MessageChannel from Mojo pipe.";
      Destroy();
      return false;
    }
    message_queue_.emplace_back(std::move(*message_converted));

    // Start draining the queue if it was empty beforehand.
    if (message_queue_.size() == 1u)
      DeliverMessageToFidl();

    return true;
  }

  base::circular_deque<fuchsia::web::WebMessage> message_queue_;
  std::unique_ptr<mojo::Connector> mojo_port_;

  DISALLOW_COPY_AND_ASSIGN(MessagePort);
};

// Binds a handle to a remote MessagePort to a Mojo MessagePipe.
class FidlMessagePortClient : public MessagePort {
 public:
  FidlMessagePortClient(
      mojo::ScopedMessagePipeHandle mojo_port,
      fidl::InterfaceHandle<fuchsia::web::MessagePort> fidl_port)
      : MessagePort(std::move(mojo_port)), port_(fidl_port.Bind()) {
    ReadMessageFromFidl();

    port_.set_error_handler([this](zx_status_t status) {
      ZX_LOG_IF(ERROR,
                status != ZX_ERR_PEER_CLOSED && status != ZX_ERR_CANCELED,
                status)
          << " MessagePort disconnected.";
      Destroy();
    });
  }

 private:
  ~FidlMessagePortClient() override = default;

  void ReadMessageFromFidl() {
    port_->ReceiveMessage(
        fit::bind_member(this, &FidlMessagePortClient::OnMessageReceived));
  }

  void OnMessageReceived(fuchsia::web::WebMessage message) {
    mojo::Message mojo_message;
    base::Optional<fuchsia::web::FrameError> result =
        MojoMessageFromFidl(std::move(message), &mojo_message);
    if (result) {
      LOG(WARNING) << "Received bad message, error: "
                   << static_cast<int32_t>(*result);
      Destroy();
      return;
    }

    SendMojoMessage(&mojo_message);

    ReadMessageFromFidl();
  }

  void OnMessagePosted(fuchsia::web::MessagePort_PostMessage_Result result) {
    if (result.is_err()) {
      LOG(ERROR) << "PostMessage failed, reason: "
                 << static_cast<int32_t>(result.err());
      Destroy();
      return;
    }

    DeliverMessageToFidl();
  }

  // cr_fuchsia::MessagePort implementation.
  void DeliverMessageToFidl() override {
    base::Optional<fuchsia::web::WebMessage> message = GetNextMojoMessage();
    if (!message)
      return;

    port_->PostMessage(
        std::move(*message),
        fit::bind_member(this, &FidlMessagePortClient::OnMessagePosted));

    OnDeliverMessageToFidlComplete();
  }

  fuchsia::web::MessagePortPtr port_;

  DISALLOW_COPY_AND_ASSIGN(FidlMessagePortClient);
};

// Binds a MessagePort FIDL service from a Mojo MessagePipe.
class FidlMessagePortServer : public fuchsia::web::MessagePort,
                              public MessagePort {
 public:
  explicit FidlMessagePortServer(mojo::ScopedMessagePipeHandle mojo_port)
      : cr_fuchsia::MessagePort(std::move(mojo_port)), binding_(this) {
    binding_.set_error_handler([this](zx_status_t status) {
      ZX_LOG_IF(ERROR,
                status != ZX_ERR_PEER_CLOSED && status != ZX_ERR_CANCELED,
                status)
          << " MessagePort disconnected.";
      Destroy();
    });
  }

  FidlMessagePortServer(
      mojo::ScopedMessagePipeHandle mojo_port,
      fidl::InterfaceRequest<fuchsia::web::MessagePort> request)
      : FidlMessagePortServer(std::move(mojo_port)) {
    binding_.Bind(std::move(request));
  }

  fidl::InterfaceHandle<fuchsia::web::MessagePort> NewBinding() {
    return binding_.NewBinding();
  }

 private:
  ~FidlMessagePortServer() override = default;

  // MessagePort implementation.
  void DeliverMessageToFidl() override {
    // Do nothing if the client hasn't requested a read, or if there's nothing
    // to read.
    if (!pending_receive_message_callback_)
      return;

    base::Optional<fuchsia::web::WebMessage> message = GetNextMojoMessage();
    if (!message)
      return;

    pending_receive_message_callback_(std::move(*message));
    pending_receive_message_callback_ = {};
    OnDeliverMessageToFidlComplete();
  }

  // fuchsia::web::MessagePort implementation.
  void PostMessage(fuchsia::web::WebMessage message,
                   PostMessageCallback callback) override {
    mojo::Message mojo_message;
    base::Optional<fuchsia::web::FrameError> status =
        MojoMessageFromFidl(std::move(message), &mojo_message);

    if (status) {
      LOG(ERROR) << "Error when reading message from FIDL: "
                 << static_cast<int32_t>(*status);
      Destroy();
      return;
    }

    SendMojoMessage(&mojo_message);
    fuchsia::web::MessagePort_PostMessage_Result result;
    result.set_response(fuchsia::web::MessagePort_PostMessage_Response());
    callback(std::move(result));
  }

  void ReceiveMessage(ReceiveMessageCallback callback) override {
    if (pending_receive_message_callback_) {
      LOG(WARNING)
          << "ReceiveMessage called multiple times without acknowledgement.";
      Destroy();
      return;
    }
    pending_receive_message_callback_ = std::move(callback);
    DeliverMessageToFidl();
  }

  PostMessageCallback post_message_ack_;
  ReceiveMessageCallback pending_receive_message_callback_;
  fidl::Binding<fuchsia::web::MessagePort> binding_;

  DISALLOW_COPY_AND_ASSIGN(FidlMessagePortServer);
};

}  // namespace

mojo::ScopedMessagePipeHandle MessagePortFromFidl(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> port) {
  mojo::ScopedMessagePipeHandle client_port;
  mojo::ScopedMessagePipeHandle content_port;
  mojo::CreateMessagePipe(0, &content_port, &client_port);

  new FidlMessagePortServer(std::move(client_port), std::move(port));

  return content_port;
}

mojo::ScopedMessagePipeHandle MessagePortFromFidl(
    fidl::InterfaceHandle<fuchsia::web::MessagePort> port) {
  mojo::ScopedMessagePipeHandle client_port;
  mojo::ScopedMessagePipeHandle content_port;
  mojo::CreateMessagePipe(0, &content_port, &client_port);

  new FidlMessagePortClient(std::move(content_port), std::move(port));

  return client_port;
}

fidl::InterfaceHandle<fuchsia::web::MessagePort> MessagePortFromMojo(
    mojo::ScopedMessagePipeHandle port) {
  return (new FidlMessagePortServer(std::move(port)))->NewBinding();
}

}  // namespace cr_fuchsia
