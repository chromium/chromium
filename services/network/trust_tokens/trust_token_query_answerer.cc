// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_query_answerer.h"

#include "base/memory/ptr_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitments.h"
#include "url/url_constants.h"

namespace network {

TrustTokenQueryAnswerer::TrustTokenQueryAnswerer(
    SuitableTrustTokenOrigin top_frame_origin,
    PendingTrustTokenStore* pending_trust_token_store,
    const SynchronousTrustTokenKeyCommitmentGetter* key_commitment_getter)
    : top_frame_origin_(std::move(top_frame_origin)),
      pending_trust_token_store_(pending_trust_token_store),
      key_commitment_getter_(key_commitment_getter) {
  DCHECK(pending_trust_token_store);
  DCHECK(key_commitment_getter);
}

TrustTokenQueryAnswerer::~TrustTokenQueryAnswerer() = default;

void TrustTokenQueryAnswerer::HasTrustTokens(const url::Origin& issuer,
                                             HasTrustTokensCallback callback) {
  std::optional<SuitableTrustTokenOrigin> maybe_suitable_issuer =
      SuitableTrustTokenOrigin::Create(issuer);

  if (!maybe_suitable_issuer) {
    std::move(callback).Run(mojom::HasTrustTokensResult::New(
        mojom::TrustTokenOperationStatus::kInvalidArgument,
        /*has_trust_tokens=*/false));
    return;
  }

  pending_trust_token_store_->ExecuteOrEnqueue(
      base::BindOnce(&TrustTokenQueryAnswerer::AnswerTokenQueryWithStore,
                     weak_factory_.GetWeakPtr(),
                     std::move(*maybe_suitable_issuer), std::move(callback)));
}

void TrustTokenQueryAnswerer::HasRedemptionRecord(
    const url::Origin& issuer,
    HasRedemptionRecordCallback callback) {
  std::optional<SuitableTrustTokenOrigin> maybe_suitable_issuer =
      SuitableTrustTokenOrigin::Create(issuer);

  if (!maybe_suitable_issuer) {
    std::move(callback).Run(mojom::HasRedemptionRecordResult::New(
        mojom::TrustTokenOperationStatus::kInvalidArgument,
        /*has_redemption_record=*/false));
    return;
  }

  pending_trust_token_store_->ExecuteOrEnqueue(
      base::BindOnce(&TrustTokenQueryAnswerer::AnswerRedemptionQueryWithStore,
                     weak_factory_.GetWeakPtr(),
                     std::move(*maybe_suitable_issuer), std::move(callback)));
}

void TrustTokenQueryAnswerer::AnswerTokenQueryWithStore(
    const SuitableTrustTokenOrigin& issuer,
    HasTrustTokensCallback callback,
    TrustTokenStore* trust_token_store) const {
  DCHECK(trust_token_store);

  if (!trust_token_store->SetAssociation(issuer, top_frame_origin_)) {
    std::move(callback).Run(mojom::HasTrustTokensResult::New(
        mojom::TrustTokenOperationStatus::kResourceLimited,
        /*has_trust_tokens=*/false));
    return;
  }

  const mojom::TrustTokenKeyCommitmentResultPtr result =
      key_commitment_getter_->GetSync(issuer.origin());

  if (result)
    trust_token_store->PruneStaleIssuerState(issuer, result->keys);

  bool has_trust_tokens = trust_token_store->CountTokens(issuer);
  std::move(callback).Run(mojom::HasTrustTokensResult::New(
      mojom::TrustTokenOperationStatus::kOk, has_trust_tokens));
}

void TrustTokenQueryAnswerer::AnswerRedemptionQueryWithStore(
    const SuitableTrustTokenOrigin& issuer,
    HasRedemptionRecordCallback callback,
    TrustTokenStore* trust_token_store) const {
  DCHECK(trust_token_store);

  if (!trust_token_store->IsAssociated(issuer, top_frame_origin_)) {
    std::move(callback).Run(mojom::HasRedemptionRecordResult::New(
        mojom::TrustTokenOperationStatus::kOk,
        /*has_redemption_record=*/false));
    return;
  }

  const mojom::TrustTokenKeyCommitmentResultPtr result =
      key_commitment_getter_->GetSync(issuer.origin());

  if (result)
    trust_token_store->PruneStaleIssuerState(issuer, result->keys);

  auto redemption_record = trust_token_store->RetrieveNonstaleRedemptionRecord(
      issuer, top_frame_origin_);
  bool has_redemption_record = false;
  if (redemption_record)
    has_redemption_record = true;

  std::move(callback).Run(mojom::HasRedemptionRecordResult::New(
      mojom::TrustTokenOperationStatus::kOk, has_redemption_record));
}

}  // namespace network
