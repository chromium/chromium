// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/debugger/coordinator/aim_debugger_coordinator.h"

#import "ios/chrome/browser/aim/debugger/coordinator/aim_debugger_mediator.h"
#import "ios/chrome/browser/aim/debugger/ui/aim_debugger_view_controller.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@interface AimDebuggerCoordinator () <UIAdaptivePresentationControllerDelegate>

@end

@implementation AimDebuggerCoordinator {
  AimDebuggerViewController* _viewController;
  AimDebuggerMediator* _mediator;
  TableViewNavigationController* _navigationController;
}

- (void)start {
  CHECK(experimental_flags::IsOmniboxDebuggingEnabled());
  ProfileIOS* profile = self.browser->GetProfile();

  _viewController =
      [[AimDebuggerViewController alloc] initWithStyle:ChromeTableViewStyle()];

  _mediator = [[AimDebuggerMediator alloc]
      initWithService:IOSChromeAimEligibilityServiceFactory::GetForProfile(
                          profile)
          prefService:profile->GetPrefs()];

  _mediator.consumer = _viewController;
  _mediator.snackbarHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  _viewController.mutator = _mediator;

  _navigationController =
      [[TableViewNavigationController alloc] initWithTable:_viewController];
  _navigationController.presentationController.delegate = self;
  [_navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];

  // Add a close button
  _viewController.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(didTapCloseButton)];

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stopAnimatedWithCompletion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  auto dismissComplete = ^{
    [weakSelf stop];
    if (completion) {
      completion();
    }
  };

  if (!_navigationController.presentingViewController) {
    dismissComplete();
    return;
  }

  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:dismissComplete];
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  [self cleanup];
}

- (void)cleanup {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  _navigationController = nil;
}

- (void)didTapCloseButton {
  [self.presenter dismissAimDebuggerWithAnimation:YES];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.presenter dismissAimDebuggerWithAnimation:NO];
}

@end
