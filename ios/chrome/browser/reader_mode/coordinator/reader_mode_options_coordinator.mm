// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_coordinator.h"

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"

@interface ReaderModeOptionsCoordinator () <
    UIAdaptivePresentationControllerDelegate>

@end

@implementation ReaderModeOptionsCoordinator {
  ReaderModeOptionsMediator* _mediator;
  ReaderModeOptionsViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[ReaderModeOptionsViewController alloc] init];
  _viewController.presentationController.delegate = self;
  _mediator = [[ReaderModeOptionsMediator alloc] init];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // If the navigation controller is not dismissed programmatically i.e. not
  // dismissed using `dismissViewControllerAnimated:completion:`, then call
  // `-hideReaderModeOptions`.
  id<ReaderModeOptionsCommands> readerModeOptionsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ReaderModeOptionsCommands);
  [readerModeOptionsHandler hideReaderModeOptions];
}

@end
