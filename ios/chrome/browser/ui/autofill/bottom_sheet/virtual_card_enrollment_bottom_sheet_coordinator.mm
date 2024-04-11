// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_coordinator.h"

#import "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#import "components/autofill/core/browser/payments/virtual_card_enroll_metrics_logger.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_delegate.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_mediator.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_view_controller.h"

@interface VirtualCardEnrollmentBottomSheetCoordinator () <
    VirtualCardEnrollmentBottomSheetDelegate>

@property(nonatomic, strong) VirtualCardEnrollmentBottomSheetMediator* mediator;
@property(nonatomic, strong)
    VirtualCardEnrollmentBottomSheetViewController* viewController;

@end

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/virtual_card_enrollment_callbacks.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@implementation VirtualCardEnrollmentBottomSheetCoordinator {
  autofill::VirtualCardEnrollUiModel model_;
  std::optional<autofill::VirtualCardEnrollmentCallbacks> callbacks_;
  Browser* browser_;
  ChromeBrowserState* browser_state_;

  // Opening links on the enrollment bottom sheet is delegated to this
  // dispatcher.
  __weak id<ApplicationCommands> dispatcher_;
}

@synthesize mediator;
@synthesize viewController;

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
    self->browser_ = browser;
    self->browser_state_ = self.browser->GetBrowserState();
    self->dispatcher_ = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                           ApplicationCommands);
  }
  return self;
}

- (void)start {
  self.mediator = [[VirtualCardEnrollmentBottomSheetMediator alloc]
      initWithUiModel:self->model_];
  self.viewController =
      [[VirtualCardEnrollmentBottomSheetViewController alloc] init];
  self.viewController.delegate = self;
  self.mediator.consumer = self.viewController;

  self.viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.viewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent,
  ];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark VirtualCardEnrollmentBottomSheetDelegate

- (void)didAccept {
  CHECK(callbacks_) << "Callbacks_ are not set. Callbacks_ should have been "
                       "set and called only once.";
  callbacks_->OnAccepted();
  callbacks_.reset();
  [self logResultMetric:autofill::VirtualCardEnrollmentBubbleResult::
                            VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED];
  [self stop];
}

- (void)didCancel {
  CHECK(callbacks_) << "Callbacks_ are not set. Callbacks_ should have been "
                       "set and called only once.";
  callbacks_->OnDeclined();
  callbacks_.reset();
  [self logResultMetric:autofill::VirtualCardEnrollmentBubbleResult::
                            VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED];
  [self stop];
}

- (void)didTapLinkURL:(CrURL*)url text:(NSString*)text {
  [dispatcher_
      openURLInNewTab:[OpenNewTabCommand
                          commandWithURLFromChrome:url.gurl
                                       inIncognito:self.browser
                                                       ->GetBrowserState()
                                                       ->IsOffTheRecord()]];
}

- (void)viewDidDisappear:(BOOL)animated {
}

#pragma mark - Private

// Logs the result metric attaching additional parameters from the model.
- (void)logResultMetric:(autofill::VirtualCardEnrollmentBubbleResult)result {
  autofill::VirtualCardEnrollMetricsLogger::OnDismissed(
      result, model_.enrollment_fields.virtual_card_enrollment_source,
      /*is_reshow=*/false, model_.enrollment_fields.previously_declined);
}

@end
