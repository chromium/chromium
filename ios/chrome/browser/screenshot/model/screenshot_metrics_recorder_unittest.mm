// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/model/screenshot_metrics_recorder.h"

#import <UIKit/UIKit.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/test/metrics/user_action_tester.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
// UMA metric name for single screen screenshot captures.
char const* kSingleScreenUserActionName = "MobileSingleScreenScreenshot";
}  // namespace

class ScreenshotMetricsRecorderTest : public PlatformTest {
 protected:
  ScreenshotMetricsRecorderTest() {}
  ~ScreenshotMetricsRecorderTest() override {}

  void SetUp() override {
    application_partial_mock_ =
        OCMPartialMock([UIApplication sharedApplication]);
    screenshot_metrics_recorder_ = [[ScreenshotMetricsRecorder alloc] init];
    [screenshot_metrics_recorder_ startRecordingMetrics];
    window_scene_mock_ = OCMClassMock([UIWindowScene class]);
  }

  void SendScreenshotNotification() {
    [NSNotificationCenter.defaultCenter
        postNotificationName:@"UIApplicationUserDidTakeScreenshotNotification"
                      object:nil
                    userInfo:nil];
  }

  id application_partial_mock_;
  id window_scene_mock_;
  base::UserActionTester user_action_tester_;
  ScreenshotMetricsRecorder* screenshot_metrics_recorder_;
};

// Tests when a UIApplicationUserDidTakeScreenshotNotification
// happens on a device where multiple windows are not enabled.
TEST_F(ScreenshotMetricsRecorderTest, iOS13MultiWindowNotEnabled) {
  // Expected: Metric recorded
  OCMStub([application_partial_mock_ supportsMultipleScenes]).andReturn(NO);
  SendScreenshotNotification();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kSingleScreenUserActionName));
}

// Tests that a metric is logged if there's a single screen with a single window
// and a UIApplicationUserDidTakeScreenshotNotification is sent.
TEST_F(ScreenshotMetricsRecorderTest, iOS13SingleScreenSingleWindow) {
  // Expected: Metric recorded
  OCMStub([application_partial_mock_ supportsMultipleScenes]).andReturn(YES);

  id window_scene_mock_ = OCMClassMock([UIWindowScene class]);
  // Mark the window as foregroundActive
  OCMStub([window_scene_mock_ activationState])
      .andReturn(UISceneActivationStateForegroundActive);

  NSSet* scenes = [NSSet setWithObject:window_scene_mock_];
  // Attach it to the sharedApplication
  OCMStub([application_partial_mock_ connectedScenes]).andReturn(scenes);
  SendScreenshotNotification();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kSingleScreenUserActionName));
}

// Tests that a metric is logged if there're are multiple screens each with
// a single window and a UIApplicationUserDidTakeScreenshotNotification is
// sent.
TEST_F(ScreenshotMetricsRecorderTest, iOS13MultiScreenSingleWindow) {
  // Expected: Metric recorded
  OCMStub([application_partial_mock_ supportsMultipleScenes]).andReturn(YES);

  // Mark the window as foregroundActive.
  id foreground_window_scene_mock = OCMClassMock([UIWindowScene class]);
  OCMStub([window_scene_mock_ activationState])
      .andReturn(UISceneActivationStateForegroundActive);

  // Mark the window as Background.
  id background_window_scene_mock = OCMClassMock([UIWindowScene class]);
  OCMStub([background_window_scene_mock activationState])
      .andReturn(UISceneActivationStateBackground);

  NSSet* scenes =
      [[NSSet alloc] initWithObjects:foreground_window_scene_mock,
                                     background_window_scene_mock, nil];
  // Attatch the Scene State to the sharedApplication.
  OCMStub([application_partial_mock_ connectedScenes]).andReturn(scenes);
  SendScreenshotNotification();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kSingleScreenUserActionName));
}

// Tests that a metric is not logged if there is a multi-window screen in the
// foreground and a UIApplicationUserDidTakeScreenshotNotification is sent.
TEST_F(ScreenshotMetricsRecorderTest, iOS13MultiScreenMultiWindow) {
  // Expected: Metric not recorded
  OCMStub([application_partial_mock_ supportsMultipleScenes]).andReturn(YES);

  // Mark the window as foregroundActive.
  id first_foreground_window_scene_mock = OCMClassMock([UIWindowScene class]);
  OCMStub([window_scene_mock_ activationState])
      .andReturn(UISceneActivationStateForegroundActive);

  // Mark the window as foregroundActive.
  id second_foreground_window_scene_mock = OCMClassMock([UIWindowScene class]);
  OCMStub([second_foreground_window_scene_mock activationState])
      .andReturn(UISceneActivationStateForegroundActive);

  NSSet* scenes =
      [[NSSet alloc] initWithObjects:first_foreground_window_scene_mock,
                                     second_foreground_window_scene_mock, nil];
  // Attatch the Scene State to the sharedApplication.
  OCMStub([application_partial_mock_ connectedScenes]).andReturn(scenes);
  SendScreenshotNotification();
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kSingleScreenUserActionName));
}
