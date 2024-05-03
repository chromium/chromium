// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_presentation_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_view_controller.h"

@interface QuickDeleteCoordinator () <QuickDeletePresentationCommands>
@end

@implementation QuickDeleteCoordinator {
  QuickDeleteViewController* _viewController;
  UINavigationController* _navigationController;
}

#pragma mark - ChromeCoordinator
- (void)start {
  _viewController = [[QuickDeleteViewController alloc] init];
  _viewController.presentationHandler = self;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController = nil;
  _viewController.presentationHandler = nil;
  _viewController = nil;
}

#pragma mark - QuickDeletePresentationCommands

- (void)dismissQuickDelete {
  id<QuickDeleteCommands> quickDeleteHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), QuickDeleteCommands);
  [quickDeleteHandler stopQuickDelete];
}
@end
