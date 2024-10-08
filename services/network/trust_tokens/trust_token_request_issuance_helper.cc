// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_issuance_helper.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"
#include "services/network/trust_tokens/trust_token_key_filtering.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "services/network/trust_tokens/types.h"
#include "url/url_constants.h"

namespace network {

using Cryptographer = TrustTokenRequestIssuanceHelper::Cryptographer;

struct TrustTokenRequestIssuanceHelper::CryptographerAndBlindedTokens {
  std::unique_ptr<Cryptographer> cryptographer;
  std::optional<std::string> blinded_tokens;
};

struct TrustTokenRequestIssuanceHelper::CryptographerAndUnblindedTokens {
  std::unique_ptr<Cryptographer> cryptographer;
  std::unique_ptr<Cryptographer::UnblindedTokens> unblinded_tokens;
};

namespace {

TrustTokenRequestIssuanceHelper::CryptographerAndBlindedTokens
BeginIssuanceOnPostedSequence(std::unique_ptr<Cryptographer> cryptographer,
                              int batch_size) {
  std::optional<std::string> blinded_tokens =
      cryptographer->BeginIssuance(batch_size);
  return {std::move(cryptographer), std::move(blinded_tokens)};
}

TrustTokenRequestIssuanceHelper::CryptographerAndUnblindedTokens
ConfirmIssuanceOnPostedSequence(std::unique_ptr<Cryptographer> cryptographer,
                                std::string response_header) {
  // From the "spec" (design doc): "If the response has an empty
  // Sec-Private-State-Token header, return; this is a 'success' response
  // bearing 0 tokens"
  if (response_header.empty()) {
    return {std::move(cryptographer),
            std::make_unique<Cryptographer::UnblindedTokens>()};
  }

  std::unique_ptr<Cryptographer::UnblindedTokens> unblinded_tokens =
      cryptographer->ConfirmIssuance(response_header);
  return {std::move(cryptographer), std::move(unblinded_tokens)};
}

base::Value::Dict CreateLogValue(std::string_view outcome) {
  return base::Value::Dict().Set("outcome", outcome);
}

// Define convenience aliases for the NetLogEventTypes for brevity.
enum NetLogOp { kBegin, kFinalize };
void LogOutcome(const net::NetLogWithSource& log,
                NetLogOp begin_or_finalize,
                std::string_view outcome) {
  log.EndEvent(
      begin_or_finalize == kBegin
          ? net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_ISSUANCE
          : net::NetLogEventType::TRUST_TOKEN_OPERATION_FINALIZE_ISSUANCE,
      [outcome]() { return CreateLogValue(outcome); });
}

}  // namespace

TrustTokenRequestIssuanceHelper::TrustTokenRequestIssuanceHelper(
    SuitableTrustTokenOrigin top_level_origin,
    TrustTokenStore* token_store,
    const TrustTokenKeyCommitmentGetter* key_commitment_getter,
    std::optional<std::string> custom_key_commitment,
    std::optional<url::Origin> custom_issuer,
    std::unique_ptr<Cryptographer> cryptographer,
    net::NetLogWithSource net_log)
    : top_level_origin_(std::move(top_level_origin)),
      token_store_(token_store),
      key_commitment_getter_(std::move(key_commitment_getter)),
      custom_key_commitment_(custom_key_commitment),
      custom_issuer_(custom_issuer),
      cryptographer_(std::move(cryptographer)),
      net_log_(std::move(net_log)) {
  DCHECK(token_store_);
  DCHECK(key_commitment_getter_);
  DCHECK(cryptographer_);
}

TrustTokenRequestIssuanceHelper::~TrustTokenRequestIssuanceHelper() = default;

TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens::
    UnblindedTokens() = default;
TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens::
    ~UnblindedTokens() = default;

void TrustTokenRequestIssuanceHelper::Begin(
    const GURL& url,
    base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                            mojom::TrustTokenOperationStatus)> done) {
  net_log_.BeginEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_ISSUANCE);

  if (custom_issuer_) {
    issuer_ = SuitableTrustTokenOrigin::Create(*custom_issuer_);
  } else {
    issuer_ = SuitableTrustTokenOrigin::Create(url);
  }

  if (!issuer_) {
    LogOutcome(net_log_, kBegin, "Unsuitable issuer URL");
    std::move(done).Run(std::nullopt,
                        mojom::TrustTokenOperationStatus::kInvalidArgument);
    return;
  }

  token_store_->RecordIssuance(*issuer_);

  if (custom_key_commitment_) {
    mojom::TrustTokenKeyCommitmentResultPtr keys =
        TrustTokenKeyCommitmentParser().Parse(*custom_key_commitment_);
    if (!keys) {
      LogOutcome(net_log_, kBegin, "Failed to parse custom keys");
      std::move(done).Run(std::nullopt,
                          mojom::TrustTokenOperationStatus::kInvalidArgument);
      return;
    }
    OnGotKeyCommitment(url, std::move(done), std::move(keys));
    return;
  }

  if (!token_store_->SetAssociation(*issuer_, top_level_origin_)) {
    LogOutcome(net_log_, kBegin, "Couldn't set issuer-toplevel association");
    std::move(done).Run(std::nullopt,
                        mojom::TrustTokenOperationStatus::kResourceLimited);
    return;
  }

  if (token_store_->CountTokens(*issuer_) ==
      kTrustTokenPerIssuerTokenCapacity) {
    LogOutcome(net_log_, kBegin, "Tokens at capacity");
    std::move(done).Run(std::nullopt,
                        mojom::TrustTokenOperationStatus::kResourceLimited);
    return;
  }

  key_commitment_getter_->Get(
      *issuer_,
      base::BindOnce(&TrustTokenRequestIssuanceHelper::OnGotKeyCommitment,
                     weak_ptr_factory_.GetWeakPtr(), std::cref(url),
                     std::move(done)));
}

