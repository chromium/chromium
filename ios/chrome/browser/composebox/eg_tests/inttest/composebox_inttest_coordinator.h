// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_INTTEST_COMPOSEBOX_INTTEST_COORDINATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_INTTEST_COMPOSEBOX_INTTEST_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator that presents composebox UI elements for integration tests.
// While the software keyboard is shown, an opaque view covers the keyboard
// area so that EarlGrey visibility checks consider views covered by the
// keyboard as not visible.
@interface ComposeboxInttestCoordinator : ChromeCoordinator
@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_INTTEST_COMPOSEBOX_INTTEST_COORDINATOR_H_
