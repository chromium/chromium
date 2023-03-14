// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_attestation_mediator_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/network/attribution/attribution_attestation_mediator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
using base::test::TaskEnvironment;
using GetHeadersStatus = AttributionAttestationMediator::GetHeadersStatus;
using ProcessAttestationStatus =
    AttributionAttestationMediator::ProcessAttestationStatus;
using Step = AttributionAttestationMediator::Step;

class AttributionAttestationMediatorMetricsRecorderTest : public testing::Test {
 protected:
  TaskEnvironment env_ =
      TaskEnvironment(TaskEnvironment::TimeSource::MOCK_TIME);
  AttributionAttestationMediatorMetricsRecorder recorder_;
  base::HistogramTester histograms_;
};

// Begin -> Complete(commitment) -> Fail(Commitment)
TEST_F(AttributionAttestationMediatorMetricsRecorderTest, FailGetCommitment) {
  recorder_.Start();

  env_.FastForwardBy(base::Seconds(1));
  recorder_.Complete(Step::kGetKeyCommitment);

  env_.FastForwardBy(base::Seconds(2));
  recorder_.FinishGetHeadersWith(GetHeadersStatus::kIssuerNotRegistered);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.GetKeyCommitment.Failure",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      GetHeadersStatus::kIssuerNotRegistered,
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.InitializeCryptographer.Failure",
      base::Seconds(2), /*expected_bucket_count=*/0);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Fail(initialize)
TEST_F(AttributionAttestationMediatorMetricsRecorderTest, FailCrypto) {
  recorder_.Start();

  env_.FastForwardBy(base::Seconds(1));
  recorder_.Complete(Step::kGetKeyCommitment);

  env_.FastForwardBy(base::Seconds(2));
  recorder_.Complete(Step::kInitializeCryptographer);

  env_.FastForwardBy(base::Seconds(3));
  recorder_.FinishGetHeadersWith(
      GetHeadersStatus::kUnableToInitializeCryptographer);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.InitializeCryptographer.Failure",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.BlindMessage.Failure",
      base::Seconds(3),
      /*expected_bucket_count=*/0);

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      GetHeadersStatus::kUnableToInitializeCryptographer,
      /*expected_bucket_count=*/1);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Complete(blind) ->
// Fail(blind)
TEST_F(AttributionAttestationMediatorMetricsRecorderTest, FailBlindMessage) {
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
      "Conversions.TriggerAttestation.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.InitializeCryptographer.Success",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.BlindMessage.Failure",
      base::Seconds(3),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.SignBlindMessage.Failure",
      base::Seconds(9),
      /*expected_bucket_count=*/0);

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      GetHeadersStatus::kUnableToBlindMessage,
      /*expected_bucket_count=*/1);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Complete(blind) ->
// Complete(sign) -> Fail(sign)
TEST_F(AttributionAttestationMediatorMetricsRecorderTest,
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
  recorder_.FinishProcessAttestationWith(
      ProcessAttestationStatus::kNoSignatureReceivedFromIssuer);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.InitializeCryptographer.Success",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.BlindMessage.Success",
      base::Seconds(3),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.SignBlindMessage.Failure",
      base::Seconds(9),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.UnblindSignature.Failure",
      base::Seconds(6),
      /*expected_bucket_count=*/0);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.Total.Failure",
      base::Seconds(1 + 2 + 3 + 4 + 5 + 6),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      GetHeadersStatus::kSuccess,
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.ProcessAttestationStatus",
      ProcessAttestationStatus::kNoSignatureReceivedFromIssuer,
      /*expected_bucket_count=*/1);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Complete(blind ->
// Complete(sign) -> Complete(unblind) -> Fail(unblind)
TEST_F(AttributionAttestationMediatorMetricsRecorderTest,
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
  recorder_.FinishProcessAttestationWith(
      ProcessAttestationStatus::kUnableToUnblindSignature);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.InitializeCryptographer.Success",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.BlindMessage.Success",
      base::Seconds(3),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.SignBlindMessage.Success",
      base::Seconds(9),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.UnblindSignature.Failure",
      base::Seconds(6),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.Total.Failure",
      base::Seconds(1 + 2 + 3 + 4 + 5 + 6 + 7),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      GetHeadersStatus::kSuccess,
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.ProcessAttestationStatus",
      ProcessAttestationStatus::kUnableToUnblindSignature,
      /*expected_bucket_count=*/1);
}

// Begin -> Complete(commitment) -> Complete(initialize) -> Complete(blind) ->
// Complete(sign) -> Complete(unblind) -> Success()
TEST_F(AttributionAttestationMediatorMetricsRecorderTest, Success) {
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
  recorder_.FinishProcessAttestationWith(ProcessAttestationStatus::kSuccess);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.GetKeyCommitment.Success",
      base::Seconds(1), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.InitializeCryptographer.Success",
      base::Seconds(2), /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.BlindMessage.Success",
      base::Seconds(3),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.SignBlindMessage.Success",
      base::Seconds(9),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.UnblindSignature.Success",
      base::Seconds(6),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueTimeSample(
      "Conversions.TriggerAttestation.Duration.Total.Success",
      base::Seconds(1 + 2 + 3 + 4 + 5 + 6 + 6 + 7),
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.GetHeadersStatus",
      GetHeadersStatus::kSuccess,
      /*expected_bucket_count=*/1);

  histograms_.ExpectUniqueSample(
      "Conversions.TriggerAttestation.ProcessAttestationStatus",
      ProcessAttestationStatus::kSuccess,
      /*expected_bucket_count=*/1);
}
}  // namespace network
