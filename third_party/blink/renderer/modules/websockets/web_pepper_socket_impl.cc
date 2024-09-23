/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/websockets/web_pepper_socket_impl.h"

#include <stddef.h>

#include <memory>

#include "base/functional/callback.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_array_buffer.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/websockets/web_pepper_socket_channel_client_proxy.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

std::unique_ptr<WebPepperSocket> WebPepperSocket::Create(
    const WebDocument& document,
    WebPepperSocketClient* client) {
  DCHECK(client);

  return std::make_unique<WebPepperSocketImpl>(document, client);
}

WebPepperSocketImpl::WebPepperSocketImpl(const WebDocument& document,
                                         WebPepperSocketClient* client)
    : client_(client),
      channel_proxy_(
          MakeGarbageCollected<WebPepperSocketChannelClientProxy>(this)),
      is_closing_or_closed_(false),
      buffered_amount_(0),
      buffered_amount_after_close_(0) {
  Document* core_document = document;
  private_ = WebSocketChannelImpl::Create(core_document->GetExecutionContext(),
                                          channel_proxy_.Get(),
                                          CaptureSourceLocation());
  DCHECK(private_);
}

WebPepperSocketImpl::~WebPepperSocketImpl() {
  private_->Disconnect();
}

void WebPepperSocketImpl::Connect(const WebURL& url,
                                  const WebString& protocol) {
  private_->Connect(url, protocol);
}

WebString WebPepperSocketImpl::Subprotocol() {
  return subprotocol_;
}

bool WebPepperSocketImpl::SendText(const WebString& message) {
  String core_message = message;
  std::string encoded_message = core_message.Utf8();
  size_t size = encoded_message.length();
  buffered_amount_ += size;
  if (is_closing_or_closed_)
    buffered_amount_after_close_ += size;

  // FIXME: Deprecate this call.
  client_->DidUpdateBufferedAmount(buffered_amount_);

  if (is_closing_or_closed_)
    return true;

  private_->Send(encoded_message, base::OnceClosure());
  return true;
}

bool WebPepperSocketImpl::SendArrayBuffer(
    const WebArrayBuffer& web_array_buffer) {
  size_t size = web_array_buffer.ByteLength();
  buffered_amount_ += size;
  if (is_closing_or_closed_)
    buffered_amount_after_close_ += size;

  // FIXME: Deprecate this call.
  client_->DidUpdateBufferedAmount(buffered_amount_);

  if (is_closing_or_closed_)
    return true;

  DOMArrayBuffer* array_buffer = web_array_buffer;
  private_->Send(*array_buffer, 0, array_buffer->ByteLength(),
                 base::OnceClosure());
  return true;
}

void WebPepperSocketImpl::Close(int code, const WebString& reason) {
  is_closing_or_closed_ = true;
  private_->Close(code, reason);
}

void WebPepperSocketImpl::Fail(const WebString& reason) {
  private_->Fail(
      reason, mojom::ConsoleMessageLevel::kError,
      std::make_unique<SourceLocation>(String(), String(), 0, 0, nullptr));
}

void WebPepperSocketImpl::Disconnect() {
  private_->Disconnect();
  client_ = nullptr;
}

void WebPepperSocketImpl::DidConnect(const String& subprotocol,
                                     const String& extensions) {
  client_->DidConnect(subprotocol, extensions);

  // FIXME: Deprecate these statements.
  subprotocol_ = subprotocol;
  client_->DidConnect();
}

void WebPepperSocketImpl::DidReceiveTextMessage(const String& payload) {
  client_->DidReceiveMessage(WebString(payload));
}

void WebPepperSocketImpl::DidReceiveBinaryMessage(
    std::unique_ptr<Vector<char>> payload) {
  client_->DidReceiveArrayBuffer(
      WebArrayBuffer(DOMArrayBuffer::Create(base::as_byte_span(*payload))));
}

void WebPepperSocketImpl::DidError() {
  client_->DidReceiveMessageError();
}

void WebPepperSocketImpl::DidConsumeBufferedAmount(uint64_t consumed) {
  client_->DidConsumeBufferedAmount(consumed);

  // FIXME: Deprecate the following statements.
  buffered_amount_ -= consumed;
  client_->DidUpdateBufferedAmount(buffered_amount_);
}

void WebPepperSocketImpl::DidStartClosingHandshake() {
  client_->DidStartClosingHandshake();
}

void WebPepperSocketImpl::DidClose(
    WebSocketChannelClient::ClosingHandshakeCompletionStatus status,
    uint16_t code,
    const String& reason) {
  is_closing_or_closed_ = true;
  client_->DidClose(
      static_cast<WebPepperSocketClient::ClosingHandshakeCompletionStatus>(
          status),
      code, WebString(reason));

  // FIXME: Deprecate this call.
  client_->DidClose(
      buffered_amount_ - buffered_amount_after_close_,
      static_cast<WebPepperSocketClient::ClosingHandshakeCompletionStatus>(
          status),
      code, WebString(reason));
}

}  // namespace blink
