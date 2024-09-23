// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key_metrics.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

namespace {

// Mock that wraps the stateless software unexportable key provider while
// tracking key creation and removal. CHECKs if there are keys left that have
// not been removed when destroyed.
class MockTrackingUnexportableKeyProvider : public UnexportableKeyProvider {
 public:
  MockTrackingUnexportableKeyProvider()
      : key_provider_(GetSoftwareUnsecureUnexportableKeyProvider()) {}

  ~MockTrackingUnexportableKeyProvider() override {
    CHECK(keys_.empty()) << keys_.size() << " key(s) not deleted.";
  }

  // UnexportableKeyProvider:
  std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    return key_provider_->SelectAlgorithm(acceptable_algorithms);
  }
  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    std::unique_ptr<UnexportableSigningKey> key =
        key_provider_->GenerateSigningKeySlowly(acceptable_algorithms);
    if (key) {
      keys_.emplace(key->GetWrappedKey());
    }
    return key;
  }
  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    CHECK(keys_.contains(
        std::vector<uint8_t>(wrapped_key.begin(), wrapped_key.end())))
        << "Attempted to delete non existing key";
    return key_provider_->FromWrappedSigningKeySlowly(wrapped_key);
  }
  bool DeleteSigningKeySlowly(base::span<const uint8_t> wrapped_key) override {
    key_provider_->DeleteSigningKeySlowly(wrapped_key);
    return keys_.erase(
        std::vector<uint8_t>(wrapped_key.begin(), wrapped_key.end()));
  }

 private:
  std::unique_ptr<UnexportableKeyProvider> key_provider_;
  std::set<std::vector<uint8_t>> keys_;
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
  histogram_tester.ExpectTotalCount("Crypto.TPMDuration.MessageSigningECDSA",
                                    0);
  histogram_tester.ExpectTotalCount("Crypto.TPMOperation.NewKeyCreation", 0);
  histogram_tester.ExpectTotalCount("Crypto.TPMOperation.WrappedKeyCreation",
                                    0);
  histogram_tester.ExpectTotalCount("Crypto.TPMOperation.MessageSigning", 0);
  histogram_tester.ExpectTotalCount("Crypto.TPMOperation.MessageVerify", 0);

  internal::MeasureTpmOperationsInternalForTesting();

  EXPECT_THAT(histogram_tester.GetAllSamples("Crypto.TPMSupport2"),
              BucketsAre(base::Bucket(internal::TPMSupport::kECDSA, 1)));
  histogram_tester.ExpectTotalCount("Crypto.TPMDuration.NewKeyCreationECDSA",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Crypto.TPMDuration.WrappedKeyCreationECDSA", 1);
  histogram_tester.ExpectTotalCount("Crypto.TPMDuration.MessageSigningECDSA",
                                    1);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Crypto.TPMOperation.NewKeyCreationECDSA"),
      BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Crypto.TPMOperation.WrappedKeyCreationECDSA"),
              BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Crypto.TPMOperation.MessageSigningECDSA"),
      BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Crypto.TPMOperation.MessageVerifyECDSA"),
      BucketsAre(base::Bucket(true, 1)));
}

}  // namespace

}  // namespace crypto
