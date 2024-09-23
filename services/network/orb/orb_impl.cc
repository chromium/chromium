// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/orb/orb_impl.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "services/network/orb/orb_mimetypes.h"
#include "services/network/orb/orb_sniffers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using Decision = network::orb::ResponseAnalyzer::Decision;

namespace network::orb {

namespace {

bool IsNonSniffableImageMimeType(std::string_view mime_type) {
  // TODO(lukasza): Once full Javascript sniffing is implemented, we may start
  // to undesirably block future (=unsniffable) image formats.  We should
  // explicitly recognize MIME types of such image formats below.  See also
  // https://github.com/annevk/orb/issues/3#issuecomment-974334651

  // This function returns true for image formats that are not recognized by
  // net::SniffMimeTypeFromLocalData.  This helps to allow such images.
  return base::EqualsCaseInsensitiveASCII(mime_type, "image/svg+xml");
}

bool IsAudioOrVideoMimeType(std::string_view mime_type) {
  // TODO(lukasza): Restrict this to only known, non-sniffable audio/video types
  // (hopefully we can reach agreement on this approach + document this in ORB
  // spec).  See also https://github.com/annevk/orb/issues/3.  Notes:
  // - In the long-term (once Javascript sniffing is implemented) this will
  //   prevent non-webby images (e.g. image/vnd.adobe.photoshop) from being
  //   unnecessarily allowed by ORB.
  // - In the short-term this shouldn't matter for security of 200 responses
  //   (with only HTML/XML/JSON sniffing current implementation wouldn't block
  //   such non-webby images anyway).
  // - The current implementation reduces risk of blocking range requests for
  //   A) non-sniffable types and B) range responses for middle-of-resource
  //   when first-bytes-response wasn't seen earlier.
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (base::StartsWith(mime_type, "audio/", kCaseInsensitive) ||
      base::StartsWith(mime_type, "video/", kCaseInsensitive)) {
    return true;
  }

  // Special-casing "application/ogg" here is a minor departure from the spec
  // when IsAudioOrVideoMimeType is called from IsOpaqueSafelistedMimeType.
  // OTOH, covering "application/ogg" here helps helps implement step 7 from ORB
  // (sniffing audio/video in the OpaqueResponseBlockingAnalyzer::Sniff method
  // below) because net::SniffMimeTypeFromLocalData may return
  // "application/ogg".
  if (base::EqualsCaseInsensitiveASCII(mime_type, "application/ogg"))
    return true;

  // TODO(lukasza): Address this departure from the spec (which doesn't
  // explicitly mention DASH and other MIME types here).  The current
  // implementation enforces strict MIME types for DASH/HLS resources - if this
  // can ship without too much of web-compatibility issues, then we should
  // modify ORB spec to match this implementation.  If there is too much
  // web-compatibility risk, then ORB might need to fully parse DASH/HLS
  // manifests.
  if (base::EqualsCaseInsensitiveASCII(mime_type, "application/dash+xml"))
    return true;
  if (base::EqualsCaseInsensitiveASCII(mime_type,
                                       "application/vnd.apple.mpegurl"))
    return true;
  if (base::EqualsCaseInsensitiveASCII(mime_type, "text/vtt"))
    return true;

  return false;
}

bool IsTextCssMimeType(std::string_view mime_type) {
  return base::EqualsCaseInsensitiveASCII(mime_type, "text/css");
}

// ORB spec says that "An opaque-safelisted MIME type" is a JavaScript MIME type
// or a MIME type whose essence is "text/css" or "image/svg+xml".
bool IsOpaqueSafelistedMimeType(std::string_view mime_type) {
  // Based on the spec: Is it a MIME type whose essence is text/css [...] ?
  if (IsTextCssMimeType(mime_type))
    return true;

  // Based on the spec: Is it a MIME type whose essence is [...] image/svg+xml?
  if (IsNonSniffableImageMimeType(mime_type))
    return true;

  // Deviation from spec: We do not handle JavaScript MIME types here. See
  // comments at IsOpaqueSafelistedMimeTypeThatWeSniffAnyway and the
  // IsOpaqueSafelistedMimeType call site for details.

  // TODO(vogelheim): Departure from the spec - see the comment in
  // IsAudioOrVideoMimeType for more details.
  if (IsAudioOrVideoMimeType(mime_type))
    return true;

  return false;
}

// ORB spec defines "an opaque-safelisted MIME type". Until we have full ORB
// compliance, we'll need to handle some MIME types differently and run the
// JavaScript-parser-breaker sniffer from CORB on these resources.
bool IsOpaqueSafelistedMimeTypeThatWeSniffAnyway(std::string_view mime_type) {
  // Based on the spec, but handled in HandleEndOfSniffableResponseBody:
  // Is it a JavaScript MIME type?
  if (IsJavascriptMimeType(mime_type)) {
    return true;
  }

  return false;
}

// This corresponds to https://fetch.spec.whatwg.org/#ok-status
bool IsOkayHttpStatus(const mojom::URLResponseHead& response) {
  if (!response.headers)
    return false;

  int code = response.headers->response_code();
  return (200 <= code) && (code <= 299);
}

bool IsHttpStatus(const mojom::URLResponseHead& response,
                  int expected_status_code) {
  if (!response.headers)
    return false;

  int code = response.headers->response_code();
  return code == expected_status_code;
}

bool IsRangeResponseWithMiddleOfResource(
    const mojom::URLResponseHead& response) {
  if (!response.headers)
    return false;

  if (!IsHttpStatus(response, 206))
    return false;

  std::string range;
  if (!response.headers->GetNormalizedHeader("content-range", &range))
    return false;

  int64_t first_byte_position = -1;
  int64_t last_byte_position = -1;
  int64_t instance_length = -1;
  if (!net::HttpUtil::ParseContentRangeHeaderFor206(
          range, &first_byte_position, &last_byte_position, &instance_length)) {
    return false;
  }

  return first_byte_position > 0;
}

bool IsOpaqueResponse(const std::optional<url::Origin>& request_initiator,
                      mojom::RequestMode request_mode,
                      const mojom::URLResponseHead& response) {
  // ORB only applies to "no-cors" requests.
  if (request_mode != mojom::RequestMode::kNoCors)
    return false;

  // Browser-initiated requests are never opaque.
  if (!request_initiator.has_value())
    return false;

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
      case network::mojom::FetchResponseType::kBasic:
      case network::mojom::FetchResponseType::kCors:
      case network::mojom::FetchResponseType::kDefault:
      case network::mojom::FetchResponseType::kError:
        // Non-opaque responses shouldn't be blocked.
        return false;
      case network::mojom::FetchResponseType::kOpaque:
      case network::mojom::FetchResponseType::kOpaqueRedirect:
        // Opaque responses are eligible for blocking. Continue on...
        break;
    }
  }

