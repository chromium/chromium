// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_REDIRECT_UTIL_H_
#define NET_URL_REQUEST_REDIRECT_UTIL_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace net {

struct RedirectInfo;
class HttpRequestHeaders;
class HttpResponseHeaders;

class RedirectUtil {
 public:
  // Updates HTTP headers in |request_headers| for a redirect.
  // |removed_headers| and |modified_headers| are specified by
  // clients to add or override existing headers for the redirect.
  // |should_clear_upload| is set to true when the request body should be
  // cleared during the redirect.
  NET_EXPORT static void UpdateHttpRequest(
      const GURL& original_url,
      const std::string& original_method,
      const RedirectInfo& redirect_info,
      const absl::optional<std::vector<std::string>>& removed_headers,
      const absl::optional<net::HttpRequestHeaders>& modified_headers,
      HttpRequestHeaders* request_headers,
      bool* should_clear_upload);

  // Returns the the "normalized" value of Referrer-Policy header if available.
  // Otherwise returns absl::nullopt.
  NET_EXPORT static absl::optional<std::string> GetReferrerPolicyHeader(
      const HttpResponseHeaders* response_headers);
};

}  // namespace net

#endif  // NET_URL_REQUEST_REDIRECT_UTIL_H_
