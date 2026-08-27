// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_private_inference_notice_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class AutofillAIPrivateInferenceNoticeMediatorTest : public PlatformTest {
 protected:
  AutofillAIPrivateInferenceNoticeMediatorTest() {
    pref_service_.registry()->RegisterTimePref(
        autofill::prefs::kAutofillAiPrivateInferenceNoticeShownTimestamp,
        base::Time());
    pref_service_.registry()->RegisterTimePref(
        autofill::prefs::kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp,
        base::Time());
  }

  void SetUp() override {
    PlatformTest::SetUp();
    mock_autofill_commands_ =
        OCMStrictProtocolMock(@protocol(AutofillCommands));
    mock_settings_commands_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
  }

  TestingPrefServiceSimple pref_service_;
  id mock_autofill_commands_;
  id mock_settings_commands_;
};

// Tests that markNoticeShown updates the shown timestamp pref and logs the
// "Shown" UMA metric.
TEST_F(AutofillAIPrivateInferenceNoticeMediatorTest,
       TestMarkNoticeShownUpdatesShownTimestampAndLogsMetric) {
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(
      pref_service_
          .GetTime(
              autofill::prefs::kAutofillAiPrivateInferenceNoticeShownTimestamp)
          .is_null());

  AutofillAIPrivateInferenceNoticeMediator* mediator =
      [[AutofillAIPrivateInferenceNoticeMediator alloc]
          initWithPrefService:&pref_service_
              autofillHandler:mock_autofill_commands_
              settingsHandler:mock_settings_commands_];
  EXPECT_NE(mediator, nil);

  // Initializing should not modify prefs or log metrics.
  EXPECT_TRUE(
      pref_service_
          .GetTime(
              autofill::prefs::kAutofillAiPrivateInferenceNoticeShownTimestamp)
          .is_null());
  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 0);

  [mediator markNoticeShown];

  EXPECT_FALSE(
      pref_service_
          .GetTime(
              autofill::prefs::kAutofillAiPrivateInferenceNoticeShownTimestamp)
          .is_null());
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kShown, 1);
}

// Tests that acknowledging the notice updates the timestamp, logs the
// "Acknowledged" UMA metric, and dismisses the notice. Repeated calls are
// ignored.
TEST_F(AutofillAIPrivateInferenceNoticeMediatorTest,
       TestAcknowledgeNoticeUpdatesPrefsAndLogsMetric) {
  base::HistogramTester histogram_tester;
  AutofillAIPrivateInferenceNoticeMediator* mediator =
      [[AutofillAIPrivateInferenceNoticeMediator alloc]
          initWithPrefService:&pref_service_
              autofillHandler:mock_autofill_commands_
              settingsHandler:mock_settings_commands_];

  [mediator markNoticeShown];

  EXPECT_TRUE(
      pref_service_
          .GetTime(autofill::prefs::
                       kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp)
          .is_null());

  OCMExpect([mock_autofill_commands_ dismissAutofillAIPrivateInferenceNotice]);

  [mediator didAcknowledgeNotice];

  EXPECT_FALSE(
      pref_service_
          .GetTime(autofill::prefs::
                       kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp)
          .is_null());

  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kAcknowledged, 1);

  // Simulate sheet teardown dismissal and repeated acknowledge calls.
  [mediator didDismissNotice];
  [mediator didAcknowledgeNotice];

  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kDismissed, 0);

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_);
}

// Tests that clicking the settings action updates the acknowledged timestamp,
// logs the "LinkButtonClicked" metric, opens settings, and dismisses the
// notice. Repeated calls are ignored.
TEST_F(AutofillAIPrivateInferenceNoticeMediatorTest,
       TestTapSettingsOpensSettingsAndUpdatesPrefsAndLogsMetric) {
  base::HistogramTester histogram_tester;
  AutofillAIPrivateInferenceNoticeMediator* mediator =
      [[AutofillAIPrivateInferenceNoticeMediator alloc]
          initWithPrefService:&pref_service_
              autofillHandler:mock_autofill_commands_
              settingsHandler:mock_settings_commands_];

  [mediator markNoticeShown];

  EXPECT_TRUE(
      pref_service_
          .GetTime(autofill::prefs::
                       kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp)
          .is_null());

  OCMExpect([mock_settings_commands_ showAutofillSettingsFromNotice]);

  [mediator didTapSettings];

  EXPECT_FALSE(
      pref_service_
          .GetTime(autofill::prefs::
                       kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp)
          .is_null());

  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kLinkButtonClicked,
      1);

  // Simulate sheet teardown dismissal and repeated settings calls.
  [mediator didDismissNotice];
  [mediator didTapSettings];

  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kDismissed, 0);

  EXPECT_OCMOCK_VERIFY(mock_settings_commands_);
}

// Tests that dismissing the notice calls dismiss on the handler, logs the
// "Dismissed" metric, and does not set the acknowledged timestamp. Repeated
// dismiss calls are ignored.
TEST_F(AutofillAIPrivateInferenceNoticeMediatorTest,
       TestDismissNoticeDismissesSheetAndLogsMetric) {
  base::HistogramTester histogram_tester;
  AutofillAIPrivateInferenceNoticeMediator* mediator =
      [[AutofillAIPrivateInferenceNoticeMediator alloc]
          initWithPrefService:&pref_service_
              autofillHandler:mock_autofill_commands_
              settingsHandler:mock_settings_commands_];

  [mediator markNoticeShown];

  EXPECT_TRUE(
      pref_service_
          .GetTime(autofill::prefs::
                       kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp)
          .is_null());

  OCMExpect([mock_autofill_commands_ dismissAutofillAIPrivateInferenceNotice]);

  [mediator didDismissNotice];

  EXPECT_TRUE(
      pref_service_
          .GetTime(autofill::prefs::
                       kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp)
          .is_null());

  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      autofill::AutofillMetrics::PopupNoticeInteractions::kDismissed, 1);

  // Repeated dismiss call should be a no-op.
  [mediator didDismissNotice];

  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_);
}

}  // namespace
