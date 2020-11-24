// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_redemption_helper.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "url/url_constants.h"

namespace network {

namespace {

base::Value CreateLogValue(base::StringPiece outcome) {
  base::Value ret(base::Value::Type::DICTIONARY);
  ret.SetStringKey("outcome", outcome);
  return ret;
}

// Define convenience aliases for the NetLogEventTypes for brevity.
enum NetLogOp { kBegin, kFinalize };
void LogOutcome(const net::NetLogWithSource& log,
                NetLogOp begin_or_finalize,
                base::StringPiece outcome) {
  log.EndEvent(
      begin_or_finalize == kBegin
          ? net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_REDEMPTION
          : net::NetLogEventType::TRUST_TOKEN_OPERATION_FINALIZE_REDEMPTION,
      [outcome]() { return CreateLogValue(outcome); });
}

}  // namespace

TrustTokenRequestRedemptionHelper::TrustTokenRequestRedemptionHelper(
    SuitableTrustTokenOrigin top_level_origin,
    mojom::TrustTokenRefreshPolicy refresh_policy,
    TrustTokenStore* token_store,
    const TrustTokenKeyCommitmentGetter* key_commitment_getter,
    std::unique_ptr<KeyPairGenerator> key_pair_generator,
    std::unique_ptr<Cryptographer> cryptographer,
    net::NetLogWithSource net_log)
    : top_level_origin_(top_level_origin),
      refresh_policy_(refresh_policy),
      token_store_(token_store),
      key_commitment_getter_(std::move(key_commitment_getter)),
      key_pair_generator_(std::move(key_pair_generator)),
      cryptographer_(std::move(cryptographer)),
      net_log_(std::move(net_log)) {
  DCHECK(token_store_);
  DCHECK(key_commitment_getter_);
  DCHECK(key_pair_generator_);
  DCHECK(cryptographer_);
}

TrustTokenRequestRedemptionHelper::~TrustTokenRequestRedemptionHelper() =
    default;

void TrustTokenRequestRedemptionHelper::Begin(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  DCHECK(request);

  net_log_.BeginEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_REDEMPTION);

  issuer_ = SuitableTrustTokenOrigin::Create(request->url());
  if (!issuer_) {
    LogOutcome(net_log_, kBegin, "Unsuitable issuer URL (request destination)");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInvalidArgument);
    return;
  }

  if (refresh_policy_ == mojom::TrustTokenRefreshPolicy::kRefresh &&
      (!request->initiator() ||
       !request->initiator()->IsSameOriginWith(*issuer_))) {
    LogOutcome(net_log_, kBegin, "Refresh from non-issuer context");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kFailedPrecondition);
    return;
  }

  if (!token_store_->SetAssociation(*issuer_, top_level_origin_)) {
    LogOutcome(net_log_, kBegin, "Couldn't set issuer-toplevel association");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kResourceExhausted);
    return;
  }

  if (refresh_policy_ == mojom::TrustTokenRefreshPolicy::kUseCached &&
      token_store_->RetrieveNonstaleRedemptionRecord(*issuer_,
                                                     top_level_origin_)) {
    LogOutcome(net_log_, kBegin, "Redemption record cache hit");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kAlreadyExists);
    return;
  }

  key_commitment_getter_->Get(
      *issuer_,
      base::BindOnce(&TrustTokenRequestRedemptionHelper::OnGotKeyCommitment,
                     weak_factory_.GetWeakPtr(), request, std::move(done)));
}

