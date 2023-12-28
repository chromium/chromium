// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// A ConfirmationAlertViewController with the bottom sheet presentation style.
//
// The presentationController will be automatically set up on viewDidLoad, and
// changes handled in traitCollectionDidChange:.
//
// Subclasses must call expandBottomSheet when the bottom sheet needs to be
// resized.
@interface BottomSheetViewController : ConfirmationAlertViewController

// Sets detents based on the result of self.preferredHeightForContent.
- (void)expandBottomSheet;

// Configures the bottom sheet's presentation controller appearance.
- (void)setUpBottomSheetPresentationController;

// Configures the bottom sheet's detents. Subclasses that affect
// layout (by adding constraints) after calling [super viewDidLoad] are expected
// to call this in viewDidLoad.
- (void)setUpBottomSheetDetents;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_BOTTOM_SHEET_VIEW_CONTROLLER_H_
