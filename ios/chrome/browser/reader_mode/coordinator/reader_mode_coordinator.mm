// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_coordinator.h"

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_mediator.h"
#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_coordinator.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"

@interface ReaderModeCoordinator () <ReaderModeOptionsCommands>
@end

@implementation ReaderModeCoordinator {
  ReaderModeViewController* _viewController;
  ReaderModeMediator* _mediator;
  ReaderModeOptionsCoordinator* _optionsCoordinator;
}

- (UIView*)viewForSnapshot {
  return _viewController.view;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[ReaderModeViewController alloc] init];
  _mediator = [[ReaderModeMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()];
  _mediator.consumer = _viewController;
  [self.baseViewController addChildViewController:_viewController];
  [_viewController didMoveToParentViewController:self.baseViewController];
  // Start handling Reader mode options commands.
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ReaderModeOptionsCommands)];
}

- (void)stop {
  // Stop handling Reader mode options commands.
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  // Ensure the options UI is dismissed.
  [self hideReaderModeOptions];
  // Disconnect mediator from model layer.
  [_mediator disconnect];
  _mediator = nil;
  // Dismiss Reader mode UI.
  [_viewController willMoveToParentViewController:nil];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

#pragma mark - ReaderModeOptionsCommands

- (void)showReaderModeOptions {
  _optionsCoordinator = [[ReaderModeOptionsCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  [_optionsCoordinator start];
}

- (void)hideReaderModeOptions {
  [_optionsCoordinator stop];
  _optionsCoordinator = nil;
}

@end