  return true;
}

bool HasNoSniff(
    const mojom::URLResponseHead& response) {
  // TODO(vogelheim): Check for compatibility with spec &
  //   ParseContentTypeOptionsHeader. Maybe move this to parsed_headers.
  if (!response.headers) {
    return false;
  }
  std::string nosniff_header;
  response.headers->GetNormalizedHeader("x-content-type-options",
                                        &nosniff_header);
  return base::EqualsCaseInsensitiveASCII(nosniff_header, "nosniff");
}

}  // namespace

OpaqueResponseBlockingAnalyzer::OpaqueResponseBlockingAnalyzer(
    PerFactoryState* state)
    : per_factory_state_(*state) {
  CHECK(state);
}

OpaqueResponseBlockingAnalyzer::~OpaqueResponseBlockingAnalyzer() {
  // TODO(crbug.com/40169301): Add UMA tracking the size of ORB state
  // from `per_factory_state_`.
}

Decision OpaqueResponseBlockingAnalyzer::Init(
    const GURL& request_url,
    const std::optional<url::Origin>& request_initiator,
    mojom::RequestMode request_mode,
    mojom::RequestDestination request_destination_from_renderer,
    const network::mojom::URLResponseHead& response) {
  // Exclude responses that ORB doesn't apply to.
  if (!IsOpaqueResponse(request_initiator, request_mode, response))
    return Decision::kAllow;
  DCHECK(request_initiator.has_value());

  // Same-origin requests are allowed (the ORB spec doesn't explicitly deal with
  // this, because it assumes that the Fetch spec has already determined that
  // the request is cross-origin, before handing off to ORB).
  if (request_initiator->IsSameOriginWith(request_url))
    return Decision::kAllow;

  // Remember request properties that will be needed later.
  is_http_status_okay_ = IsOkayHttpStatus(response);
  if (response.content_length == 0)
    is_empty_response_ = true;
  if (response.headers && response.headers->response_code() == 204)
    is_empty_response_ = true;
  if (response.headers &&
      (response.headers->HasHeader("Attribution-Reporting-Register-Source") ||
       response.headers->HasHeader("Attribution-Reporting-Register-Trigger") ||
       response.headers->HasHeader(
           "Attribution-Reporting-Register-OS-Source") ||
       response.headers->HasHeader(
           "Attribution-Reporting-Register-OS-Trigger"))) {
    is_attribution_response_ = true;
  }
  // TODO(lukasza): Consider tweaking how `final_request_url_` is used to
  // properly handle interactions between redirects and range requests.  For
  // example, ORB might sniff an initial a.com/a1 -> a.com/a2 redirect as media
  // which should allow future range requests to the "same" resource.  But what
  // if in the future something like load-balancing kicks-in and a.com/a1 ->
  // a.com/a3 redirect happens instead?  This might require remembering that not
  // just a2, but also a1 is safe.  Similar considerations (checking all
  // consecutive, same-origin redirect hops) apply both to the initial request
  // (deciding which URLs from the redirect chain to store as validated as
  // media) and to the subsequent range requests (deciding which URLs from the
  // chain to validate against the ones in the store of validated URLs).
  final_request_url_ = request_url;

  request_destination_from_renderer_ = request_destination_from_renderer;

  // 1. Let mimeType be the result of extracting a MIME type from response's
  //    header list.
  if (response.headers)
    response.headers->GetMimeType(&mime_type_);

  // 2. Let nosniff be the result of determining nosniff given response's header
  //    list.
  is_no_sniff_header_present_ =
      HasNoSniff(response);

  // 3. If mimeType is not failure, then:
  if (!mime_type_.empty()) {
    // 3.i. If mimeType is an opaque-safelisted MIME type, then return true.
    //
    // Because "ORB v0.1" does not have a JSON/JS parser step, we will not
    // consider JS resources here and instead employ JSON-or-JS-parser-breaker
    // sniffer on these resources. This means that for JS resources, step 3.i.
    // from ORB is postponed until HandleEndOfSniffableResponseBody, instead of
    // being handled here.
    //
    // Whether ORB spec can adopt this behavior is being discussed in
    // https://github.com/annevk/orb/issues/30.
    //
    // TODO(vogelheim/lukasza): Resolve this difference from the ORB spec.
    // TODO(vogelheim/lukasza): Consider other early-allow mechanisms (e.g. CORP
    // - see https://github.com/annevk/orb/issues/30#issuecomment-971373842).
    if (IsOpaqueSafelistedMimeType(mime_type_))
      return Decision::kAllow;

    // ii. If mimeType is an opaque-blocklisted-never-sniffed MIME type, then
    //     return false.
    // iv. If nosniff is true and mimeType is an opaque-blocklisted MIME type or
    //     its essence is "text/plain", then return false.
    //
    // Step iii. is missing - this is departure from how full ORB handles 206
    // responses labeled as html/json/xml.  This seems okay given that we
    // tighten our implementation of step 4 below (handling of range requests).
    switch (GetCanonicalMimeType(mime_type_)) {
      case MimeType::kNeverSniffed:
        blocking_decision_reason_ =
            BlockingDecisionReason::kNeverSniffedMimeType;
        return Decision::kBlock;  // Step ii.

      case MimeType::kHtml:
      case MimeType::kJson:
      case MimeType::kPlain:
      case MimeType::kXml:
        if (is_no_sniff_header_present_) {
          blocking_decision_reason_ = BlockingDecisionReason::kNoSniffHeader;
          return Decision::kBlock;  // Step iv.
        }
        break;

      case MimeType::kOthers:
        // TODO(vogelheim/lukasza): Departure from the spec: We currently
        // handle audio/video MIME types as "opaque safelisted", to prevent
        // sniffing on them and on XML-based media types in particular.
        CHECK(!IsAudioOrVideoMimeType(mime_type_));
        break;

      case MimeType::kInvalidMimeType:
        break;
    }
  }

  // 4. If request's no-cors media request state is "subsequent", then return
  //    true.
  //
  // TODO(lukasza): Departure from the spec:
  // Diff from the (blocking) step 3.iii.:
  // - Moved slightly later
  // - No extra conditions like "and mimeType is an opaque-blocklisted MIME
  //   type" (e.g. html, xml, or json).
  // Diff from the (allowing) step 4.:
  // - Only applying this step to IsRangeResponseWithMiddleOfResource cases
  if (IsRangeResponseWithMiddleOfResource(response)) {
    if (IsAllowedAudioVideoRequest(request_url)) {
      return Decision::kAllow;
    } else {
      blocking_decision_reason_ =
          BlockingDecisionReason::kUnexpectedRangeResponse;
      return Decision::kBlock;
    }
  }

  // 5. Wait for 1024 bytes of response or end-of-file, whichever comes first
  //    and let bytes be those bytes.
  return Decision::kSniffMore;
}

