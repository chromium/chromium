// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_STORE_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_STORE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_persister.h"
#include "services/network/trust_tokens/types.h"

namespace network {

// A TrustTokenStore provides operations on persistent state necessary for
// the various steps of the Trust TrustTokens protocol.
//
// For more information about the protocol, see the explainer at
// https://github.com/WICG/trust-token-api.
//
// TrustTokenStore translates operations germane to different steps
// of token issuance, token redemption, and request signing into
// operations in the key-value representation used by the persistence
// layer.
//
// For example, it provides operations:
// - checking preconditions for the different protocol steps;
// - storing unblinded, signed tokens; and
// - managing Redemption Records (RRs).
//
// TrustTokenStore's methods do minimal precondition checking and, in
// particular, only selectively verify protocol-level invariants and
// input integrity.
class TrustTokenStore {
 public:
  class RecordExpiryDelegate {
   public:
    virtual ~RecordExpiryDelegate() = default;

    // Returns whether the given Redemption Record has expired.
    // This is implemented with a delegate to abstract away reading
    // the values of RRs (they're opaque to this store).
    //
    // |time_since_last_redemption| is time elapsed since last redemption.
    // |issuer| is the issuer that issued the RR.
    virtual bool IsRecordExpired(
        const TrustTokenRedemptionRecord& record,
        const base::TimeDelta& time_since_last_redemption,
        const SuitableTrustTokenOrigin& issuer) = 0;
  };

  // Creates a TrustTokenStore relying on the given persister for underlying
  // storage and the given delegate for judging whether redemption records
  // have expired.
  TrustTokenStore(std::unique_ptr<TrustTokenPersister> persister,
                  std::unique_ptr<RecordExpiryDelegate> expiry_delegate);

  virtual ~TrustTokenStore();

  // Creates a TrustTokenStore with defaults useful for testing: an in-memory
  // persister and never expiring stored RRs. Callers may provide custom values
  // for one argument or both.
  static std::unique_ptr<TrustTokenStore> CreateForTesting(
      std::unique_ptr<TrustTokenPersister> persister = nullptr,
      std::unique_ptr<RecordExpiryDelegate> expiry_delegate = nullptr);

  //// Methods related to ratelimits:

  // Updates the given issuer's last issuance time to now.
  virtual void RecordIssuance(const SuitableTrustTokenOrigin& issuer);

  // Returns the time since the last call to RecordIssuance for
  // issuer |issuer|, or nullopt in the following two cases:
  // 1. there is no currently-recorded prior issuance for the
  // issuer, or
  // 2. the time since the last issuance is negative (because
  // of, for instance, corruption or clock skew).
  //
  // |issuer| must not be opaque.
  [[nodiscard]] virtual std::optional<base::TimeDelta> TimeSinceLastIssuance(
      const SuitableTrustTokenOrigin& issuer);

  // Returns the time elapsed since the last redemption recorded by
  // SetRedemptionRecord for issuer |issuer| and top level |top_level|,
  // or nullopt in the following two cases:
  // 1. there was no prior redemption for the (issuer,
  // top-level origin) pair.
  // 2. the time since the last redepmption is negative (because
  // of, for instance, corruption or clock skew).
  [[nodiscard]] virtual std::optional<base::TimeDelta> TimeSinceLastRedemption(
      const SuitableTrustTokenOrigin& issuer,
      const SuitableTrustTokenOrigin& top_level);

  // Returns whether |issuer| is associated with |top_level|.
  [[nodiscard]] virtual bool IsAssociated(
      const SuitableTrustTokenOrigin& issuer,
      const SuitableTrustTokenOrigin& top_level);

  // If associating |issuer| with |top_level| would exceed the cap on the number
  // of issuers allowed to be associated with a given top-level origin, returns
  // false. Otherwise, associates |issuer| with |top_level| and returns true.
  //
  // TODO(crbug.com/40679190): As part of adding solid support for multiple
  // issuers, it'd be good to make these associations expire after some
  // reasonably long amount of time, so that top-level origins can change their
  // minds about their associated issuers.
  [[nodiscard]] virtual bool SetAssociation(
      const SuitableTrustTokenOrigin& issuer,
      const SuitableTrustTokenOrigin& top_level);

  //// Methods related to reading and writing issuer values configured via key
  //// commitment queries, such as key commitments and batch sizes:

