// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_

#include <string>

#include "base/optional.h"
#include "content/browser/web_package/origins_list.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/content_export.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace network {
struct ResourceResponseHead;
}  // namespace network

namespace content {

class SignedExchangeDevToolsProxy;

namespace signed_exchange_utils {

// Utility method to call SignedExchangeDevToolsProxy::ReportError() and
// TRACE_EVENT_INSTANT1 to report the error to both DevTools and about:tracing.
// If |devtools_proxy| is nullptr, it just calls TRACE_EVENT_INSTANT1().
void ReportErrorAndTraceEvent(
    SignedExchangeDevToolsProxy* devtools_proxy,
    const std::string& error_message,
    base::Optional<SignedExchangeError::FieldIndexPair> error_field =
        base::nullopt);

// Returns true when SignedHTTPExchange feature is NOT enabled and
// SignedHTTPExchangeOriginTrial and SignedHTTPExchangeAcceptHeader features are
// enabled.
bool NeedToCheckRedirectedURLForAcceptHeader();

// Returns true if Accept headers should be sent with
// "application/signed-exchange".
CONTENT_EXPORT bool ShouldAdvertiseAcceptHeader(const url::Origin& origin);

// Returns true when SignedHTTPExchange feature or SignedHTTPExchangeOriginTrial
// feature is enabled.
bool IsSignedExchangeHandlingEnabled();

// Returns true when the response should be handled as a signed exchange by
// checking the mime type and the feature flags. When SignedHTTPExchange feature
// is not enabled and SignedHTTPExchangeOriginTrial feature is enabled, this
// method also checks the Origin Trial header.
bool ShouldHandleAsSignedHTTPExchange(
    const GURL& request_url,
    const network::ResourceResponseHead& head);

// Extracts the signed exchange version [1] from |content_type|, and converts it
// to SignedExchanveVersion. Returns nullopt if the mime type is not a variant
// of application/signed-exchange. Returns SignedExchangeVersion::kUnknown if an
// unsupported signed exchange version is found.
// [1] https://wicg.github.io/webpackage/loading.html#signed-exchange-version
CONTENT_EXPORT base::Optional<SignedExchangeVersion> GetSignedExchangeVersion(
    const std::string& content_type);

}  // namespace signed_exchange_utils
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
