// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_COORDINATOR_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_COORDINATOR_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_H_

#import "ios/chrome/browser/autofill/manual_fill/coordinator/fallback_coordinator.h"

@protocol ManualFillAllPasswordCoordinatorDelegate;

// Creates and manages a view controller to present all the passwords to the
// user. The view controller contains a search bar. The presentation is done
// with a table view presentation controller. Any selected password will be sent
// to the current field in the active web state.
@interface ManualFillAllPasswordCoordinator : FallbackCoordinator

@property(nonatomic, weak) id<ManualFillAllPasswordCoordinatorDelegate>
    manualFillAllPasswordCoordinatorDelegate;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_COORDINATOR_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_H_
