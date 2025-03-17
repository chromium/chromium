// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_coordinator.h"

#import <memory>
#import <utility>

#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mediator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

// TODO(crbug.com/391366601): Implement SaveCardBottomSheetCoordinator.
@implementation SaveCardBottomSheetCoordinator {
  // The model providing resources and callbacks for save card bottomsheet.
  std::unique_ptr<autofill::SaveCardBottomSheetModel> _saveCardBottomSheetModel;

  // The mediator for save card bottomsheet created and owned by the
  // coordinator.
  SaveCardBottomSheetMediator* _mediator;
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

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator = [[SaveCardBottomSheetMediator alloc]
      initWithUIModel:std::move(_saveCardBottomSheetModel)];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
}

@end
