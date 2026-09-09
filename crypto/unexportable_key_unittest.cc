// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span_reader.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/mock_unexportable_key.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/sign.h"
#include "crypto/signature_verifier.h"
#include "crypto/tpm_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_fake_keychain_v2.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
// clang-format off
#include <windows.h>
#include <ncrypt.h>
// clang-format on

#include "crypto/scoped_cng_types.h"
#include "crypto/unexportable_key_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace crypto {

namespace {

using ::testing::ElementsAre;
using ::testing::Return;

constexpr std::optional<crypto::hash::HashKind> ToHashKind(
    tpm::TpmAlgHash alg) {
  switch (alg) {
    case tpm::TPM_ALG_SHA1:
      return hash::kSha1;
    case tpm::TPM_ALG_SHA256:
      return hash::kSha256;
    case tpm::TPM_ALG_SHA384:
      return hash::kSha384;
    case tpm::TPM_ALG_SHA512:
      return hash::kSha512;
  }
  return std::nullopt;
}

template <typename T>
constexpr std::optional<base::span<T>> ReadTpm2b(base::SpanReader<T>& r) {
  ASSIGN_OR_RETURN(auto len, r.ReadU16BigEndian());
  return r.Read(len);
}

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

const sign::SignatureKind kAllAlgorithms[] = {
    sign::ECDSA_SHA256,
    sign::RSA_PKCS1_SHA256,
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
    : public testing::TestWithParam<std::tuple<sign::SignatureKind, Provider>> {
 protected:
  std::unique_ptr<crypto::UnexportableKeyProvider> CreateProvider() const {
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

  sign::SignatureKind algorithm() const { return std::get<0>(GetParam()); }

  Provider provider_type() const { return std::get<1>(GetParam()); }

  bool CurrentAlgorithmSupported(
      crypto::UnexportableKeyProvider* provider) const {
    if (!provider) {
      return false;
    }
    const sign::SignatureKind algorithms[] = {algorithm()};
    return provider->SelectAlgorithm(algorithms) == algorithm();
  }

  sign::SignatureKind signature_kind() const { return algorithm(); }
  hash::HashKind hash_kind() const {
    switch (algorithm()) {
      case sign::ECDSA_SHA256:
      case sign::RSA_PKCS1_SHA256:
        return hash::kSha256;
      default:
        NOTREACHED();
    }
  }

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
    case sign::ECDSA_SHA256:
      LOG(INFO) << "ECDSA P-256, provider=" << ToString(provider_type());
      break;
    case sign::RSA_PKCS1_SHA256:
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

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!CurrentAlgorithmSupported(provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by provider.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};
  const base::TimeTicks generate_start = base::TimeTicks::Now();
  std::unique_ptr<crypto::UnexportableSigningKey> key =
      provider->GenerateSigningKeySlowly(algorithms);
  if (provider_type() == Provider::kFake) {
    ASSERT_TRUE(key);
  } else if (!key) {
    GTEST_SKIP() << "Key generation failed (see https://crbug.com/41494935).";
  }

  EXPECT_EQ(key->IsHardwareBacked(), expected_is_hardware_backed);
#if BUILDFLAG(IS_WIN)
  if (provider_type() == Provider::kFake ||
      provider_type() == Provider::kMicrosoftSoftware) {
    EXPECT_TRUE(key->SupportsTls13());
  } else if (provider_type() == Provider::kTPM) {
    // Verify that the call does not crash, even if the TPM doesn't support
    // TLS 1.3.
    std::ignore = key->SupportsTls13();
  }
#endif
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
  if (!key2) {
    GTEST_SKIP()
        << "Importing wrapped key failed (see https://crbug.com/41494935).";
  }
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
  if (!CurrentAlgorithmSupported(provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by provider.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};
  auto key = provider->GenerateSigningKeySlowly(algorithms);
  if (!key) {
    GTEST_SKIP() << "Key generation failed (see https://crbug.com/41494935).";
  }

  auto ncrypt_key = crypto::DuplicatePlatformKeyHandle(*key);
  EXPECT_TRUE(ncrypt_key.is_valid());
}

TEST_P(UnexportableKeyTest, AttestationKeyCannotSign) {
  if (provider_type() != Provider::kTPM) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!CurrentAlgorithmSupported(provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by provider.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};
  auto key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!key) {
    // Software providers or missing TPM support.
    GTEST_SKIP() << "Skipping test because of lack of hardware support for "
                    "attestation keys (see https://crbug.com/41494935).";
  }

  auto ncrypt_key = crypto::DuplicatePlatformKeyHandle(*key);
  ASSERT_TRUE(ncrypt_key.is_valid());

  std::vector<uint8_t> dummy_hash(32, 0x01);
  DWORD cb_signature = 0;

  BCRYPT_PKCS1_PADDING_INFO pkcs1_padding_info = {0};
  pkcs1_padding_info.pszAlgId = BCRYPT_SHA256_ALGORITHM;
  void* padding_info = nullptr;
  DWORD flags = NCRYPT_SILENT_FLAG;

  if (algorithm() == sign::RSA_PKCS1_SHA256) {
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

// TODO(crbug.com/558952298): Fix and re-enable on win11-arm64-rel.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_CertifyFailsForSoftwareSigningKey \
  DISABLED_CertifyFailsForSoftwareSigningKey
#else
#define MAYBE_CertifyFailsForSoftwareSigningKey \
  CertifyFailsForSoftwareSigningKey
#endif
TEST_P(UnexportableKeyTest, MAYBE_CertifyFailsForSoftwareSigningKey) {
  if (provider_type() != Provider::kTPM) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> tpm_provider =
      CreateProvider();
  if (!CurrentAlgorithmSupported(tpm_provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by TPM provider.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> sw_provider =
      crypto::GetMicrosoftSoftwareUnexportableKeyProvider();
  if (!CurrentAlgorithmSupported(sw_provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by software provider.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};

  auto attestation_key = tpm_provider->GenerateAttestationKeySlowly(algorithms);
  if (!attestation_key) {
    GTEST_SKIP() << "Attestation key generation failed (see "
                    "https://crbug.com/41494935).";
  }

  auto software_signing_key = sw_provider->GenerateSigningKeySlowly(algorithms);
  if (!software_signing_key) {
    GTEST_SKIP() << "Software signing key generation failed (see "
                    "https://crbug.com/41494935).";
  }

  base::HistogramTester histogram_tester;
  std::vector<uint8_t> challenge = {1, 2, 3, 4};

  auto statement =
      attestation_key->CertifySlowly(*software_signing_key, challenge);

  EXPECT_FALSE(statement.has_value());

  histogram_tester.ExpectTotalCount(
      "Crypto.TPMOperation.Win.TpmCertifyExtractProperty.Result", 1);
}

TEST_P(UnexportableKeyTest, FromWrappedAttestationKeyFailsForSigningKey) {
  if (provider_type() != Provider::kTPM) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!CurrentAlgorithmSupported(provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by provider.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};

  // 1. Generate a signing key.
  auto signing_key = provider->GenerateSigningKeySlowly(algorithms);
  if (!signing_key) {
    GTEST_SKIP()
        << "Signing key generation failed (see https://crbug.com/41494935).";
  }
  std::vector<uint8_t> signing_wrapped = signing_key->GetWrappedKey();

  // 2. Try to load it as an attestation key. It should fail.
  auto loaded_attestation_key =
      provider->FromWrappedAttestationKeySlowly(signing_wrapped);
  EXPECT_FALSE(loaded_attestation_key);
}

TEST_P(UnexportableKeyTest,
       FromWrappedAttestationKeySucceedsForAttestationKey) {
  if (provider_type() != Provider::kTPM) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!CurrentAlgorithmSupported(provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by provider.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};

  // 1. Generate an attestation key.
  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!attestation_key) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support for "
                    "attestation keys (see https://crbug.com/41494935).";
  }
  std::vector<uint8_t> attestation_wrapped = attestation_key->GetWrappedKey();

  // 2. Load it as an attestation key. It should succeed.
  auto loaded_attestation_key =
      provider->FromWrappedAttestationKeySlowly(attestation_wrapped);
  ASSERT_TRUE(loaded_attestation_key);
  EXPECT_EQ(loaded_attestation_key->Algorithm(), algorithm());
  EXPECT_EQ(loaded_attestation_key->GetSubjectPublicKeyInfo(),
            attestation_key->GetSubjectPublicKeyInfo());
  EXPECT_EQ(loaded_attestation_key->GetWrappedKey(), attestation_wrapped);

  // 3. Verify that the loaded attestation key can sign and the signature
  // verifies.
  const uint8_t msg[] = {1, 2, 3, 4};
  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> sig,
                       loaded_attestation_key->SignSlowly(msg));

  ASSERT_OK_AND_ASSIGN(auto public_key,
                       crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
                           loaded_attestation_key->GetSubjectPublicKeyInfo()));

  EXPECT_TRUE(sign::Verify(signature_kind(), public_key, msg, sig));
}

TEST_P(UnexportableKeyTest, FromWrappedSigningKeyFailsForAttestationKey) {
  if (provider_type() != Provider::kTPM) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM.";
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!CurrentAlgorithmSupported(provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by provider.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};

  // 1. Generate an attestation key.
  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!attestation_key) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support for "
                    "attestation keys (see https://crbug.com/41494935).";
  }
  std::vector<uint8_t> attestation_wrapped = attestation_key->GetWrappedKey();

  // 2. Try to load it as a signing key. It should fail.
  auto loaded_signing_key =
      provider->FromWrappedSigningKeySlowly(attestation_wrapped);
  EXPECT_FALSE(loaded_signing_key);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_P(UnexportableKeyTest, AttestationKeyCanSignSlowly) {
  if (provider_type() != Provider::kTPM && provider_type() != Provider::kFake) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM or Fake.";
  }

  std::optional<crypto::ScopedFakeUnexportableKeyProvider> fake;
  if (provider_type() == Provider::kFake) {
    fake.emplace();
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};
  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!attestation_key) {
    GTEST_SKIP()
        << "Provider does not support the requested attestation algorithm.";
  }

  const uint8_t msg[] = {1, 2, 3, 4};
  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> sig,
                       attestation_key->SignSlowly(msg));

