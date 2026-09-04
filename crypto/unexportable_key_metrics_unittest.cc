// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key_metrics.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/containers/transparent_hash.h"
#include "base/test/metrics/histogram_tester.h"
#include "crypto/unexportable_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace crypto {

namespace {

// Mock that wraps the stateless software unexportable key provider while
// tracking key creation and removal. CHECKs if there are keys left that have
// not been removed when destroyed.
class MockTrackingUnexportableKeyProvider
    : public StatefulUnexportableKeyProvider {
 public:
  MockTrackingUnexportableKeyProvider()
      : key_provider_(GetSoftwareUnsecureUnexportableKeyProvider()) {}

  ~MockTrackingUnexportableKeyProvider() override {
    CHECK(keys_.empty()) << keys_.size() << " key(s) not deleted.";
  }

  // UnexportableKeyProvider:
  std::optional<sign::SignatureKind> SelectAlgorithm(
      base::span<const sign::SignatureKind> acceptable_algorithms) override {
    return key_provider_->SelectAlgorithm(acceptable_algorithms);
  }
  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const sign::SignatureKind> acceptable_algorithms) override {
    std::unique_ptr<UnexportableSigningKey> key =
        key_provider_->GenerateSigningKeySlowly(acceptable_algorithms);
    if (key) {
      keys_.emplace(key->GetWrappedKey());
    }
    return key;
  }
  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    CHECK(keys_.contains(wrapped_key))
        << "Attempted to delete non existing key";
    return key_provider_->FromWrappedSigningKeySlowly(wrapped_key);
  }

  std::unique_ptr<UnexportableAttestationKey> GenerateAttestationKeySlowly(
      base::span<const sign::SignatureKind> acceptable_algorithms) override {
    std::unique_ptr<UnexportableAttestationKey> key =
        key_provider_->GenerateAttestationKeySlowly(acceptable_algorithms);
    if (key) {
      keys_.emplace(key->GetWrappedKey());
    }
    return key;
  }

  std::unique_ptr<UnexportableAttestationKey> FromWrappedAttestationKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    CHECK(keys_.contains(wrapped_key))
        << "Attempted to load non existing attestation key";
    return key_provider_->FromWrappedAttestationKeySlowly(wrapped_key);
  }

  StatefulUnexportableKeyProvider* AsStatefulUnexportableKeyProvider()
      override {
    return this;
  }

  // StatefulUnexportableKeyProvider:
  std::optional<std::vector<std::unique_ptr<UnexportableSigningKey>>>
  GetAllKeysSlowly() override {
    return base::ToVector(keys_, [&](base::span<const uint8_t> key) {
      return FromWrappedSigningKeySlowly(key);
    });
  }

  std::optional<size_t> DeleteWrappedKeysSlowly(
      base::span<const base::span<const uint8_t>> wrapped_keys) override {
    if (StatefulUnexportableKeyProvider* stateful_key_provider =
            key_provider_->AsStatefulUnexportableKeyProvider()) {
      stateful_key_provider->DeleteWrappedKeysSlowly(wrapped_keys);
    }
    return std::ranges::count_if(
        wrapped_keys, [&](auto key) { return keys_.erase(key) != 0; });
  }

  std::optional<size_t> DeleteKeysSlowly(
      base::span<const UnexportableSigningKey* const> keys) override {
    if (StatefulUnexportableKeyProvider* stateful_key_provider =
            key_provider_->AsStatefulUnexportableKeyProvider()) {
      stateful_key_provider->DeleteKeysSlowly(keys);
    }
    return std::ranges::count_if(keys, [&](auto* key) {
      return keys_.erase(key->GetWrappedKey()) != 0;
    });
  }

  std::optional<size_t> DeleteAllKeysSlowly() override {
    if (StatefulUnexportableKeyProvider* stateful_key_provider =
            key_provider_->AsStatefulUnexportableKeyProvider()) {
      stateful_key_provider->DeleteAllKeysSlowly();
    }
    return std::exchange(keys_, {}).size();
  }

 private:
  std::unique_ptr<UnexportableKeyProvider> key_provider_;
  absl::flat_hash_set<std::vector<uint8_t>,
                      base::TransparentHashAs<base::span<const uint8_t>>,
                      base::TransparentEqualAs<base::span<const uint8_t>>>
      keys_;
};

