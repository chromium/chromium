// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/api_key_request_test_util.h"

#include <string>

#include "google_apis/common/api_key_request_util.h"
#include "services/network/public/cpp/resource_request.h"

namespace google_apis::test_util {

bool HasAPIKey(const network::ResourceRequest& request) {
  return google_apis::internal::GetAPIKey(request.url).has_value() ||
         google_apis::internal::GetAPIKey(request.headers).has_value();
}

std::optional<std::string> GetAPIKeyFromRequest(
    const network::ResourceRequest& request) {
  std::optional<std::string> header_api_key =
      google_apis::internal::GetAPIKey(request.headers);
  std::optional<std::string> url_api_key =
      google_apis::internal::GetAPIKey(request.url);
  if (header_api_key) {
    // The API key should not be present in both the URL query parameter and the
    // HTTP header.
    CHECK(!url_api_key);
    return header_api_key;
  }
  return url_api_key;
}

}  // namespace google_apis::test_util
