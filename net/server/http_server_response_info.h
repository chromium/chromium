// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_HTTP_SERVER_RESPONSE_INFO_H_
#define NET_SERVER_HTTP_SERVER_RESPONSE_INFO_H_

#include <stddef.h>

#include <string>
#include <utility>

#include "base/strings/string_split.h"
#include "net/http/http_status_code.h"

namespace net {

class NET_EXPORT HttpServerResponseInfo {
 public:
  // Creates a 200 OK HttpServerResponseInfo.
  HttpServerResponseInfo();
  explicit HttpServerResponseInfo(HttpStatusCode status_code);
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

  HttpStatusCode status_code() const;
  const std::string& body() const;

 private:
  using Headers = base::StringPairs;

  HttpStatusCode status_code_;
  Headers headers_;
  std::string body_;
};

}  // namespace net

#endif  // NET_SERVER_HTTP_SERVER_RESPONSE_INFO_H_
