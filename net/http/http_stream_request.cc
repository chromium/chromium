// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_request.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/log/net_log_event_type.h"
#include "net/spdy/bidirectional_stream_spdy_impl.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"

namespace net {

HttpStreamRequest::HttpStreamRequest(
    Helper* helper,
    WebSocketHandshakeStreamBase::CreateHelper*
        websocket_handshake_stream_create_helper,
    const NetLogWithSource& net_log,
    StreamType stream_type)
    : helper_(helper),
      websocket_handshake_stream_create_helper_(
          websocket_handshake_stream_create_helper),
      net_log_(net_log),
      stream_type_(stream_type) {
  net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_REQUEST);
}

HttpStreamRequest::~HttpStreamRequest() {
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_REQUEST);
  helper_.ExtractAsDangling()->OnRequestComplete();  // May delete `*helper_`;
}

void HttpStreamRequest::Complete(
    NextProto negotiated_protocol,
    AlternateProtocolUsage alternate_protocol_usage) {
  DCHECK(!completed_);
  completed_ = true;
  negotiated_protocol_ = negotiated_protocol;
  alternate_protocol_usage_ = alternate_protocol_usage;
}

int HttpStreamRequest::RestartTunnelWithProxyAuth() {
  return helper_->RestartTunnelWithProxyAuth();
}

void HttpStreamRequest::SetPriority(RequestPriority priority) {
  helper_->SetPriority(priority);
}

LoadState HttpStreamRequest::GetLoadState() const {
  return helper_->GetLoadState();
}

NextProto HttpStreamRequest::negotiated_protocol() const {
  DCHECK(completed_);
  return negotiated_protocol_;
}

AlternateProtocolUsage HttpStreamRequest::alternate_protocol_usage() const {
  DCHECK(completed_);
  return alternate_protocol_usage_;
}

const ConnectionAttempts& HttpStreamRequest::connection_attempts() const {
  return connection_attempts_;
}

void HttpStreamRequest::AddConnectionAttempts(
    const ConnectionAttempts& attempts) {
  for (const auto& attempt : attempts) {
    connection_attempts_.push_back(attempt);
  }
}

WebSocketHandshakeStreamBase::CreateHelper*
HttpStreamRequest::websocket_handshake_stream_create_helper() const {
  return websocket_handshake_stream_create_helper_;
}

void HttpStreamRequest::SetDnsResolutionTimeOverrides(
    base::TimeTicks dns_resolution_start_time_override,
    base::TimeTicks dns_resolution_end_time_override) {
  CHECK(!dns_resolution_start_time_override.is_null());
  CHECK(!dns_resolution_end_time_override.is_null());
  if (dns_resolution_start_time_override_.is_null() ||
      (dns_resolution_start_time_override <
       dns_resolution_start_time_override_)) {
    dns_resolution_start_time_override_ = dns_resolution_start_time_override;
  }
  if (dns_resolution_end_time_override_.is_null() ||
      (dns_resolution_end_time_override < dns_resolution_end_time_override_)) {
    dns_resolution_end_time_override_ = dns_resolution_end_time_override;
  }
}

}  // namespace net
