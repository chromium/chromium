// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_

#include <string>

#include "base/optional.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/browser/web_package/signed_exchange_signature_verifier.h"
#include "content/common/content_export.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

class BrowserContext;
class SignedExchangeDevToolsProxy;

namespace signed_exchange_utils {

// URLWithRawString holds a parsed URL along with its raw bytes.
struct URLWithRawString {
  GURL url;
  std::string raw_string;
  URLWithRawString() = default;
  URLWithRawString(base::StringPiece url_string)
      : url(url_string), raw_string(url_string.as_string()) {}
};

// Utility method to call SignedExchangeDevToolsProxy::ReportError() and
// TRACE_EVENT_INSTANT1 to report the error to both DevTools and about:tracing.
// If |devtools_proxy| is nullptr, it just calls TRACE_EVENT_INSTANT1().
void ReportErrorAndTraceEvent(
    SignedExchangeDevToolsProxy* devtools_proxy,
    const std::string& error_message,
    base::Optional<SignedExchangeError::FieldIndexPair> error_field =
        base::nullopt);

// Returns true when SignedHTTPExchange feature is enabled. This must be called
// on the UI thread.
CONTENT_EXPORT bool IsSignedExchangeHandlingEnabled(BrowserContext* context);

// Returns true when SignedExchangeReportingForDistributors feature is enabled.
bool IsSignedExchangeReportingForDistributorsEnabled();

// Returns true when the response should be handled as a signed exchange by
// checking the url and the response headers. Note that the caller should also
// check IsSignedExchangeHandlingEnabled() before really enabling the feature.
bool ShouldHandleAsSignedHTTPExchange(
    const GURL& request_url,
    const network::mojom::URLResponseHead& head);

// Extracts the signed exchange version [1] from |content_type|, and converts it
// to SignedExchanveVersion. Returns nullopt if the mime type is not a variant
// of application/signed-exchange. Returns SignedExchangeVersion::kUnknown if an
// unsupported signed exchange version is found.
// [1] https://wicg.github.io/webpackage/loading.html#signed-exchange-version
CONTENT_EXPORT base::Optional<SignedExchangeVersion> GetSignedExchangeVersion(
    const std::string& content_type);

// Returns the matching SignedExchangeLoadResult for the verifier's result.
// There is a gap between the logic of SignedExchangeSignatureVerifier and the
// spec of Loading Signed Exchanges [1]. This method is used to fill the gap
// and send a correct signed exchange report.
// [1] https://wicg.github.io/webpackage/loading.html
SignedExchangeLoadResult GetLoadResultFromSignatureVerifierResult(
    SignedExchangeSignatureVerifier::Result verify_result);

// Creates a RedirectInfo of synthesized redirect for signed exchange loading.
net::RedirectInfo CreateRedirectInfo(
    const GURL& new_url,
    const network::ResourceRequest& outer_request,
    const network::mojom::URLResponseHead& outer_response,
    bool is_fallback_redirect);

// Creates a URLResponseHead of synthesized redirect for signed exchange
// loading.
network::mojom::URLResponseHeadPtr CreateRedirectResponseHead(
    const network::mojom::URLResponseHead& outer_response,
    bool is_fallback_redirect);

// Creates a new request ID for browser initiated requests. Can be called on
// any thread.
int MakeRequestID();

// Returns the time to be used for verifying signed exchange. Can be overridden
// using SetVerificationTimeForTesting().
base::Time GetVerificationTime();

// Override the time which is used for verifying signed exchange.
CONTENT_EXPORT void SetVerificationTimeForTesting(
    base::Optional<base::Time> verification_time_for_testing);

}  // namespace signed_exchange_utils
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
