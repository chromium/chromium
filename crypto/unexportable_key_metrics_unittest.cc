// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Note mock provider only supports ECDSA
TEST(UnexportableKeyMetricTest, GatherAllMetrics) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider;

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

  crypto::internal::MeasureTpmOperationsInternalForTesting();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Crypto.TPMSupport2"),
      BucketsAre(base::Bucket(crypto::internal::TPMSupport::kECDSA, 1)));
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
