// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_coordinator.h"

#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_mediator.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PaymentsSuggestionBottomSheetCoordinator ()

// This mediator is used to fetch data related to the bottom sheet.
@property(nonatomic, strong) PaymentsSuggestionBottomSheetMediator* mediator;

// This view controller is used to display the bottom sheet.
@property(nonatomic, strong)
    PaymentsSuggestionBottomSheetViewController* viewController;

// Used to find the CreditCard object and use it to open the credit card details
// view.
@property(nonatomic, assign) autofill::PersonalDataManager* personalDataManager;

@end

@implementation PaymentsSuggestionBottomSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(const autofill::FormActivityParams&)
                                               params {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    ChromeBrowserState* browserState =
        browser->GetBrowserState()->GetOriginalChromeBrowserState();

    self.personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            browserState->GetOriginalChromeBrowserState());
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.mediator = [[PaymentsSuggestionBottomSheetMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
       personalDataManager:self.personalDataManager];
  self.viewController =
      [[PaymentsSuggestionBottomSheetViewController alloc] init];
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [self.viewController dismissViewControllerAnimated:NO completion:nil];
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
}

@end
