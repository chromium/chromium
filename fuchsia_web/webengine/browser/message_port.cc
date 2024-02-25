// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/message_port.h"

#include <fuchsia/mem/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fit/function.h>
#include <lib/fpromise/result.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/functional/bind.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
namespace {

using BlinkMessage = blink::WebMessagePort::Message;

// Converts a fuchsia::web::WebMessage to a BlinkMessage.
// An empty result indicates that conversion was successful.
// Data validation errors are returned as a FrameError.
std::optional<fuchsia::web::FrameError> BlinkMessageFromFidl(
    fuchsia::web::WebMessage fidl_message,
    BlinkMessage* blink_message) {
  if (!fidl_message.has_data()) {
    return fuchsia::web::FrameError::NO_DATA_IN_MESSAGE;
  }

  std::optional<std::u16string> data_utf16 =
      base::ReadUTF8FromVMOAsUTF16(fidl_message.data());
  if (!data_utf16) {
    return fuchsia::web::FrameError::BUFFER_NOT_UTF8;
  }
  blink_message->data = std::move(*data_utf16);

  if (fidl_message.has_outgoing_transfer() &&
      fidl_message.has_incoming_transfer()) {
    DLOG(WARNING) << "WebMessage may only have incoming or outgoing transfer.";
    return fuchsia::web::FrameError::INTERNAL_ERROR;
  }
  if (fidl_message.has_outgoing_transfer()) {
    for (fuchsia::web::OutgoingTransferable& transferrable :
         *fidl_message.mutable_outgoing_transfer()) {
      if (!transferrable.is_message_port())
        return fuchsia::web::FrameError::INTERNAL_ERROR;
      blink_message->ports.push_back(
          BlinkMessagePortFromFidl(std::move(transferrable.message_port())));
    }
  } else if (fidl_message.has_incoming_transfer()) {
    for (fuchsia::web::IncomingTransferable& transferrable :
         *fidl_message.mutable_incoming_transfer()) {
      if (!transferrable.is_message_port())
        return fuchsia::web::FrameError::INTERNAL_ERROR;
      blink_message->ports.push_back(
          BlinkMessagePortFromFidl(std::move(transferrable.message_port())));
    }
  }

  return std::nullopt;
}

// Defines a MessagePortAdapter, which translates and routes messages between a
// FIDL MessagePort and a blink::WebMessagePort. Every MessagePortAdapter has
// exactly one FIDL MessagePort and one blink::WebMessagePort.
//
// MessagePortAdapter instances are self-managed; they destroy themselves when
// the connection is terminated from either the Blink or FIDL side.
class MessagePortAdapter : public blink::WebMessagePort::MessageReceiver {
 public:
  MessagePortAdapter(const MessagePortAdapter&) = delete;
  MessagePortAdapter& operator=(const MessagePortAdapter&) = delete;

