// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// The coordinator responsible for presenting the expanded manual fill view.
@interface ExpandedManualFillCoordinator : ChromeCoordinator

// Returns the coordinator's view controller.
- (UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_