Decision OpaqueResponseBlockingAnalyzer::Sniff(std::string_view data) {
  std::string sniffed_mime_type;
  net::SniffMimeTypeFromLocalData(data, &sniffed_mime_type);

  // 7. If the audio or video type pattern matching algorithm given bytes does
  //    not return undefined, then:
  if (IsAudioOrVideoMimeType(sniffed_mime_type)) {
    // i. Append (request's opaque media identifier, request's current URL) to
    //    the user agent's opaque-safelisted requesters set.
    StoreAllowedAudioVideoRequest(final_request_url_);

    // ii. Return true.
    return Decision::kAllow;
  }

  // Spec-divergence: no step 8:
  // 8. If requests's no-cors media request state is not "N/A", then return
  //    false.
  // This implementation doesn't know if the request came from a media element
  // or not.  Making the decision based on earlier sniffing should be okay.

  // 9. If the image type pattern matching algorithm given bytes does not
  //    return undefined, then return true.
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (base::StartsWith(sniffed_mime_type, "image/", kCaseInsensitive))
    return Decision::kAllow;

  // At this point, a number of MIME types should be out of the running.
  CHECK(!IsTextCssMimeType(mime_type_));  // OpaqueSafelistedMimeType are not
                                          // sniffed.
  CHECK(!IsAudioOrVideoMimeType(mime_type_));       // Ditto.
  CHECK(!IsNonSniffableImageMimeType(mime_type_));  // Ditto.

  // 12. If mimeType is failure, then return true.
  //
  // The spec proposal handles this step before checking for JS and JSON. To
  // be compatible, we handle this before our 'sniffing' steps that handle
  // those formats.
  //
  // TODO(lukasza): This is not fully accurate - it doesn't capture all the
  // possible failure modes of
  // https://fetch.spec.whatwg.org/#concept-header-extract-mime-type
  if (mime_type_.empty()) {
    return Decision::kAllow;
  }

  // Check if the response is HTML, XML, or JSON, in which case it is surely not
  // JavaScript.  (The sniffers account for HTML/JS polyglot cases - see
  // https://crbug.com/839945 and https://crbug.com/839425.  OTOH, the sniffers
  // do not account for CSS/HTML or CSS/JS-parser-breakers polyglots so CSS is
  // explicitly excluded from the sniffing below.)
  //
  // TODO(lukasza): Departure from the spec.  This avoids having to sniff
  // Javascript in the full response as described in the "Gradual CORB -> ORB
  // transition" doc at
  // https://docs.google.com/document/d/1qUbE2ySi6av3arUEw5DNdFJIKKBbWGRGsXz_ew3S7HQ/edit?usp=sharing
  // Diff: This is a new sniffing step for the 1st 1024 bytes.
  // Diff: This doesn't sniff for JavaScript, but for non-Html/Xml/Json.
  if (SniffForHTML(data) == SniffingResult::kYes) {
    blocking_decision_reason_ = BlockingDecisionReason::kSniffedAsHtml;
    return Decision::kBlock;
  }

  if (SniffForXML(data) == SniffingResult::kYes) {
    blocking_decision_reason_ = BlockingDecisionReason::kSniffedAsXml;
    return Decision::kBlock;
  }

  // Check for JSON and JS parser breakers.
  if (SniffForFetchOnlyResource(data) == SniffingResult::kYes) {
    blocking_decision_reason_ = BlockingDecisionReason::kSniffedAsJson;
    return Decision::kBlock;
  }

  return Decision::kSniffMore;
}

