// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_coordinator.h"

#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_mediator.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_view_controller.h"
#import "ios/web/public/web_state.h"

@interface PaymentsSuggestionBottomSheetCoordinator () {
  // Information regarding the triggering form for this bottom sheet.
  autofill::FormActivityParams _params;

  // Currently in the process of dismissing the bottom sheet.
  bool _dismissing;
}

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
    _params = params;
    _dismissing = NO;

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
  WebStateList* webStateList = self.browser->GetWebStateList();
  self.mediator = [[PaymentsSuggestionBottomSheetMediator alloc]
      initWithWebStateList:webStateList
                    params:_params
       personalDataManager:self.personalDataManager];
  const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();
  self.viewController =
      [[PaymentsSuggestionBottomSheetViewController alloc] initWithHandler:self
                                                                       URL:URL];
  self.mediator.consumer = self.viewController;
  self.viewController.delegate = self.mediator;

  // This is a fallback since the code enabling the bottom sheet happens earlier
  // than the code which retrieves credit card suggestions for the bottom sheet
  // and other operations which may modify the list of available credit cards
  // can happen between these two operations.
  if (!self.mediator.hasCreditCards) {
    [self.mediator disableBottomSheet];
    [self.mediator disconnect];
    return;
  }

  self.viewController.parentViewControllerHeight =
      self.baseViewController.view.frame.size.height;
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

#pragma mark - PaymentsSuggestionBottomSheetHandler

- (void)displayPaymentMethods {
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf stop];
                           [weakSelf.applicationCommandsHandler
                                   showCreditCardSettings];
                         }];
}

- (void)displayPaymentDetailsForCreditCardIdentifier:
    (NSString*)creditCardIdentifier {
  autofill::CreditCard* creditCard =
      [self.mediator creditCardForIdentifier:creditCardIdentifier];
  if (creditCard) {
    __weak __typeof(self) weakSelf = self;
    [self.baseViewController.presentedViewController
        dismissViewControllerAnimated:NO
                           completion:^{
                             [weakSelf stop];
                             [weakSelf.applicationCommandsHandler
                                 showCreditCardDetails:creditCard];
                           }];
  }
}

- (void)primaryButtonTapped:(NSString*)backendIdentifier {
  _dismissing = YES;
  __weak __typeof(self) weakSelf = self;
  [self.viewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf didSelectCreditCard:backendIdentifier];
                         }];
}

- (void)secondaryButtonTapped {
  // "No thanks" button, which dismisses the bottom sheet.
  [self.viewController dismissViewControllerAnimated:YES completion:NULL];
}

- (void)viewDidDisappear:(BOOL)animated {
  if (_dismissing) {
    return;
  }

  [self.mediator disconnect];
}

#pragma mark - Private

- (void)didSelectCreditCard:(NSString*)backendIdentifier {
  // Send a notification to fill the credit card related fields.
  [self.mediator didSelectCreditCard:backendIdentifier];
  [self.mediator disconnect];
}

@end
