// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FULLSCREEN_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FULLSCREEN_COMMANDS_H_

#import <Foundation/Foundation.h>

enum class FullscreenModeTransitionTrigger;

// Features that can force fullscreen mode.
enum class ForceFullscreenFeature {
  // Lower boundary for base::EnumSet.
  kMinValue = 0,
  kHideToolbars = kMinValue,
  kFindInPage,
  kLensOverlay,
  kAssistant,
  // Upper boundary for base::EnumSet. Must be updated when adding new features.
  kMaxValue = kAssistant,
};

// Protocol for commands that control the fullscreen state.
@protocol FullscreenCommands

// Enters fullscreen mode.
- (void)enterFullscreenWithTrigger:(FullscreenModeTransitionTrigger)trigger
                          animated:(BOOL)animated;

// Exits fullscreen mode.
- (void)exitFullscreenWithTrigger:(FullscreenModeTransitionTrigger)trigger
                         animated:(BOOL)animated;

// Disables fullscreen. Increments the disabled counter.
- (void)disableFullscreenAnimated:(BOOL)animated;

// Re-enables fullscreen. Decrements the disabled counter.
- (void)reenableFullscreen;

// Forces fullscreen mode for `feature` when `enable` is YES, or removes
// `feature` from the set of features forcing fullscreen when `enable` is NO.
- (void)forceFullscreen:(BOOL)enable feature:(ForceFullscreenFeature)feature;

// Exits forced fullscreen mode for all features immediately.
- (void)exitForceFullscreen;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FULLSCREEN_COMMANDS_H_
