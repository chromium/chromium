// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/virtual_card_enrollment_callbacks.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@implementation VirtualCardEnrollmentBottomSheetCoordinator {
  autofill::VirtualCardEnrollUiModel model_;
  autofill::VirtualCardEnrollmentCallbacks callbacks_;
}

- (instancetype)initWithUIModel:(autofill::VirtualCardEnrollUiModel)model
             baseViewController:(UIViewController*)baseViewController
                        browser:(Browser*)browser {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    web::WebState* activeWebState =
        self.browser->GetWebStateList()->GetActiveWebState();
    self->model_ = model;
    self->callbacks_ =
        AutofillBottomSheetTabHelper::FromWebState(activeWebState)
            ->GetVirtualCardEnrollmentCallbacks();
  }
  return self;
}

- (void)start {
  // TODO(crbug.com/1485376): Implement the virtual card enrollment
  // bottom sheet view.
}

- (void)stop {
  // TODO(crbug.com/1485376): Implement the virtual card enrollment
  // bottom sheet view.
}

@end
