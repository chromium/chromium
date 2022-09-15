// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/expiry_inspecting_record_expiry_delegate.h"

#include <vector>

#include "base/test/task_environment.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class FixedKeyCommitmentGetter
    : public SynchronousTrustTokenKeyCommitmentGetter {
 public:
  explicit FixedKeyCommitmentGetter(
      mojom::TrustTokenKeyCommitmentResultPtr keys)
      : keys_(std::move(keys)) {}
  mojom::TrustTokenKeyCommitmentResultPtr GetSync(
      const url::Origin& origin) const override {
    return keys_.Clone();
  }

 private:
  mojom::TrustTokenKeyCommitmentResultPtr keys_;
};

}  // namespace

// If storage contains no key commitment result for the record's issuer
// whatsoever, then, in particular, it doesn't contain the token verification
// key associated with the record; this means that the record should be marked
// as expired.
TEST(ExpiryInspectingRecordExpiryDelegate, ExpiresRecordWhenNoKeys) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  FixedKeyCommitmentGetter getter(nullptr);  // return no keys
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;

  EXPECT_TRUE(delegate.IsRecordExpired(
      record, base::Minutes(1),
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

// When we have a key commitment result for the record's issuer, but the
// record's key is not in the commitment result, the record should be marked
// expired.
TEST(ExpiryInspectingRecordExpiryDelegate, ExpiresRecordWithNoMatchingKey) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // This commitment result is valid, but it contains no keys.
  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  // "key" is not present in the commitment
  record.set_token_verification_key("key");

  EXPECT_TRUE(delegate.IsRecordExpired(
      record, base::Minutes(12345),
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

// Lifetime expired
TEST(ExpiryInspectingRecordExpiryDelegate, ExpiresRecordOutOfLifetime) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");
  record.set_lifetime(42);

  EXPECT_TRUE(delegate.IsRecordExpired(
      record, base::Seconds(43),
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

// Zero Lifetime
TEST(ExpiryInspectingRecordExpiryDelegate, ExpiresRecordWithZeroLifetime) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");
  record.set_lifetime(0);

  EXPECT_TRUE(delegate.IsRecordExpired(
      record, base::Seconds(0),
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

// When a record's issuer has the record's key in its current commitment, the
// record should not be marked as expired.
TEST(ExpiryInspectingRecordExpiryDelegate,
     DoesntExpireUnexpiredRecordLifetimeNotSet) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");

  EXPECT_FALSE(delegate.IsRecordExpired(
      record, base::Minutes(99999),
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

TEST(ExpiryInspectingRecordExpiryDelegate,
     DoesntExpireUnexpiredRecordLifetimeSet) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");
  record.set_lifetime(27);

  EXPECT_FALSE(delegate.IsRecordExpired(
      record, base::Seconds(26),
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

}  // namespace network
