// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/corb/corb_impl.h"

#include <stddef.h>

#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/corb/orb_mimetypes.h"
#include "services/network/public/cpp/corb/orb_sniffers.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using Decision = network::corb::ResponseAnalyzer::Decision;
using MimeType = network::corb::MimeType;
using SniffingResult = network::corb::SniffingResult;

namespace network::corb {

// An interface to enable incremental content sniffing. These are instantiated
// for each each request; thus they can be stateful.
class CrossOriginReadBlocking::CorbResponseAnalyzer::ConfirmationSniffer {
 public:
  virtual ~ConfirmationSniffer() = default;

  // Called after data is read from the network. |sniffing_buffer| contains the
  // entire response body delivered thus far.
  virtual void OnDataAvailable(std::string_view sniffing_buffer) = 0;

  // Returns true if the return value of IsConfirmedContentType() might change
  // with the addition of more data. Returns false if a final decision is
  // available.
  virtual bool WantsMoreData() const = 0;

  // Returns true if the data has been confirmed to be of the CORB-protected
  // content type that this sniffer is intended to detect.
  virtual bool IsConfirmedContentType() const = 0;
};

// A ConfirmationSniffer that wraps one of the sniffing functions from
// CrossOriginReadBlocking.
class CrossOriginReadBlocking::CorbResponseAnalyzer::SimpleConfirmationSniffer
    : public CrossOriginReadBlocking::CorbResponseAnalyzer::
          ConfirmationSniffer {
 public:
  // The function pointer type corresponding to one of the available sniffing
  // functions from CrossOriginReadBlocking.
  using SnifferFunction = decltype(&SniffForHTML);

  explicit SimpleConfirmationSniffer(SnifferFunction sniffer_function)
      : sniffer_function_(sniffer_function) {}
  ~SimpleConfirmationSniffer() override = default;

  SimpleConfirmationSniffer(const SimpleConfirmationSniffer*) = delete;
  SimpleConfirmationSniffer& operator=(const SimpleConfirmationSniffer*) =
      delete;

  void OnDataAvailable(std::string_view sniffing_buffer) final {
    // The sniffing functions don't support streaming, so with each new chunk of
    // data, call the sniffer on the whole buffer.
    last_sniff_result_ = (*sniffer_function_)(sniffing_buffer);
  }

  bool WantsMoreData() const final {
    // kNo and kYes results are final, meaning that sniffing can stop once they
    // occur. A kMaybe result corresponds to an indeterminate state, that could
    // change to kYes or kNo with more data.
    return last_sniff_result_ == SniffingResult::kMaybe;
  }

  bool IsConfirmedContentType() const final {
    // Only confirm the mime type if an affirmative pattern (e.g. an HTML tag,
    // if using the HTML sniffer) was detected.
    //
    // Note that if the stream ends (or net::kMaxBytesToSniff has been reached)
    // and |last_sniff_result_| is kMaybe, the response is allowed to go
    // through.
    return last_sniff_result_ == SniffingResult::kYes;
  }

 private:
  // The function that actually knows how to sniff for a content type.
  SnifferFunction sniffer_function_;

  // Result of sniffing the data available thus far.
  SniffingResult last_sniff_result_ = SniffingResult::kMaybe;
};

Decision CrossOriginReadBlocking::CorbResponseAnalyzer::Init(
    const GURL& request_url,
    const std::optional<url::Origin>& request_initiator,
    mojom::RequestMode request_mode,
    mojom::RequestDestination /*request_destination_from_renderer*/,
    const mojom::URLResponseHead& response) {
  seems_sensitive_from_cors_heuristic_ =
      SeemsSensitiveFromCORSHeuristic(response);
  seems_sensitive_from_cache_heuristic_ =
      SeemsSensitiveFromCacheHeuristic(response);
  supports_range_requests_ = SupportsRangeRequests(response);
  has_nosniff_header_ = HasNoSniff(response);
  content_length_ = response.content_length;
  http_response_code_ =
      response.headers ? response.headers->response_code() : 0;

  // CORB should look directly at the Content-Type header if one has been
  // received from the network. Ignoring |response.mime_type| helps avoid
  // breaking legitimate websites (which might happen more often when blocking
  // would be based on the mime type sniffed by MimeSniffingResourceHandler).
  //
  // This value could be computed later in ShouldBlockBasedOnHeaders after
  // has_nosniff_header, but we compute it here to keep
  // ShouldBlockBasedOnHeaders (which is called twice) const.
  //
  // TODO(nick): What if the mime type is omitted? Should that be treated the
  // same as text/plain? https://crbug.com/795971
  std::string mime_type;
  if (response.headers)
    response.headers->GetMimeType(&mime_type);
  // Canonicalize the MIME type.  Note that even if it doesn't claim to be a
  // blockable type (i.e., HTML, XML, JSON, or plain text), it may still fail
  // the checks during the SniffForFetchOnlyResource() phase.
  canonical_mime_type_ = GetCanonicalMimeType(mime_type);

  should_block_based_on_headers_ =
      ShouldBlockBasedOnHeaders(request_mode, request_url, request_initiator,
                                response, canonical_mime_type_);

  // Check if the response seems sensitive and if so include in our CORB
  // protection logging. We have not sniffed yet, so the answer might be
  // kSniffMore.
  if (seems_sensitive_from_cors_heuristic_ ||
      seems_sensitive_from_cache_heuristic_) {
    // Create a new Origin with a unique internal identifier so we can pretend
    // the request is cross-origin.
    url::Origin cross_origin_request_initiator = url::Origin();
    // kNoCors is used (instead of passing `request_mode`) to also cover CORS
    // requests with the CORB Protection heuristics and UMAs.  Using kNoCors
    // simulates an attacker requesting "seems-sensitive" subresources from a
    // script tag.
    Decision would_protect_based_on_headers = ShouldBlockBasedOnHeaders(
        mojom::RequestMode::kNoCors, request_url,
        cross_origin_request_initiator, response, canonical_mime_type_);
    corb_protection_logging_needs_sniffing_ =
        (would_protect_based_on_headers == Decision::kSniffMore);
    hypothetical_sniffing_mode_ =
        corb_protection_logging_needs_sniffing_ &&
        should_block_based_on_headers_ != Decision::kSniffMore;
    mime_type_bucket_ = GetMimeTypeBucket(response);
  }
  if (needs_sniffing())
    CreateSniffers();

  return GetCorbDecision();
}

CrossOriginReadBlocking::CorbResponseAnalyzer::CorbResponseAnalyzer() = default;

CrossOriginReadBlocking::CorbResponseAnalyzer::~CorbResponseAnalyzer() =
    default;

// static
Decision
CrossOriginReadBlocking::CorbResponseAnalyzer::ShouldBlockBasedOnHeaders(
    mojom::RequestMode request_mode,
    const GURL& request_url,
    const std::optional<url::Origin>& request_initiator,
    const mojom::URLResponseHead& response,
    MimeType canonical_mime_type) {
  // The checks in this method are ordered to rule out blocking in most cases as
  // quickly as possible.  Checks that are likely to lead to returning false or
  // that are inexpensive should be near the top.

  // Extract the `initiator` of the request, allowing requests with no
  // initiator.  (Such requests are browser-initiated and therefore trustworthy;
  // CorsURLLoaderFactory::IsValidRequest enforces that renderer-initiated
  // requests specify a non-null `request_initiator`.)
  if (!request_initiator.has_value())
    return Decision::kAllow;
  const url::Origin& initiator = request_initiator.value();

  // Don't block same-origin documents.
  if (initiator.IsSameOriginWith(request_url))
    return Decision::kAllow;

  // Only apply CORB to `no-cors` requests.
  //
  // CORB doesn't need to block kNavigate requests because results of these are
  // OOPIF-isolated (note that CorsURLLoaderFactory::IsValidRequest
  // validates that only the Browser process can initiate requests in kNavigate
  // mode).
  //
  // CORB doesn't need to work with kSameOrigin, kCors, nor
  // kCorsWithForcedPreflight modes, because these are covered by OOR-CORS.
  if (request_mode != mojom::RequestMode::kNoCors)
    return Decision::kAllow;

  // Requests from foo.example.com will consult foo.example.com's service worker
  // first (if one has been registered).  The service worker can handle requests
  // initiated by foo.example.com even if they are cross-origin (e.g. requests
  // for bar.example.com).  This is okay, because there is no security boundary
  // between foo.example.com and the service worker of foo.example.com + because
  // the response data is "conjured" within the service worker of
  // foo.example.com (rather than being fetched from bar.example.com).
  // Therefore such responses should not be blocked by CORB, unless the
  // initiator opted out of CORS / opted into receiving an opaque response.  See
  // also https://crbug.com/803672.
  if (response.was_fetched_via_service_worker) {
    switch (response.response_type) {
      case mojom::FetchResponseType::kBasic:
      case mojom::FetchResponseType::kCors:
      case mojom::FetchResponseType::kDefault:
      case mojom::FetchResponseType::kError:
        // Non-opaque responses shouldn't be blocked.
        return Decision::kAllow;
      case mojom::FetchResponseType::kOpaque:
      case mojom::FetchResponseType::kOpaqueRedirect:
        // Opaque responses are eligible for blocking. Continue on...
        break;
    }
  }

  // Some types (e.g. ZIP) are protected without any confirmation sniffing.
  if (canonical_mime_type == MimeType::kNeverSniffed)
    return Decision::kBlock;

  // If this is a partial response, sniffing is not possible, so allow the
  // response if it's not a protected mime type.
  std::string range_header;
  response.headers->GetNormalizedHeader("content-range", &range_header);
  bool has_range_header = !range_header.empty();
  if (has_range_header) {
    switch (canonical_mime_type) {
      case MimeType::kOthers:
      case MimeType::kPlain:  // See also https://crbug.com/801709
        return Decision::kAllow;
      case MimeType::kHtml:
      case MimeType::kJson:
      case MimeType::kXml:
        return Decision::kBlock;
      case MimeType::kInvalidMimeType:
      case MimeType::kNeverSniffed:  // Handled much earlier.
        NOTREACHED();
        return Decision::kBlock;
    }
  }

  // We intend to block the response at this point.  However, we will usually
  // sniff the contents to confirm the MIME type, to avoid blocking incorrectly
  // labeled JavaScript, JSONP, etc files.
  //
  // Note: if there is a nosniff header, it means we should honor the response
  // mime type without trying to confirm it.
  //
  // Decide whether to block based on the MIME type.
  switch (canonical_mime_type) {
    case MimeType::kHtml:
    case MimeType::kXml:
    case MimeType::kJson:
    case MimeType::kPlain:
      if (HasNoSniff(response))
        return Decision::kBlock;
      return Decision::kSniffMore;

    case MimeType::kOthers:
      // Stylesheets shouldn't be sniffed for JSON parser breakers - see
      // https://crbug.com/809259.
      if (base::EqualsCaseInsensitiveASCII(response.mime_type, "text/css"))
        return Decision::kAllow;
      return Decision::kSniffMore;

    case MimeType::kInvalidMimeType:
    case MimeType::kNeverSniffed:  // Handled much earlier.
      NOTREACHED();
      return Decision::kBlock;
  }
  NOTREACHED();
  return Decision::kBlock;
}

// static
bool CrossOriginReadBlocking::CorbResponseAnalyzer::
    SeemsSensitiveFromCORSHeuristic(const mojom::URLResponseHead& response) {
  // Check if the response has an Access-Control-Allow-Origin with a value other
  // than "*" or "null" ("null" offers no more protection than "*" because it
  // matches any unique origin).
  if (!response.headers)
    return false;
  std::string cors_header_value;
  response.headers->GetNormalizedHeader("access-control-allow-origin",
                                        &cors_header_value);
  if (cors_header_value != "*" && cors_header_value != "null" &&
      cors_header_value != "") {
    return true;
  }
  return false;
}

// static
bool CrossOriginReadBlocking::CorbResponseAnalyzer::
    SeemsSensitiveFromCacheHeuristic(const mojom::URLResponseHead& response) {
  // Check if the response has both Vary: Origin and Cache-Control: Private
  // headers, which we take as a signal that it may be a sensitive resource. We
  // require both to reduce the number of false positives (as both headers are
  // sometimes used on non-sensitive resources). Cache-Control: no-store appears
  // on non-sensitive resources that change frequently, so we ignore it here.
  if (!response.headers)
    return false;
  bool has_vary_origin = response.headers->HasHeaderValue("vary", "origin");
  bool has_cache_private =
      response.headers->HasHeaderValue("cache-control", "private");
  return has_vary_origin && has_cache_private;
}

// static
bool CrossOriginReadBlocking::CorbResponseAnalyzer::SupportsRangeRequests(
    const mojom::URLResponseHead& response) {
  if (response.headers) {
    std::string value;
    response.headers->GetNormalizedHeader("accept-ranges", &value);
    if (!value.empty() && !base::EqualsCaseInsensitiveASCII(value, "none")) {
      return true;
    }
  }
  return false;
}

// static
CrossOriginReadBlocking::CorbResponseAnalyzer::MimeTypeBucket
CrossOriginReadBlocking::CorbResponseAnalyzer::GetMimeTypeBucket(
    const mojom::URLResponseHead& response) {
  std::string mime_type;
  if (response.headers)
    response.headers->GetMimeType(&mime_type);
  MimeType canonical_mime_type = GetCanonicalMimeType(mime_type);
  switch (canonical_mime_type) {
    case MimeType::kHtml:
    case MimeType::kXml:
    case MimeType::kJson:
    case MimeType::kNeverSniffed:
    case MimeType::kPlain:
      return kProtected;
    case MimeType::kOthers:
      break;
    case MimeType::kInvalidMimeType:
      NOTREACHED();
      break;
  }

  // Javascript is assumed public. See also
  // https://mimesniff.spec.whatwg.org/#javascript-mime-type.
  if (IsJavascriptMimeType(mime_type)) {
    return kPublic;
  }

  // Images are assumed public. See also
  // https://mimesniff.spec.whatwg.org/#image-mime-type.
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (base::StartsWith(mime_type, "image", kCaseInsensitive)) {
    return kPublic;
  }

  // Audio and video are assumed public. See also
  // https://mimesniff.spec.whatwg.org/#audio-or-video-mime-type.
  if (base::StartsWith(mime_type, "audio", kCaseInsensitive) ||
      base::StartsWith(mime_type, "video", kCaseInsensitive) ||
      base::EqualsCaseInsensitiveASCII(mime_type, "application/ogg") ||
      base::EqualsCaseInsensitiveASCII(mime_type, "application/dash+xml")) {
    return kPublic;
  }

  // CSS files are assumed public and must be sent with text/css.
  if (base::EqualsCaseInsensitiveASCII(mime_type, "text/css")) {
    return kPublic;
  }
  return kOther;
}

void CrossOriginReadBlocking::CorbResponseAnalyzer::CreateSniffers() {
  // Create one or more |sniffers_| to confirm that the body is actually the
  // MIME type advertised in the Content-Type header.
  DCHECK(needs_sniffing());
  DCHECK(sniffers_.empty());

  // When the MIME type is "text/plain", create sniffers for HTML, XML and
  // JSON. If any of these sniffers match, the response will be blocked.
  const bool use_all = canonical_mime_type_ == MimeType::kPlain;

  // HTML sniffer.
  if (use_all || canonical_mime_type_ == MimeType::kHtml) {
    sniffers_.push_back(
        std::make_unique<SimpleConfirmationSniffer>(&SniffForHTML));
  }

  // XML sniffer.
  if (use_all || canonical_mime_type_ == MimeType::kXml) {
    sniffers_.push_back(
        std::make_unique<SimpleConfirmationSniffer>(&SniffForXML));
  }

  // JSON sniffer.
  if (use_all || canonical_mime_type_ == MimeType::kJson) {
    sniffers_.push_back(
        std::make_unique<SimpleConfirmationSniffer>(&SniffForJSON));
  }

  // Parser-breaker sniffer.
  //
  // Because these prefixes are an XSSI-defeating mechanism, CORB considers
  // them distinctive enough to be worth blocking no matter the Content-Type
  // header. So this sniffer is created unconditionally.
  //
  // For MimeType::kOthers, this will be the only sniffer that's active.
  sniffers_.push_back(
      std::make_unique<SimpleConfirmationSniffer>(&SniffForFetchOnlyResource));
}

Decision CrossOriginReadBlocking::CorbResponseAnalyzer::Sniff(
    std::string_view data) {
  DCHECK(needs_sniffing());
  DCHECK(!sniffers_.empty());
  DCHECK(!found_blockable_content_);

  DCHECK_LE(data.size(), static_cast<size_t>(net::kMaxBytesToSniff));

  for (size_t i = 0; i < sniffers_.size();) {
    sniffers_[i]->OnDataAvailable(data);

    if (sniffers_[i]->WantsMoreData()) {
      i++;
      continue;
    }

    if (sniffers_[i]->IsConfirmedContentType()) {
      found_blockable_content_ = true;
      sniffers_.clear();
      break;
    } else {
      // This response is CORB-exempt as far as this sniffer is concerned;
      // remove it from the list.
      sniffers_.erase(sniffers_.begin() + i);
    }
  }

  return GetCorbDecision();
}

Decision CrossOriginReadBlocking::CorbResponseAnalyzer::
    HandleEndOfSniffableResponseBody() {
  // If CORB reached the end of sniffable response body, then it means that the
  // HTML, XML, and JSON confirmation sniffers weren't able to confirm that the
  // response body contains HTML, XML, or JSON.  In this case CORB fails open,
  // by assuming that the response body might contain an allowed resource (e.g.
  // an image, or a script).
  return Decision::kAllow;
}

bool CrossOriginReadBlocking::CorbResponseAnalyzer::ShouldAllow() const {
  // If we're in hypothetical mode then CORB must have decided to kAllow (see
  // comment in ShouldBlock). Thus we just need to wait until the sniffers are
  // all done (i.e. empty).
  if (hypothetical_sniffing_mode_) {
    DCHECK_EQ(should_block_based_on_headers_, Decision::kAllow);
    return sniffers_.empty();
  }
  switch (should_block_based_on_headers_) {
    case Decision::kAllow:
      return true;
    case Decision::kSniffMore:
      return sniffers_.empty() && !found_blockable_content_;
    case Decision::kBlock:
      return false;
  }
}

bool CrossOriginReadBlocking::CorbResponseAnalyzer::ShouldBlock() const {
  // If we're in *hypothetical* sniffing mode then the following must be true:
  // (1) We are only sniffing to find out if CORB would have blocked the request
  // were it made cross origin (CORB itself did *not* need to sniff the file).
  // (2) CORB must have decided to kAllow (if it was kBlock then the protection
  // decision would have been kBlock as well, no hypothetical mode needed).
  if (hypothetical_sniffing_mode_) {
    DCHECK_EQ(should_block_based_on_headers_, Decision::kAllow);
    return false;
  }
  switch (should_block_based_on_headers_) {
    case Decision::kAllow:
      return false;
    case Decision::kSniffMore:
      return sniffers_.empty() && found_blockable_content_;
    case Decision::kBlock:
      return true;
  }
}

bool CrossOriginReadBlocking::CorbResponseAnalyzer::
    ShouldReportBlockedResponse() const {
  if (!ShouldBlock())
    return false;

  // Don't bother showing a warning message when blocking responses that are
  // already empty.
  if (content_length_ == 0)
    return false;
  if (http_response_code_ == 204)
    return false;

  // Don't bother showing a warning message when blocking responses that are
  // associated with error responses (e.g. it is quite common to serve a
  // text/html 404 error page for an <img> tag pointing to a wrong URL).
  if (400 <= http_response_code_ && http_response_code_ <= 599)
    return false;

  return true;
}

ResponseAnalyzer::BlockedResponseHandling
CrossOriginReadBlocking::CorbResponseAnalyzer::ShouldHandleBlockedResponseAs()
    const {
  // CORB wants blocked responses to be empty responses.
  return ResponseAnalyzer::BlockedResponseHandling::kEmptyResponse;
}

Decision CrossOriginReadBlocking::CorbResponseAnalyzer::GetCorbDecision() {
  if (ShouldBlock())
    return Decision::kBlock;
  else if (ShouldAllow())
    return Decision::kAllow;
  else
    return Decision::kSniffMore;
}

// static
bool CrossOriginReadBlocking::CorbResponseAnalyzer::HasNoSniff(
    const mojom::URLResponseHead& response) {
  // TODO(vogelheim): Check for compatibility with spec &
  //   ParseContentTypeOptionsHeader. Maybe move this to parsed_headers.
  if (!response.headers)
    return false;
  std::string nosniff_header;
  response.headers->GetNormalizedHeader("x-content-type-options",
                                        &nosniff_header);
  return base::EqualsCaseInsensitiveASCII(nosniff_header, "nosniff");
}

// static
CrossOriginReadBlocking::CorbResponseAnalyzer::CrossOriginProtectionDecision
CrossOriginReadBlocking::CorbResponseAnalyzer::
    SniffingDecisionToProtectionDecision(bool found_blockable_content) {
  if (found_blockable_content)
    return CrossOriginProtectionDecision::kBlockedAfterSniffing;
  return CrossOriginProtectionDecision::kAllowedAfterSniffing;
}

}  // namespace network::corb
