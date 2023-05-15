// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_verification_mediator_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/network/attribution/attribution_verification_mediator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
using base::test::TaskEnvironment;
using GetHeadersStatus = AttributionVerificationMediator::GetHeadersStatus;
using ProcessVerificationStatus =
    AttributionVerificationMediator::ProcessVerificationStatus;
using Step = AttributionVerificationMediator::Step;

class AttributionVerificationMediatorMetricsRecorderTest
    : public testing::Test {
 protected:
  TaskEnvironment env_ =
      TaskEnvironment(TaskEnvironment::TimeSource::MOCK_TIME);
  AttributionVerificationMediatorMetricsRecorder recorder_;
  base::HistogramTester histograms_;
};

// Begin -> Complete(commitment) -> Fail(Commitment)
TEST_F(AttributionVerificationMediatorMetricsRecorderTest, FailGetCommitment) {
  recorder_.Start();

  env_.FastForwardBy(base::Seconds(1));
  recorder_.Complete(Step::kGetKeyCommitment);

  env_.FastForwardBy(base::Seconds(2));
  recorder_.FinishGetHeadersWith(GetHeadersStatus::kIssuerNotRegistered);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.GetKeyCommitment.Failure",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      GetHeadersStatus::kIssuerNotRegistered,
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.InitializeCryptographer.Failure",
      base::Seconds(2), /*expected_bucket_count=*/0);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Fail(initialize)
TEST_F(AttributionVerificationMediatorMetricsRecorderTest, FailCrypto) {
  recorder_.Start();

  env_.FastForwardBy(base::Seconds(1));
  recorder_.Complete(Step::kGetKeyCommitment);

  env_.FastForwardBy(base::Seconds(2));
  recorder_.Complete(Step::kInitializeCryptographer);

  env_.FastForwardBy(base::Seconds(3));
  recorder_.FinishGetHeadersWith(
      GetHeadersStatus::kUnableToInitializeCryptographer);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.InitializeCryptographer.Failure",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.BlindMessage.Failure",
      base::Seconds(3),
      /*expected_bucket_count=*/0);

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      GetHeadersStatus::kUnableToInitializeCryptographer,
      /*expected_bucket_count=*/1);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Complete(blind) ->
// Fail(blind)
TEST_F(AttributionVerificationMediatorMetricsRecorderTest, FailBlindMessage) {
  recorder_.Start();

  env_.FastForwardBy(base::Seconds(1));
  recorder_.Complete(Step::kGetKeyCommitment);

  env_.FastForwardBy(base::Seconds(2));
  recorder_.Complete(Step::kInitializeCryptographer);

  env_.FastForwardBy(base::Seconds(3));
  recorder_.Complete(Step::kBlindMessage);

  env_.FastForwardBy(base::Seconds(4));
  recorder_.FinishGetHeadersWith(GetHeadersStatus::kUnableToBlindMessage);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.InitializeCryptographer.Success",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.BlindMessage.Failure",
      base::Seconds(3),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.SignBlindMessage.Failure",
      base::Seconds(9),
      /*expected_bucket_count=*/0);

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      GetHeadersStatus::kUnableToBlindMessage,
      /*expected_bucket_count=*/1);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Complete(blind) ->
