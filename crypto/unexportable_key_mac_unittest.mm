// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include "base/test/scoped_feature_list.h"
#include "crypto/fake_apple_keychain_v2.h"
#include "crypto/features.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

namespace {

constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";

const UnexportableKeyProvider::Config config = {
    .keychain_access_group = kTestKeychainAccessGroup,
};

// Tests behaviour that is unique to the macOS implementation of unexportable
// keys.
class UnexportableKeyMacTest : public testing::Test {
 protected:
  ScopedFakeAppleKeychainV2 scoped_fake_apple_keychain_{
      kTestKeychainAccessGroup};

  base::test::ScopedFeatureList scoped_feature_list_{
      kEnableMacUnexportableKeys};
};

TEST_F(UnexportableKeyMacTest, SecureEnclaveAvailability) {
  for (bool available : {true, false}) {
    scoped_fake_apple_keychain_.keychain()->set_secure_enclave_available(
        available);
    EXPECT_EQ(GetUnexportableKeyProvider(config) != nullptr, available);
  }
}

}  // namespace

}  // namespace crypto