  // Given an issuer's current set |keys| of key commitments, prunes all state
  // for |issuer| that does *not* correspond to token verification keys in
  // |keys|:
  // - removes all stored signed tokens for |issuer| that were signed with
  // keys not in |keys|
  //
  // The commitments in |keys| must have distinct keys.
  virtual void PruneStaleIssuerState(
      const SuitableTrustTokenOrigin& issuer,
      const std::vector<mojom::TrustTokenVerificationKeyPtr>& keys);

  //// Methods related to reading and writing signed tokens:

  // Associates to the given issuer additional signed
  // trust tokens with:
  // - token bodies given by |token_bodies|
  // - signing keys given by |issuing_key|.
  //
  // Note: This method makes no assumption about tokens matching an issuer's
  // current key commitments; it's the caller's responsibility to avoid using
  // tokens issued against non-current keys.
  virtual void AddTokens(const SuitableTrustTokenOrigin& issuer,
                         base::span<const std::string> token_bodies,
                         std::string_view issuing_key);

  // Returns the number of tokens stored for |issuer|.
  [[nodiscard]] virtual int CountTokens(const SuitableTrustTokenOrigin& issuer);

  // Returns the number of stored tokens per issuer.
  [[nodiscard]] virtual base::flat_map<SuitableTrustTokenOrigin, int>
  GetStoredTrustTokenCounts();

  // Returns all signed tokens from |issuer| signed by keys matching
  // the given predicate.
  [[nodiscard]] virtual std::vector<TrustToken> RetrieveMatchingTokens(
      const SuitableTrustTokenOrigin& issuer,
      base::RepeatingCallback<bool(const std::string&)> key_matcher);

  // If |to_delete| is a currently stored token issued by |issuer|, deletes the
  // token.
  void DeleteToken(const SuitableTrustTokenOrigin& issuer,
                   const TrustToken& to_delete);

  //// Methods concerning Redemption Records (RRs)

  // Sets the cached RR corresponding to the pair (issuer, top_level)
  // to |record|. Overwrites any existing record.
  virtual void SetRedemptionRecord(const SuitableTrustTokenOrigin& issuer,
                                   const SuitableTrustTokenOrigin& top_level,
                                   const TrustTokenRedemptionRecord& record);

  // Return redemption records per issuer/toplevel origin
  [[nodiscard]] virtual IssuerRedemptionRecordMap GetRedemptionRecords();

  // Attempts to retrieve the stored RR for the given pair of (issuer,
  // top-level) origins.
  // - If the pair has a current (i.e., non-expired) RR, returns that RR.
  // - Otherwise, returns nullopt.
  [[nodiscard]] virtual std::optional<TrustTokenRedemptionRecord>
  RetrieveNonstaleRedemptionRecord(const SuitableTrustTokenOrigin& issuer,
                                   const SuitableTrustTokenOrigin& top_level);

  //// Methods concerning data removal

  // Deletes any data stored keyed by matching origins (as issuers or top-level
  // origins).
  //
  // An origin "matches" |filter| means it compares equal to a member of
  // |filter->origins| or its domain-and-registry string---aka "eTLD+1"---is an
  // exact match to a member of |filter->domains|.
  //
  // If |filter->type| is KEEP_MATCHING, deletes all data for every origin *not*
  // matching the filter. (In particular, this will still delete data keyed by
  // a pair of origins, one of which matches and one of which does not.)
  //
  // |filter| is allowed to be null: nullptr is a wildcard filter indicating
  // that all data should be cleared.
  //
  // Returns whether any data was deleted.
  [[nodiscard]] virtual bool ClearDataForFilter(
      mojom::ClearDataFilterPtr filter);

  [[nodiscard]] virtual bool ClearDataForPredicate(
      base::RepeatingCallback<bool(const std::string&)> predicate);

  // Deletes all stored tokens issued by |issuer| but leaves other stored
  // data, including the issuer's Redemption Records (RRs), intact.
  // Returns whether any data was deleted.
  [[nodiscard]] virtual bool DeleteStoredTrustTokens(
      const SuitableTrustTokenOrigin& issuer);

  [[nodiscard]] bool IsRedemptionLimitHit(
      const SuitableTrustTokenOrigin& issuer,
      const SuitableTrustTokenOrigin& top_level) const;

 private:
  std::unique_ptr<TrustTokenPersister> persister_;
  std::unique_ptr<RecordExpiryDelegate> record_expiry_delegate_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_STORE_H_