// Complete(sign) -> Fail(sign)
TEST_F(AttributionVerificationMediatorMetricsRecorderTest,
       FailSignBlindMessage) {
  recorder_.Start();

  env_.FastForwardBy(base::Seconds(1));
  recorder_.Complete(Step::kGetKeyCommitment);

  env_.FastForwardBy(base::Seconds(2));
  recorder_.Complete(Step::kInitializeCryptographer);

  env_.FastForwardBy(base::Seconds(3));
  recorder_.Complete(Step::kBlindMessage);

  env_.FastForwardBy(base::Seconds(4));
  recorder_.FinishGetHeadersWith(GetHeadersStatus::kSuccess);

  env_.FastForwardBy(base::Seconds(5));
  recorder_.Complete(Step::kSignBlindMessage);

  env_.FastForwardBy(base::Seconds(6));
  recorder_.FinishProcessVerificationWith(
      ProcessVerificationStatus::kNoSignatureReceivedFromIssuer);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.InitializeCryptographer.Success",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.BlindMessage.Success",
      base::Seconds(3),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.SignBlindMessage.Failure",
      base::Seconds(9),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.UnblindSignature.Failure",
      base::Seconds(6),
      /*expected_bucket_count=*/0);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.Total.Failure",
      base::Seconds(1 + 2 + 3 + 4 + 5 + 6),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      GetHeadersStatus::kSuccess,
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.ProcessVerificationStatus",
      ProcessVerificationStatus::kNoSignatureReceivedFromIssuer,
      /*expected_bucket_count=*/1);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Complete(blind ->
// Complete(sign) -> Complete(unblind) -> Fail(unblind)
TEST_F(AttributionVerificationMediatorMetricsRecorderTest,
       FailUnblindSignature) {
  recorder_.Start();

  env_.FastForwardBy(base::Seconds(1));
  recorder_.Complete(Step::kGetKeyCommitment);

  env_.FastForwardBy(base::Seconds(2));
  recorder_.Complete(Step::kInitializeCryptographer);

  env_.FastForwardBy(base::Seconds(3));
  recorder_.Complete(Step::kBlindMessage);

  env_.FastForwardBy(base::Seconds(4));
  recorder_.FinishGetHeadersWith(GetHeadersStatus::kSuccess);

  env_.FastForwardBy(base::Seconds(5));
  recorder_.Complete(Step::kSignBlindMessage);

  env_.FastForwardBy(base::Seconds(6));
  recorder_.Complete(Step::kUnblindMessage);

  env_.FastForwardBy(base::Seconds(7));
  recorder_.FinishProcessVerificationWith(
      ProcessVerificationStatus::kUnableToUnblindSignature);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.InitializeCryptographer.Success",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.BlindMessage.Success",
      base::Seconds(3),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.SignBlindMessage.Success",
      base::Seconds(9),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.UnblindSignature.Failure",
      base::Seconds(6),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.Total.Failure",
      base::Seconds(1 + 2 + 3 + 4 + 5 + 6 + 7),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      GetHeadersStatus::kSuccess,
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.ProcessVerificationStatus",
      ProcessVerificationStatus::kUnableToUnblindSignature,
      /*expected_bucket_count=*/1);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Complete(blind) ->
// Complete(sign) -> Complete(unblind) -> Success()
TEST_F(AttributionVerificationMediatorMetricsRecorderTest, Success) {
  recorder_.Start();

  env_.FastForwardBy(base::Seconds(1));
  recorder_.Complete(Step::kGetKeyCommitment);

  env_.FastForwardBy(base::Seconds(2));
  recorder_.Complete(Step::kInitializeCryptographer);

  env_.FastForwardBy(base::Seconds(3));
  recorder_.Complete(Step::kBlindMessage);

  env_.FastForwardBy(base::Seconds(4));
  recorder_.FinishGetHeadersWith(GetHeadersStatus::kSuccess);

  env_.FastForwardBy(base::Seconds(5));
  recorder_.Complete(Step::kSignBlindMessage);

  env_.FastForwardBy(base::Seconds(6));
  recorder_.Complete(Step::kUnblindMessage);

  env_.FastForwardBy(base::Seconds(7));
  recorder_.FinishProcessVerificationWith(ProcessVerificationStatus::kSuccess);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.InitializeCryptographer.Success",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.BlindMessage.Success",
      base::Seconds(3),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.SignBlindMessage.Success",
      base::Seconds(9),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.UnblindSignature.Success",
      base::Seconds(6),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.ReportVerification.Duration.Total.Success",
      base::Seconds(1 + 2 + 3 + 4 + 5 + 6 + 6 + 7),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.GetHeadersStatus",
      GetHeadersStatus::kSuccess,
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.ReportVerification.ProcessVerificationStatus",
      ProcessVerificationStatus::kSuccess,
      /*expected_bucket_count=*/1);
}
}  // namespace network
