// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_utils.h"

#include <string_view>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/download_utils_impl.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/browser/web_package/signed_exchange_request_handler.h"
#include "content/common/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {
namespace signed_exchange_utils {

namespace {
constexpr char kLoadResultHistogram[] = "SignedExchange.LoadResult2";
std::optional<base::Time> g_verification_time_for_testing;
}  // namespace

void RecordLoadResultHistogram(SignedExchangeLoadResult result) {
  base::UmaHistogramEnumeration(kLoadResultHistogram, result);
}

void ReportErrorAndTraceEvent(
    SignedExchangeDevToolsProxy* devtools_proxy,
    const std::string& error_message,
    std::optional<SignedExchangeError::FieldIndexPair> error_field) {
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("loading"),
                       "SignedExchangeError", TRACE_EVENT_SCOPE_THREAD, "error",
                       error_message);
  if (devtools_proxy)
    devtools_proxy->ReportError(error_message, std::move(error_field));
}

bool IsSignedExchangeHandlingEnabled(BrowserContext* context) {
  return GetContentClient()->browser()->AllowSignedExchange(context);
}

bool IsSignedExchangeReportingForDistributorsEnabled() {
  return base::FeatureList::IsEnabled(network::features::kReporting);
}

bool ShouldHandleAsSignedHTTPExchange(
    const GURL& request_url,
    const network::mojom::URLResponseHead& head) {
  // Currently we don't support the signed exchange which is returned from a
  // service worker.
  // TODO(crbug.com/40558902): Decide whether we should support it or not.
  if (head.was_fetched_via_service_worker)
    return false;
  if (!SignedExchangeRequestHandler::IsSupportedMimeType(head.mime_type))
    return false;
  // Do not handle responses without HttpResponseHeaders.
  // (Example: data:application/signed-exchange,)
  if (!head.headers.get())
    return false;
  if (download_utils::MustDownload(/*browser_context=*/nullptr, request_url,
                                   head.headers.get(), head.mime_type)) {
    return false;
  }
  return true;
}

std::optional<SignedExchangeVersion> GetSignedExchangeVersion(
    std::string_view content_type) {
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
    return std::nullopt;

  // Step 4.Let params be mimeType's parameters. [spec text]
  std::map<std::string, std::string> params;
  if (semicolon != std::string_view::npos) {
    net::HttpUtil::NameValuePairsIterator parser(
        content_type.substr(semicolon + 1), ';');
    while (parser.GetNext()) {
      params[base::ToLowerASCII(parser.name())] = parser.value();
    }
    if (!parser.valid())
      return std::nullopt;
  }
  // Step 5. If params["v"] exists, return it. Otherwise, return undefined.
  //        [spec text]
  auto iter = params.find("v");
  if (iter != params.end()) {
    if (iter->second == "b3")
      return std::make_optional(SignedExchangeVersion::kB3);
    return std::make_optional(SignedExchangeVersion::kUnknown);
  }
  return std::nullopt;
}

SignedExchangeLoadResult GetLoadResultFromSignatureVerifierResult(
    SignedExchangeSignatureVerifier::Result verify_result) {
  switch (verify_result) {
    case SignedExchangeSignatureVerifier::Result::kSuccess:
      return SignedExchangeLoadResult::kSuccess;
    case SignedExchangeSignatureVerifier::Result::kErrCertificateSHA256Mismatch:
      // "Handling the certificate reference
      //   ...
      //   - If the SHA-256 hash of chain’s leaf's certificate is not equal to
      //     certSha256, return "signature_verification_error"." [spec text]
      return SignedExchangeLoadResult::kSignatureVerificationError;
    case SignedExchangeSignatureVerifier::Result::
        kErrSignatureVerificationFailed:
      // "Validating a signature
      //   ...
      //   - If parsedSignature’s signature is not a valid signature of message
      //     by publicKey using the ecdsa_secp256r1_sha256 algorithm, return
      //     invalid." [spec text]
      //
      // "Parsing signed exchanges
      //   - ...
      //   - If parsedSignature is not valid for headerBytes and
      //     requestUrlBytes, and signed exchange version version, return
      //     "signature_verification_error"." [spec text]
      return SignedExchangeLoadResult::kSignatureVerificationError;
    case SignedExchangeSignatureVerifier::Result::kErrUnsupportedCertType:
      // "Validating a signature
      //   ...
      //   - If parsedSignature’s signature is not a valid signature of message
      //     by publicKey using the ecdsa_secp256r1_sha256 algorithm, return
      //     invalid." [spec text]
      //
      // "Parsing signed exchanges
      //   - ...
      //   - If parsedSignature is not valid for headerBytes and
      //     requestUrlBytes, and signed exchange version version, return
      //     "signature_verification_error"." [spec text]
      return SignedExchangeLoadResult::kSignatureVerificationError;
    case SignedExchangeSignatureVerifier::Result::kErrValidityPeriodTooLong:
      // "Cross-origin trust
      //   ...
      //   - If signature’s expiration time is more than 604800 seconds (7 days)
      //     after signature’s date, return "untrusted"." [spec text]
      //
      // "Parsing signed exchanges
      //   - ...
      //   - If parsedSignature does not establish cross-origin trust for
      //     parsedExchange, return "cert_verification_error"." [spec text]
      return SignedExchangeLoadResult::kCertVerificationError;
    case SignedExchangeSignatureVerifier::Result::kErrFutureDate:
    case SignedExchangeSignatureVerifier::Result::kErrExpired:
      // "Validating a signature
      //   ...
      //   - If the UA’s estimate of the current time is more than clockSkew
      //     before signature’s date, return "untrusted".
      //   - If the UA’s estimate of the current time is after signature’s
      //     expiration time, return "untrusted"." [spec text]
      //
      // "Parsing signed exchanges
      //   - ...
      //   - If parsedSignature is not valid for headerBytes and
      //     requestUrlBytes, and signed exchange version version, return
      //     "signature_verification_error"." [spec text]
      return SignedExchangeLoadResult::kSignatureVerificationError;

    // Deprecated error results.
    case SignedExchangeSignatureVerifier::Result::kErrNoCertificate_deprecated:
    case SignedExchangeSignatureVerifier::Result::
        kErrNoCertificateSHA256_deprecated:
    case SignedExchangeSignatureVerifier::Result::
        kErrInvalidSignatureFormat_deprecated:
    case SignedExchangeSignatureVerifier::Result::
        kErrInvalidSignatureIntegrity_deprecated:
    case SignedExchangeSignatureVerifier::Result::
        kErrInvalidTimestamp_deprecated:
      NOTREACHED_IN_MIGRATION();
      return SignedExchangeLoadResult::kSignatureVerificationError;
  }

  NOTREACHED_IN_MIGRATION();
  return SignedExchangeLoadResult::kSignatureVerificationError;
}

