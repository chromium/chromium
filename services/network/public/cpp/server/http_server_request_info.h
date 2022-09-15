// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SERVER_HTTP_SERVER_REQUEST_INFO_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SERVER_HTTP_SERVER_REQUEST_INFO_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "net/base/ip_endpoint.h"

namespace network {

namespace server {

// Meta information about an HTTP request.
// This is geared toward servers in that it keeps a map of the headers and
// values rather than just a list of header strings (which net::HttpRequestInfo
// does).
class COMPONENT_EXPORT(NETWORK_CPP) HttpServerRequestInfo {
 public:
  HttpServerRequestInfo();
  HttpServerRequestInfo(const HttpServerRequestInfo& other);
  ~HttpServerRequestInfo();

  // Returns header value for given header name. |header_name| should be
  // lower case.
  std::string GetHeaderValue(const std::string& header_name) const;

  // Checks for item in comma-separated header value for given header name.
  // Both |header_name| and |header_value| should be lower case.
  bool HasHeaderValue(const std::string& header_name,
                      const std::string& header_value) const;

  // Request peer address.
  net::IPEndPoint peer;

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

}  // namespace server

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SERVER_HTTP_SERVER_REQUEST_INFO_H_
