// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/api_key_request_util.h"

#include "base/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace google_apis {

void AddDefaultAPIKeyToRequest(network::ResourceRequest& request,
                               version_info::Channel channel) {
  AddAPIKeyToRequest(request, GetAPIKey(channel));
}

void AddAPIKeyToRequest(network::ResourceRequest& request,
                        std::string_view api_key) {
  DLOG_ASSERT(!internal::GetAPIKey(request.url).has_value())
      << "API key already present in URL query parameter.";
  AddAPIKeyToRequest(request.headers, api_key);
}

void AddAPIKeyToRequest(net::HttpRequestHeaders& request_headers,
                        std::string_view api_key) {
  // TODO(b/355544759): check that no Authorization header is present.
  // Don't use CHECK for validation, to make migrating to this API less risky.
  if (api_key.empty()) {
    DLOG(FATAL) << "API key cannot be empty.";
    return;
  }
  DLOG_ASSERT(!internal::GetAPIKey(request_headers).has_value())
      << "API key already present in request header.";

  request_headers.SetHeader(internal::kApiKeyHeaderName, api_key);
}

namespace internal {

std::optional<std::string> GetAPIKey(const GURL& url) {
  std::string api_key;
  return net::GetValueForKeyInQuery(url, kApiKeyQueryParameterName, &api_key)
             ? std::make_optional(api_key)
             : std::nullopt;
}

std::optional<std::string> GetAPIKey(
    const net::HttpRequestHeaders& request_headers) {
  return request_headers.GetHeader(kApiKeyHeaderName);
}

}  // namespace internal

}  // namespace google_apis