Decision OpaqueResponseBlockingAnalyzer::HandleEndOfSniffableResponseBody() {
  // Deviation from spec: We run JSON-or-JS-parser-breaker sniffer on some
  // MIME types. To do so, we have taken them out of IsOpaqueSafelistedMimeType
  // and instead handle them here. So this effectively handles some cases
  // the spec handles in step 3.i.
  //
  // TODO(vogelheim/lukasza): Resolve this difference from the ORB spec.
  // TODO(vogelheim/lukasza): Consider other early-allow mechanisms (e.g. CORP -
  // see https://github.com/annevk/orb/issues/30#issuecomment-971373842).
  if (IsOpaqueSafelistedMimeTypeThatWeSniffAnyway(mime_type_))
    return Decision::kAllow;

  // TODO(lukasza): Implement the following steps from ORB spec:
  // 10. If nosniff is true, then return false.
  // 11. If response's status is not an ok status, then return false.
  // (Skipping these steps minimizes the risk of shipping the initial ORB
  // implementation.)

  // TODO(lukasza): Departure from the spec discussed in
  // https://github.com/annevk/orb/issues/3.
  // Diff: Removing step 13:
  //     13. If mimeType's essence starts with "audio/", "image/", or "video/",
  //          then return false.

  // TODO(lukasza): Departure from the spec, because the current implementation
  // avoids full Javascript parsing as described in the "Gradual CORB -> ORB
  // transition" doc at
  // https://docs.google.com/document/d/1qUbE2ySi6av3arUEw5DNdFJIKKBbWGRGsXz_ew3S7HQ/edit?usp=sharing
  // Diff: Skipping/ignoring step 15:
  //     15. If response's body parses as JavaScript and does not parse as JSON,
  //         then return true.
  // Diff: Changing step 16 to fail open (e.g. return true / kAllow):
  //     16. Return false.
  return Decision::kAllow;
}

