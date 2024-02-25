// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/model/screenshot_metrics_recorder.h"

#import <UIKit/UIKit.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"

namespace {
// UMA metric name for single screen screenshot captures.
char const* kSingleScreenUserActionName = "MobileSingleScreenScreenshot";
}  // namespace

@implementation ScreenshotMetricsRecorder

#pragma mark - Public

- (void)startRecordingMetrics {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(collectMetricFromNotification:)
             name:@"UIApplicationUserDidTakeScreenshotNotification"
           object:nil];
}

#pragma mark - Private

- (void)collectMetricFromNotification:(NSNotification*)notification {
  // If the device does not support multiple scenes or if the iOS version is
  // bellow iOS13 it will record the SingleScreenUserActionName metric.
  // Otherwise it will record the SingleScreenUserActionName metric only if
  // there is a single window in the foreground.
  UIApplication* sharedApplication = [UIApplication sharedApplication];
  NSInteger countForegroundScenes = 1;
  if (sharedApplication.supportsMultipleScenes) {
    countForegroundScenes =
        [self countForegroundScenes:[sharedApplication connectedScenes]];
  }

  // Only register screenshots taken of chrome in a single screen in the
  // foreground.
  if (countForegroundScenes == 1) {
    base::RecordAction(base::UserMetricsAction(kSingleScreenUserActionName));
  }
}

#pragma mark - Private

- (NSInteger)countForegroundScenes:(NSSet<UIScene*>*)scenes
    NS_AVAILABLE_IOS(13.0) {
  // Inspect the connectScenes and return the number of scenes
  // in the foreground.
  NSInteger foregroundSceneCount = 0;
  for (UIScene* scene in scenes) {
    switch (scene.activationState) {
      case UISceneActivationStateForegroundInactive:
      case UISceneActivationStateForegroundActive:
        foregroundSceneCount++;
        break;
      default:
        // Catch for UISceneActivationStateUnattached
        // and UISceneActivationStateBackground.
        // TODO (crbug.com/1091818): Add state inpection to identify other
        // scenarios.
        break;
    }
  }
  return foregroundSceneCount;
}
@end
