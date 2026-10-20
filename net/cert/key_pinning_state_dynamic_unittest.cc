// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/key_pinning_state_dynamic.h"

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

SHA256HashValue GetSampleSPKIHash(uint8_t value) {
  SHA256HashValue hash;
  hash.fill(value);
  return hash;
}

HashValueVector GetSampleSPKIHashes() {
  HashValueVector spki_hashes;
  spki_hashes.push_back(net::HashValue(net::HashValueTag::HASH_VALUE_SHA256,
                                       GetSampleSPKIHash(1)));
  return spki_hashes;
}

// Tests subdomain matching.
TEST(KeyPinningStateDynamicTest, Subdomains) {
  auto state = base::MakeRefCounted<KeyPinningStateDynamic>();
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);

  state->AddHPKP("example1.test", expiry, false, GetSampleSPKIHashes());
  state->AddHPKP("example2.test", expiry, true, GetSampleSPKIHashes());
  EXPECT_TRUE(state->HasPublicKeyPins("example1.test"));
  EXPECT_FALSE(state->HasPublicKeyPins("foo.example1.test"));
  EXPECT_FALSE(state->HasPublicKeyPins("foo.bar.example1.test"));
  EXPECT_TRUE(state->HasPublicKeyPins("example2.test"));
  EXPECT_TRUE(state->HasPublicKeyPins("foo.example2.test"));
  EXPECT_TRUE(state->HasPublicKeyPins("foo.bar.example2.test"));
}

// Tests that a more-specific HPKP rule overrides a less-specific rule
// with it, regardless of the includeSubDomains bit. Note this behavior does not
// match HSTS. See https://crbug.com/821811.
TEST(KeyPinningStateDynamicTest, PKPSubdomainCarveout) {
  auto state = base::MakeRefCounted<KeyPinningStateDynamic>();
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  const base::Time older = current_time - base::Seconds(1000);

  state->AddHPKP("example.test", expiry, true, GetSampleSPKIHashes());
  state->AddHPKP("foo.example.test", expiry, false, GetSampleSPKIHashes());
  EXPECT_TRUE(state->HasPublicKeyPins("example.test"));
  EXPECT_TRUE(state->HasPublicKeyPins("foo.example.test"));

  // The foo.example.test rule overrides the example1.test rule, so
  // bar.foo.example.test has no HPKP state->
  EXPECT_FALSE(state->HasPublicKeyPins("bar.foo.example.test"));

  // Other subdomains should be unaffected.
  EXPECT_TRUE(state->HasPublicKeyPins("baz.example.test"));

  // Expire the foo.example.test rule.
  state->AddHPKP("foo.example.test", older, false, GetSampleSPKIHashes());

  // Now the base example.test rule applies to bar.foo.example.test.
  EXPECT_TRUE(state->HasPublicKeyPins("bar.foo.example.test"));
}

// Tests that HPKP states expire.
TEST(KeyPinningStateDynamicTest, Expiration) {
  auto state = base::MakeRefCounted<KeyPinningStateDynamic>();
  const base::Time current_time(base::Time::Now());
  const base::Time future = current_time + base::Seconds(1000);
  const base::Time older = current_time - base::Seconds(1000);

  state->AddHPKP("example1.test", older, false, GetSampleSPKIHashes());
  state->AddHPKP("example2.test", future, false, GetSampleSPKIHashes());
  EXPECT_FALSE(state->HasPublicKeyPins("example1.test"));
  EXPECT_TRUE(state->HasPublicKeyPins("example2.test"));
}

// Tests that GetPKPState returns the correct data.
TEST(KeyPinningStateDynamicTest, DynamicDomainState) {
  auto state = base::MakeRefCounted<KeyPinningStateDynamic>();
  const base::Time current_time(base::Time::Now());
  const base::Time expiry1 = current_time + base::Seconds(1000);
  const base::Time expiry2 = current_time + base::Seconds(2000);

  state->AddHPKP("example.com", expiry1, true, GetSampleSPKIHashes());
  state->AddHPKP("foo.example.com", expiry2, false, GetSampleSPKIHashes());

  KeyPinningStateDynamic::PKPState pkp_state1;
  KeyPinningStateDynamic::PKPState pkp_state2;
  ASSERT_TRUE(state->GetPKPState("example.com", &pkp_state1));
  ASSERT_TRUE(state->GetPKPState("foo.example.com", &pkp_state2));

  EXPECT_TRUE(pkp_state1.HasPublicKeyPins());
  EXPECT_TRUE(pkp_state1.include_subdomains);
  EXPECT_EQ(expiry1, pkp_state1.expiry);

  EXPECT_TRUE(pkp_state2.HasPublicKeyPins());
  EXPECT_FALSE(pkp_state2.include_subdomains);
  EXPECT_EQ(expiry2, pkp_state2.expiry);
}

// Tests that new pins always override previous pins. This should be true for
// both pins at the same domain or includeSubdomains pins at a parent domain.
TEST(KeyPinningStateDynamicTest, NewPinsOverride) {
  auto state = base::MakeRefCounted<KeyPinningStateDynamic>();
  KeyPinningStateDynamic::PKPState pkp_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  std::array<uint8_t, crypto::hash::kSha256Size> hash1_bytes;
  std::ranges::fill(hash1_bytes, 0x01);
  HashValue hash1(HASH_VALUE_SHA256, hash1_bytes);
  std::array<uint8_t, crypto::hash::kSha256Size> hash2_bytes;
  std::ranges::fill(hash2_bytes, 0x02);
  HashValue hash2(HASH_VALUE_SHA256, hash2_bytes);
  std::array<uint8_t, crypto::hash::kSha256Size> hash3_bytes;
  std::ranges::fill(hash3_bytes, 0x03);
  HashValue hash3(HASH_VALUE_SHA256, hash3_bytes);

  state->AddHPKP("example.com", expiry, true, HashValueVector(1, hash1));

  ASSERT_TRUE(state->GetPKPState("foo.example.com", &pkp_state));
  EXPECT_THAT(pkp_state.spki_hashes,
              testing::UnorderedElementsAre(hash1.sha256hashvalue()));

  state->AddHPKP("foo.example.com", expiry, false, HashValueVector(1, hash2));

  ASSERT_TRUE(state->GetPKPState("foo.example.com", &pkp_state));
  EXPECT_THAT(pkp_state.spki_hashes,
              testing::UnorderedElementsAre(hash2.sha256hashvalue()));

  state->AddHPKP("foo.example.com", expiry, false, HashValueVector(1, hash3));

  ASSERT_TRUE(state->GetPKPState("foo.example.com", &pkp_state));
  EXPECT_THAT(pkp_state.spki_hashes,
              testing::UnorderedElementsAre(hash3.sha256hashvalue()));
}

TEST(KeyPinningStateDynamic, LongNames) {
  auto state = base::MakeRefCounted<KeyPinningStateDynamic>();
  const char kLongName[] =
      "lookupByWaveIdHashAndWaveIdIdAndWaveIdDomainAndWaveletIdIdAnd"
      "WaveletIdDomainAndBlipBlipid";
  KeyPinningStateDynamic::PKPState pkp_state;
  // Just checks that we don't hit a NOTREACHED
  EXPECT_FALSE(state->GetPKPState(kLongName, &pkp_state));
  EXPECT_FALSE(state->HasPublicKeyPins(kLongName));
}

}  // namespace
}  // namespace net
