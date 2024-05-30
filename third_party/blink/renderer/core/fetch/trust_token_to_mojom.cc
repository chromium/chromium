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
using network::mojom::blink::TrustTokenOperationStatus;
using network::mojom::blink::TrustTokenOperationType;

PSTFeatures GetPSTFeatures(const ExecutionContext& execution_context) {
  PSTFeatures features;
  features.issuance_enabled = execution_context.IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kPrivateStateTokenIssuance);
  features.redemption_enabled = execution_context.IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kTrustTokenRedemption);
  return features;
}

bool ConvertTrustTokenToMojomAndCheckPermissions(
    const PrivateToken& in,
    const PSTFeatures& pst_features,
    ExceptionState* exception_state,
    network::mojom::blink::TrustTokenParams* out) {
  // The current implementation always has these fields; the implementation
  // always initializes them, and the hasFoo functions always return true. These
  // DCHECKs serve as canaries for implementation changes.
  DCHECK(in.hasOperation());
  DCHECK(in.hasVersion());

  // only version 1 exists at this time
  DCHECK_EQ(in.version().AsEnum(), VersionType::k1);

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
      if (!pst_features.redemption_enabled) {
        exception_state->ThrowDOMException(
            DOMExceptionCode::kNotAllowedError,
            "Private State Token Redemption ('token-redemption') and signing "
            "('send-redemption-record') operations require that the "
            "private-state-token-redemption "
            "Permissions Policy feature be enabled.");
        return false;
      }
      break;
    case TrustTokenOperationType::kIssuance:
      if (!pst_features.issuance_enabled) {
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

DOMException* TrustTokenErrorToDOMException(TrustTokenOperationStatus error) {
  auto create = [](const String& message, DOMExceptionCode code) {
    return DOMException::Create(message, DOMException::GetErrorName(code));
  };

  // This should only be called on failure.
  DCHECK_NE(error, TrustTokenOperationStatus::kOk);

  switch (error) {
    case TrustTokenOperationStatus::kAlreadyExists:
      return create(
          "Redemption operation aborted due to Redemption Record cache hit",
          DOMExceptionCode::kNoModificationAllowedError);
    case TrustTokenOperationStatus::kOperationSuccessfullyFulfilledLocally:
      return create(
          "Private State Tokens operation satisfied locally, without needing "
          "to send the request to its initial destination",
          DOMExceptionCode::kNoModificationAllowedError);
    case TrustTokenOperationStatus::kMissingIssuerKeys:
      return create(
          "No keys currently available for PST issuer. Issuer may need to "
          "register their key commitments.",
          DOMExceptionCode::kInvalidStateError);
    case TrustTokenOperationStatus::kFailedPrecondition:
      return create("Precondition failed during Private State Tokens operation",
                    DOMExceptionCode::kInvalidStateError);
    case TrustTokenOperationStatus::kInvalidArgument:
      return create("Invalid arguments for Private State Tokens operation",
                    DOMExceptionCode::kOperationError);
    case TrustTokenOperationStatus::kResourceExhausted:
      return create("Tokens exhausted for Private State Tokens operation",
                    DOMExceptionCode::kOperationError);
    case TrustTokenOperationStatus::kResourceLimited:
      return create("Quota hit for Private State Tokens operation",
                    DOMExceptionCode::kOperationError);
    case TrustTokenOperationStatus::kUnauthorized:
      return create(
          "Private State Tokens API unavailable due to user settings.",
          DOMExceptionCode::kOperationError);
    case TrustTokenOperationStatus::kBadResponse:
      return create("Unknown response for Private State Tokens operation",
                    DOMExceptionCode::kOperationError);
    default:
      return create("Error executing Trust Tokens operation",
                    DOMExceptionCode::kOperationError);
  }
}

}  // namespace blink
