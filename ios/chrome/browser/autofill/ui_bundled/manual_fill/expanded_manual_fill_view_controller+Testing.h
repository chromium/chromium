// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_EXPANDED_MANUAL_FILL_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_EXPANDED_MANUAL_FILL_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/expanded_manual_fill_view_controller.h"

// Testing category exposing a private property of
// ExpandedManualFillViewController for tests.
@interface ExpandedManualFillViewController (Testing)

// Control allowing switching between the different data types.
@property(nonatomic, strong, readonly) UISegmentedControl* segmentedControl;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_EXPANDED_MANUAL_FILL_VIEW_CONTROLLER_TESTING_H_
