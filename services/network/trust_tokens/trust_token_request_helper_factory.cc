// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_helper_factory.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "net/base/isolation_info.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/boringssl_trust_token_issuance_cryptographer.h"
#include "services/network/trust_tokens/boringssl_trust_token_redemption_cryptographer.h"
#include "services/network/trust_tokens/operating_system_matching.h"
#include "services/network/trust_tokens/operation_timing_request_helper_wrapper.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_controller.h"
#include "services/network/trust_tokens/trust_token_operation_metrics_recorder.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_request_redemption_helper.h"
#include "services/network/trust_tokens/trust_token_request_signing_helper.h"
#include "services/network/trust_tokens/types.h"

namespace network {

namespace {

using Outcome = internal::TrustTokenRequestHelperFactoryOutcome;

std::string_view OutcomeToString(Outcome outcome) {
  switch (outcome) {
    case Outcome::kSuccessfullyCreatedAnIssuanceHelper:
      return "Successfully created an issuance helper";
    case Outcome::kSuccessfullyCreatedARedemptionHelper:
      return "Successfully created a redemption helper";
    case Outcome::kSuccessfullyCreatedASigningHelper:
      return "Successfully created a signing helper";
    case Outcome::kEmptyIssuersParameter:
      return "Empty 'issuers' parameter";
    case Outcome::kUnsuitableIssuerInIssuersParameter:
      return "Unsuitable issuer in 'issuers' parameter";
    case Outcome::kUnsuitableTopFrameOrigin:
      return "Unsuitable top frame origin";
    case Outcome::kRequestRejectedDueToBearingAnInternalTrustTokensHeader:
      return "Request rejected due to bearing an internal Trust Tokens header";
    case Outcome::kRejectedByAuthorizer:
      return "Rejected by authorizer (check cookie settings?)";
  }
}

void LogOutcome(const net::NetLogWithSource& log,
                mojom::TrustTokenOperationType operation,
                Outcome outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Net.TrustTokens.RequestHelperFactoryOutcome.",
                    internal::TrustTokenOperationTypeToString(operation)}),
      outcome);
  log.EndEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_REQUESTED, [outcome]() {
        return base::Value::Dict().Set("outcome", OutcomeToString(outcome));
      });
}

}  // namespace

TrustTokenRequestHelperFactory::TrustTokenRequestHelperFactory(
    PendingTrustTokenStore* store,
    const TrustTokenKeyCommitmentGetter* key_commitment_getter,
    base::RepeatingCallback<mojom::NetworkContextClient*(void)>
        context_client_provider,
    base::RepeatingCallback<bool(void)> authorizer)
    : store_(store),
      key_commitment_getter_(key_commitment_getter),
      context_client_provider_(std::move(context_client_provider)),
      authorizer_(std::move(authorizer)) {}
TrustTokenRequestHelperFactory::~TrustTokenRequestHelperFactory() = default;

void TrustTokenRequestHelperFactory::CreateTrustTokenHelperForRequest(
    const url::Origin& top_frame_origin,
    const net::HttpRequestHeaders& headers,
    const mojom::TrustTokenParams& params,
    const net::NetLogWithSource& net_log,
    base::OnceCallback<void(TrustTokenStatusOrRequestHelper)> done) {
  net_log.BeginEventWithIntParams(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_REQUESTED,
      "Operation type (mojom.TrustTokenOperationType)",
      static_cast<int>(params.operation));

  if (!authorizer_.Run()) {
    LogOutcome(net_log, params.operation, Outcome::kRejectedByAuthorizer);
    std::move(done).Run(mojom::TrustTokenOperationStatus::kUnauthorized);
    return;
  }

  for (std::string_view header : TrustTokensRequestHeaders()) {
    if (headers.HasHeader(header)) {
      LogOutcome(
          net_log, params.operation,
          Outcome::kRequestRejectedDueToBearingAnInternalTrustTokensHeader);
      std::move(done).Run(mojom::TrustTokenOperationStatus::kInvalidArgument);
      return;
    }
  }

  std::optional<SuitableTrustTokenOrigin> maybe_top_frame_origin =
      SuitableTrustTokenOrigin::Create(top_frame_origin);
  if (!maybe_top_frame_origin) {
    LogOutcome(net_log, params.operation, Outcome::kUnsuitableTopFrameOrigin);
    std::move(done).Run(mojom::TrustTokenOperationStatus::kFailedPrecondition);
    return;
  }

  store_->ExecuteOrEnqueue(
      base::BindOnce(&TrustTokenRequestHelperFactory::ConstructHelperUsingStore,
                     weak_factory_.GetWeakPtr(), *maybe_top_frame_origin,
                     params.Clone(), net_log, std::move(done)));
}

