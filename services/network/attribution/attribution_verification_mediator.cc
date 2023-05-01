// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_verification_mediator.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/attribution/attribution_verification_mediator_metrics_recorder.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace network {

using Cryptographer = AttributionVerificationMediator::Cryptographer;
using metrics_recorder = AttributionVerificationMediator::MetricsRecorder;

struct AttributionVerificationMediator::CryptographerAndBlindMessage {
  std::unique_ptr<Cryptographer> cryptographer;
  absl::optional<std::string> blind_message;
};

struct AttributionVerificationMediator::CryptographerAndToken {
  std::unique_ptr<Cryptographer> cryptographer;
  absl::optional<std::string> token;
};

AttributionVerificationMediator::AttributionVerificationMediator(
    const TrustTokenKeyCommitmentGetter* key_commitment_getter,
    std::unique_ptr<Cryptographer> cryptographer,
    std::unique_ptr<MetricsRecorder> metrics_recorder)
    : key_commitment_getter_(std::move(key_commitment_getter)),
      cryptographer_(std::move(cryptographer)),
      metrics_recorder_(std::move(metrics_recorder)) {
  DCHECK(key_commitment_getter_);
  DCHECK(cryptographer_);
  DCHECK(metrics_recorder_);
}

AttributionVerificationMediator::~AttributionVerificationMediator() = default;

void AttributionVerificationMediator::GetHeadersForVerification(
    const GURL& url,
    const std::string& message,
    base::OnceCallback<void(net::HttpRequestHeaders)> done) {
  DCHECK(!message_);
  message_ = message;

  metrics_recorder_->Start();

  absl::optional<SuitableTrustTokenOrigin> issuer =
      SuitableTrustTokenOrigin::Create(url);
  if (!issuer.has_value()) {
    metrics_recorder_->FinishGetHeadersWith(
        GetHeadersStatus::kIssuerOriginNotSuitable);
    std::move(done).Run(net::HttpRequestHeaders());
    return;
  }

  key_commitment_getter_->Get(
      issuer.value(),
      base::BindOnce(&AttributionVerificationMediator::OnGotKeyCommitment,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done)));
}

void AttributionVerificationMediator::OnGotKeyCommitment(
    base::OnceCallback<void(net::HttpRequestHeaders)> done,
    mojom::TrustTokenKeyCommitmentResultPtr commitment_result) {
  metrics_recorder_->Complete(Step::kGetKeyCommitment);

  if (!commitment_result) {
    metrics_recorder_->FinishGetHeadersWith(
        GetHeadersStatus::kIssuerNotRegistered);
    std::move(done).Run(net::HttpRequestHeaders());
    return;
  }

  if (!cryptographer_->Initialize(commitment_result->protocol_version)) {
    metrics_recorder_->FinishGetHeadersWith(
        GetHeadersStatus::kUnableToInitializeCryptographer);
    std::move(done).Run(net::HttpRequestHeaders());
    return;
  }
  for (const mojom::TrustTokenVerificationKeyPtr& key :
       commitment_result->keys) {
    if (!cryptographer_->AddKey(key->body)) {
      metrics_recorder_->FinishGetHeadersWith(
          GetHeadersStatus::kUnableToAddKeysOnCryptographer);
      std::move(done).Run(net::HttpRequestHeaders());
      return;
    }
  }
  metrics_recorder_->Complete(Step::kInitializeCryptographer);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<Cryptographer> cryptographer,
             std::string message) {
            absl::optional<std::string> blind_message =
                cryptographer->BeginIssuance(message);
            return AttributionVerificationMediator::
                CryptographerAndBlindMessage{std::move(cryptographer),
                                             std::move(blind_message)};
          },
          std::move(cryptographer_), message_.value()),
      base::BindOnce(&AttributionVerificationMediator::OnDoneBeginIssuance,
                     weak_ptr_factory_.GetWeakPtr(),
                     commitment_result->protocol_version, std::move(done)));
}

void AttributionVerificationMediator::OnDoneBeginIssuance(
    mojom::TrustTokenProtocolVersion protocol_version,
    base::OnceCallback<void(net::HttpRequestHeaders)> done,
    AttributionVerificationMediator::CryptographerAndBlindMessage
        cryptographer_and_blind_message) {
  cryptographer_ = std::move(cryptographer_and_blind_message.cryptographer);
  metrics_recorder_->Complete(Step::kBlindMessage);

  if (!cryptographer_and_blind_message.blind_message.has_value()) {
    metrics_recorder_->FinishGetHeadersWith(
        GetHeadersStatus::kUnableToBlindMessage);
    std::move(done).Run(net::HttpRequestHeaders());
    return;
  }

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(
      kReportVerificationHeader,
      std::move(cryptographer_and_blind_message.blind_message.value()));
  request_headers.SetHeader(
      kTrustTokensSecTrustTokenVersionHeader,
      internal::ProtocolVersionToString(protocol_version));

  metrics_recorder_->FinishGetHeadersWith(GetHeadersStatus::kSuccess);
  std::move(done).Run(std::move(request_headers));
}

void AttributionVerificationMediator::ProcessVerificationToGetToken(
    net::HttpResponseHeaders& response_headers,
    base::OnceCallback<void(absl::optional<std::string>)> done) {
  DCHECK(message_.has_value());

  metrics_recorder_->Complete(Step::kSignBlindMessage);

  std::string header_value;

  // EnumerateHeader(|iter|=nullptr) asks for the first instance of the header,
  // if any. At most one `kReportVerificationHeader` is expected as only one
  // token can verify a trigger. Subsequent instances of the header are ignored.
  if (!response_headers.EnumerateHeader(
          /*iter=*/nullptr, kReportVerificationHeader, &header_value)) {
    metrics_recorder_->FinishProcessVerificationWith(
        ProcessVerificationStatus::kNoSignatureReceivedFromIssuer);
    std::move(done).Run(absl::nullopt);
    return;
  }
  response_headers.RemoveHeader(kReportVerificationHeader);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<Cryptographer> cryptographer,
             std::string blind_token) {
            absl::optional<std::string> token =
                cryptographer->ConfirmIssuanceAndBeginRedemption(blind_token);

            return AttributionVerificationMediator::CryptographerAndToken{
                std::move(cryptographer), std::move(token)};
          },
          std::move(cryptographer_), std::move(header_value)),
      base::BindOnce(
          &AttributionVerificationMediator::OnDoneProcessingIssuanceResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(done)));
}

void AttributionVerificationMediator::OnDoneProcessingIssuanceResponse(
    base::OnceCallback<void(absl::optional<std::string>)> done,
    AttributionVerificationMediator::CryptographerAndToken
        cryptographer_and_token) {
  cryptographer_ = std::move(cryptographer_and_token.cryptographer);

  metrics_recorder_->Complete(Step::kUnblindMessage);

  if (!cryptographer_and_token.token.has_value()) {
    // The response was rejected by the underlying cryptographic library as
    // malformed or otherwise invalid.
    metrics_recorder_->FinishProcessVerificationWith(
        ProcessVerificationStatus::kUnableToUnblindSignature);
    std::move(done).Run(absl::nullopt);
    return;
  }

  metrics_recorder_->FinishProcessVerificationWith(
      ProcessVerificationStatus::kSuccess);
  std::move(done).Run(std::move(cryptographer_and_token.token.value()));
}

}  // namespace network