void TrustTokenRequestRedemptionHelper::OnGotKeyCommitment(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    mojom::TrustTokenKeyCommitmentResultPtr commitment_result) {
  if (!commitment_result) {
    LogOutcome(net_log_, kBegin, "No keys for issuer");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kFailedPrecondition);
    return;
  }

  // Evict tokens signed with keys other than those from the issuer's most
  // recent commitments.
  token_store_->PruneStaleIssuerState(*issuer_, commitment_result->keys);

  base::Optional<TrustToken> maybe_token_to_redeem = RetrieveSingleToken();
  if (!maybe_token_to_redeem) {
    LogOutcome(net_log_, kBegin, "No tokens to redeem");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kResourceExhausted);
    return;
  }

  if (!commitment_result->batch_size ||
      !cryptographer_->Initialize(commitment_result->protocol_version,
                                  commitment_result->batch_size)) {
    LogOutcome(net_log_, kBegin,
               "Internal error initializing BoringSSL redemption state "
               "(possibly due to bad batch size)");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  if (!key_pair_generator_->Generate(&bound_signing_key_,
                                     &bound_verification_key_)) {
    LogOutcome(net_log_, kBegin, "Internal error generating RR-bound key pair");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  base::Optional<std::string> maybe_redemption_header =
      cryptographer_->BeginRedemption(
          *maybe_token_to_redeem, bound_verification_key_, top_level_origin_);

  if (!maybe_redemption_header) {
    LogOutcome(net_log_, kBegin, "Internal error beginning redemption");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  request->SetExtraRequestHeaderByName(kTrustTokensSecTrustTokenHeader,
                                       std::move(*maybe_redemption_header),
                                       /*overwrite=*/true);

  std::string protocol_string_version =
      internal::ProtocolVersionToString(commitment_result->protocol_version);
  request->SetExtraRequestHeaderByName(kTrustTokensSecTrustTokenVersionHeader,
                                       protocol_string_version,
                                       /*overwrite=*/true);

  // We don't want cache reads, because the highest priority is to execute the
  // protocol operation by sending the server the Trust Tokens request header
  // and getting the corresponding response header, but we want cache writes
  // in case subsequent requests are made to the same URL in non-trust-token
  // settings.
  request->SetLoadFlags(request->load_flags() | net::LOAD_BYPASS_CACHE);

  token_verification_key_ = *maybe_token_to_redeem->mutable_signing_key();
  token_store_->DeleteToken(*issuer_, *maybe_token_to_redeem);

  LogOutcome(net_log_, kBegin, "Success");
  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

void TrustTokenRequestRedemptionHelper::Finalize(
    mojom::URLResponseHead* response,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  // Numbers 1-4 below correspond to the lines of the "Process a redemption
  // response" pseudocode from the design doc.

  net_log_.BeginEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_FINALIZE_REDEMPTION);
  DCHECK(response);

  // A response headers object should be present on all responses for
  // HTTP requests (which Trust Tokens requests are).
  DCHECK(response->headers);

  // 1. If the response has no Sec-Trust-Token header, return an error.

  std::string header_value;

  // EnumerateHeader(|iter|=nullptr) asks for the first instance of the header,
  // if any.
  if (!response->headers->EnumerateHeader(
          /*iter=*/nullptr, kTrustTokensSecTrustTokenHeader, &header_value)) {
    LogOutcome(net_log_, kFinalize, "Response missing Trust Tokens header");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kBadResponse);
    return;
  }

  // 2. Strip the Sec-Trust-Token header, from the response and pass the header,
  // base64-decoded, to BoringSSL.
  response->headers->RemoveHeader(kTrustTokensSecTrustTokenHeader);

  base::Optional<std::string> maybe_redemption_record =
      cryptographer_->ConfirmRedemption(header_value);

  // 3. If BoringSSL fails its structural validation / signature check, return
  // an error.
  if (!maybe_redemption_record) {
    // The response was rejected by the underlying cryptographic library as
    // malformed or otherwise invalid.
    LogOutcome(net_log_, kFinalize, "RR validation failed");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kBadResponse);
    return;
  }

  // 4. Otherwise, if these checks succeed, store the RR and return success.

  TrustTokenRedemptionRecord record_to_store;
  record_to_store.set_body(std::move(*maybe_redemption_record));
  record_to_store.set_signing_key(std::move(bound_signing_key_));
  record_to_store.set_public_key(std::move(bound_verification_key_));
  record_to_store.set_token_verification_key(
      std::move(token_verification_key_));
  token_store_->SetRedemptionRecord(*issuer_, top_level_origin_,
                                    std::move(record_to_store));

  LogOutcome(net_log_, kFinalize, "Success");
  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

base::Optional<TrustToken>
TrustTokenRequestRedemptionHelper::RetrieveSingleToken() {
  // As a postcondition of UpdateTokenStoreFromKeyCommitmentResult, all of the
  // store's tokens for |issuer_| match the key commitment result obtained at
  // the beginning of this redemption. Consequently, it's OK to use any
  // |issuer_| token in the store.
  auto key_matcher =
      base::BindRepeating([](const std::string&) { return true; });

  std::vector<TrustToken> matching_tokens =
      token_store_->RetrieveMatchingTokens(*issuer_, key_matcher);

  if (matching_tokens.empty())
    return base::nullopt;

  return matching_tokens.front();
}

mojom::TrustTokenOperationResultPtr
TrustTokenRequestRedemptionHelper::CollectOperationResultWithStatus(
    mojom::TrustTokenOperationStatus status) {
  mojom::TrustTokenOperationResultPtr operation_result =
      mojom::TrustTokenOperationResult::New();
  operation_result->status = status;
  operation_result->type = mojom::TrustTokenOperationType::kRedemption;
  operation_result->top_level_origin = top_level_origin_;
  if (issuer_) {
    operation_result->issuer = *issuer_;
  }
  return operation_result;
}

}  // namespace network