bool OpaqueResponseBlockingAnalyzer::ShouldReportBlockedResponse() const {
  // Empty attribution responses may still result in changes to web-visible
  // behavior when blocked, so they should always be reported. See
  // https://crbug.com/1369637.
  return (!is_empty_response_ && is_http_status_okay_) ||
         is_attribution_response_;
}

ResponseAnalyzer::BlockedResponseHandling
OpaqueResponseBlockingAnalyzer::ShouldHandleBlockedResponseAs() const {
  // "ORB v0.1" uses CORB-style error handling with injecting an empty response.
  // "ORB v0.2" uses ORB-specified error handling (injecting a network error)
  // for non-script fetches, by injecting a network error.
  // "ORB errors-for-all-fetches" uses ORB-specified error handling everywhere.

  if (base::FeatureList::IsEnabled(
          features::kOpaqueResponseBlockingErrorsForAllFetches)) {
    return BlockedResponseHandling::kNetworkError;
  }

  if (request_destination_from_renderer_ != mojom::RequestDestination::kEmpty) {
    return BlockedResponseHandling::kNetworkError;
  }

  return BlockedResponseHandling::kEmptyResponse;
}

void OpaqueResponseBlockingAnalyzer::StoreAllowedAudioVideoRequest(
    const GURL& media_url) {
  per_factory_state_->insert(media_url);
}

bool OpaqueResponseBlockingAnalyzer::IsAllowedAudioVideoRequest(
    const GURL& media_url) {
  return base::Contains(*per_factory_state_, media_url);
}

}  // namespace network::orb
