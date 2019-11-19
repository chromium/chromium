// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_REQUEST_INFO_H__
#define NET_HTTP_HTTP_REQUEST_INFO_H__

#include <string>

#include "base/optional.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/http/http_request_headers.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace net {

class UploadDataStream;

struct NET_EXPORT HttpRequestInfo {
  HttpRequestInfo();
  HttpRequestInfo(const HttpRequestInfo& other);
  ~HttpRequestInfo();

  // The requested URL.
  GURL url;

  // The method to use (GET, POST, etc.).
  std::string method;

  // This key is used to isolate requests from different contexts in accessing
  // shared network resources like the cache.
  NetworkIsolationKey network_isolation_key;

  // Any extra request headers (including User-Agent).
  HttpRequestHeaders extra_headers;

  // Any upload data.
  UploadDataStream* upload_data_stream;

  // Any load flags (see load_flags.h).
  int load_flags;

  // If enabled, then request must be sent over connection that cannot be
  // tracked by the server (e.g. without channel id).
  PrivacyMode privacy_mode;

  // Whether secure DNS should be disabled for the request.
  bool disable_secure_dns;

  // Tag applied to all sockets used to service request.
  SocketTag socket_tag;

  // Network traffic annotation received from URL request.
  net::MutableNetworkTrafficAnnotationTag traffic_annotation;

  // Reporting upload nesting depth of this request.
  //
  // If the request is not a Reporting upload, the depth is 0.
  //
  // If the request is a Reporting upload, the depth is the max of the depth
  // of the requests reported within it plus 1.
  int reporting_upload_depth;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_REQUEST_INFO_H__
