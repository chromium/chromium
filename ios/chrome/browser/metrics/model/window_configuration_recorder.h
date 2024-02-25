// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_WINDOW_CONFIGURATION_RECORDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_WINDOW_CONFIGURATION_RECORDER_H_

#import <UIKit/UIKit.h>

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WindowConfiguration {
  kUnspecified = 0,
  // Chrome is showing one window, fullscreen.
  kFullscreen,
  // Chrome is showing two windows, one fullscreen and one slide-over.
  kFullscreenWithSlideover,
  // Chrome is showing one standard-width window, and another app has
  // a window alongside it.
  kSharedStandard,
  // As above, and Chrome also has a slide-over window.
  kSharedStandardWithSlideover,
  // Chrome is showing one compact-width window, and another app has
  // a window alongside it.
  kSharedCompact,
  // As above, and Chrome also has a slide-over window.
  kSharedCompactWithSlideover,
  // Chrome has only one foreground window, and it's in slide-over mode.
  kSlideoverOnly,
  // Chrome has two windows, both standard-width
  kStandardBesideStandard,
  // As above, and Chrome also has a third window in  slide-over mode.
  kStandardBesideStandardWithSlideover,
  // Chrome has two windows, one standard-width and one compact-width.
  kStandardBesideCompact,
  // As above, and Chrome also has a third window in  slide-over mode.
  kStandardBesideCompactWithSlideover,
  // Chrome has two windows, both compact-width
  kCompactBesideCompact,
  // As above, and Chrome also has a third window in  slide-over mode.
  kCompactBesideCompactWithSlideover,
  // NOTE: add new configurations above this line.
  kMaxValue = kCompactBesideCompactWithSlideover
};

// Reports time spent for each MultiWindow configuration.
// It looks at the window configuration every minute, and increments the
// histogram bucket for that configuration. Suspends when app is
// backgrounded and restarts when foregrounded.
@interface WindowConfigurationRecorder : NSObject

// State of recording.
@property(nonatomic) BOOL recording;

@end

@interface WindowConfigurationRecorder (VisibleForTesting)

// Computes configuration from given screen and windows.
- (WindowConfiguration)configurationForScreen:(UIScreen*)screen
                                      windows:(NSArray<UIWindow*>*)windows;
@end

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_WINDOW_CONFIGURATION_RECORDER_H_