void TrustTokenRequestHelperFactory::ConstructHelperUsingStore(
    SuitableTrustTokenOrigin top_frame_origin,
    mojom::TrustTokenParamsPtr params,
    net::NetLogWithSource net_log,
    base::OnceCallback<void(TrustTokenStatusOrRequestHelper)> done,
    TrustTokenStore* store) {
  DCHECK(params);

  auto metrics_recorder =
      std::make_unique<TrustTokenOperationMetricsRecorder>(params->operation);

  switch (params->operation) {
    case mojom::TrustTokenOperationType::kIssuance: {
      LogOutcome(net_log, params->operation,
                 Outcome::kSuccessfullyCreatedAnIssuanceHelper);
      auto helper = std::make_unique<TrustTokenRequestIssuanceHelper>(
          std::move(top_frame_origin), store, key_commitment_getter_,
          params->custom_key_commitment, params->custom_issuer,
          std::make_unique<BoringsslTrustTokenIssuanceCryptographer>(),
          std::move(net_log));
      std::move(done).Run(TrustTokenStatusOrRequestHelper(
          std::make_unique<OperationTimingRequestHelperWrapper>(
              std::move(metrics_recorder), std::move(helper))));
      return;
    }

    case mojom::TrustTokenOperationType::kRedemption: {
      LogOutcome(net_log, params->operation,
                 Outcome::kSuccessfullyCreatedARedemptionHelper);
      auto helper = std::make_unique<TrustTokenRequestRedemptionHelper>(
          std::move(top_frame_origin), params->refresh_policy, store,
          key_commitment_getter_, params->custom_key_commitment,
          params->custom_issuer,
          std::make_unique<BoringsslTrustTokenRedemptionCryptographer>(),
          std::move(net_log));
      std::move(done).Run(TrustTokenStatusOrRequestHelper(
          std::make_unique<OperationTimingRequestHelperWrapper>(
              std::move(metrics_recorder), std::move(helper))));
      return;
    }

    case mojom::TrustTokenOperationType::kSigning: {
      if (params->issuers.empty()) {
        LogOutcome(net_log, params->operation, Outcome::kEmptyIssuersParameter);
        std::move(done).Run(mojom::TrustTokenOperationStatus::kInvalidArgument);
        return;
      }

      std::vector<SuitableTrustTokenOrigin> issuers;
      for (url::Origin& potentially_unsuitable_issuer : params->issuers) {
        std::optional<SuitableTrustTokenOrigin> maybe_issuer =
            SuitableTrustTokenOrigin::Create(
                std::move(potentially_unsuitable_issuer));
        if (!maybe_issuer) {
          LogOutcome(net_log, params->operation,
                     Outcome::kUnsuitableIssuerInIssuersParameter);
          std::move(done).Run(
              mojom::TrustTokenOperationStatus::kInvalidArgument);
          return;
        }

        issuers.emplace_back(std::move(*maybe_issuer));
      }

      TrustTokenRequestSigningHelper::Params signing_params(std::move(issuers),
                                                            top_frame_origin);

      LogOutcome(net_log, params->operation,
                 Outcome::kSuccessfullyCreatedASigningHelper);
      auto helper = std::make_unique<TrustTokenRequestSigningHelper>(
          store, std::move(signing_params), std::move(net_log));
      std::move(done).Run(TrustTokenStatusOrRequestHelper(
          std::make_unique<OperationTimingRequestHelperWrapper>(
              std::move(metrics_recorder), std::move(helper))));
      return;
    }
  }
}

TrustTokenStatusOrRequestHelper::TrustTokenStatusOrRequestHelper() = default;

TrustTokenStatusOrRequestHelper::TrustTokenStatusOrRequestHelper(
    mojom::TrustTokenOperationStatus status)
    : status_(status) {
  DCHECK_NE(status_, mojom::TrustTokenOperationStatus::kOk);
}
TrustTokenStatusOrRequestHelper::TrustTokenStatusOrRequestHelper(
    std::unique_ptr<TrustTokenRequestHelper> helper)
    : status_(mojom::TrustTokenOperationStatus::kOk),
      helper_(std::move(helper)) {
  DCHECK(helper_);
}

TrustTokenStatusOrRequestHelper::~TrustTokenStatusOrRequestHelper() = default;

TrustTokenStatusOrRequestHelper::TrustTokenStatusOrRequestHelper(
    TrustTokenStatusOrRequestHelper&&) = default;
TrustTokenStatusOrRequestHelper& TrustTokenStatusOrRequestHelper::operator=(
    TrustTokenStatusOrRequestHelper&&) = default;

}  // namespace network
