// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_CHROME_COORDINATOR_FULLSCREEN_DISABLING_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_CHROME_COORDINATOR_FULLSCREEN_DISABLING_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// A ChromeCoordinator category that handles disabling fullscreen calculations
// while a coordinator has been started.
@interface ChromeCoordinator (FullscreenDisabling)

// Increments and decrements the fullscreen disable counter for the
// FullscreenController associated with this coordinator's ProfileIOS.
- (void)didStartFullscreenDisablingUI;
- (void)didStopFullscreenDisablingUI;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_CHROME_COORDINATOR_FULLSCREEN_DISABLING_H_
