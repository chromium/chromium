// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_COORDINATOR_H_

#import <Foundation/Foundation.h>
#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/web/public/web_state.h"

@interface VirtualCardEnrollmentBottomSheetCoordinator : ChromeCoordinator

- (instancetype)initWithUIModel:(autofill::VirtualCardEnrollUiModel)model
             baseViewController:(UIViewController*)baseViewController
                        browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_COORDINATOR_H_
