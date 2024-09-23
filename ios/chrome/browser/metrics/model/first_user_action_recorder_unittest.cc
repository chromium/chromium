// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "ios/chrome/browser/metrics/model/first_user_action_recorder.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/device_form_factor.h"

using base::UserMetricsAction;

class FirstUserActionRecorderTest : public PlatformTest {
 protected:
  void SetUp() override {
    base::TimeDelta delta = base::Seconds(60);
    recorder_ = std::make_unique<FirstUserActionRecorder>(delta);

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    is_pad_ = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  }

  web::WebTaskEnvironment task_environment_;
  bool is_pad_;
  std::unique_ptr<FirstUserActionRecorder> recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(FirstUserActionRecorderTest, Expire) {
  recorder_->Expire();

  // Verify the first user action histogram contains the single expiration
  // value.
  histogram_tester_->ExpectUniqueSample(
      kFirstUserActionTypeHistogramName[is_pad_],
      FirstUserActionRecorder::EXPIRATION, 1);

  // Verify the expiration histogram contains a single duration value.
  // TODO(crbug.com/41211458): Ideally this would also verify the value is in
  // the correct bucket.
  histogram_tester_->ExpectTotalCount(
      kFirstUserActionExpirationHistogramName[is_pad_], 1);
}

TEST_F(FirstUserActionRecorderTest, RecordStartOnNTP) {
  recorder_->RecordStartOnNTP();

  // Verify the first user action histogram contains the single start on NTP
  // value.
  histogram_tester_->ExpectUniqueSample(
      kFirstUserActionTypeHistogramName[is_pad_],
      FirstUserActionRecorder::START_ON_NTP, 1);
}

TEST_F(FirstUserActionRecorderTest, OnUserAction_Continuation) {
  base::RecordAction(UserMetricsAction("MobileContextMenuOpenLink"));

  // Verify the first user action histogram contains the single continuation
  // value.
  histogram_tester_->ExpectUniqueSample(
      kFirstUserActionTypeHistogramName[is_pad_],
      FirstUserActionRecorder::CONTINUATION, 1);

  // Verify the continuation histogram contains a single duration value.
  // TODO(crbug.com/41211458): Ideally this would also verify the value is in
  // the correct bucket.
  histogram_tester_->ExpectTotalCount(
      kFirstUserActionContinuationHistogramName[is_pad_], 1);
}

TEST_F(FirstUserActionRecorderTest, OnUserAction_NewTask) {
  base::RecordAction(UserMetricsAction("MobileMenuNewTab"));

  // Verify the first user action histogram contains the single 'new task'
  // value.
  histogram_tester_->ExpectUniqueSample(
      kFirstUserActionTypeHistogramName[is_pad_],
      FirstUserActionRecorder::NEW_TASK, 1);

  // Verify the 'new task' histogram contains a single duration value.
  // TODO(crbug.com/41211458): Ideally this would also verify the value is in
  // the correct bucket.
  histogram_tester_->ExpectTotalCount(
      kFirstUserActionNewTaskHistogramName[is_pad_], 1);
}

TEST_F(FirstUserActionRecorderTest, OnUserAction_Ignored) {
  base::RecordAction(UserMetricsAction("MobileTabClosed"));

  // Verify the first user action histogram contains no values.
  histogram_tester_->ExpectTotalCount(
      kFirstUserActionTypeHistogramName[is_pad_], 0);

  // Verify the duration histograms contain no values.
  histogram_tester_->ExpectTotalCount(
      kFirstUserActionNewTaskHistogramName[is_pad_], 0);
  histogram_tester_->ExpectTotalCount(
      kFirstUserActionContinuationHistogramName[is_pad_], 0);
  histogram_tester_->ExpectTotalCount(
      kFirstUserActionExpirationHistogramName[is_pad_], 0);
}

TEST_F(FirstUserActionRecorderTest, OnUserAction_RethrowAction_Continuation) {
  base::RecordAction(UserMetricsAction("MobileTabSwitched"));
  base::RunLoop().RunUntilIdle();

  // Verify the first user action histogram contains the single continuation
  // value.
  histogram_tester_->ExpectUniqueSample(
      kFirstUserActionTypeHistogramName[is_pad_],
      FirstUserActionRecorder::CONTINUATION, 1);

  // Verify the continuation histogram contains a single duration value.
  // TODO(crbug.com/41211458): Ideally this would also verify the value is in
  // the correct bucket.
  histogram_tester_->ExpectTotalCount(
      kFirstUserActionContinuationHistogramName[is_pad_], 1);
}

TEST_F(FirstUserActionRecorderTest, OnUserAction_RethrowAction_NewTask) {
  base::RecordAction(UserMetricsAction("MobileTabSwitched"));
  base::RecordAction(UserMetricsAction("MobileTabStripNewTab"));

  // Verify the first user action histogram contains the single 'new task'
  // value.
  histogram_tester_->ExpectUniqueSample(
      kFirstUserActionTypeHistogramName[is_pad_],
      FirstUserActionRecorder::NEW_TASK, 1);

  // Verify the 'new task' histogram contains the a single duration value.
  // TODO(crbug.com/41211458): Ideally this would also verify the value is in
  // the correct bucket.
  histogram_tester_->ExpectTotalCount(
      kFirstUserActionNewTaskHistogramName[is_pad_], 1);
}
