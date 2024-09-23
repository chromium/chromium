// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/virtual_card_enrollment_callbacks.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"

@interface VirtualCardEnrollmentBottomSheetCoordinator () <
    VirtualCardEnrollmentBottomSheetDelegate>

@property(nonatomic, strong) VirtualCardEnrollmentBottomSheetMediator* mediator;
@property(nonatomic, strong)
    VirtualCardEnrollmentBottomSheetViewController* viewController;

@end

@implementation VirtualCardEnrollmentBottomSheetCoordinator {
  std::unique_ptr<autofill::VirtualCardEnrollUiModel> _model;
  std::optional<autofill::VirtualCardEnrollmentCallbacks> _callbacks;

  // Opening links on the enrollment bottom sheet is delegated to this
  // handler.
  __weak id<ApplicationCommands> _applicationHandler;
}

@synthesize mediator;
@synthesize viewController;

- (instancetype)initWithUIModel:
                    (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model
             baseViewController:(UIViewController*)baseViewController
                        browser:(Browser*)browser {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    web::WebState* activeWebState =
        browser->GetWebStateList()->GetActiveWebState();
    _model = std::move(model);
    _callbacks = AutofillBottomSheetTabHelper::FromWebState(activeWebState)
                     ->GetVirtualCardEnrollmentCallbacks();
    _applicationHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
  }
  return self;
}

- (void)start {
  self.mediator = [[VirtualCardEnrollmentBottomSheetMediator alloc]
                initWithUIModel:std::move(_model)
                      callbacks:std::move(_callbacks.value())
      browserCoordinatorHandler:HandlerForProtocol(
                                    self.browser->GetCommandDispatcher(),
                                    BrowserCoordinatorCommands)];
  self.viewController =
      [[VirtualCardEnrollmentBottomSheetViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.mutator = self.mediator;
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

- (void)didTapLinkURL:(CrURL*)URL text:(NSString*)text {
  [_applicationHandler
      openURLInNewTab:[OpenNewTabCommand
                          commandWithURLFromChrome:URL.gurl
                                       inIncognito:self.browser->GetProfile()
                                                       ->IsOffTheRecord()]];
}

- (void)viewDidDisappear:(BOOL)animated {
}

@end