void TrustTokenRequestIssuanceHelper::OnGotKeyCommitment(
    const GURL& url,
    base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                            mojom::TrustTokenOperationStatus)> done,
    mojom::TrustTokenKeyCommitmentResultPtr commitment_result) {
  if (!commitment_result) {
    LogOutcome(net_log_, kBegin, "No keys for issuer");
    std::move(done).Run(std::nullopt,
                        mojom::TrustTokenOperationStatus::kMissingIssuerKeys);
    return;
  }

  protocol_version_ = commitment_result->protocol_version;
  if (!commitment_result->batch_size ||
      !cryptographer_->Initialize(protocol_version_,
                                  commitment_result->batch_size)) {
    LogOutcome(net_log_, kBegin,
               "Internal error initializing cryptography delegate");
    std::move(done).Run(std::nullopt,
                        mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  for (const mojom::TrustTokenVerificationKeyPtr& key :
       commitment_result->keys) {
    if (!cryptographer_->AddKey(key->body)) {
      LogOutcome(net_log_, kBegin, "Bad key");
      std::move(done).Run(
          std::nullopt, mojom::TrustTokenOperationStatus::kFailedPrecondition);
      return;
    }
  }

  // Evict tokens signed with keys other than those from the issuer's most
  // recent commitments.
  token_store_->PruneStaleIssuerState(*issuer_, commitment_result->keys);

  int batch_size = std::min(commitment_result->batch_size,
                            kMaximumTrustTokenIssuanceBatchSize);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&BeginIssuanceOnPostedSequence, std::move(cryptographer_),
                     batch_size),
      base::BindOnce(
          &TrustTokenRequestIssuanceHelper::OnDelegateBeginIssuanceCallComplete,
          weak_ptr_factory_.GetWeakPtr(), std::cref(url), std::move(done)));
  // Logic continues... in the continuation
  // OnDelegateBeginIssuanceCallComplete; don't add more code here. In
  // particular, |cryptographer_| is empty at this point.
}

