// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_handle_impl.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/websockets/websocket_handle_client.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_log.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

const uint16_t kAbnormalShutdownOpCode = 1006;

}  // namespace

WebSocketHandleImpl::WebSocketHandleImpl()
    : client_(nullptr), client_binding_(this) {
  NETWORK_DVLOG(1) << this << " created";
}

WebSocketHandleImpl::~WebSocketHandleImpl() {
  NETWORK_DVLOG(1) << this << " deleted";

  if (websocket_)
    websocket_->StartClosingHandshake(kAbnormalShutdownOpCode, g_empty_string);
}

void WebSocketHandleImpl::Connect(network::mojom::blink::WebSocketPtr websocket,
                                  const KURL& url,
                                  const Vector<String>& protocols,
                                  const KURL& site_for_cookies,
                                  const String& user_agent_override,
                                  WebSocketHandleClient* client,
                                  base::SingleThreadTaskRunner* task_runner) {
  DCHECK(!websocket_);
  websocket_ = std::move(websocket);
  websocket_.set_connection_error_with_reason_handler(WTF::Bind(
      &WebSocketHandleImpl::OnConnectionError, WTF::Unretained(this)));
  DCHECK(websocket_);

  NETWORK_DVLOG(1) << this << " connect(" << url.GetString() << ")";

  DCHECK(!client_);
  DCHECK(client);
  client_ = client;

  network::mojom::blink::WebSocketClientPtr client_proxy;
  Vector<network::mojom::blink::HttpHeaderPtr> additional_headers;
  if (!user_agent_override.IsNull()) {
    additional_headers.push_back(network::mojom::blink::HttpHeader::New(
        HTTPNames::User_Agent, user_agent_override));
  }
  client_binding_.Bind(mojo::MakeRequest(&client_proxy, task_runner));
  websocket_->AddChannelRequest(url, protocols, site_for_cookies,
                                std::move(additional_headers),
                                std::move(client_proxy));
}

void WebSocketHandleImpl::Send(bool fin,
                               WebSocketHandle::MessageType type,
                               const char* data,
                               wtf_size_t size) {
  DCHECK(websocket_);

  network::mojom::blink::WebSocketMessageType type_to_pass;
  switch (type) {
    case WebSocketHandle::kMessageTypeContinuation:
      type_to_pass = network::mojom::blink::WebSocketMessageType::CONTINUATION;
      break;
    case WebSocketHandle::kMessageTypeText:
      type_to_pass = network::mojom::blink::WebSocketMessageType::TEXT;
      break;
    case WebSocketHandle::kMessageTypeBinary:
      type_to_pass = network::mojom::blink::WebSocketMessageType::BINARY;
      break;
    default:
      NOTREACHED();
      return;
  }

  NETWORK_DVLOG(1) << this << " send(" << fin << ", " << type_to_pass << ", "
                   << "(data size = " << size << "))";

  // TODO(darin): Avoid this copy.
  Vector<uint8_t> data_to_pass(size);
  std::copy(data, data + size, data_to_pass.begin());

  websocket_->SendFrame(fin, type_to_pass, data_to_pass);
}

void WebSocketHandleImpl::FlowControl(int64_t quota) {
  DCHECK(websocket_);

  NETWORK_DVLOG(1) << this << " flowControl(" << quota << ")";

  websocket_->SendFlowControl(quota);
}

void WebSocketHandleImpl::Close(unsigned short code, const String& reason) {
  DCHECK(websocket_);

  NETWORK_DVLOG(1) << this << " close(" << code << ", " << reason << ")";

  websocket_->StartClosingHandshake(code,
                                    reason.IsNull() ? g_empty_string : reason);
}

void WebSocketHandleImpl::Disconnect() {
  websocket_.reset();
  client_ = nullptr;
}

void WebSocketHandleImpl::OnConnectionError(uint32_t custom_reason,
                                            const std::string& description) {
  // Our connection to the WebSocket was dropped. This could be due to
  // exceeding the maximum number of concurrent websockets from this process.
  String failure_message;
  if (custom_reason ==
      network::mojom::blink::WebSocket::kInsufficientResources) {
    failure_message =
        description.empty()
            ? "Insufficient resources"
            : String::FromUTF8(description.c_str(), description.size());
  } else {
    DCHECK(description.empty());
    failure_message = "Unspecified reason";
  }
  OnFailChannel(failure_message);
}

void WebSocketHandleImpl::OnFailChannel(const String& message) {
  NETWORK_DVLOG(1) << this << " OnFailChannel(" << message << ")";

  WebSocketHandleClient* client = client_;
  Disconnect();
  if (!client)
    return;

  client->DidFail(this, message);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnStartOpeningHandshake(
    network::mojom::blink::WebSocketHandshakeRequestPtr request) {
  NETWORK_DVLOG(1) << this << " OnStartOpeningHandshake("
                   << request->url.GetString() << ")";
  client_->DidStartOpeningHandshake(this, std::move(request));
}

void WebSocketHandleImpl::OnFinishOpeningHandshake(
    network::mojom::blink::WebSocketHandshakeResponsePtr response) {
  NETWORK_DVLOG(1) << this << " OnFinishOpeningHandshake("
                   << response->url.GetString() << ")";
  client_->DidFinishOpeningHandshake(this, std::move(response));
}

void WebSocketHandleImpl::OnAddChannelResponse(const String& protocol,
                                               const String& extensions) {
  NETWORK_DVLOG(1) << this << " OnAddChannelResponse(" << protocol << ", "
                   << extensions << ")";

  if (!client_)
    return;

  client_->DidConnect(this, protocol, extensions);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDataFrame(
    bool fin,
    network::mojom::blink::WebSocketMessageType type,
    const Vector<uint8_t>& data) {
  NETWORK_DVLOG(1) << this << " OnDataFrame(" << fin << ", " << type << ", "
                   << "(data size = " << data.size() << "))";
  if (!client_)
    return;

  WebSocketHandle::MessageType type_to_pass =
      WebSocketHandle::kMessageTypeContinuation;
  switch (type) {
    case network::mojom::blink::WebSocketMessageType::CONTINUATION:
      type_to_pass = WebSocketHandle::kMessageTypeContinuation;
      break;
    case network::mojom::blink::WebSocketMessageType::TEXT:
      type_to_pass = WebSocketHandle::kMessageTypeText;
      break;
    case network::mojom::blink::WebSocketMessageType::BINARY:
      type_to_pass = WebSocketHandle::kMessageTypeBinary;
      break;
  }
  const char* data_to_pass =
      reinterpret_cast<const char*>(data.IsEmpty() ? nullptr : &data[0]);
  client_->DidReceiveData(this, fin, type_to_pass, data_to_pass, data.size());
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnFlowControl(int64_t quota) {
  NETWORK_DVLOG(1) << this << " OnFlowControl(" << quota << ")";
  if (!client_)
    return;

  client_->DidReceiveFlowControl(this, quota);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDropChannel(bool was_clean,
                                        uint16_t code,
                                        const String& reason) {
  NETWORK_DVLOG(1) << this << " OnDropChannel(" << was_clean << ", " << code
                   << ", " << reason << ")";

  WebSocketHandleClient* client = client_;
  Disconnect();
  if (!client)
    return;

  client->DidClose(this, was_clean, code, reason);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnClosingHandshake() {
  NETWORK_DVLOG(1) << this << " OnClosingHandshake()";
  if (!client_)
    return;

  client_->DidStartClosingHandshake(this);
  // |this| can be deleted here.
}

}  // namespace blink
