// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/redirect_util.h"

#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

// static
void RedirectUtil::UpdateHttpRequest(
    const GURL& original_url,
    const std::string& original_method,
    const RedirectInfo& redirect_info,
    const absl::optional<std::vector<std::string>>& removed_headers,
    const absl::optional<net::HttpRequestHeaders>& modified_headers,
    HttpRequestHeaders* request_headers,
    bool* should_clear_upload) {
  DCHECK(request_headers);
  DCHECK(should_clear_upload);

  *should_clear_upload = false;

  if (removed_headers) {
    for (const std::string& key : removed_headers.value())
      request_headers->RemoveHeader(key);
  }

  if (redirect_info.new_method != original_method) {
    // TODO(davidben): This logic still needs to be replicated at the consumers.
    //
    // The Origin header is sent on anything that is not a GET or HEAD, which
    // suggests all redirects that change methods (since they always change to
    // GET) should drop the Origin header.
    // See https://fetch.spec.whatwg.org/#origin-header
    // TODO(jww): This is Origin header removal is probably layering violation
    // and should be refactored into //content. See https://crbug.com/471397.
    // See also: https://crbug.com/760487
    request_headers->RemoveHeader(HttpRequestHeaders::kOrigin);

    // This header should only be present further down the stack, but remove it
    // here just in case.
    request_headers->RemoveHeader(HttpRequestHeaders::kContentLength);

    // These are "request-body-headers" and should be removed on redirects that
    // change the method, per the fetch spec.
    // https://fetch.spec.whatwg.org/
    request_headers->RemoveHeader(HttpRequestHeaders::kContentType);
    request_headers->RemoveHeader("Content-Encoding");
    request_headers->RemoveHeader("Content-Language");
    request_headers->RemoveHeader("Content-Location");

    *should_clear_upload = true;
  }

  // Cross-origin redirects should not result in an Origin header value that is
  // equal to the original request's Origin header. This is necessary to prevent
  // a reflection of POST requests to bypass CSRF protections. If the header was
  // not set to "null", a POST request from origin A to a malicious origin M
  // could be redirected by M back to A.
  //
  // This behavior is specified in step 10 of the HTTP-redirect fetch
  // algorithm[1] (which supercedes the behavior outlined in RFC 6454[2].
  //
  // [1]: https://fetch.spec.whatwg.org/#http-redirect-fetch
  // [2]: https://tools.ietf.org/html/rfc6454#section-7
  //
  // TODO(jww): This is a layering violation and should be refactored somewhere
  // up into //net's embedder. https://crbug.com/471397
  if (!url::Origin::Create(redirect_info.new_url)
           .IsSameOriginWith(url::Origin::Create(original_url)) &&
      request_headers->HasHeader(HttpRequestHeaders::kOrigin)) {
    request_headers->SetHeader(HttpRequestHeaders::kOrigin,
                               url::Origin().Serialize());
  }

  if (modified_headers)
    request_headers->MergeFrom(modified_headers.value());
}

// static
absl::optional<std::string> RedirectUtil::GetReferrerPolicyHeader(
    const HttpResponseHeaders* response_headers) {
  if (!response_headers)
    return absl::nullopt;
  std::string referrer_policy_header;
  if (!response_headers->GetNormalizedHeader("Referrer-Policy",
                                             &referrer_policy_header)) {
    return absl::nullopt;
  }
  return referrer_policy_header;
}

}  // namespace net
