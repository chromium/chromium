// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/keychain_util.h"

#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";

class AppleKeychainUtilTest : public testing::Test {
 protected:
  crypto::apple::ScopedFakeKeychainV2 scoped_fake_keychain_{
      kTestKeychainAccessGroup};
};

#if !BUILDFLAG(IS_IOS)
TEST_F(AppleKeychainUtilTest, ExecutableHasKeychainAccessGroupEntitlement) {
  EXPECT_TRUE(crypto::apple::ExecutableHasKeychainAccessGroupEntitlement(
      kTestKeychainAccessGroup));
  EXPECT_FALSE(crypto::apple::ExecutableHasKeychainAccessGroupEntitlement(
      "some-other-keychain-access-group"));
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace
