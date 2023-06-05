// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_verification_mediator.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/attribution/attribution_verification_mediator_metrics_recorder.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace network {

namespace {

using Cryptographer = AttributionVerificationMediator::Cryptographer;
using metrics_recorder = AttributionVerificationMediator::MetricsRecorder;

using Message = AttributionVerificationMediator::Message;
using BlindedMessage = AttributionVerificationMediator::BlindedMessage;
using BlindedToken = AttributionVerificationMediator::BlindedToken;
using Token = AttributionVerificationMediator::Token;

}  // namespace

struct AttributionVerificationMediator::CryptographersAndBlindedMessages {
  std::vector<std::unique_ptr<Cryptographer>> cryptographers;
  std::vector<BlindedMessage> blinded_messages;
};

struct AttributionVerificationMediator::CryptographersAndTokens {
  std::vector<std::unique_ptr<Cryptographer>> cryptographers;
  std::vector<Token> tokens;
};

AttributionVerificationMediator::AttributionVerificationMediator(
    const TrustTokenKeyCommitmentGetter* key_commitment_getter,
    std::vector<std::unique_ptr<Cryptographer>> cryptographers,
    std::unique_ptr<MetricsRecorder> metrics_recorder)
    : key_commitment_getter_(std::move(key_commitment_getter)),
      cryptographers_(std::move(cryptographers)),
      metrics_recorder_(std::move(metrics_recorder)) {
  CHECK(key_commitment_getter_);
  CHECK(metrics_recorder_);
}

AttributionVerificationMediator::~AttributionVerificationMediator() = default;

void AttributionVerificationMediator::GetHeadersForVerification(
    const GURL& url,
    std::vector<Message> messages,
    base::OnceCallback<void(net::HttpRequestHeaders)> done) {
  CHECK(messages_.empty());
  CHECK(!messages.empty());
  CHECK_EQ(messages.size(), cryptographers_.size());

  messages_ = std::move(messages);

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

  for (const auto& cryptographer : cryptographers_) {
    CHECK(cryptographer);
    if (!cryptographer->Initialize(commitment_result->protocol_version)) {
      metrics_recorder_->FinishGetHeadersWith(
          GetHeadersStatus::kUnableToInitializeCryptographer);
      std::move(done).Run(net::HttpRequestHeaders());
      return;
    }
    for (const mojom::TrustTokenVerificationKeyPtr& key :
         commitment_result->keys) {
      if (!cryptographer->AddKey(key->body)) {
        metrics_recorder_->FinishGetHeadersWith(
            GetHeadersStatus::kUnableToAddKeysOnCryptographer);
        std::move(done).Run(net::HttpRequestHeaders());
        return;
      }
    }
  }

  metrics_recorder_->Complete(Step::kInitializeCryptographer);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AttributionVerificationMediator::BeginIssuances,
                     std::move(cryptographers_), messages_),
      base::BindOnce(&AttributionVerificationMediator::OnDoneBeginIssuance,
                     weak_ptr_factory_.GetWeakPtr(),
                     commitment_result->protocol_version, std::move(done)));
}

// static
AttributionVerificationMediator::CryptographersAndBlindedMessages
AttributionVerificationMediator::BeginIssuances(
    std::vector<std::unique_ptr<Cryptographer>> cryptographers,
    const std::vector<Message>& messages) {
  CHECK_GE(cryptographers.size(), messages.size());
  std::vector<BlindedMessage> blinded_messages;
  blinded_messages.reserve(messages.size());

  for (size_t i = 0; i < messages.size(); ++i) {
    absl::optional<BlindedMessage> blinded_message =
        cryptographers.at(i)->BeginIssuance(messages.at(i));
    if (!blinded_message.has_value()) {
      return AttributionVerificationMediator::CryptographersAndBlindedMessages{
          .cryptographers = std::move(cryptographers),
          .blinded_messages = {},
      };
    }
    blinded_messages.push_back(std::move(blinded_message.value()));
  }
  return AttributionVerificationMediator::CryptographersAndBlindedMessages{
      .cryptographers = std::move(cryptographers),
      .blinded_messages = std::move(blinded_messages)};
}

void AttributionVerificationMediator::OnDoneBeginIssuance(
    mojom::TrustTokenProtocolVersion protocol_version,
    base::OnceCallback<void(net::HttpRequestHeaders)> done,
    AttributionVerificationMediator::CryptographersAndBlindedMessages
        cryptographer_and_blind_message) {
  cryptographers_ = std::move(cryptographer_and_blind_message.cryptographers);
  metrics_recorder_->Complete(Step::kBlindMessage);

  if (cryptographer_and_blind_message.blinded_messages.empty()) {
    metrics_recorder_->FinishGetHeadersWith(
        GetHeadersStatus::kUnableToBlindMessage);
    std::move(done).Run(net::HttpRequestHeaders());
    return;
  }
  CHECK_EQ(cryptographer_and_blind_message.blinded_messages.size(),
           messages_.size());
  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(
      kReportVerificationHeader,
      AttributionVerificationMediator::SerializeBlindedMessages(
          cryptographer_and_blind_message.blinded_messages));
  request_headers.SetHeader(
      kTrustTokensSecTrustTokenVersionHeader,
      internal::ProtocolVersionToString(protocol_version));

  metrics_recorder_->FinishGetHeadersWith(GetHeadersStatus::kSuccess);
  std::move(done).Run(std::move(request_headers));
}

