// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_coordinator.h"

#import <memory>

#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

// TODO(crbug.com/391366601): Implement SaveCardBottomSheetCoordinator.
@implementation SaveCardBottomSheetCoordinator {
  // The model providing resources and callbacks for save card bottomsheet.
  std::unique_ptr<autofill::SaveCardBottomSheetModel> _saveCardBottomSheetModel;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    AutofillBottomSheetTabHelper* tabHelper =
        AutofillBottomSheetTabHelper::FromWebState(
            browser->GetWebStateList()->GetActiveWebState());
    _saveCardBottomSheetModel = tabHelper->GetSaveCardBottomSheetModel();
    CHECK(_saveCardBottomSheetModel);
  }
  return self;
}

@end
