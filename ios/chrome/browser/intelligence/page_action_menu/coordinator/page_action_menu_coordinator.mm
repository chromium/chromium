// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"

namespace {

const CGFloat kMenuCornerRadius = 20;

}

@interface PageActionMenuCoordinator () <UINavigationControllerDelegate>
@end

@implementation PageActionMenuCoordinator {
  UINavigationController* _navigationController;
  PageActionMenuViewController* _viewController;
  PageActionMenuMediator* _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  BOOL readerModeActive = NO;
  if (IsReaderModeAvailable()) {
    ReaderModeTabHelper* readerModeTabHelper =
        ReaderModeTabHelper::FromWebState(
            self.browser->GetWebStateList()->GetActiveWebState());
    readerModeActive = readerModeTabHelper->IsActive();
  }
  _viewController = [[PageActionMenuViewController alloc]
      initWithReaderModeActive:readerModeActive];
  _mediator = [[PageActionMenuMediator alloc] init];
  _viewController.BWGHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), BWGCommands);
  _viewController.readerModeHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ReaderModeCommands);
  _viewController.pageActionMenuHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PageActionMenuCommands);

  if (IsLensOverlayAvailable(self.profile->GetPrefs())) {
    _viewController.lensOverlayHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), LensOverlayCommands);
  }

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.delegate = self;
  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;
  // Configure presentation sheet.
  __weak __typeof(self) weakSelf = self;
  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf resolveDetentValueForSheetPresentation:context];
  };
  UISheetPresentationControllerDetent* initialDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kAIHubDetentIdentifier
                            resolver:detentResolver];
  _navigationController.sheetPresentationController.detents = @[
    initialDetent,
  ];
  _navigationController.sheetPresentationController.selectedDetentIdentifier =
      kAIHubDetentIdentifier;
  _navigationController.sheetPresentationController.preferredCornerRadius =
      kMenuCornerRadius;
  _navigationController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;
  _navigationController.sheetPresentationController.prefersGrabberVisible = NO;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];

  [super start];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

#pragma mark - Public

- (void)stopWithCompletion:(ProceduralBlock)completion {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:completion];
  }
  _viewController = nil;
  _mediator = nil;
  [super stop];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  // Invalidate detents.
  [navigationController.sheetPresentationController animateChanges:^{
    [navigationController.sheetPresentationController invalidateDetents];
  }];
}

#pragma mark - Private

// Returns the appropriate detent value for a sheet presentation in `context`.
- (CGFloat)resolveDetentValueForSheetPresentation:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  return [_viewController resolveDetentValueForSheetPresentation:context];
}

@end
