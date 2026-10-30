// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/coordinator/gemini_contextual_cue_settings_coordinator.h"

#import "base/check_op.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/coordinator/gemini_contextual_cue_settings_mediator.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

@interface GeminiContextualCueSettingsCoordinator () <
    GeminiContextualCueSettingsMediatorDelegate,
    GeminiContextualCueSettingsViewControllerPresentationDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation GeminiContextualCueSettingsCoordinator {
  // Mediator for the contextual cue settings sheet.
  GeminiContextualCueSettingsMediator* _mediator;
  // View controller presented by this coordinator.
  GeminiContextualCueSettingsViewController* _viewController;
  // Navigation controller wrapping `_viewController` in a sheet.
  UINavigationController* _navigationController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _mediator = [[GeminiContextualCueSettingsMediator alloc]
      initWithWebState:self.browser->GetWebStateList()->GetActiveWebState()
         geminiHandler:HandlerForProtocol(dispatcher, GeminiCommands)
       settingsHandler:HandlerForProtocol(dispatcher, SettingsCommands)];
  _mediator.delegate = self;

  _viewController = [[GeminiContextualCueSettingsViewController alloc] init];
  _viewController.presentationDelegate = self;
  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;
  _navigationController.presentationController.delegate = self;
  UISheetPresentationController* sheetController =
      _navigationController.sheetPresentationController;
  if (sheetController) {
    sheetController.detents =
        @[ [UISheetPresentationControllerDetent mediumDetent] ];
    sheetController.prefersGrabberVisible = YES;
  }

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  if (_navigationController.presentingViewController &&
      !_navigationController.beingDismissed) {
    [_navigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
  _viewController.presentationDelegate = nil;
  _viewController.mutator = nil;
  _viewController = nil;

  [super stop];
}

#pragma mark - GeminiContextualCueSettingsMediatorDelegate

- (void)geminiContextualCueSettingsMediator:
            (GeminiContextualCueSettingsMediator*)mediator
              dismissSettingsWithCompletion:(ProceduralBlock)completion {
  CHECK_EQ(_mediator, mediator);
  __weak __typeof(self) weakSelf = self;
  [self dismissModalWithCompletion:^{
    [weakSelf didDismissModalWithCompletion:completion];
  }];
}

#pragma mark - GeminiContextualCueSettingsViewControllerPresentationDelegate

- (void)geminiContextualCueSettingsViewControllerDidTapDone:
    (GeminiContextualCueSettingsViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate geminiContextualCueSettingsCoordinatorDidDismiss:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate geminiContextualCueSettingsCoordinatorDidDismiss:self];
}

#pragma mark - Private

// Dismisses `_navigationController` if it is currently presented, then invokes
// `completion`.
- (void)dismissModalWithCompletion:(ProceduralBlock)completion {
  if (_navigationController.presentingViewController &&
      !_navigationController.beingDismissed) {
    [_navigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:completion];
    return;
  }
  if (completion) {
    completion();
  }
}

// Notifies the coordinator delegate of dismissal and executes `completion`.
- (void)didDismissModalWithCompletion:(ProceduralBlock)completion {
  [self.delegate geminiContextualCueSettingsCoordinatorDidDismiss:self];
  if (completion) {
    completion();
  }
}

@end
