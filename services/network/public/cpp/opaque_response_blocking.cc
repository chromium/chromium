// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/opaque_response_blocking.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/cross_origin_read_blocking.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

namespace {

// This corresponds to "opaque-blocklisted-never-sniffed MIME type" in ORB spec.
bool IsOpaqueBlocklistedNeverSniffedMimeType(base::StringPiece mime_type) {
  return CrossOriginReadBlocking::GetCanonicalMimeType(mime_type) ==
         CrossOriginReadBlocking::MimeType::kNeverSniffed;
}

// ORB spec says that "An opaque-safelisted MIME type" is a JavaScript MIME type
// or a MIME type whose essence is "text/css" or "image/svg+xml".
bool IsOpaqueSafelistedMimeType(base::StringPiece mime_type) {
  // Based on the spec: Is it a MIME type whose essence is "text/css" or
  // "image/svg+xml"?
  if (base::LowerCaseEqualsASCII(mime_type, "image/svg+xml") ||
      base::LowerCaseEqualsASCII(mime_type, "text/css")) {
    return true;
  }

  // Based on the spec: Is it a JavaScript MIME type?
  if (CrossOriginReadBlocking::IsJavascriptMimeType(mime_type))
    return true;

  // https://github.com/annevk/orb/issues/20 tracks explicitly covering DASH
  // mime type in the ORB algorithm.
  if (base::LowerCaseEqualsASCII(mime_type, "application/dash+xml"))
    return true;

  return false;
}

// Return true for multimedia MIME types that
// 1) are not explicitly covered by ORB (e.g. that do not begin with "audio/",
//    "image/", "video/" and that are not covered by
//    IsOpaqueSafelistedMimeType).
// 2) would be recognized by sniffing from steps 6 or 7 of ORB:
//      step 6. If the image type pattern matching algorithm ...
//      step 7. If the audio or video type pattern matching algorithm ...
bool IsSniffableMultimediaType(base::StringPiece mime_type) {
  if (base::LowerCaseEqualsASCII(mime_type, "application/ogg"))
    return true;

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

bool IsOpaqueResponse(const base::Optional<url::Origin>& request_initiator,
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

ResponseHeadersHeuristicForUma CalculateResponseHeadersHeuristicForUma(
    const GURL& request_url,
    const base::Optional<url::Origin>& request_initiator,
    mojom::RequestMode request_mode,
    const mojom::URLResponseHead& response) {
  // Exclude responses that ORB doesn't apply to.
  if (!IsOpaqueResponse(request_initiator, request_mode, response))
    return ResponseHeadersHeuristicForUma::kNonOpaqueResponse;
  DCHECK(request_initiator.has_value());

  // Same-origin requests are allowed (the spec doesn't explicitly deal with
  // this).
  url::Origin target_origin = url::Origin::Create(request_url);
  if (request_initiator->IsSameOriginWith(target_origin))
    return ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders;

  // Presence of an "X-Content-Type-Options: nosniff" header means that ORB will
  // reach a final decision in step 8, before reaching Javascript parsing in
  // step 12:
  //     step 8. If nosniff is true, then return false.
  //     ...
  //     step 12. If response's body parses as JavaScript ...
  if (CrossOriginReadBlocking::ResponseAnalyzer::HasNoSniff(response))
    return ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders;

  // If a mime type is missing then ORB will reach a final decision in step 10,
  // before reaching Javascript parsing in step 12:
  //     step 10. If mimeType is failure, then return true.
  //     ...
  //     step 12. If response's body parses as JavaScript ...
  std::string mime_type;
  if (!response.headers || !response.headers->GetMimeType(&mime_type))
    return ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders;

  // Specific MIME types might make ORB reach a final decision before reaching
  // Javascript parsing step:
  //     step 3.i. If mimeType is an opaque-safelisted MIME type, then return
  //               true.
  //     step 3.ii. If mimeType is an opaque-blocklisted-never-sniffed MIME
  //                type, then return false.
  //     ...
  //     step 11. If mimeType's essence starts with "audio/", "image/", or
  //              "video/", then return false.
  //     ...
  //     step 12. If response's body parses as JavaScript ...
  if (IsOpaqueBlocklistedNeverSniffedMimeType(mime_type) ||
      IsOpaqueSafelistedMimeType(mime_type) ||
      IsSniffableMultimediaType(mime_type)) {
    return ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders;
  }
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (base::StartsWith(mime_type, "audio/", kCaseInsensitive) ||
      base::StartsWith(mime_type, "image/", kCaseInsensitive) ||
      base::StartsWith(mime_type, "video/", kCaseInsensitive)) {
    return ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders;
  }

  // If the http response indicates an error, or a 206 response, then ORB will
  // reach a final decision before reaching Javascript parsing in step 12:
  //     step 9. If response's status is not an ok status, then return false.
  //     ...
  //     step 12. If response's body parses as JavaScript ...
  if (!IsOkayHttpStatus(response) || IsHttpStatus(response, 206))
    return ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders;

  // Otherwise we need to parse the response body as Javascript.
  return ResponseHeadersHeuristicForUma::kRequiresJavascriptParsing;
}

}  // namespace

void LogUmaForOpaqueResponseBlocking(
    const GURL& request_url,
    const base::Optional<url::Origin>& request_initiator,
    mojom::RequestMode request_mode,
    mojom::RequestDestination request_destination,
    const mojom::URLResponseHead& response) {
  ResponseHeadersHeuristicForUma response_headers_decision =
      CalculateResponseHeadersHeuristicForUma(request_url, request_initiator,
                                              request_mode, response);
  base::UmaHistogramEnumeration(
      "SiteIsolation.ORB.ResponseHeadersHeuristic.Decision",
      response_headers_decision);

  switch (response_headers_decision) {
    case ResponseHeadersHeuristicForUma::kNonOpaqueResponse:
      break;

    case ResponseHeadersHeuristicForUma::kProcessedBasedOnHeaders:
      base::UmaHistogramEnumeration(
          "SiteIsolation.ORB.ResponseHeadersHeuristic.ProcessedBasedOnHeaders",
          request_destination);
      break;

    case ResponseHeadersHeuristicForUma::kRequiresJavascriptParsing:
      base::UmaHistogramEnumeration(
          "SiteIsolation.ORB.ResponseHeadersHeuristic."
          "RequiresJavascriptParsing",
          request_destination);
      break;
  }
}

}  // namespace network