net::RedirectInfo CreateRedirectInfo(
    const GURL& new_url,
    const network::ResourceRequest& outer_request,
    const network::mojom::URLResponseHead& outer_response,
    bool is_fallback_redirect) {
  // https://wicg.github.io/webpackage/loading.html#mp-http-fetch
  // Step 3. Set actualResponse's status to 303. [spec text]
  return net::RedirectInfo::ComputeRedirectInfo(
      "GET", outer_request.url, outer_request.site_for_cookies,
      outer_request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
      outer_request.referrer_policy, outer_request.referrer.spec(), 303,
      new_url,
      net::RedirectUtil::GetReferrerPolicyHeader(outer_response.headers.get()),
      false /* insecure_scheme_was_upgraded */, true /* copy_fragment */,
      is_fallback_redirect);
}

network::mojom::URLResponseHeadPtr CreateRedirectResponseHead(
    const network::mojom::URLResponseHead& outer_response,
    bool is_fallback_redirect) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->encoded_data_length = 0;
  std::string buf;
  std::string link_header;
  if (!is_fallback_redirect &&
      outer_response.headers) {
    outer_response.headers->GetNormalizedHeader("link", &link_header);
  }
  if (link_header.empty()) {
    buf = base::StringPrintf("HTTP/1.1 %d %s\r\n", 303, "See Other");
  } else {
    buf = base::StringPrintf(
        "HTTP/1.1 %d %s\r\n"
        "link: %s\r\n",
        303, "See Other", link_header.c_str());
  }
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(buf));
  response_head->encoded_data_length = 0;
  response_head->request_start = outer_response.request_start;
  response_head->response_start = outer_response.response_start;
  response_head->request_time = outer_response.request_time;
  response_head->response_time = outer_response.response_time;
  response_head->load_timing = outer_response.load_timing;
  return response_head;
}

int MakeRequestID() {
  // Request ID for browser initiated requests. request_ids generated by
  // child processes are counted up from 0, while browser created requests
  // start at -2 and go down from there. (We need to start at -2 because -1 is
  // used as a special value all over the resource_dispatcher_host for
  // uninitialized variables.) This way, we no longer have the unlikely (but
  // observed in the real world!) event where we have two requests with the same
  // request_id_.
  static std::atomic_int request_id(-1);

  return --request_id;
}

base::Time GetVerificationTime() {
  if (g_verification_time_for_testing)
    return *g_verification_time_for_testing;
  return base::Time::Now();
}

void SetVerificationTimeForTesting(
    std::optional<base::Time> verification_time_for_testing) {
  g_verification_time_for_testing = verification_time_for_testing;
}

bool IsCookielessOnlyExchange(const net::HttpResponseHeaders& inner_headers) {
  std::optional<std::string_view> value;
  size_t iter = 0;
  while ((value = inner_headers.EnumerateHeader(&iter, "Vary"))) {
    if (base::EqualsCaseInsensitiveASCII(*value, "cookie")) {
      return true;
    }
  }
  return false;
}

}  // namespace signed_exchange_utils
}  // namespace content
