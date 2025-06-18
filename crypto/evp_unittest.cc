// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/evp.h"

#include "crypto/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(EVPTest, PublicKeyRoundTrip) {
  auto key = crypto::test::FixedRsa2048PublicKeyForTesting();
  auto bytes = crypto::evp::PublicKeyToBytes(key.key());
  auto unwrapped = crypto::evp::PublicKeyFromBytes(bytes);

  EXPECT_EQ(1, EVP_PKEY_cmp(key.key(), unwrapped.get()));
}

TEST(EVPTest, PrivateKeyRoundTrip) {
  auto key = crypto::test::FixedRsa2048PrivateKeyForTesting();
  auto bytes = crypto::evp::PrivateKeyToBytes(key.key());
  auto unwrapped = crypto::evp::PrivateKeyFromBytes(bytes);

  EXPECT_EQ(1, EVP_PKEY_cmp(key.key(), unwrapped.get()));
}
