// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_REDIRECT_UTIL_H_
#define NET_URL_REQUEST_REDIRECT_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

struct RedirectInfo;
class HttpRequestHeaders;
class HttpResponseHeaders;

class RedirectUtil {
 public:
  // Valid status codes for the redirect job. Other 30x codes are theoretically
  // valid, but unused so far.  Both 302 and 307 are temporary redirects, with
  // the difference being that 302 converts POSTs to GETs and removes upload
  // data.
  enum class ResponseCode {
    REDIRECT_302_FOUND = 302,
    REDIRECT_307_TEMPORARY_REDIRECT = 307,
  };

  // Updates HTTP headers in |request_headers| for a redirect.
  // |removed_headers| and |modified_headers| are specified by
  // clients to add or override existing headers for the redirect.
  // |should_clear_upload| is set to true when the request body should be
  // cleared during the redirect.
  NET_EXPORT static void UpdateHttpRequest(
      const GURL& original_url,
      const std::string& original_method,
      const RedirectInfo& redirect_info,
      const std::optional<std::vector<std::string>>& removed_headers,
      const std::optional<net::HttpRequestHeaders>& modified_headers,
      HttpRequestHeaders* request_headers,
      bool* should_clear_upload);

  // Returns the the "normalized" value of Referrer-Policy header if available.
  // Otherwise returns std::nullopt.
  NET_EXPORT static std::optional<std::string> GetReferrerPolicyHeader(
      const HttpResponseHeaders* response_headers);

  NET_EXPORT static scoped_refptr<HttpResponseHeaders>
  SynthesizeRedirectHeaders(const GURL& redirect_destination,
                            ResponseCode response_code,
                            const std::string& redirect_reason,
                            const HttpRequestHeaders& request_headers);
};

}  // namespace net

#endif  // NET_URL_REQUEST_REDIRECT_UTIL_H_