  ASSERT_OK_AND_ASSIGN(auto public_key,
                       crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
                           attestation_key->GetSubjectPublicKeyInfo()));

  EXPECT_TRUE(sign::Verify(signature_kind(), public_key, msg, sig));
}

TEST_P(UnexportableKeyTest, AttestationKeyCanSignArbitraryPayloadSizes) {
  if (provider_type() != Provider::kTPM && provider_type() != Provider::kFake) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM or Fake.";
  }

  std::optional<crypto::ScopedFakeUnexportableKeyProvider> fake;
  if (provider_type() == Provider::kFake) {
    fake.emplace();
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};
  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!attestation_key) {
    GTEST_SKIP()
        << "Provider does not support the requested attestation algorithm.";
  }

  ASSERT_OK_AND_ASSIGN(auto public_key,
                       crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
                           attestation_key->GetSubjectPublicKeyInfo()));

  for (size_t size : {0u, 512u, 1024u, 1025u, 2048u, 3500u}) {
    SCOPED_TRACE(testing::Message() << "Payload size: " << size);
    std::vector<uint8_t> msg(size);
    std::ranges::generate(
        msg, [i = 0]() mutable { return static_cast<uint8_t>(i++); });

    ASSERT_OK_AND_ASSIGN(auto sig, attestation_key->SignSlowly(msg));
    EXPECT_TRUE(sign::Verify(signature_kind(), public_key, msg, sig));
  }
}

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

  const sign::SignatureKind algorithms[] = {algorithm()};

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

