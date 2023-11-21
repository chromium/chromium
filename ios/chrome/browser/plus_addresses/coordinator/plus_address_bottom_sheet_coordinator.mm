// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_coordinator.h"

#import "components/plus_addresses/plus_address_service.h"
#import "components/plus_addresses/plus_address_types.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@interface PlusAddressBottomSheetCoordinator () <ConfirmationAlertActionHandler>

@end

@implementation PlusAddressBottomSheetCoordinator {
  plus_addresses::PlusAddressCallback _callback;
  url::Origin _mainFrameOrigin;
  PlusAddressBottomSheetViewController* _viewController;
  plus_addresses::PlusAddressService* _plusAddressService;
  id<BrowserCoordinatorCommands> _browserCoordinatorHandler;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    ChromeBrowserState* browserState =
        browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _plusAddressService = PlusAddressServiceFactory::GetForBrowserState(
        browserState->GetOriginalChromeBrowserState());
    _browserCoordinatorHandler = HandlerForProtocol(
        browser->GetCommandDispatcher(), BrowserCoordinatorCommands);

    web::WebState* activeWebState =
        browser->GetWebStateList()->GetActiveWebState();
    AutofillBottomSheetTabHelper* bottomSheetTabHelper =
        AutofillBottomSheetTabHelper::FromWebState(activeWebState);
    _mainFrameOrigin =
        url::Origin::Create(activeWebState->GetLastCommittedURL());
    _callback = bottomSheetTabHelper->GetPendingPlusAddressFillCallback();
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[PlusAddressBottomSheetViewController alloc] init];

  // Indicate a preference for half sheet detents, and other styling concerns.
  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  _viewController.actionHandler = self;
  UISheetPresentationController* presentationController =
      _viewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [_viewController.presentingViewController dismissViewControllerAnimated:NO
                                                               completion:nil];
  _viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  __weak __typeof(self) weakSelf = self;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf didConfirm];
                         }];
}

- (void)confirmationAlertSecondaryAction {
  // The cancel button was tapped, which dismisses the bottom sheet.
  // Call out to the command handler to hide the view and stop the coordinator.
  [_browserCoordinatorHandler dismissPlusAddressBottomSheet];
}

#pragma mark - Private

- (void)didConfirm {
  _plusAddressService->OfferPlusAddressCreation(_mainFrameOrigin,
                                                std::move(_callback));
  [_browserCoordinatorHandler dismissPlusAddressBottomSheet];
}

@end
