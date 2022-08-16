// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_redemption_helper.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"
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
    absl::optional<std::string> custom_key_commitment,
    absl::optional<url::Origin> custom_issuer,
    std::unique_ptr<KeyPairGenerator> key_pair_generator,
    std::unique_ptr<Cryptographer> cryptographer,
    net::NetLogWithSource net_log)
    : top_level_origin_(top_level_origin),
      refresh_policy_(refresh_policy),
      token_store_(token_store),
      key_commitment_getter_(std::move(key_commitment_getter)),
      custom_key_commitment_(custom_key_commitment),
      custom_issuer_(custom_issuer),
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

  if (custom_issuer_) {
    issuer_ = SuitableTrustTokenOrigin::Create(*custom_issuer_);
  } else {
    issuer_ = SuitableTrustTokenOrigin::Create(request->url());
  }

  if (!issuer_) {
    LogOutcome(net_log_, kBegin, "Unsuitable issuer URL (request destination)");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInvalidArgument);
    return;
  }

  if (custom_key_commitment_) {
    mojom::TrustTokenKeyCommitmentResultPtr keys =
        TrustTokenKeyCommitmentParser().Parse(*custom_key_commitment_);
    if (!keys) {
      LogOutcome(net_log_, kBegin, "Failed to parse custom keys");
      std::move(done).Run(mojom::TrustTokenOperationStatus::kInvalidArgument);
      return;
    }
    OnGotKeyCommitment(request, std::move(done), std::move(keys));
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

  absl::optional<TrustToken> maybe_token_to_redeem = RetrieveSingleToken();
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

  absl::optional<std::string> maybe_redemption_header =
      cryptographer_->BeginRedemption(
          *maybe_token_to_redeem, bound_verification_key_, top_level_origin_);

  if (!maybe_redemption_header) {
    LogOutcome(net_log_, kBegin, "Internal error beginning redemption");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  base::UmaHistogramBoolean("Net.TrustTokens.RedemptionRequestEmpty",
                            maybe_redemption_header->empty());

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
    response->headers->RemoveHeader(
        kTrustTokensResponseHeaderSecTrustTokenLifetime);
    std::move(done).Run(mojom::TrustTokenOperationStatus::kBadResponse);
    return;
  }

  // 2. Strip the Sec-Trust-Token header, from the response and pass the header,
  // base64-decoded, to BoringSSL.
  response->headers->RemoveHeader(kTrustTokensSecTrustTokenHeader);

  absl::optional<std::string> maybe_redemption_record =
      cryptographer_->ConfirmRedemption(header_value);

  // 3. If BoringSSL fails its structural validation / signature check, return
  // an error.
  if (!maybe_redemption_record) {
    // The response was rejected by the underlying cryptographic library as
    // malformed or otherwise invalid.
    LogOutcome(net_log_, kFinalize, "RR validation failed");
    response->headers->RemoveHeader(
        kTrustTokensResponseHeaderSecTrustTokenLifetime);
    std::move(done).Run(mojom::TrustTokenOperationStatus::kBadResponse);
    return;
  }

  // 4. Get lifetime from response header
  // If there are multiple lifetime headers, the last one is used.
  bool has_lifetime = false;
  uint64_t lifetime = 0;
  if (response->headers->HasHeader(
          kTrustTokensResponseHeaderSecTrustTokenLifetime)) {
    // GetInt64HeaderValue returns -1 in case of errors, if not -1, then
    // non-negative values ensuring non-negative values is important since we
    // cast it to unsigned
    int64_t maybe_lifetime = response->headers->GetInt64HeaderValue(
        kTrustTokensResponseHeaderSecTrustTokenLifetime);
    if (maybe_lifetime != -1) {
      has_lifetime = true;
      lifetime = static_cast<uint64_t>(maybe_lifetime);
    }
    response->headers->RemoveHeader(
        kTrustTokensResponseHeaderSecTrustTokenLifetime);
  }

  // 5. Otherwise, if these checks succeed, store the RR and return success.
  TrustTokenRedemptionRecord record_to_store;
  record_to_store.set_body(std::move(*maybe_redemption_record));
  record_to_store.set_signing_key(std::move(bound_signing_key_));
  record_to_store.set_public_key(std::move(bound_verification_key_));
  record_to_store.set_token_verification_key(
      std::move(token_verification_key_));
  if (has_lifetime)
    record_to_store.set_lifetime(lifetime);
  token_store_->SetRedemptionRecord(*issuer_, top_level_origin_,
                                    std::move(record_to_store));

  LogOutcome(net_log_, kFinalize, "Success");
  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

absl::optional<TrustToken>
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
    return absl::nullopt;

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
