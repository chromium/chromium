// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_HTTP_SERVER_REQUEST_INFO_H_
#define NET_SERVER_HTTP_SERVER_REQUEST_INFO_H_

#include <map>
#include <string>

#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"

namespace net {

// Meta information about an HTTP request.
// This is geared toward servers in that it keeps a map of the headers and
// values rather than just a list of header strings (which net::HttpRequestInfo
// does).
class NET_EXPORT HttpServerRequestInfo {
 public:
  HttpServerRequestInfo();
  HttpServerRequestInfo(const HttpServerRequestInfo& other);
  ~HttpServerRequestInfo();

  // Returns header value for given header name. |header_name| should be
  // lower case.
  std::string GetHeaderValue(const std::string& header_name) const;

  // Checks for item in comma-separated header value for given header name.
  // Both |header_name| and |header_value| should be lower case.
  bool HasHeaderValue(
      const std::string& header_name,
      const std::string& header_value) const;

  // Request peer address.
  IPEndPoint peer;

  // Request method.
  std::string method;

  // Request line.
  std::string path;

  // Request data.
  std::string data;

  // A map of the names -> values for HTTP headers. These should always
  // contain lower case field names.
  using HeadersMap = std::map<std::string, std::string>;
  HeadersMap headers;
};

}  // namespace net

#endif  // NET_SERVER_HTTP_SERVER_REQUEST_INFO_H_
