// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/forms_ai_private_inference_infobar_delegate_ios.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class FormsAiPrivateInferenceInfoBarDelegateIOSTest : public PlatformTest {
 protected:
  FormsAiPrivateInferenceInfoBarDelegateIOSTest() {
    pref_service_.registry()->RegisterTimePref(
        autofill::prefs::kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp,
        base::Time());
  }

  TestingPrefServiceSimple pref_service_;
};

// Tests that creating the delegate logs the "Shown" UMA metric.
TEST_F(FormsAiPrivateInferenceInfoBarDelegateIOSTest, LogsShownMetric) {
  base::HistogramTester histogram_tester;

  FormsAiPrivateInferenceInfoBarDelegateIOS delegate(&pref_service_);

  // Expect kShown interaction logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 0, 1);
}

// Tests that accepting the infobar updates the timestamp and logs the
// "Acknowledged" UMA metric.
TEST_F(FormsAiPrivateInferenceInfoBarDelegateIOSTest,
       AcceptUpdatesPrefsAndLogsMetric) {
  base::HistogramTester histogram_tester;
  FormsAiPrivateInferenceInfoBarDelegateIOS delegate(&pref_service_);

  EXPECT_TRUE(
      pref_service_
          .GetTime(autofill::prefs::
                       kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp)
          .is_null());

  EXPECT_TRUE(delegate.Accept());

  EXPECT_FALSE(
      pref_service_
          .GetTime(autofill::prefs::
                       kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp)
          .is_null());

  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  // Expect kShown.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 0, 1);
  // Expect kAcknowledged.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 1, 1);

  // Simulate banner dismissal which follows Accept.
  delegate.InfoBarDismissed();

  // Total count should still be 2 (no additional kDismissed logged).
  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  // Expect kDismissed is NOT logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2, 0);
}

// Tests that dismissing the infobar logs the "Dismissed" UMA metric.
TEST_F(FormsAiPrivateInferenceInfoBarDelegateIOSTest, LogsDismissedMetric) {
  base::HistogramTester histogram_tester;
  FormsAiPrivateInferenceInfoBarDelegateIOS delegate(&pref_service_);

  delegate.InfoBarDismissed();

  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  // Expect kShown.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 0, 1);
  // Expect kDismissed.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2, 1);
}

// Tests that clicking the settings link logs the "LinkButtonClicked" metric and
// blocks "Dismissed" from being logged afterwards.
TEST_F(FormsAiPrivateInferenceInfoBarDelegateIOSTest,
       LogsSettingsLinkClickedAndPreventsDoubleLogging) {
  base::HistogramTester histogram_tester;
  FormsAiPrivateInferenceInfoBarDelegateIOS delegate(&pref_service_);

  delegate.OnSettingsLinkClicked();

  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  // Expect kShown.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 0, 1);
  // Expect kLinkButtonClicked.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 3, 1);

  // Simulate banner dismissal which follows link click.
  delegate.InfoBarDismissed();

  // Total count should still be 2 (no additional kDismissed logged).
  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
  // Expect kDismissed is NOT logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2, 0);
}

}  // namespace