 protected:
  explicit MessagePortAdapter(blink::WebMessagePort blink_port)
      : blink_port_(std::move(blink_port)) {
    blink_port_.SetReceiver(this,
                            base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  ~MessagePortAdapter() override = default;

  // Deletes |this|, implicitly disconnecting the FIDL and Blink ports.
  void Destroy() { delete this; }

  // Sends a message to |blink_port_|.
  void SendBlinkMessage(BlinkMessage message) {
    CHECK(blink_port_.PostMessage(std::move(message)));
  }

  // Called when a Blink message was received through |blink_port_|.
  virtual void DeliverMessageToFidl() = 0;

  // Returns the next messagefrom Blink, or an empty value if there
  // are no more messages in the incoming queue.
  std::optional<fuchsia::web::WebMessage> GetNextBlinkMessage() {
    if (message_queue_.empty())
      return std::nullopt;

    return std::move(message_queue_.front());
  }

  void OnDeliverMessageToFidlComplete() {
    DCHECK(!message_queue_.empty());
    message_queue_.pop_front();
  }

 private:
  // blink::WebMessagePort::MessageReceiver implementation:
  bool OnMessage(BlinkMessage message) override {
    std::optional<fuchsia::web::WebMessage> message_converted =
        FidlWebMessageFromBlink(std::move(message),
                                TransferableHostType::kLocal);
    if (!message_converted) {
      DLOG(ERROR) << "Couldn't decode WebMessage from blink::WebMessagePort.";
      Destroy();
      return false;
    }
    message_queue_.emplace_back(std::move(*message_converted));

    // Start draining the queue if it was empty beforehand.
    if (message_queue_.size() == 1u)
      DeliverMessageToFidl();

    return true;
  }

  // blink::WebMessagePort::MessageReceiver implementation:
  void OnPipeError() override { Destroy(); }

  base::circular_deque<fuchsia::web::WebMessage> message_queue_;
  blink::WebMessagePort blink_port_;
};

// Binds a handle to a remote MessagePort to a blink::WebMessagePort.
class FidlMessagePortClientAdapter : public MessagePortAdapter {
 public:
  FidlMessagePortClientAdapter(
      blink::WebMessagePort blink_port,
      fidl::InterfaceHandle<fuchsia::web::MessagePort> fidl_port)
      : MessagePortAdapter(std::move(blink_port)), port_(fidl_port.Bind()) {
    ReadMessageFromFidl();

    port_.set_error_handler([this](zx_status_t status) {
      ZX_LOG_IF(ERROR,
                status != ZX_ERR_PEER_CLOSED && status != ZX_ERR_CANCELED,
                status)
          << " MessagePort disconnected.";
      Destroy();
    });
  }

  FidlMessagePortClientAdapter(const FidlMessagePortClientAdapter&) = delete;
  FidlMessagePortClientAdapter& operator=(const FidlMessagePortClientAdapter&) =
      delete;

  fidl::InterfaceRequest<fuchsia::web::MessagePort> NewRequest() {
    return port_.NewRequest();
  }

 private:
  ~FidlMessagePortClientAdapter() override = default;

  void ReadMessageFromFidl() {
    port_->ReceiveMessage(fit::bind_member(
        this, &FidlMessagePortClientAdapter::OnMessageReceived));
  }

  void OnMessageReceived(fuchsia::web::WebMessage message) {
    BlinkMessage blink_message;
    std::optional<fuchsia::web::FrameError> result =
        BlinkMessageFromFidl(std::move(message), &blink_message);
    if (result) {
      LOG(WARNING) << "Received bad message, error: "
                   << static_cast<int32_t>(*result);
      Destroy();
      return;
    }

    SendBlinkMessage(std::move(blink_message));

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

  // MessagePortAdapter implementation.
  void DeliverMessageToFidl() override {
    std::optional<fuchsia::web::WebMessage> message = GetNextBlinkMessage();
    if (!message)
      return;

    port_->PostMessage(
        std::move(*message),
        fit::bind_member(this, &FidlMessagePortClientAdapter::OnMessagePosted));

    OnDeliverMessageToFidlComplete();
  }

  fuchsia::web::MessagePortPtr port_;
};

// Binds a MessagePort FIDL service from a blink::WebMessagePort.
class FidlMessagePortServerAdapter : public fuchsia::web::MessagePort,
                                     public MessagePortAdapter {
 public:
  explicit FidlMessagePortServerAdapter(blink::WebMessagePort blink_port)
      : MessagePortAdapter(std::move(blink_port)), binding_(this) {
    binding_.set_error_handler([this](zx_status_t status) {
      ZX_LOG_IF(ERROR,
                status != ZX_ERR_PEER_CLOSED && status != ZX_ERR_CANCELED,
                status)
          << " MessagePort disconnected.";
      Destroy();
    });
  }

  FidlMessagePortServerAdapter(
      blink::WebMessagePort blink_port,
      fidl::InterfaceRequest<fuchsia::web::MessagePort> request)
      : FidlMessagePortServerAdapter(std::move(blink_port)) {
    binding_.Bind(std::move(request));
  }

  FidlMessagePortServerAdapter(const FidlMessagePortServerAdapter&) = delete;
  FidlMessagePortServerAdapter& operator=(const FidlMessagePortServerAdapter&) =
      delete;

  fidl::InterfaceHandle<fuchsia::web::MessagePort> NewBinding() {
    return binding_.NewBinding();
  }

 private:
  ~FidlMessagePortServerAdapter() override = default;

  // MessagePortAdapter implementation.
  void DeliverMessageToFidl() override {
    // Do nothing if the client hasn't requested a read, or if there's nothing
    // to read.
    if (!pending_receive_message_callback_)
      return;

    std::optional<fuchsia::web::WebMessage> message = GetNextBlinkMessage();
    if (!message)
      return;

    pending_receive_message_callback_(std::move(*message));
    pending_receive_message_callback_ = {};
    OnDeliverMessageToFidlComplete();
  }

  // fuchsia::web::MessagePort implementation.
  void PostMessage(fuchsia::web::WebMessage message,
                   PostMessageCallback callback) override {
    BlinkMessage blink_message;
    std::optional<fuchsia::web::FrameError> status =
        BlinkMessageFromFidl(std::move(message), &blink_message);

    if (status) {
      LOG(ERROR) << "Error when reading message from FIDL: "
                 << static_cast<int32_t>(*status);
      Destroy();
      return;
    }

    SendBlinkMessage(std::move(blink_message));
    callback(fpromise::ok());
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
};

fidl::InterfaceRequest<fuchsia::web::MessagePort>
RemoteFidlMessagePortFromBlink(blink::WebMessagePort blink_port) {
  fidl::InterfaceHandle<fuchsia::web::MessagePort> fidl_handle;
  auto request = fidl_handle.NewRequest();
  new FidlMessagePortClientAdapter(std::move(blink_port),
                                   std::move(fidl_handle));
  return request;
}

}  // namespace

// Methods for constructing MessagePortAdapters for various port types and
// origins. The adapters manage their own lifetimes and will self-delete when
// either endpoint of their channels are disconnected.

blink::WebMessagePort BlinkMessagePortFromFidl(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> fidl_port) {
  auto port_pair = blink::WebMessagePort::CreatePair();
  new FidlMessagePortServerAdapter(std::move(port_pair.first),
                                   std::move(fidl_port));
  return std::move(port_pair.second);
}

blink::WebMessagePort BlinkMessagePortFromFidl(
    fidl::InterfaceHandle<fuchsia::web::MessagePort> fidl_port) {
  auto port_pair = blink::WebMessagePort::CreatePair();
  new FidlMessagePortClientAdapter(std::move(port_pair.first),
                                   std::move(fidl_port));
  return std::move(port_pair.second);
}

fidl::InterfaceHandle<fuchsia::web::MessagePort> FidlMessagePortFromBlink(
    blink::WebMessagePort blink_port) {
  auto* adapter = new FidlMessagePortServerAdapter(std::move(blink_port));
  return adapter->NewBinding();
}

std::optional<fuchsia::web::WebMessage> FidlWebMessageFromBlink(
    BlinkMessage blink_message,
    TransferableHostType port_type) {
  fuchsia::web::WebMessage fidl_message;

  if (!blink_message.ports.empty()) {
    switch (port_type) {
      case TransferableHostType::kLocal:
        for (blink::WebMessagePort& port : blink_message.ports) {
          fuchsia::web::IncomingTransferable incoming;
          incoming.set_message_port(FidlMessagePortFromBlink(std::move(port)));
          fidl_message.mutable_incoming_transfer()->push_back(
              std::move(incoming));
        }
        break;
      case TransferableHostType::kRemote:
        for (blink::WebMessagePort& port : blink_message.ports) {
          fuchsia::web::OutgoingTransferable outgoing;
          outgoing.set_message_port(
              RemoteFidlMessagePortFromBlink(std::move(port)));
          fidl_message.mutable_outgoing_transfer()->push_back(
              std::move(outgoing));
        }
        break;
    }
    blink_message.ports.clear();
  }

  std::u16string data_utf16 = std::move(blink_message.data);
  std::string data_utf8;
  if (!base::UTF16ToUTF8(data_utf16.data(), data_utf16.size(), &data_utf8))
    return std::nullopt;

  base::STLClearObject(&data_utf16);

  constexpr char kBufferVmoName[] = "cr-web-message-from-blink";
  fuchsia::mem::Buffer data_buffer =
      base::MemBufferFromString(data_utf8, kBufferVmoName);
  if (!data_buffer.vmo)
    return std::nullopt;

  fidl_message.set_data(std::move(data_buffer));
  return fidl_message;
}
