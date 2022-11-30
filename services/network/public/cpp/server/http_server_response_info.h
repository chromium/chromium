// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SERVER_HTTP_SERVER_RESPONSE_INFO_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SERVER_HTTP_SERVER_RESPONSE_INFO_H_

#include <stddef.h>

#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/strings/string_split.h"
#include "net/http/http_status_code.h"

namespace network {

namespace server {

class COMPONENT_EXPORT(NETWORK_CPP) HttpServerResponseInfo {
 public:
  // Creates a 200 OK HttpServerResponseInfo.
  HttpServerResponseInfo();
  explicit HttpServerResponseInfo(net::HttpStatusCode status_code);
  HttpServerResponseInfo(const HttpServerResponseInfo& other);
  ~HttpServerResponseInfo();

  static HttpServerResponseInfo CreateFor404();
  static HttpServerResponseInfo CreateFor500(const std::string& body);

  void AddHeader(const std::string& name, const std::string& value);

  // This also adds an appropriate Content-Length header.
  void SetBody(const std::string& body, const std::string& content_type);
  // Sets content-length and content-type. Body should be sent separately.
  void SetContentHeaders(size_t content_length,
                         const std::string& content_type);

  std::string Serialize() const;

  net::HttpStatusCode status_code() const;
  const std::string& body() const;

 private:
  using Headers = base::StringPairs;

  net::HttpStatusCode status_code_;
  Headers headers_;
  std::string body_;
};

}  // namespace server

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SERVER_HTTP_SERVER_RESPONSE_INFO_H_
