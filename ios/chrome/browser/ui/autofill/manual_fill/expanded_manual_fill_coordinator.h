// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace manual_fill {
enum class ManualFillDataType;
}

// The coordinator responsible for presenting the expanded manual fill view.
@interface ExpandedManualFillCoordinator : ChromeCoordinator

// Designated initializer. `dataType` represents the type of manual filling
// options to show in the expanded manual fill view.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               forDataType:
                                   (manual_fill::ManualFillDataType)dataType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Returns the coordinator's view controller.
- (UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_COORDINATOR_H_
