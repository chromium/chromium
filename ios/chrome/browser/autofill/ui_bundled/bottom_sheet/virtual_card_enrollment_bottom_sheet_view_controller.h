// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_mutator.h"

// A bottom sheet view controller for the virtual card enrollment prompt.
@interface VirtualCardEnrollmentBottomSheetViewController
    : BottomSheetViewController <VirtualCardEnrollmentBottomSheetConsumer>

// Link opening is delegated.
@property(nonatomic, weak) id<VirtualCardEnrollmentBottomSheetDelegate>
    delegate;

// User actions are delagated to this mutator.
@property(nonatomic, weak) id<VirtualCardEnrollmentBottomSheetMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_VIEW_CONTROLLER_H_
