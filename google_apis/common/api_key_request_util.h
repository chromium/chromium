// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_API_KEY_REQUEST_UTIL_H_
#define GOOGLE_APIS_COMMON_API_KEY_REQUEST_UTIL_H_

#include <optional>
#include <string_view>

class GURL;

namespace net {
class HttpRequestHeaders;
}

namespace network {
struct ResourceRequest;
}

namespace version_info {
enum class Channel;
}

namespace google_apis {

// Adds the default API key to the request.
//
// This uses the default API key as returned by `GetAPIKey(channel)`. Use this
// method if your request does not have separate per-service or overridable key.
void AddDefaultAPIKeyToRequest(network::ResourceRequest& request,
                               version_info::Channel channel);

// Adds a specific API key to the request.
//
// Use this method if your request has separate per-service or overridable key.
void AddAPIKeyToRequest(network::ResourceRequest& request,
                        std::string_view api_key);
void AddAPIKeyToRequest(net::HttpRequestHeaders& request_headers,
                        std::string_view api_key);

// The below are internal definitions that should not be called from outside
// google_apis/common.
namespace internal {

// The API key can be present as either a URL parameter or as an HTTP header.
// This class uses the header form when setting the key, but getters support
// both forms.
//
// See https://cloud.google.com/apis/docs/system-parameters
inline constexpr char kApiKeyQueryParameterName[] = "key";
inline constexpr char kApiKeyHeaderName[] = "X-Goog-Api-Key";

// Returns the API key from a URL query parameter.
std::optional<std::string> GetAPIKey(const GURL& url);

// Returns the API key from the request headers.
std::optional<std::string> GetAPIKey(
    const net::HttpRequestHeaders& request_headers);

}  // namespace internal

}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_API_KEY_REQUEST_UTIL_H_
