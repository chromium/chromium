// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/multiplexed_http_stream.h"

#include <utility>

#include "base/notreached.h"
#include "base/strings/abseil_string_conversions.h"
#include "net/http/http_raw_request_headers.h"
#include "net/third_party/quiche/src/quiche/spdy/core/http2_header_block.h"

namespace net {

MultiplexedHttpStream::MultiplexedHttpStream(
    std::unique_ptr<MultiplexedSessionHandle> session)
    : session_(std::move(session)) {}

MultiplexedHttpStream::~MultiplexedHttpStream() = default;

int MultiplexedHttpStream::GetRemoteEndpoint(IPEndPoint* endpoint) {
  return session_->GetRemoteEndpoint(endpoint);
}

void MultiplexedHttpStream::GetSSLInfo(SSLInfo* ssl_info) {
  session_->GetSSLInfo(ssl_info);
}

void MultiplexedHttpStream::SaveSSLInfo() {
  session_->SaveSSLInfo();
}

void MultiplexedHttpStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  // A multiplexed stream cannot request client certificates. Client
  // authentication may only occur during the initial SSL handshake.
  NOTREACHED();
}

void MultiplexedHttpStream::Drain(HttpNetworkSession* session) {
  NOTREACHED();
  Close(false);
  delete this;
}

std::unique_ptr<HttpStream> MultiplexedHttpStream::RenewStreamForAuth() {
  return nullptr;
}

void MultiplexedHttpStream::SetConnectionReused() {}

bool MultiplexedHttpStream::CanReuseConnection() const {
  // Multiplexed streams aren't considered reusable.
  return false;
}

void MultiplexedHttpStream::SetRequestHeadersCallback(
    RequestHeadersCallback callback) {
  request_headers_callback_ = std::move(callback);
}

void MultiplexedHttpStream::DispatchRequestHeadersCallback(
    const spdy::Http2HeaderBlock& spdy_headers) {
  if (!request_headers_callback_)
    return;
  HttpRawRequestHeaders raw_headers;
  for (const auto& entry : spdy_headers)
    raw_headers.Add(base::StringViewToStringPiece(entry.first),
                    base::StringViewToStringPiece(entry.second));
  request_headers_callback_.Run(std::move(raw_headers));
}

}  // namespace net
