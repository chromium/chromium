// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_stream.h"

#include <set>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "net/http/http_network_session.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_parser.h"
#include "net/socket/stream_socket_handle.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"

namespace net {

HttpBasicStream::HttpBasicStream(std::unique_ptr<StreamSocketHandle> connection,
                                 bool is_for_get_to_http_proxy)
    : state_(std::move(connection), is_for_get_to_http_proxy) {}

HttpBasicStream::~HttpBasicStream() = default;

void HttpBasicStream::RegisterRequest(const HttpRequestInfo* request_info) {
  DCHECK(request_info);
  DCHECK(request_info->traffic_annotation.is_valid());
  request_info_ = request_info;
}

int HttpBasicStream::InitializeStream(bool can_send_early,
                                      RequestPriority priority,
                                      const NetLogWithSource& net_log,
                                      CompletionOnceCallback callback) {
  DCHECK(request_info_);
  state_.Initialize(request_info_, priority, net_log);
  // RequestInfo is no longer needed after this point.
  request_info_ = nullptr;

  int ret = OK;
  if (!can_send_early) {
    // parser() cannot outlive |this|, so we can use base::Unretained().
    ret = parser()->ConfirmHandshake(
        base::BindOnce(&HttpBasicStream::OnHandshakeConfirmed,
                       base::Unretained(this), std::move(callback)));
  }
  return ret;
}

int HttpBasicStream::SendRequest(const HttpRequestHeaders& headers,
                                 HttpResponseInfo* response,
                                 CompletionOnceCallback callback) {
  DCHECK(parser());
  if (request_headers_callback_) {
    HttpRawRequestHeaders raw_headers;
    raw_headers.set_request_line(state_.GenerateRequestLine());
    for (HttpRequestHeaders::Iterator it(headers); it.GetNext();) {
      raw_headers.Add(it.name(), it.value());
    }
    request_headers_callback_.Run(std::move(raw_headers));
  }
  return parser()->SendRequest(
      state_.GenerateRequestLine(), headers,
      NetworkTrafficAnnotationTag(state_.traffic_annotation()), response,
      std::move(callback));
}

int HttpBasicStream::ReadResponseHeaders(CompletionOnceCallback callback) {
  return parser()->ReadResponseHeaders(std::move(callback));
}

int HttpBasicStream::ReadResponseBody(IOBuffer* buf,
                                      int buf_len,
                                      CompletionOnceCallback callback) {
  return parser()->ReadResponseBody(buf, buf_len, std::move(callback));
}

void HttpBasicStream::Close(bool not_reusable) {
  state_.Close(not_reusable);
}

std::unique_ptr<HttpStream> HttpBasicStream::RenewStreamForAuth() {
  DCHECK(IsResponseBodyComplete());
  DCHECK(!parser()->IsMoreDataBuffered());
  return std::make_unique<HttpBasicStream>(state_.ReleaseConnection(),
                                           state_.is_for_get_to_http_proxy());
}

bool HttpBasicStream::IsResponseBodyComplete() const {
  return parser()->IsResponseBodyComplete();
}

bool HttpBasicStream::IsConnectionReused() const {
  return state_.IsConnectionReused();
}

void HttpBasicStream::SetConnectionReused() {
  state_.SetConnectionReused();
}

bool HttpBasicStream::CanReuseConnection() const {
  return state_.CanReuseConnection();
}

int64_t HttpBasicStream::GetTotalReceivedBytes() const {
  if (parser())
    return parser()->received_bytes();
  return 0;
}

int64_t HttpBasicStream::GetTotalSentBytes() const {
  if (parser())
    return parser()->sent_bytes();
  return 0;
}

bool HttpBasicStream::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  if (!state_.GetLoadTimingInfo(load_timing_info) || !parser()) {
    return false;
  }

  // If the request waited for handshake confirmation, shift |ssl_end| to
  // include that time.
  if (!load_timing_info->connect_timing.ssl_end.is_null() &&
      !confirm_handshake_end_.is_null()) {
    load_timing_info->connect_timing.ssl_end = confirm_handshake_end_;
    load_timing_info->connect_timing.connect_end = confirm_handshake_end_;
  }

  load_timing_info->receive_headers_start =
      parser()->first_response_start_time();
  load_timing_info->receive_non_informational_headers_start =
      parser()->non_informational_response_start_time();
  load_timing_info->first_early_hints_time = parser()->first_early_hints_time();
  return true;
}

bool HttpBasicStream::GetAlternativeService(
    AlternativeService* alternative_service) const {
  return false;
}

void HttpBasicStream::GetSSLInfo(SSLInfo* ssl_info) {
  state_.GetSSLInfo(ssl_info);
}

int HttpBasicStream::GetRemoteEndpoint(IPEndPoint* endpoint) {
  return state_.GetRemoteEndpoint(endpoint);
}

void HttpBasicStream::Drain(HttpNetworkSession* session) {
  session->StartResponseDrainer(
      std::make_unique<HttpResponseBodyDrainer>(this));
  // |drainer| will delete itself.
}

void HttpBasicStream::PopulateNetErrorDetails(NetErrorDetails* details) {
  // TODO(mmenke):  Consumers don't actually care about HTTP version, but seems
  // like the right version should be reported, if headers were received.
  details->connection_info = HttpConnectionInfo::kHTTP1_1;
  return;
}

void HttpBasicStream::SetPriority(RequestPriority priority) {
  // TODO(akalin): Plumb this through to |connection_|.
}

void HttpBasicStream::SetRequestHeadersCallback(
    RequestHeadersCallback callback) {
  request_headers_callback_ = std::move(callback);
}

const std::set<std::string>& HttpBasicStream::GetDnsAliases() const {
  return state_.GetDnsAliases();
}

std::string_view HttpBasicStream::GetAcceptChViaAlps() const {
  return {};
}

void HttpBasicStream::OnHandshakeConfirmed(CompletionOnceCallback callback,
                                           int rv) {
  if (rv == OK) {
    // Note this time is only recorded if ConfirmHandshake() completed
    // asynchronously. If it was synchronous, GetLoadTimingInfo() assumes the
    // handshake was already confirmed or there was nothing to confirm.
    confirm_handshake_end_ = base::TimeTicks::Now();
  }
  std::move(callback).Run(rv);
}

}  // namespace net
