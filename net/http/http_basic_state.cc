// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_state.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_parser.h"
#include "net/http/http_util.h"
#include "net/socket/client_socket_handle.h"
#include "url/gurl.h"

namespace net {

HttpBasicState::HttpBasicState(std::unique_ptr<ClientSocketHandle> connection,
                               bool using_proxy)
    : read_buf_(base::MakeRefCounted<GrowableIOBuffer>()),
      connection_(std::move(connection)),
      using_proxy_(using_proxy) {
  CHECK(connection_) << "ClientSocketHandle passed to HttpBasicState must "
                        "not be NULL. See crbug.com/790776";
}

HttpBasicState::~HttpBasicState() = default;

void HttpBasicState::Initialize(const HttpRequestInfo* request_info,
                                RequestPriority priority,
                                const NetLogWithSource& net_log) {
  DCHECK(!parser_.get());
  url_ = request_info->url;
  traffic_annotation_ = request_info->traffic_annotation;
  request_method_ = request_info->method;
  parser_ = std::make_unique<HttpStreamParser>(
      connection_->socket(), connection_->is_reused(), request_info,
      read_buf_.get(), net_log);
}

std::unique_ptr<ClientSocketHandle> HttpBasicState::ReleaseConnection() {
  return std::move(connection_);
}

scoped_refptr<GrowableIOBuffer> HttpBasicState::read_buf() const {
  return read_buf_;
}

void HttpBasicState::DeleteParser() { parser_.reset(); }

std::string HttpBasicState::GenerateRequestLine() const {
  static const char kSuffix[] = " HTTP/1.1\r\n";
  const size_t kSuffixLen = base::size(kSuffix) - 1;
  const std::string path =
      using_proxy_ ? HttpUtil::SpecForRequest(url_) : url_.PathForRequest();
  // Don't use StringPrintf for concatenation because it is very inefficient.
  std::string request_line;
  const size_t expected_size =
      request_method_.size() + 1 + path.size() + kSuffixLen;
  request_line.reserve(expected_size);
  request_line.append(request_method_);
  request_line.append(1, ' ');
  request_line.append(path);
  request_line.append(kSuffix, kSuffixLen);
  DCHECK_EQ(expected_size, request_line.size());
  return request_line;
}

bool HttpBasicState::IsConnectionReused() const {
  return connection_->is_reused() ||
         connection_->reuse_type() == ClientSocketHandle::UNUSED_IDLE;
}

}  // namespace net
