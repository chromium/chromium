// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_utils.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/web_package/origins_list.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/browser/web_package/signed_exchange_request_handler.h"
#include "content/public/common/content_features.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {
namespace signed_exchange_utils {

void ReportErrorAndTraceEvent(
    SignedExchangeDevToolsProxy* devtools_proxy,
    const std::string& error_message,
    base::Optional<SignedExchangeError::FieldIndexPair> error_field) {
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("loading"),
                       "SignedExchangeError", TRACE_EVENT_SCOPE_THREAD, "error",
                       error_message);
  if (devtools_proxy)
    devtools_proxy->ReportError(error_message, std::move(error_field));
}

namespace {

OriginsList CreateAdvertiseAcceptHeaderOriginsList() {
  std::string param = base::GetFieldTrialParamValueByFeature(
      features::kSignedHTTPExchangeAcceptHeader,
      features::kSignedHTTPExchangeAcceptHeaderFieldTrialParamName);
  if (param.empty())
    DLOG(ERROR) << "The Accept-SXG origins list param is empty.";

  return OriginsList(param);
}

}  //  namespace

bool NeedToCheckRedirectedURLForAcceptHeader() {
  // When SignedHTTPExchange is enabled, the SignedExchange accept header must
  // be sent to all origins. So we don't need to check the redirected URL.
  return !base::FeatureList::IsEnabled(features::kSignedHTTPExchange) &&
         base::FeatureList::IsEnabled(
             features::kSignedHTTPExchangeOriginTrial) &&
         base::FeatureList::IsEnabled(
             features::kSignedHTTPExchangeAcceptHeader);
}

bool ShouldAdvertiseAcceptHeader(const url::Origin& origin) {
  // When SignedHTTPExchange is enabled, we must send the SignedExchange accept
  // header to all origins.
  if (base::FeatureList::IsEnabled(features::kSignedHTTPExchange))
    return true;
  // When SignedHTTPExchangeOriginTrial is not enabled or
  // SignedHTTPExchangeAcceptHeader is not enabled, we must not send the
  // SignedExchange accept header.
  if (!base::FeatureList::IsEnabled(features::kSignedHTTPExchangeOriginTrial) ||
      !base::FeatureList::IsEnabled(
          features::kSignedHTTPExchangeAcceptHeader)) {
    return false;
  }

  // |origins_list| is initialized in a thread-safe manner.
  // Querying OriginsList::Match() should be safe since it's read-only access.
  static base::NoDestructor<OriginsList> origins_list(
      CreateAdvertiseAcceptHeaderOriginsList());
  return origins_list->Match(origin);
}

bool IsSignedExchangeHandlingEnabled() {
  return base::FeatureList::IsEnabled(features::kSignedHTTPExchange) ||
         base::FeatureList::IsEnabled(features::kSignedHTTPExchangeOriginTrial);
}

bool ShouldHandleAsSignedHTTPExchange(
    const GURL& request_url,
    const network::ResourceResponseHead& head) {
  // Currently we don't support the signed exchange which is returned from a
  // service worker.
  // TODO(crbug/803774): Decide whether we should support it or not.
  if (head.was_fetched_via_service_worker)
    return false;
  if (!SignedExchangeRequestHandler::IsSupportedMimeType(head.mime_type))
    return false;
  if (base::FeatureList::IsEnabled(features::kSignedHTTPExchange))
    return true;
  if (!base::FeatureList::IsEnabled(features::kSignedHTTPExchangeOriginTrial))
    return false;
  std::unique_ptr<blink::TrialTokenValidator> validator =
      std::make_unique<blink::TrialTokenValidator>();
  return validator->RequestEnablesFeature(
      request_url, head.headers.get(),
      features::kSignedHTTPExchangeOriginTrial.name, base::Time::Now());
}

base::Optional<SignedExchangeVersion> GetSignedExchangeVersion(
    const std::string& content_type) {
  // https://wicg.github.io/webpackage/loading.html#signed-exchange-version
  // Step 1. Let mimeType be the supplied MIME type of response. [spec text]
  // |content_type| is the supplied MIME type.
  // Step 2. If mimeType is undefined, return undefined. [spec text]
  // Step 3. If mimeType's essence is not "application/signed-exchange", return
  //         undefined. [spec text]
  const std::string::size_type semicolon = content_type.find(';');
  const std::string essence = base::ToLowerASCII(base::TrimWhitespaceASCII(
      content_type.substr(0, semicolon), base::TRIM_ALL));
  if (essence != "application/signed-exchange")
    return base::nullopt;

  // Step 4.Let params be mimeType's parameters. [spec text]
  std::map<std::string, std::string> params;
  if (semicolon != base::StringPiece::npos) {
    net::HttpUtil::NameValuePairsIterator parser(
        content_type.begin() + semicolon + 1, content_type.end(), ';');
    while (parser.GetNext()) {
      const base::StringPiece name(parser.name_begin(), parser.name_end());
      params[base::ToLowerASCII(name)] = parser.value();
    }
    if (!parser.valid())
      return base::nullopt;
  }
  // Step 5. If params["v"] exists, return it. Otherwise, return undefined.
  //        [spec text]
  auto iter = params.find("v");
  if (iter != params.end()) {
    if (iter->second == "b2")
      return base::make_optional(SignedExchangeVersion::kB2);
    return base::make_optional(SignedExchangeVersion::kUnknown);
  }
  return base::nullopt;
}

}  // namespace signed_exchange_utils
}  // namespace content
