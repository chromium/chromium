// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/symmetric_key.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SymmetricKeyTest, GenerateRandomKey) {
  std::unique_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES, 256));
  ASSERT_TRUE(key);
  EXPECT_EQ(32U, key->key().size());

  // Do it again and check that the keys are different.
  // (Note: this has a one-in-10^77 chance of failure!)
  std::unique_ptr<crypto::SymmetricKey> key2(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES, 256));
  ASSERT_TRUE(key2);
  EXPECT_EQ(32U, key2->key().size());
  EXPECT_NE(key->key(), key2->key());
}

TEST(SymmetricKeyTest, ImportGeneratedKey) {
  std::unique_ptr<crypto::SymmetricKey> key1(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES, 256));
  ASSERT_TRUE(key1);

  std::unique_ptr<crypto::SymmetricKey> key2(
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, key1->key()));
  ASSERT_TRUE(key2);

  EXPECT_EQ(key1->key(), key2->key());
}

TEST(SymmetricKeyTest, ImportDerivedKey) {
  std::unique_ptr<crypto::SymmetricKey> key1(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::HMAC_SHA1, "password", "somesalt", 1024, 128));
  ASSERT_TRUE(key1);

  std::unique_ptr<crypto::SymmetricKey> key2(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::HMAC_SHA1, key1->key()));
  ASSERT_TRUE(key2);

  EXPECT_EQ(key1->key(), key2->key());
}
