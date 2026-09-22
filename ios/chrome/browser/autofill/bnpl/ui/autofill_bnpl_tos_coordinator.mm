// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_coordinator.h"

#import <memory>

#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_mediator.h"
#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@interface AutofillBnplTosCoordinator () <AutofillBnplTosViewControllerDelegate,
                                          UISheetPresentationControllerDelegate>

@property(nonatomic, strong) AutofillBnplTosViewController* viewController;
@property(nonatomic, strong) AutofillBnplTosMediator* mediator;

@end

@implementation AutofillBnplTosCoordinator {
  std::unique_ptr<autofill::payments::BnplTosModel> _model;
  std::unique_ptr<BnplCallbacks> _callbacks;
  __weak id<SceneCommands> _sceneHandler;
}

- (instancetype)initWithModel:
                    (std::unique_ptr<autofill::payments::BnplTosModel>)model
                    callbacks:(std::unique_ptr<BnplCallbacks>)callbacks
           baseViewController:(UIViewController*)baseViewController
                      browser:(Browser*)browser {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _model = std::move(model);
    _callbacks = std::move(callbacks);
    _sceneHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  }
  return self;
}

- (void)start {
  self.mediator =
      [[AutofillBnplTosMediator alloc] initWithModel:std::move(*_model)
                                           callbacks:std::move(_callbacks)];
  _model.reset();
  _callbacks.reset();

  self.viewController = [[AutofillBnplTosViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.mutator = self.mediator;
  self.mediator.consumer = self.viewController;

  self.viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.viewController.sheetPresentationController;
  if (presentationController) {
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.detents = @[
      [UISheetPresentationControllerDetent largeDetent],
    ];
    presentationController.delegate = self;
  }

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - AutofillBnplTosViewControllerDelegate

- (void)tosViewController:(AutofillBnplTosViewController*)viewController
              didTapOnURL:(NSURL*)url {
  // Ensure the link URL is a valid HTTP or HTTPS URL before opening in a new
  // tab.
  const GURL gurl = net::GURLWithNSURL(url);
  if (!gurl.is_valid() || !gurl.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:gurl
                   inIncognito:self.browser->GetProfile()->IsOffTheRecord()];
  [_sceneHandler openURLInNewTab:command];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.mediator didTapCancel];
}

@end
