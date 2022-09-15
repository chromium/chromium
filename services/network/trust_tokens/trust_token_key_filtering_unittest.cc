// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/trust_token_key_filtering.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(TrustTokenKeyFiltering, EmptyContainer) {
  std::vector<mojom::TrustTokenVerificationKeyPtr> keys;
  RetainSoonestToExpireTrustTokenKeys(&keys, 0);
  RetainSoonestToExpireTrustTokenKeys(&keys, 57);
}

TEST(TrustTokenKeyFiltering, KEqualsZero) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::vector<mojom::TrustTokenVerificationKeyPtr> keys;
  keys.emplace_back(mojom::TrustTokenVerificationKey::New());
  keys.front()->expiry = base::Time::Now() + base::Minutes(1);

  // Even though the key's expiry is in the future, passing k=0 should remove
  // the key.
  RetainSoonestToExpireTrustTokenKeys(&keys, 0);

  EXPECT_EQ(keys.size(), 0u);
}

TEST(TrustTokenKeyFiltering, Simple) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::vector<mojom::TrustTokenVerificationKeyPtr> keys;
  keys.emplace_back(mojom::TrustTokenVerificationKey::New());
  keys.front()->expiry = base::Time::Now() + base::Minutes(1);

  RetainSoonestToExpireTrustTokenKeys(&keys, 1);
  EXPECT_EQ(keys.size(), 1u);

  RetainSoonestToExpireTrustTokenKeys(&keys, 30);
  EXPECT_EQ(keys.size(), 1u);
}

TEST(TrustTokenKeyFiltering, ExpiredKey) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::vector<mojom::TrustTokenVerificationKeyPtr> keys;
  keys.emplace_back(mojom::TrustTokenVerificationKey::New());
  keys.front()->expiry = base::Time::Now() - base::Minutes(1);

  RetainSoonestToExpireTrustTokenKeys(&keys, 1);

  // Since the key's expired, it should have been deleted.
  EXPECT_EQ(keys.size(), 0u);
}

TEST(TrustTokenKeyFiltering, JustBarelyExpiredKey) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::vector<mojom::TrustTokenVerificationKeyPtr> keys;
  keys.emplace_back(mojom::TrustTokenVerificationKey::New());
  keys.front()->expiry = base::Time::Now();

  RetainSoonestToExpireTrustTokenKeys(&keys, 1);

  // The interface defines an expired key as one whose expiry is not in the
  // future, so one with expiry base::Time::Now() should be deleted, too.
  EXPECT_EQ(keys.size(), 0u);
}

TEST(TrustTokenKeyFiltering, PrioritizesSoonerToExpire) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto early_key = mojom::TrustTokenVerificationKey::New();
  early_key->expiry = base::Time::Now() + base::Minutes(1);
  auto late_key = mojom::TrustTokenVerificationKey::New();
  late_key->expiry = base::Time::Now() + base::Minutes(2);

  std::vector<mojom::TrustTokenVerificationKeyPtr> keys;
  keys.emplace_back(late_key.Clone());
  keys.emplace_back(early_key.Clone());

  RetainSoonestToExpireTrustTokenKeys(&keys, 1);

  EXPECT_EQ(keys.size(), 1u);
  EXPECT_TRUE(mojo::Equals(keys.front(), early_key));
}

TEST(TrustTokenKeyFiltering, MixOfPastAndFutureKeys) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto expired_key = mojom::TrustTokenVerificationKey::New();
  expired_key->expiry = base::Time::Now() - base::Minutes(1);
  auto early_key = mojom::TrustTokenVerificationKey::New();
  early_key->expiry = base::Time::Now() + base::Minutes(1);
  auto late_key = mojom::TrustTokenVerificationKey::New();
  late_key->expiry = base::Time::Now() + base::Minutes(2);

  std::vector<mojom::TrustTokenVerificationKeyPtr> keys;
  keys.emplace_back(late_key.Clone());
  keys.emplace_back(early_key.Clone());
  keys.emplace_back(expired_key.Clone());

  // This should drop the key that's expired while keeping the two keys that
  // have not.
  RetainSoonestToExpireTrustTokenKeys(&keys, 3);

  EXPECT_EQ(keys.size(), 2u);
  EXPECT_TRUE(base::ranges::any_of(
      keys, [&early_key](const mojom::TrustTokenVerificationKeyPtr& key) {
        return mojo::Equals(key, early_key);
      }));
  EXPECT_TRUE(base::ranges::any_of(
      keys, [&late_key](const mojom::TrustTokenVerificationKeyPtr& key) {
        return mojo::Equals(key, late_key);
      }));

  // This should drop the key with the latest expiry.
  RetainSoonestToExpireTrustTokenKeys(&keys, 1);

  EXPECT_EQ(keys.size(), 1u);
  EXPECT_TRUE(mojo::Equals(keys.front(), early_key));
}

TEST(TrustTokenKeyFiltering, BreaksTiesBasedOnBody) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto early_key = mojom::TrustTokenVerificationKey::New(
      "early", base::Time::Now() + base::Seconds(45));
  auto a_key = mojom::TrustTokenVerificationKey::New(
      "a", base::Time::Now() + base::Minutes(1));
  auto b_key = mojom::TrustTokenVerificationKey::New(
      "b", base::Time::Now() + base::Minutes(1));
  auto c_key = mojom::TrustTokenVerificationKey::New(
      "c", base::Time::Now() + base::Minutes(1));

  std::vector<mojom::TrustTokenVerificationKeyPtr> keys;
  keys.emplace_back(c_key.Clone());
  keys.emplace_back(a_key.Clone());
  keys.emplace_back(b_key.Clone());
  keys.emplace_back(early_key.Clone());

  // This should retain early_key, because it expires the earliest, and a_key,
  // because it has the least body of all of the keys expiring one minute in the
  // future.
  RetainSoonestToExpireTrustTokenKeys(&keys, 3);

  EXPECT_EQ(keys.size(), 3u);
  EXPECT_TRUE(base::ranges::any_of(
      keys, [&a_key](const mojom::TrustTokenVerificationKeyPtr& key) {
        return mojo::Equals(key, a_key);
      }));
  EXPECT_TRUE(base::ranges::any_of(
      keys, [&b_key](const mojom::TrustTokenVerificationKeyPtr& key) {
        return mojo::Equals(key, b_key);
      }));
  EXPECT_TRUE(base::ranges::any_of(
      keys, [&early_key](const mojom::TrustTokenVerificationKeyPtr& key) {
        return mojo::Equals(key, early_key);
      }));

  // Breaking the tie on expiry time should return the single key with the
  // earliest expiry.
  RetainSoonestToExpireTrustTokenKeys(&keys, 1);

  EXPECT_EQ(keys.size(), 1u);
  EXPECT_TRUE(mojo::Equals(keys.front(), early_key));
}

}  // namespace network
