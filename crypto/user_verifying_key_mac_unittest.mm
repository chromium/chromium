// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/user_verifying_key.h"

#include <iterator>
#include <memory>

#include <LocalAuthentication/LocalAuthentication.h>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "crypto/fake_apple_keychain_v2.h"
#include "crypto/features.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "crypto/scoped_lacontext.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

namespace {

constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";
constexpr SignatureVerifier::SignatureAlgorithm kAcceptableAlgos[] = {
    SignatureVerifier::ECDSA_SHA256};

UserVerifyingKeyProvider::Config MakeConfig() {
  UserVerifyingKeyProvider::Config config;
  config.keychain_access_group = kTestKeychainAccessGroup;
  return config;
}

class UserVerifyingKeyMacTest : public testing::Test {
 public:
  std::unique_ptr<UserVerifyingSigningKey> GenerateUserVerifyingSigningKey() {
    std::unique_ptr<UserVerifyingSigningKey> key;
    base::RunLoop run_loop;
    provider_->GenerateUserVerifyingSigningKey(
        kAcceptableAlgos,
        base::BindLambdaForTesting(
            [&](base::expected<std::unique_ptr<UserVerifyingSigningKey>,
                               UserVerifyingKeyCreationError> result) {
              if (result.has_value()) {
                key = std::move(result.value());
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return key;
  }

  std::unique_ptr<UserVerifyingSigningKey> GetUserVerifyingSigningKey(
      std::string key_label) {
    std::unique_ptr<UserVerifyingSigningKey> key;
    base::RunLoop run_loop;
    provider_->GetUserVerifyingSigningKey(
        key_label,
        base::BindLambdaForTesting(
            [&](base::expected<std::unique_ptr<UserVerifyingSigningKey>,
                               UserVerifyingKeyCreationError> result) {
              if (result.has_value()) {
                key = std::move(result.value());
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return key;
  }

  bool DeleteUserVerifyingKey(std::string key_label) {
    std::optional<bool> deleted;
    base::RunLoop run_loop;
    provider_->DeleteUserVerifyingKey(
        key_label, base::BindLambdaForTesting([&](bool result) {
          deleted = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return *deleted;
  }

  std::optional<std::vector<uint8_t>> Sign(UserVerifyingSigningKey* key,
                                           base::span<const uint8_t> message) {
    std::optional<std::vector<uint8_t>> signature;
    base::RunLoop run_loop;
    key->Sign(message,
              base::BindLambdaForTesting(
                  [&](base::expected<std::vector<uint8_t>,
                                     UserVerifyingKeySigningError> result) {
                    if (result.has_value()) {
                      signature = std::move(result.value());
                    }
                    run_loop.Quit();
                  }));
    run_loop.Run();
    return signature;
  }

 protected:
  ScopedFakeAppleKeychainV2 scoped_fake_apple_keychain_{
      kTestKeychainAccessGroup};

  base::test::TaskEnvironment task_environment_;

  base::test::ScopedFeatureList scoped_feature_list_{
      kEnableMacUnexportableKeys};

  std::unique_ptr<UserVerifyingKeyProvider> provider_ =
      crypto::GetUserVerifyingKeyProvider(MakeConfig());
};

TEST_F(UserVerifyingKeyMacTest, RoundTrip) {
  for (bool use_lacontext : {false, true}) {
    SCOPED_TRACE(use_lacontext);
    UserVerifyingKeyProvider::Config config = MakeConfig();
    if (use_lacontext) {
      config.lacontext = ScopedLAContext([[LAContext alloc] init]);
    }
    provider_ = crypto::GetUserVerifyingKeyProvider(std::move(config));

    std::unique_ptr<UserVerifyingSigningKey> key =
        GenerateUserVerifyingSigningKey();
    ASSERT_TRUE(key);
    ASSERT_TRUE(!key->GetKeyLabel().empty());

    const std::vector<uint8_t> spki = key->GetPublicKey();
    const uint8_t message[] = {1, 2, 3, 4};
    std::optional<std::vector<uint8_t>> signature = Sign(key.get(), message);
    ASSERT_TRUE(signature);

    crypto::SignatureVerifier verifier;
    ASSERT_TRUE(verifier.VerifyInit(kAcceptableAlgos[0], *signature, spki));
    verifier.VerifyUpdate(message);
    ASSERT_TRUE(verifier.VerifyFinal());

    std::unique_ptr<UserVerifyingSigningKey> key2 =
        GetUserVerifyingSigningKey(key->GetKeyLabel());
    ASSERT_TRUE(key2);

    std::optional<std::vector<uint8_t>> signature2 = Sign(key.get(), message);
    ASSERT_TRUE(signature2);

    crypto::SignatureVerifier verifier2;
    ASSERT_TRUE(verifier2.VerifyInit(kAcceptableAlgos[0], *signature2, spki));
    verifier2.VerifyUpdate(message);
    ASSERT_TRUE(verifier2.VerifyFinal());
  }
}

TEST_F(UserVerifyingKeyMacTest, SecureEnclaveAvailability) {
  using UVMethod = FakeAppleKeychainV2::UVMethod;
  struct {
    bool enclave_available;
    UVMethod uv_method;
    bool expected_uvk_available;
  } kTests[] = {
      {false, UVMethod::kNone, false},
      {false, UVMethod::kPasswordOnly, false},
      {false, UVMethod::kBiometrics, false},
      {true, UVMethod::kNone, false},
      {true, UVMethod::kPasswordOnly, true},
      {true, UVMethod::kBiometrics, true},
  };
  for (auto test : kTests) {
    SCOPED_TRACE(test.enclave_available);
    SCOPED_TRACE(static_cast<int>(test.uv_method));
    scoped_fake_apple_keychain_.keychain()->set_secure_enclave_available(
        test.enclave_available);
    scoped_fake_apple_keychain_.keychain()->set_uv_method(test.uv_method);
    std::optional<bool> result;
    base::RunLoop run_loop;
    AreUserVerifyingKeysSupported(MakeConfig(),
                                  base::BindLambdaForTesting([&](bool ret) {
                                    result = ret;
                                    run_loop.Quit();
                                  }));
    run_loop.Run();
    EXPECT_EQ(result.value(), test.expected_uvk_available);
  }
}

TEST_F(UserVerifyingKeyMacTest, DeleteSigningKey) {
  std::unique_ptr<UserVerifyingSigningKey> key =
      GenerateUserVerifyingSigningKey();
  ASSERT_TRUE(key);

  EXPECT_TRUE(DeleteUserVerifyingKey(key->GetKeyLabel()));
  EXPECT_FALSE(GetUserVerifyingSigningKey(key->GetKeyLabel()));
  EXPECT_FALSE(DeleteUserVerifyingKey(key->GetKeyLabel()));
}

}  // namespace

}  // namespace crypto
