// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_attestation_mediator.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/types.h"
#include "url/origin.h"

namespace network {

using Cryptographer = AttributionAttestationMediator::Cryptographer;

struct AttributionAttestationMediator::CryptographerAndBlindMessage {
  std::unique_ptr<Cryptographer> cryptographer;
  absl::optional<std::string> blind_message;
};

struct AttributionAttestationMediator::CryptographerAndToken {
  std::unique_ptr<Cryptographer> cryptographer;
  absl::optional<std::string> token;
};

AttributionAttestationMediator::AttributionAttestationMediator(
    const TrustTokenKeyCommitmentGetter* key_commitment_getter,
    std::unique_ptr<Cryptographer> cryptographer)
    : key_commitment_getter_(std::move(key_commitment_getter)),
      cryptographer_(std::move(cryptographer)) {
  DCHECK(key_commitment_getter_);
  DCHECK(cryptographer_);
}

AttributionAttestationMediator::~AttributionAttestationMediator() = default;

void AttributionAttestationMediator::GetHeadersForAttestation(
    const GURL& url,
    const std::string& message,
    base::OnceCallback<void(net::HttpRequestHeaders)> done) {
  DCHECK(!message_);
  message_ = message;

  absl::optional<SuitableTrustTokenOrigin> issuer =
      SuitableTrustTokenOrigin::Create(url);
  if (!issuer.has_value()) {
    std::move(done).Run(net::HttpRequestHeaders());
    return;
  }

  key_commitment_getter_->Get(
      issuer.value(),
      base::BindOnce(&AttributionAttestationMediator::OnGotKeyCommitment,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done)));
}

void AttributionAttestationMediator::OnGotKeyCommitment(
    base::OnceCallback<void(net::HttpRequestHeaders)> done,
    mojom::TrustTokenKeyCommitmentResultPtr commitment_result) {
  if (!commitment_result) {
    std::move(done).Run(net::HttpRequestHeaders());
    return;
  }

  if (!cryptographer_->Initialize(commitment_result->protocol_version)) {
    std::move(done).Run(net::HttpRequestHeaders());
    return;
  }

  for (const mojom::TrustTokenVerificationKeyPtr& key :
       commitment_result->keys) {
    if (!cryptographer_->AddKey(key->body)) {
      std::move(done).Run(net::HttpRequestHeaders());
      return;
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<Cryptographer> cryptographer,
             std::string message) {
            absl::optional<std::string> blind_message =
                cryptographer->BeginIssuance(message);
            return AttributionAttestationMediator::CryptographerAndBlindMessage{
                std::move(cryptographer), std::move(blind_message)};
          },
          std::move(cryptographer_), message_.value()),
      base::BindOnce(&AttributionAttestationMediator::OnDoneBeginIssuance,
                     weak_ptr_factory_.GetWeakPtr(),
                     commitment_result->protocol_version, std::move(done)));
}

void AttributionAttestationMediator::OnDoneBeginIssuance(
    mojom::TrustTokenProtocolVersion protocol_version,
    base::OnceCallback<void(net::HttpRequestHeaders)> done,
    AttributionAttestationMediator::CryptographerAndBlindMessage
        cryptographer_and_blind_message) {
  cryptographer_ = std::move(cryptographer_and_blind_message.cryptographer);

  if (!cryptographer_and_blind_message.blind_message.has_value()) {
    std::move(done).Run(net::HttpRequestHeaders());
    return;
  }

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(
      kTriggerAttestationHeader,
      std::move(cryptographer_and_blind_message.blind_message.value()));
  request_headers.SetHeader(
      kTrustTokensSecTrustTokenVersionHeader,
      internal::ProtocolVersionToString(protocol_version));
  std::move(done).Run(std::move(request_headers));
}

void AttributionAttestationMediator::ProcessAttestationToGetToken(
    net::HttpResponseHeaders& response_headers,
    base::OnceCallback<void(absl::optional<std::string>)> done) {
  DCHECK(message_.has_value());

  std::string header_value;

  // EnumerateHeader(|iter|=nullptr) asks for the first instance of the header,
  // if any. At most one `kTriggerAttestationHeader` is expected as only one
  // token can attest to a trigger. Subsequent instances of the header are
  // ignored.
  if (!response_headers.EnumerateHeader(
          /*iter=*/nullptr, kTriggerAttestationHeader, &header_value)) {
    std::move(done).Run(absl::nullopt);
    return;
  }
  response_headers.RemoveHeader(kTriggerAttestationHeader);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<Cryptographer> cryptographer,
             std::string blind_token) {
            absl::optional<std::string> token =
                cryptographer->ConfirmIssuanceAndBeginRedemption(blind_token);

            return AttributionAttestationMediator::CryptographerAndToken{
                std::move(cryptographer), std::move(token)};
          },
          std::move(cryptographer_), std::move(header_value)),
      base::BindOnce(
          &AttributionAttestationMediator::OnDoneProcessingIssuanceResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(done)));
}

void AttributionAttestationMediator::OnDoneProcessingIssuanceResponse(
    base::OnceCallback<void(absl::optional<std::string>)> done,
    AttributionAttestationMediator::CryptographerAndToken
        cryptographer_and_token) {
  cryptographer_ = std::move(cryptographer_and_token.cryptographer);

  if (!cryptographer_and_token.token.has_value()) {
    // The response was rejected by the underlying cryptographic library as
    // malformed or otherwise invalid.
    std::move(done).Run(absl::nullopt);
    return;
  }

  std::move(done).Run(std::move(cryptographer_and_token.token.value()));
}

}  // namespace network
