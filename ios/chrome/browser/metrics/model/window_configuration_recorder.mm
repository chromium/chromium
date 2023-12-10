// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/window_configuration_recorder.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/timer/timer.h"

@interface WindowConfigurationRecorder ()

// Called by the recording_timer_ to record current config.
- (void)recordConfiguration;

@end

namespace {

// Delay between a recording of a new configuration.
static constexpr base::TimeDelta kRecordDelay = base::Seconds(20);

// Timer callback for recording configuration after a delay.
void RecordWindowGeometryMetrics(WindowConfigurationRecorder* recorder) {
  [recorder recordConfiguration];
}

// Returns all Foreground active windows that are Chrome windows.
NSArray<UIWindow*>* ForegroundWindowsForApplication(
    UIApplication* application) {
  NSMutableArray<UIWindow*>* windows = [NSMutableArray arrayWithCapacity:3];

  for (UIScene* scene in application.connectedScenes) {
    if (scene.activationState != UISceneActivationStateForegroundActive)
      continue;

    UIWindowScene* windowScene = base::apple::ObjCCast<UIWindowScene>(scene);
    for (UIWindow* window in windowScene.windows) {
      // Skip other windows (like keyboard) that keep showing up.
      if (![window isKindOfClass:NSClassFromString(@"ChromeOverlayWindow")])
        continue;

      [windows addObject:window];
      break;  // Stop after one window per scene. This may be wrong.
    }
  }
  return [windows copy];
}
}  // namespace

@implementation WindowConfigurationRecorder {
  // Repeating delay timer.
  base::RepeatingTimer recording_timer_;
}

- (instancetype)init {
  if (self == [super init]) {
    // When the app becomes active, set recording on.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidBecomeActive)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];

    // When the app resigns active, turn recording off.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidEnterBackground)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];

    [self scheduleRecordConfiguration];
  }
  return self;
}

// Called when notified of UIApplicationDidBecomeActiveNotification.
- (void)applicationDidBecomeActive {
  self.recording = YES;
  [self scheduleRecordConfiguration];
}

// Called when notified of UIApplicationDidEnterBackgroundNotification
- (void)applicationDidEnterBackground {
  self.recording = NO;
  recording_timer_.Stop();
}

// Called to set or reset the timer.
- (void)scheduleRecordConfiguration {
  if (recording_timer_.IsRunning()) {
    recording_timer_.Reset();
  } else {
    recording_timer_.Start(
        FROM_HERE, kRecordDelay,
        base::BindRepeating(&RecordWindowGeometryMetrics, self));
  }
}

// Called when the timer actually fires.
- (void)recordConfiguration {
  [self recordGeometryForScreen:[UIScreen mainScreen]
                        windows:ForegroundWindowsForApplication(
                                    UIApplication.sharedApplication)];
}

// Computes configuration for given screen and windows and records it.
- (void)recordGeometryForScreen:(UIScreen*)screen
                        windows:(NSArray<UIWindow*>*)windows {
  WindowConfiguration configuration = [self configurationForScreen:screen
                                                           windows:windows];
  base::UmaHistogramEnumeration("IOS.MultiWindow.Configuration", configuration);
}

#pragma mark - Visible For Testing

- (WindowConfiguration)configurationForScreen:(UIScreen*)screen
                                      windows:(NSArray<UIWindow*>*)windows {
  NSMutableArray<UIWindow*>* fullscreenWindows = [[NSMutableArray alloc] init];
  NSMutableArray<UIWindow*>* slideoverWindows = [[NSMutableArray alloc] init];
  NSMutableArray<UIWindow*>* sharedWindows = [[NSMutableArray alloc] init];

  CGRect screenRect = screen.bounds;
  for (UIWindow* window in windows) {
    CGRect windowRect = window.frame;

    // Is the window full screen?
    if (CGRectEqualToRect(screenRect, windowRect)) {
      [fullscreenWindows addObject:window];
      continue;
    }
    // Is the window in slideover? Slideover windows are always both shorter
    // and narrower than the screen.
    if (screenRect.size.width > windowRect.size.width &&
        screenRect.size.height > windowRect.size.height) {
      [slideoverWindows addObject:window];
      continue;
    }

    // Otherwise, the window is shared. This shouldn't happen if there's
    // a fullscreen window.
    [sharedWindows addObject:window];
  }

  WindowConfiguration configuration = WindowConfiguration::kUnspecified;

  if (sharedWindows.count == 0) {
    if (fullscreenWindows.count > 0) {
      configuration = slideoverWindows.count > 0
                          ? WindowConfiguration::kFullscreenWithSlideover
                          : WindowConfiguration::kFullscreen;
    } else if (slideoverWindows.count > 0) {
      configuration = WindowConfiguration::kSlideoverOnly;
    } else {
      // Configuration remains unspecificed -- were there no windows?
    }
  } else if (sharedWindows.count == 1) {
    // Single Shared window cases.
    UIUserInterfaceSizeClass sharedWindowSize =
        sharedWindows[0].traitCollection.horizontalSizeClass;
    if (sharedWindowSize == UIUserInterfaceSizeClassRegular) {
      configuration = slideoverWindows.count > 0
                          ? WindowConfiguration::kSharedStandardWithSlideover
                          : WindowConfiguration::kSharedStandard;
    } else if (sharedWindowSize == UIUserInterfaceSizeClassCompact) {
      configuration = slideoverWindows.count > 0
                          ? WindowConfiguration::kSharedCompactWithSlideover
                          : WindowConfiguration::kSharedCompact;
    } else {
      // Configuration remains unspecified -- shared window has an unspecified
      // size class.
    }
  } else if (sharedWindows.count == 2) {
    UIUserInterfaceSizeClass firstWindowSize =
        sharedWindows[0].traitCollection.horizontalSizeClass;
    UIUserInterfaceSizeClass secondWindowSize =
        sharedWindows[1].traitCollection.horizontalSizeClass;

    if (firstWindowSize == UIUserInterfaceSizeClassRegular &&
        secondWindowSize == UIUserInterfaceSizeClassRegular) {
      configuration =
          slideoverWindows.count > 0
              ? WindowConfiguration::kStandardBesideStandardWithSlideover
              : WindowConfiguration::kStandardBesideStandard;
    } else if (firstWindowSize == UIUserInterfaceSizeClassCompact &&
               secondWindowSize == UIUserInterfaceSizeClassCompact) {
      configuration =
          slideoverWindows.count > 0
              ? WindowConfiguration::kCompactBesideCompactWithSlideover
              : WindowConfiguration::kCompactBesideCompact;
    } else if (firstWindowSize != UIUserInterfaceSizeClassUnspecified &&
               secondWindowSize != UIUserInterfaceSizeClassUnspecified) {
      // Since the sizes are neither both standard, nor both compact, nor is
      // either of them unspecified, one must be standard and the other compact.
      configuration =
          slideoverWindows.count > 0
              ? WindowConfiguration::kStandardBesideCompactWithSlideover
              : WindowConfiguration::kStandardBesideCompact;
    } else {
      // Configuration remains unspecified -- one of the two shared windows has
      // an unspecified size class.
    }
  } else {
    // Configuration remains unspecified -- more than two shared windows.
  }

  return configuration;
}

@end
