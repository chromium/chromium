// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"

@implementation PageActionMenuCoordinator {
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
  _viewController.lensOverlayHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  _viewController.readerModeHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ReaderModeCommands);
  _viewController.pageActionMenuHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PageActionMenuCommands);

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  [navigationController setNavigationBarHidden:YES animated:NO];
  navigationController.modalPresentationStyle = UIModalPresentationPageSheet;

  [self.baseViewController presentViewController:navigationController
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
    // If there is no completion block, stop the coordinator immediately by
    // skipping the animation.
    const BOOL animated = completion != nil;
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:completion];
  }
  _viewController = nil;
  _mediator = nil;
  [super stop];
}

@end
