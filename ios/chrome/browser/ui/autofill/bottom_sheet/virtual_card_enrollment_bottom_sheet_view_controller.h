// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_consumer.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_delegate.h"

#import <UIKit/UIKit.h>

// A bottom sheet view controller for the virtual card enrollment prompt.
@interface VirtualCardEnrollmentBottomSheetViewController
    : BottomSheetViewController <VirtualCardEnrollmentBottomSheetConsumer>

// Button actions and link opening are delegated.
@property(nonatomic, weak) id<VirtualCardEnrollmentBottomSheetDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_VIEW_CONTROLLER_H_
