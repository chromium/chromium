// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"

namespace blink {

bool ConvertTrustTokenToMojom(const TrustToken& in,
                              ExceptionState* exception_state,
                              network::mojom::blink::TrustTokenParams* out) {
  DCHECK(in.hasType());  // field is required in IDL
  if (in.type() == "token-request") {
    out->type = network::mojom::blink::TrustTokenOperationType::kIssuance;
    return true;
  }

  if (in.type() == "token-redemption") {
    out->type = network::mojom::blink::TrustTokenOperationType::kRedemption;

    DCHECK(in.hasRefreshPolicy());  // default is defined
    out->include_timestamp_header = in.includeTimestampHeader();

    if (in.refreshPolicy() == "none") {
      out->refresh_policy =
          network::mojom::blink::TrustTokenRefreshPolicy::kUseCached;
    } else if (in.refreshPolicy() == "refresh") {
      out->refresh_policy =
          network::mojom::blink::TrustTokenRefreshPolicy::kRefresh;
    }
    return true;
  }

  DCHECK_EQ(
      in.type(),
      "send-redemption-record");  // final possible value of the input enum
  out->type = network::mojom::blink::TrustTokenOperationType::kSigning;

  if (in.hasSignRequestData()) {
    if (in.signRequestData() == "omit") {
      out->sign_request_data =
          network::mojom::blink::TrustTokenSignRequestData::kOmit;
    } else if (in.signRequestData() == "include") {
      out->sign_request_data =
          network::mojom::blink::TrustTokenSignRequestData::kInclude;
    } else if (in.signRequestData() == "headers-only") {
      out->sign_request_data =
          network::mojom::blink::TrustTokenSignRequestData::kHeadersOnly;
    }
  }

  if (in.hasAdditionalSignedHeaders()) {
    out->additional_signed_headers = in.additionalSignedHeaders();
  }

  DCHECK(in.hasIncludeTimestampHeader());  // default is defined
  out->include_timestamp_header = in.includeTimestampHeader();

  if (in.hasIssuers() && !in.issuers().IsEmpty()) {
    for (const String& issuer : in.issuers()) {
      // Two conditions on the issuers:
      // 1. HTTP or HTTPS (because much Trust Tokens protocol state is
      // stored keyed by issuer origin, requiring HTTP or HTTPS is a way to
      // ensure these origins serialize to unique values);
      // 2. potentially trustworthy (a security requirement).
      KURL parsed_url = KURL(issuer);
      if (!parsed_url.ProtocolIsInHTTPFamily()) {
        exception_state->ThrowTypeError(
            "trustToken: operation type 'send-redemption-record' requires that "
            "the 'issuers' "
            "fields' members parse to HTTP(S) origins, but one did not: " +
            issuer);
        return false;
      }

      out->issuers.push_back(blink::SecurityOrigin::Create(parsed_url));
      DCHECK(out->issuers.back());  // SecurityOrigin::Create cannot fail.
      if (!out->issuers.back()->IsPotentiallyTrustworthy()) {
        exception_state->ThrowTypeError(
            "trustToken: operation type 'send-redemption-record' requires that "
            "the 'issuers' "
            "fields' members parse to secure origins, but one did not: " +
            issuer);
        return false;
      }
    }
  } else {
    exception_state->ThrowTypeError(
        "trustToken: operation type 'send-redemption-record' requires that the "
        "'issuers' "
        "field be present and contain at least one secure, HTTP(S) URL, but it "
        "was missing or empty.");
    return false;
  }

  if (in.hasAdditionalSigningData())
    out->possibly_unsafe_additional_signing_data = in.additionalSigningData();

  return true;
}

DOMException* TrustTokenErrorToDOMException(
    network::mojom::blink::TrustTokenOperationStatus error) {
  // This should only be called on failure.
  DCHECK_NE(error, network::mojom::blink::TrustTokenOperationStatus::kOk);

  switch (error) {
    case network::mojom::blink::TrustTokenOperationStatus::kAlreadyExists:
      return DOMException::Create(
          "Redemption operation aborted due to Signed Redemption Record "
          "cache hit",
          DOMException::GetErrorName(
              DOMExceptionCode::kNoModificationAllowedError));
    case network::mojom::blink::TrustTokenOperationStatus::kFailedPrecondition:
      return DOMException::Create(
          "Precondition failed during Trust Tokens operation",
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError));
    default:
      return DOMException::Create(
          "Error executing Trust Tokens operation",
          DOMException::GetErrorName(DOMExceptionCode::kOperationError));
  }
}

}  // namespace blink
