// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/origin_trials/origin_trials_utils.h"

#include <string_view>

#include "net/http/http_response_headers.h"

namespace content {

namespace {

std::vector<std::string> GetHeaderValues(
    std::string_view header_name,
    const net::HttpResponseHeaders* headers) {
  if (!headers) {
    return {};
  }
  size_t iter = 0;
  std::string header_value;
  std::vector<std::string> values;
  while (headers->EnumerateHeader(&iter, header_name, &header_value)) {
    values.push_back(header_value);
  }
  return values;
}

}  // namespace

std::vector<std::string> GetOriginTrialHeaderValues(
    const net::HttpResponseHeaders* headers) {
  return GetHeaderValues("Origin-Trial", headers);
}

std::vector<std::string> GetCriticalOriginTrialHeaderValues(
    const net::HttpResponseHeaders* headers) {
  return GetHeaderValues("Critical-Origin-Trial", headers);
}

}  // namespace content
