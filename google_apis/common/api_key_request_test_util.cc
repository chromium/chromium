// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/api_key_request_test_util.h"

#include <string>

#include "google_apis/common/api_key_request_util.h"
#include "services/network/public/cpp/resource_request.h"

namespace google_apis::test_util {

bool HasAPIKey(const network::ResourceRequest& request) {
  return google_apis::internal::HasAPIKey(request.headers);
}

std::optional<std::string> GetAPIKeyFromRequest(
    const network::ResourceRequest& request) {
  return request.headers.GetHeader(google_apis::internal::kApiKeyHeaderName);
}

}  // namespace google_apis::test_util
