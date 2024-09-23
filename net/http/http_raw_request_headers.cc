// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_raw_request_headers.h"

#include <string_view>

namespace net {

HttpRawRequestHeaders::HttpRawRequestHeaders() = default;
HttpRawRequestHeaders::HttpRawRequestHeaders(HttpRawRequestHeaders&&) = default;
HttpRawRequestHeaders& HttpRawRequestHeaders::operator=(
    HttpRawRequestHeaders&&) = default;
HttpRawRequestHeaders::~HttpRawRequestHeaders() = default;

void HttpRawRequestHeaders::Add(std::string_view key, std::string_view value) {
  headers_.emplace_back(std::string(key), std::string(value));
}

bool HttpRawRequestHeaders::FindHeaderForTest(std::string_view key,
                                              std::string* value) const {
  for (const auto& entry : headers_) {
    if (entry.first == key) {
      *value = entry.second;
      return true;
    }
  }
  return false;
}

}  // namespace net
