// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/redirect_util.h"

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

// static
void RedirectUtil::UpdateHttpRequest(
    const GURL& original_url,
    const std::string& original_method,
    const RedirectInfo& redirect_info,
    const std::optional<std::vector<std::string>>& removed_headers,
    const std::optional<net::HttpRequestHeaders>& modified_headers,
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
  // TODO(crbug.com/471397, crbug.com/1406737): This is a layering violation and
  // should be refactored somewhere into //net's embedder. Also, step 13 of
  // https://fetch.spec.whatwg.org/#http-redirect-fetch is implemented in
  // Blink.
  if (!url::IsSameOriginWith(redirect_info.new_url, original_url) &&
      request_headers->HasHeader(HttpRequestHeaders::kOrigin)) {
    request_headers->SetHeader(HttpRequestHeaders::kOrigin,
                               url::Origin().Serialize());
  }

  if (modified_headers)
    request_headers->MergeFrom(modified_headers.value());
}

// static
std::optional<std::string> RedirectUtil::GetReferrerPolicyHeader(
    const HttpResponseHeaders* response_headers) {
  if (!response_headers)
    return std::nullopt;
  std::string referrer_policy_header;
  if (!response_headers->GetNormalizedHeader("Referrer-Policy",
                                             &referrer_policy_header)) {
    return std::nullopt;
  }
  return referrer_policy_header;
}

// static
scoped_refptr<HttpResponseHeaders> RedirectUtil::SynthesizeRedirectHeaders(
    const GURL& redirect_destination,
    ResponseCode response_code,
    const std::string& redirect_reason,
    const HttpRequestHeaders& request_headers) {
  std::string header_string = base::StringPrintf(
      "HTTP/1.1 %i Internal Redirect\n"
      "Location: %s\n"
      "Cross-Origin-Resource-Policy: Cross-Origin\n"
      "Non-Authoritative-Reason: %s",
      static_cast<int>(response_code), redirect_destination.spec().c_str(),
      redirect_reason.c_str());

  if (std::optional<std::string> http_origin =
          request_headers.GetHeader("Origin");
      http_origin) {
    // If this redirect is used in a cross-origin request, add CORS headers to
    // make sure that the redirect gets through. Note that the destination URL
    // is still subject to the usual CORS policy, i.e. the resource will only
    // be available to web pages if the server serves the response with the
    // required CORS response headers.
    header_string += base::StringPrintf(
        "\n"
        "Access-Control-Allow-Origin: %s\n"
        "Access-Control-Allow-Credentials: true",
        http_origin->c_str());
  }

  auto fake_headers = base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(header_string));
  DCHECK(fake_headers->IsRedirect(nullptr));

  return fake_headers;
}

}  // namespace net