std::vector<BlindedToken>
AttributionVerificationMediator::DeserializeBlindedTokens(
    const std::string& blinded_tokens_header) {
  absl::optional<net::structured_headers::List> parsed_list =
      net::structured_headers::ParseList(blinded_tokens_header);
  if (!parsed_list.has_value()) {
    return {};
  }

  std::vector<BlindedToken> blinded_tokens;
  blinded_tokens.reserve(parsed_list->size());
  for (const auto& item : parsed_list.value()) {
    // The item must be a non-empty string to be valid.
    if (item.member_is_inner_list || item.member.size() != 1u ||
        !item.member.at(0).item.is_string() ||
        item.member.at(0).item.GetString().empty()) {
      return {};
    }
    blinded_tokens.emplace_back(item.member.at(0).item.GetString());
  }

  return blinded_tokens;
}

std::string AttributionVerificationMediator::SerializeBlindedMessages(
    const std::vector<BlindedMessage>& blinded_messages) {
  net::structured_headers::List headers;

  for (const BlindedMessage& blinded_message : blinded_messages) {
    net::structured_headers::Item item(
        blinded_message, net::structured_headers::Item::ItemType::kStringType);
    headers.emplace_back(
        net::structured_headers::ParameterizedMember(item, {}));
  }
  absl::optional<std::string> serialized =
      net::structured_headers::SerializeList(headers);
  CHECK(serialized.has_value());
  return serialized.value();
}

void AttributionVerificationMediator::ProcessVerificationToGetTokens(
    net::HttpResponseHeaders& response_headers,
    base::OnceCallback<void(std::vector<Token>)> done) {
  CHECK(!messages_.empty());

  metrics_recorder_->Complete(Step::kSignBlindMessage);

  std::string header_value;
  if (!response_headers.GetNormalizedHeader(kReportVerificationHeader,
                                            &header_value)) {
    metrics_recorder_->FinishProcessVerificationWith(
        ProcessVerificationStatus::kNoSignatureReceivedFromIssuer);
    std::move(done).Run({});
    return;
  }
  response_headers.RemoveHeader(kReportVerificationHeader);

  std::vector<BlindedToken> blind_tokens =
      DeserializeBlindedTokens(header_value);
  if (blind_tokens.empty()) {
    metrics_recorder_->FinishProcessVerificationWith(
        ProcessVerificationStatus::kBadSignaturesHeaderReceivedFromIssuer);
    std::move(done).Run({});
    return;
  }

  if (blind_tokens.size() > messages_.size()) {
    metrics_recorder_->FinishProcessVerificationWith(
        ProcessVerificationStatus::kTooManySignaturesReceivedFromIssuer);
    std::move(done).Run({});
    return;
  }
  CHECK_EQ(messages_.size(), cryptographers_.size());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &AttributionVerificationMediator::ConfirmIssuancesAndBeginRedemptions,
          std::move(cryptographers_), std::move(blind_tokens)),
      base::BindOnce(
          &AttributionVerificationMediator::OnDoneProcessingIssuanceResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(done)));
}

AttributionVerificationMediator::CryptographersAndTokens
AttributionVerificationMediator::ConfirmIssuancesAndBeginRedemptions(
    std::vector<std::unique_ptr<Cryptographer>> cryptographers,
    std::vector<BlindedToken> blind_tokens) {
  std::vector<Token> tokens;
  tokens.reserve(blind_tokens.size());

  for (size_t i = 0; i < blind_tokens.size(); ++i) {
    absl::optional<Token> token =
        cryptographers.at(i)->ConfirmIssuanceAndBeginRedemption(
            blind_tokens.at(i));
    if (!token.has_value()) {
      return AttributionVerificationMediator::CryptographersAndTokens{
          .cryptographers = std::move(cryptographers), .tokens = {}};
    }
    tokens.push_back(std::move(token.value()));
  }

  return AttributionVerificationMediator::CryptographersAndTokens{
      std::move(cryptographers), std::move(tokens)};
}

void AttributionVerificationMediator::OnDoneProcessingIssuanceResponse(
    base::OnceCallback<void(std::vector<Token>)> done,
    AttributionVerificationMediator::CryptographersAndTokens
        cryptographer_and_tokens) {
  cryptographers_ = std::move(cryptographer_and_tokens.cryptographers);

  metrics_recorder_->Complete(Step::kUnblindMessage);

  if (cryptographer_and_tokens.tokens.empty()) {
    // The response was rejected by the underlying cryptographic library as
    // malformed or otherwise invalid.
    metrics_recorder_->FinishProcessVerificationWith(
        ProcessVerificationStatus::kUnableToUnblindSignature);
    std::move(done).Run({});
    return;
  }

  CHECK_LE(cryptographer_and_tokens.tokens.size(), messages_.size());
  metrics_recorder_->FinishProcessVerificationWith(
      ProcessVerificationStatus::kSuccess);
  std::move(done).Run(std::move(cryptographer_and_tokens.tokens));
}

}  // namespace network
