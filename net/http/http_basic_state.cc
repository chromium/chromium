// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_state.h"

#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_parser.h"
#include "net/http/http_util.h"
#include "net/socket/stream_socket.h"
#include "net/socket/stream_socket_handle.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace net {

HttpBasicState::HttpBasicState(std::unique_ptr<StreamSocketHandle> connection,
                               bool is_for_get_to_http_proxy)
    : read_buf_(base::MakeRefCounted<GrowableIOBuffer>()),
      connection_(std::move(connection)),
      is_for_get_to_http_proxy_(is_for_get_to_http_proxy) {
  CHECK(connection_) << "StreamSocketHandle passed to HttpBasicState must "
                        "not be NULL. See crbug.com/790776";
}

HttpBasicState::~HttpBasicState() = default;

void HttpBasicState::Initialize(const HttpRequestInfo* request_info,
                                RequestPriority priority,
                                const NetLogWithSource& net_log) {
  DCHECK(!parser_.get());
  traffic_annotation_ = request_info->traffic_annotation;
  parser_ = std::make_unique<HttpStreamParser>(
      connection_->socket(),
      connection_->reuse_type() ==
          StreamSocketHandle::SocketReuseType::kReusedIdle,
      request_info->url, request_info->method, request_info->upload_data_stream,
      read_buf_.get(), net_log);
}

void HttpBasicState::Close(bool not_reusable) {
  // `parser_` is null if the owner of `this` is created by an orphaned
  // HttpStreamFactory::Job in which case InitializeStream() will not have been
  // called. This also protects against null dereference in the case where
  // ReleaseConnection() has been called.
  //
  // TODO(mmenke):  Can these cases be handled a bit more cleanly?
  if (!parser_) {
    return;
  }
  StreamSocket* socket = connection_->socket();
  if (not_reusable && socket) {
    socket->Disconnect();
  }
  parser()->OnConnectionClose();
  connection_->Reset();
}

std::unique_ptr<StreamSocketHandle> HttpBasicState::ReleaseConnection() {
  // The HttpStreamParser object still has a pointer to the connection. Just to
  // be extra-sure it doesn't touch the connection again, delete it here rather
  // than leaving it until the destructor is called.
  parser_.reset();
  return std::move(connection_);
}

scoped_refptr<GrowableIOBuffer> HttpBasicState::read_buf() const {
  return read_buf_;
}

std::string HttpBasicState::GenerateRequestLine() const {
  return HttpUtil::GenerateRequestLine(parser_->method(), parser_->url(),
                                       is_for_get_to_http_proxy_);
}

bool HttpBasicState::IsConnectionReused() const {
  return connection_->reuse_type() ==
             StreamSocketHandle::SocketReuseType::kReusedIdle ||
         connection_->reuse_type() ==
             StreamSocketHandle::SocketReuseType::kUnusedIdle;
}

void HttpBasicState::SetConnectionReused() {
  connection_->set_reuse_type(StreamSocketHandle::SocketReuseType::kReusedIdle);
}

bool HttpBasicState::CanReuseConnection() const {
  return parser_ && connection_->socket() && parser_->CanReuseConnection();
}

bool HttpBasicState::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  return connection_->GetLoadTimingInfo(IsConnectionReused(), load_timing_info);
}

void HttpBasicState::GetSSLInfo(SSLInfo* ssl_info) {
  CHECK(connection_);
  if (!connection_->socket() || !connection_->socket()->GetSSLInfo(ssl_info)) {
    ssl_info->Reset();
  }
}

int HttpBasicState::GetRemoteEndpoint(IPEndPoint* endpoint) {
  if (!connection_ || !connection_->socket()) {
    return ERR_SOCKET_NOT_CONNECTED;
  }
  return connection_->socket()->GetPeerAddress(endpoint);
}

const std::set<std::string>& HttpBasicState::GetDnsAliases() const {
  static const base::NoDestructor<std::set<std::string>> emptyset_result;
  return (connection_ && connection_->socket())
             ? connection_->socket()->GetDnsAliases()
             : *emptyset_result;
}

}  // namespace net
