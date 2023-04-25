// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_private_token.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

using VersionType = V8PrivateTokenVersion::Enum;
using OperationType = V8OperationType::Enum;
using RefreshPolicy = V8RefreshPolicy::Enum;
using network::mojom::blink::TrustTokenOperationType;

bool ConvertTrustTokenToMojomAndCheckPermissions(
    const PrivateToken& in,
    const ExecutionContext* execution_context,
    ExceptionState* exception_state,
    network::mojom::blink::TrustTokenParams* out) {
  DCHECK(in.hasOperation());  // field is required in IDL

  // get token version
  if (in.hasVersion()) {
    // only version 1 is supported
    if (in.version().AsEnum() == VersionType::k1) {
      out->version =
          network::mojom::blink::TrustTokenMajorVersion::kPrivateStateTokenV1;
    } else {
      exception_state->ThrowTypeError("privateToken: unknown token version.");
      return false;
    }
  } else {
    exception_state->ThrowTypeError(
        "trustToken: token version is not specified.");
    return false;
  }

  if (in.operation().AsEnum() == OperationType::kTokenRequest) {
    out->operation = network::mojom::blink::TrustTokenOperationType::kIssuance;
  } else if (in.operation().AsEnum() == OperationType::kTokenRedemption) {
    out->operation =
        network::mojom::blink::TrustTokenOperationType::kRedemption;

    DCHECK(in.hasRefreshPolicy());  // default is defined

    if (in.refreshPolicy().AsEnum() == RefreshPolicy::kNone) {
      out->refresh_policy =
          network::mojom::blink::TrustTokenRefreshPolicy::kUseCached;
    } else if (in.refreshPolicy().AsEnum() == RefreshPolicy::kRefresh) {
      out->refresh_policy =
          network::mojom::blink::TrustTokenRefreshPolicy::kRefresh;
    }
  } else {
    // The final possible value of the type enum.
    DCHECK_EQ(in.operation().AsEnum(), OperationType::kSendRedemptionRecord);
    out->operation = network::mojom::blink::TrustTokenOperationType::kSigning;

    if (in.hasIssuers() && !in.issuers().empty()) {
      for (const String& issuer : in.issuers()) {
        // Two conditions on the issuers:
        // 1. HTTP or HTTPS (because much Trust Tokens protocol state is
        // stored keyed by issuer origin, requiring HTTP or HTTPS is a way to
        // ensure these origins serialize to unique values);
        // 2. potentially trustworthy (a security requirement).
        KURL parsed_url = KURL(issuer);
        if (!parsed_url.ProtocolIsInHTTPFamily()) {
          exception_state->ThrowTypeError(
              "privateToken: operation type 'send-redemption-record' requires "
              "that "
              "the 'issuers' "
              "fields' members parse to HTTP(S) origins, but one did not: " +
              issuer);
          return false;
        }

        out->issuers.push_back(blink::SecurityOrigin::Create(parsed_url));
        DCHECK(out->issuers.back());  // SecurityOrigin::Create cannot fail.
        if (!out->issuers.back()->IsPotentiallyTrustworthy()) {
          exception_state->ThrowTypeError(
              "privateToken: operation type 'send-redemption-record' requires "
              "that "
              "the 'issuers' "
              "fields' members parse to secure origins, but one did not: " +
              issuer);
          return false;
        }
      }
    } else {
      exception_state->ThrowTypeError(
          "privateToken: operation type 'send-redemption-record' requires that "
          "the 'issuers' field be present and contain at least one secure, "
          "HTTP(S) URL, but it was missing or empty.");
      return false;
    }
  }

  switch (out->operation) {
    case TrustTokenOperationType::kRedemption:
    case TrustTokenOperationType::kSigning:
      if (!execution_context->IsFeatureEnabled(
              mojom::blink::PermissionsPolicyFeature::kTrustTokenRedemption)) {
        exception_state->ThrowDOMException(
            DOMExceptionCode::kNotAllowedError,
            "Private State Token Redemption ('token-redemption') and signing "
            "('send-redemption-record') operations require that the "
            "trust-token-redemption "
            "Permissions Policy feature be enabled.");
        return false;
      }
      break;
    case TrustTokenOperationType::kIssuance:
      if (!execution_context->IsFeatureEnabled(
              mojom::blink::PermissionsPolicyFeature::
                  kPrivateStateTokenIssuance)) {
        exception_state->ThrowDOMException(
            DOMExceptionCode::kNotAllowedError,
            "Private State Token Issuance ('token-request') operation "
            "requires that the private-state-token-issuance "
            "Permissions Policy feature be enabled.");
        return false;
      }
      break;
  }

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
    case network::mojom::blink::TrustTokenOperationStatus::
        kOperationSuccessfullyFulfilledLocally:
      return DOMException::Create(
          "Trust Tokens operation satisfied locally, without needing to send "
          "the request to its initial destination",
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
