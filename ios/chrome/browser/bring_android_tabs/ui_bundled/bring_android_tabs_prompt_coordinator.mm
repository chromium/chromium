// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_prompt_coordinator.h"

#import "base/check_op.h"
#import "base/notreached.h"
#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/bring_android_tabs_commands.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_prompt_mediator.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/ui_swift.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

namespace {

// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;

// Set presentation style of a half sheet modal.
void SetModalPresentationStyle(UIViewController* view_controller) {
  view_controller.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentation_controller =
      view_controller.sheetPresentationController;
  presentation_controller.prefersEdgeAttachedInCompactHeight = YES;
  presentation_controller.widthFollowsPreferredContentSizeWhenEdgeAttached =
      YES;
  presentation_controller.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent,
  ];
  presentation_controller.preferredCornerRadius = kHalfSheetCornerRadius;
}

}  // namespace

@implementation BringAndroidTabsPromptCoordinator {
  // Mediator that updates Chromium model objects; serves as a delegate to the
  // view controller.
  BringAndroidTabsPromptMediator* _mediator;
}

- (void)start {
  BringAndroidTabsToIOSService* service =
      BringAndroidTabsToIOSServiceFactory::GetForProfileIfExists(
          self.browser->GetProfile());
  _mediator = [[BringAndroidTabsPromptMediator alloc]
      initWithBringAndroidTabsService:service
                            URLLoader:UrlLoadingBrowserAgent::FromBrowser(
                                          self.browser)];

  BringAndroidTabsPromptConfirmationAlertViewController* confirmationAlert =
      [[BringAndroidTabsPromptConfirmationAlertViewController alloc]
          initWithTabsCount:static_cast<int>(
                                service->GetNumberOfAndroidTabs())];
  confirmationAlert.delegate = _mediator;
  confirmationAlert.commandHandler = self.commandHandler;
  SetModalPresentationStyle(confirmationAlert);
  _viewController = confirmationAlert;
}

- (void)stop {
  // The view controller should have already dismissed itself using the
  // Bring Android Commands handler.
  DCHECK(_viewController);
  DCHECK(_viewController.beingDismissed ||
         _viewController.parentViewController == nil);
  _viewController = nil;
  // Remove the mediator.
  DCHECK(_mediator);
  _mediator = nil;
}

@end
