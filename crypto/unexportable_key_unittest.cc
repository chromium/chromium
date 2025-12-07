// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include <optional>
#include <tuple>

#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/features.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_fake_keychain_v2.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "crypto/scoped_cng_types.h"
#include "crypto/unexportable_key_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

enum class Provider {
  kTPM,
  kFake,
  kMicrosoftSoftware,
};

const Provider kAllProviders[] = {
    Provider::kTPM,
    Provider::kFake,
    Provider::kMicrosoftSoftware,
};

const crypto::SignatureVerifier::SignatureAlgorithm kAllAlgorithms[] = {
    crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
    crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
};

#if BUILDFLAG(IS_MAC)
constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";
#endif  // BUILDFLAG(IS_MAC)

std::string ToString(Provider provider) {
  switch (provider) {
    case Provider::kTPM:
      return "TPM";
    case Provider::kFake:
      return "Fake";
    case Provider::kMicrosoftSoftware:
      return "Microsoft Software";
  }
}

class UnexportableKeySigningTest
    : public testing::TestWithParam<
          std::tuple<crypto::SignatureVerifier::SignatureAlgorithm,
                     Provider,
                     bool>> {
 protected:
  UnexportableKeySigningTest() {
    scoped_feature_list_.InitWithFeatureState(
        crypto::features::kIsHardwareBackedFixEnabled,
        is_hardware_backed_fix_enabled());
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> CreateProvider() {
    if (provider_type() == Provider::kMicrosoftSoftware) {
      return crypto::GetMicrosoftSoftwareUnexportableKeyProvider();
    }

    crypto::UnexportableKeyProvider::Config config{
#if BUILDFLAG(IS_MAC)
        .keychain_access_group = kTestKeychainAccessGroup
#endif  // BUILDLFAG(IS_MAC)
    };
    return crypto::GetUnexportableKeyProvider(std::move(config));
  }

  crypto::SignatureVerifier::SignatureAlgorithm algorithm() {
    return std::get<0>(GetParam());
  }

  Provider provider_type() { return std::get<1>(GetParam()); }

  bool is_hardware_backed_fix_enabled() { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_MAC)
  crypto::apple::ScopedFakeKeychainV2 scoped_fake_keychain_{
      kTestKeychainAccessGroup};
#endif  // BUILDFLAG(IS_MAC)
};

INSTANTIATE_TEST_SUITE_P(All,
                         UnexportableKeySigningTest,
                         testing::Combine(testing::ValuesIn(kAllAlgorithms),
                                          testing::ValuesIn(kAllProviders),
                                          testing::Bool()));

TEST_P(UnexportableKeySigningTest, RoundTrip) {
  const bool expected_is_hardware_backed =
      provider_type() == Provider::kFake ? false
      : is_hardware_backed_fix_enabled() ? (provider_type() == Provider::kTPM)
                                         : true;

  switch (algorithm()) {
    case crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      LOG(INFO) << "ECDSA P-256, provider=" << ToString(provider_type());
      break;
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      LOG(INFO) << "RSA, provider=" << ToString(provider_type());
      break;
    default:
      ASSERT_TRUE(false);
  }

  SCOPED_TRACE(static_cast<int>(algorithm()));
  SCOPED_TRACE(ToString(provider_type()));

  std::optional<crypto::ScopedFakeUnexportableKeyProvider> fake;
  if (provider_type() == Provider::kFake) {
    fake.emplace();
  }

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    LOG(INFO) << "Skipping test because of lack of hardware support.";
    return;
  }

  if (!provider->SelectAlgorithm(algorithms)) {
    LOG(INFO) << "Skipping test because of lack of support for this key type.";
    return;
  }

  const base::TimeTicks generate_start = base::TimeTicks::Now();
  std::unique_ptr<crypto::UnexportableSigningKey> key =
      provider->GenerateSigningKeySlowly(algorithms);
  if (algorithm() ==
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256) {
    if (!key) {
      GTEST_SKIP()
          << "Workaround for https://issues.chromium.org/issues/41494935";
    }
  }

  ASSERT_TRUE(key);
  EXPECT_EQ(key->IsHardwareBacked(), expected_is_hardware_backed);
  LOG(INFO) << "Generation took " << (base::TimeTicks::Now() - generate_start);

  ASSERT_EQ(key->Algorithm(), algorithm());
  const std::vector<uint8_t> wrapped = key->GetWrappedKey();
  const std::vector<uint8_t> spki = key->GetSubjectPublicKeyInfo();
  const uint8_t msg[] = {1, 2, 3, 4};

  const base::TimeTicks sign_start = base::TimeTicks::Now();
  const std::optional<std::vector<uint8_t>> sig = key->SignSlowly(msg);
  LOG(INFO) << "Signing took " << (base::TimeTicks::Now() - sign_start);
  ASSERT_TRUE(sig);

  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(algorithm(), *sig, spki));
  verifier.VerifyUpdate(msg);
  ASSERT_TRUE(verifier.VerifyFinal());

  const base::TimeTicks import2_start = base::TimeTicks::Now();
  std::unique_ptr<crypto::UnexportableSigningKey> key2 =
      provider->FromWrappedSigningKeySlowly(wrapped);
  ASSERT_TRUE(key2);
  LOG(INFO) << "Import took " << (base::TimeTicks::Now() - import2_start);

  const base::TimeTicks sign2_start = base::TimeTicks::Now();
  const std::optional<std::vector<uint8_t>> sig2 = key->SignSlowly(msg);
  LOG(INFO) << "Signing took " << (base::TimeTicks::Now() - sign2_start);
  ASSERT_TRUE(sig2);

  crypto::SignatureVerifier verifier2;
  ASSERT_TRUE(verifier2.VerifyInit(algorithm(), *sig2, spki));
  verifier2.VerifyUpdate(msg);
  ASSERT_TRUE(verifier2.VerifyFinal());

  crypto::StatefulUnexportableKeyProvider* stateful_provider =
      provider->AsStatefulUnexportableKeyProvider();
  EXPECT_TRUE(stateful_provider == nullptr ||
              stateful_provider->DeleteSigningKeySlowly(wrapped));
}

#if BUILDFLAG(IS_WIN)
TEST_P(UnexportableKeySigningTest, DuplicatePlatformKeyHandleSucceeds) {
  if (provider_type() == Provider::kFake) {
    GTEST_SKIP() << "Test only works with real platform keys.";
  }

  if (provider_type() == Provider::kMicrosoftSoftware &&
      !is_hardware_backed_fix_enabled()) {
    GTEST_SKIP() << "Fix for software keys is disabled.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support.";
  }

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};
  auto key = provider->GenerateSigningKeySlowly(algorithms);
  if (algorithm() ==
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256) {
    if (!key) {
      GTEST_SKIP()
          << "Workaround for https://issues.chromium.org/issues/41494935";
    }
  }

  ASSERT_TRUE(key);

  auto ncrypt_key = crypto::DuplicatePlatformKeyHandle(*key);
  EXPECT_TRUE(ncrypt_key.is_valid());
}
#endif

}  // namespace
