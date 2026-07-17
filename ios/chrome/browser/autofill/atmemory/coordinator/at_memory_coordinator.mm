// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_coordinator.h"

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_injection_handler.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"

@interface AtMemoryCoordinator ()
@end

@implementation AtMemoryCoordinator {
  // ViewController for the AtMemory screen.
  AtMemoryViewController* _viewController;
  // Mediator for the AtMemory coordinator.
  AtMemoryMediator* _mediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

- (void)start {
  _viewController = [[AtMemoryViewController alloc] init];
  id<AtMemoryCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);
  _viewController.atMemoryHandler = handler;
  _viewController.presentationController.delegate = self;

  _mediator = [[AtMemoryMediator alloc] init];
  _mediator.consumer = _viewController;
  _viewController.delegate = _mediator;

  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* sheet =
      _viewController.sheetPresentationController;
  if (sheet) {
    sheet.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];
    sheet.prefersGrabberVisible = YES;
    sheet.prefersScrollingExpandsWhenScrolledToEdge = YES;
    sheet.prefersEdgeAttachedInCompactHeight = YES;
  }

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  if (_viewController.presentingViewController) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
  _viewController = nil;
  _mediator = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  id<AtMemoryCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);
  [handler dismissAtMemory];
}

#pragma mark - AtMemoryCommands

- (void)openURL:(CrURL*)URL {
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:URL.gurl]];
}

@end
