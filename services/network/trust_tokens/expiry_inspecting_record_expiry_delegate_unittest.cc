// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/expiry_inspecting_record_expiry_delegate.h"

#include <vector>

#include "base/test/task_environment.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/signed_redemption_record_serialization.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Overwrites |record| with a serialized redemption record with just enough
// structure to have a well-formed expiry time of |expiry|.
//
// Reference: Trust Tokens design doc (currently the normative source for RR
// structure),
// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#heading=h.7mkzvhpqb8l5
const char kExpiryTimestampKey[] = "expiry-timestamp";
void SetRecordExpiry(TrustTokenRedemptionRecord* record, base::Time expiry) {
  cbor::Value::MapValue map;
  map[cbor::Value(kExpiryTimestampKey, cbor::Value::Type::STRING)] =
      cbor::Value((expiry - base::Time::UnixEpoch()).InSeconds());

  std::vector<uint8_t> empty_signature;
  record->set_body(*ConstructRedemptionRecord(
      *cbor::Writer::Write(cbor::Value(std::move(map))), empty_signature));
}

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

  SetRecordExpiry(&record, base::Time::Now() + base::TimeDelta::FromMinutes(1));
  EXPECT_TRUE(delegate.IsRecordExpired(
      record,
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

// A record with its expiry time in the past... should be marked as expired.
TEST(ExpiryInspectingRecordExpiryDelegate, ExpiresRecordWithPastExpiry) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");
  SetRecordExpiry(&record, base::Time::Now() - base::TimeDelta::FromMinutes(1));

  EXPECT_TRUE(delegate.IsRecordExpired(
      record,
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

// The delegate's interface defines a record as expired if its expiration time
// is not in the future.
TEST(ExpiryInspectingRecordExpiryDelegate, ExpiresRecordExpiringRightNow) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");
  SetRecordExpiry(&record, base::Time::Now());

  EXPECT_TRUE(delegate.IsRecordExpired(
      record,
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
  SetRecordExpiry(&record, base::Time::Now() + base::TimeDelta::FromMinutes(1));

  EXPECT_TRUE(delegate.IsRecordExpired(
      record,
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

// When a record's issuer has the record's key in its current commitment, and
// the record's expiry timestamp hasn't passed, the record should not be marked
// as expired.
TEST(ExpiryInspectingRecordExpiryDelegate, DoesntExpireUnexpiredRecord) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");
  SetRecordExpiry(&record, base::Time::Now() + base::TimeDelta::FromMinutes(1));

  EXPECT_FALSE(delegate.IsRecordExpired(
      record,
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

TEST(ExpiryInspectingRecordExpiryDelegate, ExpiresRecordWithMalformedBody) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  // This RR has an empty (and, consequently, invalid) body; it should be
  // marked as expired.
  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");

  EXPECT_TRUE(delegate.IsRecordExpired(
      record,
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

TEST(ExpiryInspectingRecordExpiryDelegate, RespectsDefaultTimestamp) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");

  // Construct an RR body missing the (optional) expiry timestamp field.
  std::vector<uint8_t> empty_signature;
  cbor::Value::MapValue map;
  record.set_body(*ConstructRedemptionRecord(
      *cbor::Writer::Write(cbor::Value(std::move(map))), empty_signature));

  // The record's expiry should depend on whether the default expiry is in the
  // future or the past.
  EXPECT_EQ(
      delegate.IsRecordExpired(record, *SuitableTrustTokenOrigin::Create(
                                           GURL("https://issuer.example"))),
      kTrustTokenDefaultRedemptionRecordExpiry <= base::Time::Now());

  static_assert(kTrustTokenDefaultRedemptionRecordExpiry == base::Time::Max(),
                "If kTrustTokenDefaultRedemptionRecordExpiry becomes less "
                "than base::Time::Max(), add another test case moving the "
                "clock past Time::Max() and confirming that a record with the"
                " default timestamp is marked as expired.");
}

TEST(ExpiryInspectingRecordExpiryDelegate, ExpiresRecordWithTypeUnsafeExpiry) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  FixedKeyCommitmentGetter getter(std::move(commitment_result));
  ExpiryInspectingRecordExpiryDelegate delegate(&getter);

  TrustTokenRedemptionRecord record;
  record.set_token_verification_key("key");

  std::vector<uint8_t> empty_signature;
  cbor::Value::MapValue map;
  map[cbor::Value(kExpiryTimestampKey, cbor::Value::Type::STRING)] =
      cbor::Value("oops! not an int", cbor::Value::Type::STRING);
  record.set_body(*ConstructRedemptionRecord(
      *cbor::Writer::Write(cbor::Value(std::move(map))), empty_signature));

  // Since the expiry is of the wrong type, the record should be marked as
  // expired.
  EXPECT_TRUE(delegate.IsRecordExpired(
      record,
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

}  // namespace network