void TrustTokenRequestIssuanceHelper::OnDelegateBeginIssuanceCallComplete(
    const GURL& url,
    base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                            mojom::TrustTokenOperationStatus)> done,
    CryptographerAndBlindedTokens cryptographer_and_blinded_tokens) {
  cryptographer_ = std::move(cryptographer_and_blinded_tokens.cryptographer);
  std::optional<std::string>& maybe_blinded_tokens =
      cryptographer_and_blinded_tokens.blinded_tokens;  // Convenience alias

  if (!maybe_blinded_tokens) {
    LogOutcome(net_log_, kBegin, "Internal error generating blinded tokens");
    std::move(done).Run(std::nullopt,
                        mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(kTrustTokensSecTrustTokenHeader,
                            std::move(*maybe_blinded_tokens));

  std::string protocol_string_version =
      internal::ProtocolVersionToString(protocol_version_);
  request_headers.SetHeader(kTrustTokensSecTrustTokenVersionHeader,
                            protocol_string_version);

  LogOutcome(net_log_, kBegin, "Success");
  std::move(done).Run(std::move(request_headers),
                      mojom::TrustTokenOperationStatus::kOk);
}

void TrustTokenRequestIssuanceHelper::Finalize(
    net::HttpResponseHeaders& response_headers,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  net_log_.BeginEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_FINALIZE_ISSUANCE);

  // EnumerateHeader(|iter|=nullptr) asks for a previous instance of the header,
  // and returns the next one.
  std::optional<std::string_view> header_value =
      response_headers.EnumerateHeader(
          /*iter=*/nullptr, kTrustTokensSecTrustTokenHeader);

  if (!header_value) {
    LogOutcome(net_log_, kFinalize, "Response missing Trust Tokens header");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kBadResponse);
    return;
  }

  ProcessIssuanceResponse(std::string(*header_value), std::move(done));
  // Can only remove the header after the last user of `header_value`, since it
  // holds a pointer to the kTrustTokensSecTrustTokenHeader header's value.
  response_headers.RemoveHeader(kTrustTokensSecTrustTokenHeader);
}

void TrustTokenRequestIssuanceHelper::ProcessIssuanceResponse(
    std::string issuance_response,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  if (issuance_response.empty()) {
    OnDoneProcessingIssuanceResponse(
        std::move(done), {std::move(cryptographer_),
                          std::make_unique<Cryptographer::UnblindedTokens>()});
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ConfirmIssuanceOnPostedSequence,
                     std::move(cryptographer_), std::move(issuance_response)),
      base::BindOnce(
          &TrustTokenRequestIssuanceHelper::OnDoneProcessingIssuanceResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(done)));
}

void TrustTokenRequestIssuanceHelper::OnDoneProcessingIssuanceResponse(
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    CryptographerAndUnblindedTokens cryptographer_and_unblinded_tokens) {
  cryptographer_ = std::move(cryptographer_and_unblinded_tokens.cryptographer);
  std::unique_ptr<Cryptographer::UnblindedTokens>& maybe_tokens =
      cryptographer_and_unblinded_tokens.unblinded_tokens;  // Convenience alias

  if (!maybe_tokens) {
    LogOutcome(net_log_, kFinalize,
               "Response rejected during processing (perhaps malformed?)");

    // The response was rejected by the underlying cryptographic library as
    // malformed or otherwise invalid.
    std::move(done).Run(mojom::TrustTokenOperationStatus::kBadResponse);
    return;
  }

  token_store_->AddTokens(*issuer_, base::make_span(maybe_tokens->tokens),
                          maybe_tokens->body_of_verifying_key);

  num_obtained_tokens_ = maybe_tokens->tokens.size();

  net_log_.EndEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_FINALIZE_ISSUANCE,
      [num_obtained_tokens = *num_obtained_tokens_]() {
        return CreateLogValue("Success").Set(
            "# tokens obtained", static_cast<int>(num_obtained_tokens));
      });
  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
  return;
}

mojom::TrustTokenOperationResultPtr
TrustTokenRequestIssuanceHelper::CollectOperationResultWithStatus(
    mojom::TrustTokenOperationStatus status) {
  mojom::TrustTokenOperationResultPtr operation_result =
      mojom::TrustTokenOperationResult::New();
  operation_result->status = status;
  operation_result->operation = mojom::TrustTokenOperationType::kIssuance;
  operation_result->top_level_origin = top_level_origin_;
  if (issuer_) {
    operation_result->issuer = *issuer_;
  }
  if (num_obtained_tokens_) {
    operation_result->issued_token_count = *num_obtained_tokens_;
  }
  return operation_result;
}

}  // namespace network
