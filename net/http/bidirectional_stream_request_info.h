// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_BIDIRECTIONAL_STREAM_REQUEST_INFO_H_
#define NET_HTTP_BIDIRECTIONAL_STREAM_REQUEST_INFO_H_

#include <string>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/socket/socket_tag.h"
#include "url/gurl.h"

namespace net {

// Struct containing information needed to request a BidirectionalStream.
struct NET_EXPORT BidirectionalStreamRequestInfo {
  BidirectionalStreamRequestInfo();
  ~BidirectionalStreamRequestInfo();

  // The requested URL.
  GURL url;

  // The method to use (GET, POST, etc.).
  std::string method;

  // Whether to allow early data to be used with this request, overriding the
  // early data based on the |method| semantics.
  bool allow_early_data_override = false;

  // Request priority.
  RequestPriority priority = LOW;

  // Socket tag to apply to sockets used to process this request.
  SocketTag socket_tag;

  // Any extra request headers (including User-Agent).
  HttpRequestHeaders extra_headers;

  // Whether END_STREAM should be set on the request HEADER frame.
  bool end_stream_on_headers = false;

  // Whether the implementor of the BidirectionalStream should monitor
  // the status of the connection for the lifetime of this stream.
  bool detect_broken_connection = false;

  // Suggests the period the broken connection detector should use to check
  // the status of the connection.
  base::TimeDelta heartbeat_interval;
};

}  // namespace net

#endif  // NET_HTTP_BIDIRECTIONAL_STREAM_REQUEST_INFO_H_
