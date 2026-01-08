// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/http_server_response_info.h"

#include <string_view>

#include "base/check.h"
#include "base/format_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_request_headers.h"

namespace net {

HttpServerResponseInfo::HttpServerResponseInfo() : status_code_(HTTP_OK) {}

HttpServerResponseInfo::HttpServerResponseInfo(HttpStatusCode status_code)
    : status_code_(status_code) {}

HttpServerResponseInfo::HttpServerResponseInfo(
    const HttpServerResponseInfo& other) = default;

HttpServerResponseInfo::~HttpServerResponseInfo() = default;

// static
HttpServerResponseInfo HttpServerResponseInfo::CreateFor404() {
  HttpServerResponseInfo response(HTTP_NOT_FOUND);
  response.SetBody("", "text/html");
  return response;
}

// static
HttpServerResponseInfo HttpServerResponseInfo::CreateFor500(
    std::string_view body) {
  HttpServerResponseInfo response(HTTP_INTERNAL_SERVER_ERROR);
  response.SetBody(body, "text/html");
  return response;
}

void HttpServerResponseInfo::AddHeader(std::string_view name,
                                       std::string_view value) {
  headers_.emplace_back(name, value);
}

void HttpServerResponseInfo::SetBody(std::string_view body,
                                     std::string_view content_type) {
  DCHECK(body_.empty());
  body_ = body;
  SetContentHeaders(body.length(), content_type);
}

void HttpServerResponseInfo::SetContentHeaders(size_t content_length,
                                               std::string_view content_type) {
  AddHeader(HttpRequestHeaders::kContentLength,
            base::StringPrintf("%" PRIuS, content_length));
  AddHeader(HttpRequestHeaders::kContentType, content_type);
}

std::string HttpServerResponseInfo::Serialize() const {
  std::string response = base::StringPrintf(
      "HTTP/1.1 %d %s\r\n", status_code_, GetHttpReasonPhrase(status_code_));
  for (const std::pair<std::string, std::string>& header : headers_) {
    base::StrAppend(&response, {header.first, ":", header.second, "\r\n"});
  }

  return base::StrCat({response, "\r\n", body_});
}

HttpStatusCode HttpServerResponseInfo::status_code() const {
  return status_code_;
}

const std::string& HttpServerResponseInfo::body() const {
  return body_;
}

}  // namespace net
