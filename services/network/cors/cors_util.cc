// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_util.h"

#include "base/strings/string_util.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"

namespace network::cors {

bool ShouldAllowUnsafeHeaders(
    const OriginAccessList& origin_access_list,
    const std::optional<url::Origin>& request_initiator,
    const GURL& url) {
  if (!base::FeatureList::IsEnabled(
          features::kBypassRequestForbiddenHeadersCheck)) {
    return false;
  }
  // Check the initiator origin. This covers standard extension contexts like
  // background service workers or popup pages, where `request_initiator` is
  // the extension's origin (e.g., `chrome-extension://<id>`).
  if (origin_access_list.CheckAccessState(
          request_initiator.value_or(url::Origin()), url) ==
      OriginAccessList::AccessState::kAllowed) {
    return true;
  }
  return false;
}

std::vector<std::string> CorsUnsafeNotForbiddenRequestHeaderNames(
    const net::HttpRequestHeaders::HeaderVector& headers,
    bool is_revalidating) {
  std::vector<std::string> header_names;
  std::vector<std::string> potentially_unsafe_names;

  constexpr size_t kSafeListValueSizeMax = 1024;
  size_t safe_list_value_size = 0;

  for (const auto& header : headers) {
    if (!net::HttpUtil::IsSafeHeader(header.key, header.value))
      continue;

    const std::string name = base::ToLowerASCII(header.key);

    if (is_revalidating) {
      if (name == "if-modified-since" || name == "if-none-match" ||
          name == "cache-control") {
        continue;
      }
    }
    if (!IsCorsSafelistedHeader(name, header.value)) {
      header_names.push_back(name);
    } else {
      potentially_unsafe_names.push_back(name);
      safe_list_value_size += header.value.size();
    }
  }
  if (safe_list_value_size > kSafeListValueSizeMax) {
    header_names.insert(header_names.end(), potentially_unsafe_names.begin(),
                        potentially_unsafe_names.end());
  }
  return header_names;
}

}  // namespace network::cors