TEST_P(UnexportableKeyTest, AttestationKeySignFailsForTpmGeneratedValue) {
  if (provider_type() != Provider::kTPM && provider_type() != Provider::kFake) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM or Fake.";
  }

#if BUILDFLAG(IS_APPLE)
  if (provider_type() == Provider::kTPM) {
    GTEST_SKIP() << "Apple Secure Enclave keys are not subject to TPM 2.0 "
                    "TPM_GENERATED_VALUE signing restrictions.";
  }
#endif  // BUILDFLAG(IS_APPLE)

  std::optional<crypto::ScopedFakeUnexportableKeyProvider> fake;
  if (provider_type() == Provider::kFake) {
    fake.emplace();
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of provider support.";
  }

  const sign::SignatureKind algorithms[] = {algorithm()};
  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!attestation_key) {
    GTEST_SKIP() << "Skipping test because of lack of attestation key support.";
  }

  auto payload =
      base::ToVector(base::EnumToBigEndian(crypto::tpm::TPM_GENERATED_VALUE));
  payload.insert(payload.end(), {0x01, 0x02, 0x03, 0x04});
  EXPECT_EQ(attestation_key->SignSlowly(payload), std::nullopt);
}

TEST_P(UnexportableKeyTest, CertifySlowly) {
  if (provider_type() != Provider::kTPM && provider_type() != Provider::kFake) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM or Fake.";
  }