std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderMock() {
  return std::make_unique<MockTrackingUnexportableKeyProvider>();
}

class UnexportableKeyMetricTest : public testing::Test {
  void SetUp() override {
    internal::SetUnexportableKeyProviderForTesting(
        GetUnexportableKeyProviderMock);
  }

  void TearDown() override {
    internal::SetUnexportableKeyProviderForTesting(nullptr);
  }
};

// Note mock provider only supports ECDSA.
TEST_F(UnexportableKeyMetricTest, GatherAllMetrics) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Crypto.TPMSupport2", 0);
  histogram_tester.ExpectTotalCount("Crypto.TPMDuration.NewKeyCreationECDSA",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMDuration.WrappedKeyCreationECDSA", 0);
  histogram_tester.ExpectTotalCount("Crypto.TPMDuration.KeyCertificationECDSA",
                                    0);
  histogram_tester.ExpectTotalCount("Crypto.TPMDuration.MessageSigningECDSA",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMDuration.RestrictedMessageSigningECDSA", 0);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMDuration.NewAttestationKeyCreationECDSA", 0);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMDuration.WrappedAttestationKeyCreationECDSA", 0);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMOperation.NewAttestationKeyCreationECDSA", 0);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMOperation.WrappedAttestationKeyCreationECDSA", 0);
  histogram_tester.ExpectTotalCount("Crypto.TPMOperation.NewKeyCreation", 0);
  histogram_tester.ExpectTotalCount("Crypto.TPMOperation.WrappedKeyCreation",
                                    0);
  histogram_tester.ExpectTotalCount("Crypto.TPMOperation.KeyCertification", 0);
  histogram_tester.ExpectTotalCount("Crypto.TPMOperation.MessageSigning", 0);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMOperation.RestrictedMessageSigning", 0);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMOperation.RestrictedMessageVerify", 0);
  histogram_tester.ExpectTotalCount("Crypto.TPMOperation.MessageVerify", 0);

  internal::MeasureTpmOperationsInternalForTesting();

  EXPECT_THAT(histogram_tester.GetAllSamples("Crypto.TPMSupport2"),
              BucketsAre(base::Bucket(internal::TPMSupport::kECDSA, 1)));
  histogram_tester.ExpectTotalCount("Crypto.TPMDuration.NewKeyCreationECDSA",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMDuration.WrappedKeyCreationECDSA", 1);
  histogram_tester.ExpectTotalCount("Crypto.TPMDuration.KeyCertificationECDSA",
                                    1);
  histogram_tester.ExpectTotalCount("Crypto.TPMDuration.MessageSigningECDSA",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMDuration.RestrictedMessageSigningECDSA", 1);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Crypto.TPMOperation.NewKeyCreationECDSA"),
      BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Crypto.TPMOperation.WrappedKeyCreationECDSA"),
              BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Crypto.TPMOperation.KeyCertificationECDSA"),
              BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Crypto.TPMOperation.MessageSigningECDSA"),
      BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Crypto.TPMOperation.RestrictedMessageSigningECDSA"),
              BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Crypto.TPMOperation.RestrictedMessageVerifyECDSA"),
              BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Crypto.TPMOperation.MessageVerifyECDSA"),
      BucketsAre(base::Bucket(true, 1)));
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMDuration.NewAttestationKeyCreationECDSA", 1);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMDuration.WrappedAttestationKeyCreationECDSA", 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Crypto.TPMOperation.NewAttestationKeyCreationECDSA"),
              BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Crypto.TPMOperation.WrappedAttestationKeyCreationECDSA"),
              BucketsAre(base::Bucket(true, 1)));
}

}  // namespace

}  // namespace crypto
