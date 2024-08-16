// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/icloud_recovery_key_mac.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <memory>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/trusted_vault/securebox.h"
#include "crypto/apple_keychain_v2.h"
#include "crypto/fake_apple_keychain_v2.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::enclave {

namespace {

constexpr char kKeychainAccessGroup[] = "keychain-access-group";
constexpr uint8_t kHeader[]{'h', 'e', 'a', 'd', 'e', 'r'};
constexpr uint8_t kPlaintext[]{'h', 'e', 'l', 'l', 'o'};

class ICloudRecoveryKeyTest : public testing::Test {
 public:
  std::unique_ptr<ICloudRecoveryKey> CreateKey() {
    std::unique_ptr<ICloudRecoveryKey> new_key;
    base::RunLoop run_loop;
    ICloudRecoveryKey::Create(
        base::BindLambdaForTesting([&](std::unique_ptr<ICloudRecoveryKey> ret) {
          new_key = std::move(ret);
          run_loop.Quit();
        }),
        kKeychainAccessGroup);
    run_loop.Run();
    return new_key;
  }

 protected:
  crypto::ScopedFakeAppleKeychainV2 fake_keychain_{kKeychainAccessGroup};
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ICloudRecoveryKeyTest, EndToEnd) {
  std::unique_ptr<ICloudRecoveryKey> key = CreateKey();
  std::optional<std::vector<uint8_t>> encrypted =
      key->key()->public_key().Encrypt(base::span<uint8_t>(), kHeader,
                                       kPlaintext);
  ASSERT_TRUE(encrypted);

  std::unique_ptr<ICloudRecoveryKey> retrieved;
  base::RunLoop run_loop;
  ICloudRecoveryKey::Retrieve(
      base::BindLambdaForTesting(
          [&](std::vector<std::unique_ptr<ICloudRecoveryKey>> ret) {
            ASSERT_EQ(ret.size(), 1u);
            retrieved = std::move(ret.at(0));
            run_loop.Quit();
          }),
      kKeychainAccessGroup);
  run_loop.Run();

  std::optional<std::vector<uint8_t>> decrypted =
      retrieved->key()->private_key().Decrypt(base::span<uint8_t>(), kHeader,
                                              *encrypted);
  ASSERT_TRUE(decrypted);
  EXPECT_EQ(base::span<const uint8_t>(*decrypted),
            base::span<const uint8_t>(kPlaintext));
}

TEST_F(ICloudRecoveryKeyTest, CreateAndRetrieve) {
  std::unique_ptr<ICloudRecoveryKey> key1 = CreateKey();
  ASSERT_TRUE(key1);
  ASSERT_TRUE(key1->key());

  std::unique_ptr<ICloudRecoveryKey> key2 = CreateKey();
  ASSERT_TRUE(key2);
  ASSERT_TRUE(key2->key());

  std::optional<std::vector<std::unique_ptr<ICloudRecoveryKey>>> keys;
  base::RunLoop run_loop;
  ICloudRecoveryKey::Retrieve(
      base::BindLambdaForTesting(
          [&](std::vector<std::unique_ptr<ICloudRecoveryKey>> ret) {
            keys = std::move(ret);
            run_loop.Quit();
          }),
      kKeychainAccessGroup);
  run_loop.Run();

  ASSERT_TRUE(keys);
  ASSERT_EQ(keys->size(), 2u);

  auto key1it = std::ranges::find_if(
      *keys, [&](const std::unique_ptr<ICloudRecoveryKey>& key) {
        return key->id() == key1->id();
      });
  EXPECT_NE(key1it, keys->end());

  auto key2it = std::ranges::find_if(
      *keys, [&](const std::unique_ptr<ICloudRecoveryKey>& key) {
        return key->id() == key2->id();
      });
  EXPECT_NE(key2it, keys->end());
}

TEST_F(ICloudRecoveryKeyTest, CreateKeychainError) {
  // Force a keychain error by setting the wrong access group.
  std::unique_ptr<ICloudRecoveryKey> keys;
  base::RunLoop run_loop;
  ICloudRecoveryKey::Create(
      base::BindLambdaForTesting([&](std::unique_ptr<ICloudRecoveryKey> ret) {
        keys = std::move(ret);
        run_loop.Quit();
      }),
      "wrong keychain group");
  run_loop.Run();
  EXPECT_FALSE(keys);
}

TEST_F(ICloudRecoveryKeyTest, RetrieveKeychainError) {
  // Force a keychain error by setting the wrong access group.
  std::optional<std::vector<std::unique_ptr<ICloudRecoveryKey>>> keys;
  base::RunLoop run_loop;
  ICloudRecoveryKey::Retrieve(
      base::BindLambdaForTesting(
          [&](std::vector<std::unique_ptr<ICloudRecoveryKey>> ret) {
            keys = std::move(ret);
            run_loop.Quit();
          }),
      "wrong keychain group");
  run_loop.Run();
  EXPECT_TRUE(keys->empty());
}

TEST_F(ICloudRecoveryKeyTest, RetrieveEmpty) {
  std::optional<std::vector<std::unique_ptr<ICloudRecoveryKey>>> keys;
  base::RunLoop run_loop;
  ICloudRecoveryKey::Retrieve(
      base::BindLambdaForTesting(
          [&](std::vector<std::unique_ptr<ICloudRecoveryKey>> ret) {
            keys = std::move(ret);
            run_loop.Quit();
          }),
      kKeychainAccessGroup);
  run_loop.Run();
  EXPECT_TRUE(keys->empty());
}

TEST_F(ICloudRecoveryKeyTest, RetrieveCorrupted) {
  std::unique_ptr<ICloudRecoveryKey> key1 = CreateKey();
  ASSERT_TRUE(key1);
  ASSERT_TRUE(key1->key());

  base::apple::ScopedCFTypeRef<CFDataRef> corrupted_key(
      CFDataCreate(kCFAllocatorDefault, nullptr, 0));
  CFDictionarySetValue(const_cast<CFMutableDictionaryRef>(
                           fake_keychain_.keychain()->items().at(0).get()),
                       kSecValueData, corrupted_key.get());

  std::optional<std::vector<std::unique_ptr<ICloudRecoveryKey>>> keys;
  base::RunLoop run_loop;
  ICloudRecoveryKey::Retrieve(
      base::BindLambdaForTesting(
          [&](std::vector<std::unique_ptr<ICloudRecoveryKey>> ret) {
            keys = std::move(ret);
            run_loop.Quit();
          }),
      kKeychainAccessGroup);
  run_loop.Run();
  EXPECT_TRUE(keys->empty());
}

}  // namespace

}  // namespace device::enclave
