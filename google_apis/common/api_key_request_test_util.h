// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_API_KEY_REQUEST_TEST_UTIL_H_
#define GOOGLE_APIS_COMMON_API_KEY_REQUEST_TEST_UTIL_H_

#include <optional>
#include <string>

namespace network {
struct ResourceRequest;
}

namespace google_apis::test_util {

// Whether the request has an API key set.
bool HasAPIKey(const network::ResourceRequest& request);

// Returns the API key on the request, if one is present.
std::optional<std::string> GetAPIKeyFromRequest(
    const network::ResourceRequest& request);

}  // namespace google_apis::test_util

#endif  // GOOGLE_APIS_COMMON_API_KEY_REQUEST_TEST_UTIL_H_