#if BUILDFLAG(IS_APPLE)
  if (provider_type() == Provider::kTPM) {
    GTEST_SKIP() << "Attestation is TPM 2.0 specific.";
  }
#endif  // BUILDFLAG(IS_APPLE)

  std::optional<ScopedFakeUnexportableKeyProvider> fake;
  if (provider_type() == Provider::kFake) {
    fake.emplace();
  }

  std::unique_ptr<UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of hardware support.";
  }
  if (!CurrentAlgorithmSupported(provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by provider.";
  }

  auto attestation_key = provider->GenerateAttestationKeySlowly({algorithm()});
  if (!attestation_key) {
    GTEST_SKIP() << "Attestation key generation failed (see "
                    "https://crbug.com/41494935).";
  }

  auto signing_key = provider->GenerateSigningKeySlowly({algorithm()});
  if (!signing_key) {
    GTEST_SKIP()
        << "Signing key generation failed (see https://crbug.com/41494935).";
  }

  const std::vector<uint8_t> challenge = {1, 2, 3, 4};
  ASSERT_OK_AND_ASSIGN(AttestationStatement statement,
                       attestation_key->CertifySlowly(*signing_key, challenge));

  EXPECT_EQ(statement.format, AttestationStatement::kTpm);

  // Validate the TPMS_ATTEST structure.
  base::SpanReader reader(base::span(statement.statement));

  // Validate the magic number to ensure it's a TPM-generated structure.
  ASSERT_EQ(reader.ReadEnumBigEndian<tpm::TpmConstant>(),
            tpm::TPM_GENERATED_VALUE);

  // Ensure this is specifically a certify attestation.
  ASSERT_EQ(reader.ReadEnumBigEndian<tpm::TpmSt>(), tpm::TPM_ST_ATTEST_CERTIFY);

  // qualifiedSigner (TPM2B_NAME)
  ASSERT_TRUE(ReadTpm2b(reader));

  // extraData (TPM2B_DATA): verify the challenge digest matches.
  EXPECT_EQ(ReadTpm2b(reader), hash::Hash(hash_kind(), challenge));

  // clockInfo (TPMS_CLOCK_INFO: 17 bytes)
  ASSERT_TRUE(reader.Skip(17u));

  // firmwareVersion (uint64_t: 8 bytes)
  ASSERT_TRUE(reader.Skip(8u));

  // TPMS_CERTIFY_INFO: name (TPM2B_NAME)
  ASSERT_OK_AND_ASSIGN(auto name, ReadTpm2b(reader));
  base::SpanReader name_reader(name);
  EXPECT_EQ(
      name_reader.ReadEnumBigEndian<tpm::TpmAlgHash>().and_then(ToHashKind),
      hash_kind());
  EXPECT_EQ(name_reader.remaining_span(),
            hash::Hash(hash_kind(), statement.subject_key));

  // TPMS_CERTIFY_INFO: qualifiedName (TPM2B_NAME)
  ASSERT_TRUE(ReadTpm2b(reader));

  // Ensure the entire TPMS_ATTEST structure was parsed without trailing
  // bytes.
  EXPECT_EQ(reader.remaining(), 0u);

  // Verify the attestation signature using the attestation key's SPKI and
  // verify that the signature algorithm matches.
  EXPECT_OK(tpm::VerifySignature(attestation_key->GetSubjectPublicKeyInfo(),
                                 statement.statement, statement.signature));
  ASSERT_OK_AND_ASSIGN(tpm::SignatureAlgorithms signature_algs,
                       tpm::GetSignatureAlgorithms(statement.signature));
  EXPECT_EQ(ToHashKind(signature_algs.hash_alg), hash_kind());

  // Verify that the certified subject_key (TPMT_PUBLIC) matches the signing
  // key's SubjectPublicKeyInfo.
  base::SpanReader public_reader(base::span(statement.subject_key));
  ASSERT_OK_AND_ASSIGN(auto type,
                       public_reader.ReadEnumBigEndian<tpm::TpmAlgPublic>());
  EXPECT_EQ(
      public_reader.ReadEnumBigEndian<tpm::TpmAlgHash>().and_then(ToHashKind),
      hash_kind());

  // objectAttributes (uint32_t)
  ASSERT_TRUE(public_reader.Skip(4u));

  // authPolicy (TPM2B_DIGEST)
  ASSERT_TRUE(ReadTpm2b(public_reader));

  ASSERT_OK_AND_ASSIGN(auto public_key,
                       keypair::PublicKey::FromSubjectPublicKeyInfo(
                           signing_key->GetSubjectPublicKeyInfo()));

  switch (type) {
    case tpm::TPM_ALG_RSA: {
      EXPECT_TRUE(public_key.IsRsa());

      // symmetric: TPMI_ALG_SYM_OBJECT
      EXPECT_EQ(public_reader.ReadEnumBigEndian<tpm::TpmAlgSigScheme>(),
                tpm::TPM_ALG_NULL);

      // scheme: TPMI_ALG_RSA_SCHEME
      EXPECT_EQ(public_reader.ReadEnumBigEndian<tpm::TpmAlgSigScheme>(),
                tpm::TPM_ALG_NULL);

      // keyBits: TPMI_RSA_KEY_BITS
      EXPECT_EQ(public_reader.ReadU16BigEndian(), 2048u);

      // exponent: uint32_t
      EXPECT_EQ(public_reader.ReadU32BigEndian(), 0u);

      // unique: TPM2B_PUBLIC_KEY_RSA
      EXPECT_EQ(ReadTpm2b(public_reader), public_key.GetRsaModulus());
      break;
    }
    case tpm::TPM_ALG_ECC: {
      EXPECT_TRUE(public_key.IsEcP256());

      // symmetric: TPMI_ALG_SYM_OBJECT
      EXPECT_EQ(public_reader.ReadEnumBigEndian<tpm::TpmAlgSigScheme>(),
                tpm::TPM_ALG_NULL);

      // scheme: TPMI_ALG_ECC_SCHEME
      EXPECT_EQ(public_reader.ReadEnumBigEndian<tpm::TpmAlgSigScheme>(),
                tpm::TPM_ALG_NULL);

      // curveID: TPMI_ECC_CURVE
      EXPECT_EQ(public_reader.ReadEnumBigEndian<tpm::TpmEccCurve>(),
                tpm::TPM_ECC_NIST_P256);

      // kdf: TPMT_KDF_SCHEME
      EXPECT_EQ(public_reader.ReadEnumBigEndian<tpm::TpmAlgSigScheme>(),
                tpm::TPM_ALG_NULL);

      // unique: TPMS_ECC_POINT (x and y each TPM2B_ECC_PARAMETER)
      ASSERT_OK_AND_ASSIGN(auto x, ReadTpm2b(public_reader));
      ASSERT_OK_AND_ASSIGN(auto y, ReadTpm2b(public_reader));

      std::vector<uint8_t> ec_point = public_key.ToUncompressedX962Point();
      base::SpanReader<const uint8_t> ec_point_reader(ec_point);
      EXPECT_EQ(ec_point_reader.ReadU8BigEndian(),
                POINT_CONVERSION_UNCOMPRESSED);
      EXPECT_EQ(ec_point_reader.Read(32u), x);
      EXPECT_EQ(ec_point_reader.Read(32u), y);
      EXPECT_EQ(ec_point_reader.remaining(), 0u);
      break;
    }
    default:
      FAIL() << "Unexpected TPM public key type: " << std::to_underlying(type);
  }

  EXPECT_EQ(public_reader.remaining(), 0u);

  std::vector<uint8_t> wrapped_attestation = attestation_key->GetWrappedKey();
  auto loaded_attestation_key =
      provider->FromWrappedAttestationKeySlowly(wrapped_attestation);
  ASSERT_TRUE(loaded_attestation_key);
  EXPECT_EQ(loaded_attestation_key->Algorithm(), attestation_key->Algorithm());
}

}  // namespace

}  // namespace crypto
