// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include <optional>
#include <tuple>

#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/mock_unexportable_key.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_fake_keychain_v2.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "crypto/scoped_cng_types.h"
#include "crypto/unexportable_key_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

using ::testing::ElementsAre;
using ::testing::Return;

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

#if BUILDFLAG(IS_APPLE)
constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";
#endif  // BUILDFLAG(IS_APPLE)

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

class UnexportableKeyTest
    : public testing::TestWithParam<
          std::tuple<crypto::SignatureVerifier::SignatureAlgorithm, Provider>> {
 protected:
  std::unique_ptr<crypto::UnexportableKeyProvider> CreateProvider() {
    if (provider_type() == Provider::kMicrosoftSoftware) {
      return crypto::GetMicrosoftSoftwareUnexportableKeyProvider();
    }

    crypto::UnexportableKeyProvider::Config config{
#if BUILDFLAG(IS_APPLE)
        .keychain_access_group = kTestKeychainAccessGroup
#endif  // BUILDFLAG(IS_APPLE)
    };
    return crypto::GetUnexportableKeyProvider(std::move(config));
  }

  crypto::SignatureVerifier::SignatureAlgorithm algorithm() {
    return std::get<0>(GetParam());
  }

  Provider provider_type() { return std::get<1>(GetParam()); }

 private:
#if BUILDFLAG(IS_MAC)
  crypto::apple::ScopedFakeKeychainV2 scoped_fake_keychain_{
      kTestKeychainAccessGroup};
#endif  // BUILDFLAG(IS_MAC)
};

INSTANTIATE_TEST_SUITE_P(All,
                         UnexportableKeyTest,
                         testing::Combine(testing::ValuesIn(kAllAlgorithms),
                                          testing::ValuesIn(kAllProviders)));

TEST_P(UnexportableKeyTest, RoundTrip) {
  const bool expected_is_hardware_backed =
      provider_type() == Provider::kFake ? false
                                         : provider_type() == Provider::kTPM;

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
              stateful_provider->DeleteWrappedKeysSlowly({wrapped}));
}

#if BUILDFLAG(IS_WIN)
TEST_P(UnexportableKeyTest, DuplicatePlatformKeyHandleSucceeds) {
  if (provider_type() == Provider::kFake) {
    GTEST_SKIP() << "Test only works with real platform keys.";
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

TEST_P(UnexportableKeyTest, AttestationKeyCannotSign) {
  if (provider_type() != Provider::kTPM) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support.";
  }

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};
  auto key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!key) {
    // Software providers or missing TPM support.
    GTEST_SKIP() << "Skipping test because of lack of hardware support for "
                    "attestation keys.";
  }

  auto ncrypt_key = crypto::DuplicatePlatformKeyHandle(*key);
  ASSERT_TRUE(ncrypt_key.is_valid());

  std::vector<uint8_t> dummy_hash(32, 0x01);
  DWORD cb_signature = 0;

  BCRYPT_PKCS1_PADDING_INFO pkcs1_padding_info = {0};
  pkcs1_padding_info.pszAlgId = BCRYPT_SHA256_ALGORITHM;
  void* padding_info = nullptr;
  DWORD flags = NCRYPT_SILENT_FLAG;

  if (algorithm() ==
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256) {
    padding_info = &pkcs1_padding_info;
    flags |= BCRYPT_PAD_PKCS1;
  }

  SECURITY_STATUS status =
      NCryptSignHash(ncrypt_key.get(), padding_info, dummy_hash.data(),
                     dummy_hash.size(), nullptr, 0, &cb_signature, flags);
  // For AIKs, signing arbitrary data should fail because of
  // NCRYPT_PCP_IDENTITY_KEY.
  EXPECT_NE(status, 0);
}

TEST_P(UnexportableKeyTest, FromWrappedAttestationKeyFailsForSigningKey) {
  if (provider_type() != Provider::kTPM) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support.";
  }

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};

  // 1. Generate a signing key.
  auto signing_key = provider->GenerateSigningKeySlowly(algorithms);
  if (algorithm() ==
          crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256 &&
      !signing_key) {
    GTEST_SKIP()
        << "Workaround for https://issues.chromium.org/issues/41494935";
  }
  ASSERT_TRUE(signing_key);
  std::vector<uint8_t> signing_wrapped = signing_key->GetWrappedKey();

  // 2. Try to load it as an attestation key. It should fail.
  auto loaded_attestation_key =
      provider->FromWrappedAttestationKeySlowly(signing_wrapped);
  EXPECT_FALSE(loaded_attestation_key);
}

TEST_P(UnexportableKeyTest,
       FromWrappedAttestationKeySucceedsForAttestationKey) {
  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support.";
  }

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};

  // 1. Generate an attestation key.
  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!attestation_key) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support for "
                    "attestation keys.";
  }
  std::vector<uint8_t> attestation_wrapped = attestation_key->GetWrappedKey();

  // 2. Load it as an attestation key. It should succeed.
  auto loaded_attestation_key =
      provider->FromWrappedAttestationKeySlowly(attestation_wrapped);
  EXPECT_TRUE(loaded_attestation_key);
}

TEST_P(UnexportableKeyTest, FromWrappedSigningKeyFailsForAttestationKey) {
  if (provider_type() != Provider::kTPM) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support.";
  }

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};

  // 1. Generate an attestation key.
  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!attestation_key) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support for "
                    "attestation keys.";
  }
  std::vector<uint8_t> attestation_wrapped = attestation_key->GetWrappedKey();

  // 2. Try to load it as a signing key. It should fail.
  auto loaded_signing_key =
      provider->FromWrappedSigningKeySlowly(attestation_wrapped);
  EXPECT_FALSE(loaded_signing_key);
}
#endif

TEST_P(UnexportableKeyTest, AttestationKeyMock) {
  crypto::ScopedMockUnexportableKeyProvider mock_provider;

  auto mock_attestation_key =
      std::make_unique<crypto::MockUnexportableAttestationKey>();

  EXPECT_CALL(*mock_attestation_key, CertifySlowly)
      .WillOnce(testing::Return(crypto::AttestationStatement{
          .format = crypto::AttestationStatement::kTpm,
          .statement = {1, 2, 3},
          .signature = {4, 5, 6},
      }));

  EXPECT_CALL(mock_provider.mock(), GenerateAttestationKeySlowly)
      .WillOnce(Return(std::move(mock_attestation_key)));

  auto provider = CreateProvider();
  ASSERT_TRUE(provider);

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};

  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  ASSERT_TRUE(attestation_key);

  auto software_provider = crypto::GetSoftwareUnsecureUnexportableKeyProvider();
  auto signing_key = software_provider->GenerateSigningKeySlowly(algorithms);
  ASSERT_TRUE(signing_key);

  auto statement = attestation_key->CertifySlowly(
      *signing_key, std::vector<uint8_t>{7, 8, 9});
  ASSERT_TRUE(statement);
  EXPECT_EQ(statement->format, crypto::AttestationStatement::kTpm);
  EXPECT_THAT(statement->statement, ElementsAre(1, 2, 3));
  EXPECT_THAT(statement->signature, ElementsAre(4, 5, 6));
}

}  // namespace
