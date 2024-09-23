// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_COORDINATOR_H_

#import <Foundation/Foundation.h>
#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_delegate.h"
#import "ios/web/public/web_state.h"

// This coordinator is shows a prompt when the user is prompted to enroll a
// virtual card. The AutofillBottomSheetTabHelper is expected to be in WebState
// and provide VirtualCardEnrollmentCallbacks.
@interface VirtualCardEnrollmentBottomSheetCoordinator
    : ChromeCoordinator <VirtualCardEnrollmentBottomSheetDelegate>

// Initialize this Coordinator with the model.
- (instancetype)initWithUIModel:
                    (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model
             baseViewController:(UIViewController*)baseViewController
                        browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_COORDINATOR_H_
