// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/qr_scanner/coordinator/qr_scanner_legacy_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/qr_scanner/coordinator/qr_scanner_mediator.h"
#import "ios/chrome/browser/qr_scanner/ui/qr_scanner_view_controller.h"
#import "ios/chrome/browser/scanner/ui_bundled/scanner_presenting.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

@interface QRScannerLegacyCoordinator () <QRScannerCommands, ScannerPresenting>

@property(nonatomic, readwrite, strong) QRScannerViewController* viewController;
@property(nonatomic, readwrite, strong) QRScannerMediator* mediator;

@end

@implementation QRScannerLegacyCoordinator

@synthesize viewController = _viewController;
@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(QRScannerCommands)];
}

- (void)stop {
  self.viewController.presentationProvider = nil;
  [super stop];
  if (self.baseViewController.presentedViewController == self.viewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
  self.viewController = nil;
  self.mediator = nil;
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
}

#pragma mark - Commands

- (void)showQRScanner {
  DCHECK(self.browser);
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> browserCoordinatorHandler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [browserCoordinatorHandler hideComposebox];

  UrlLoadingBrowserAgent* loader =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  self.mediator = [[QRScannerMediator alloc] initWithLoader:loader];

  self.viewController =
      [[QRScannerViewController alloc] initWithPresentationProvider:self];
  self.viewController.mutator = self.mediator;
  self.viewController.browserCoordinatorHandler = browserCoordinatorHandler;
  self.viewController.modalPresentationStyle = UIModalPresentationFullScreen;

  SceneState* sceneState = self.browser->GetSceneState();
  DCHECK(sceneState);

  [self.baseViewController
      presentViewController:[self.viewController viewControllerToPresent]
                   animated:YES
                 completion:^{
                   sceneState.QRScannerVisible = YES;
                 }];
}

#pragma mark - QRScannerPresenting

- (void)dismissScannerViewController:(UIViewController*)controller
                          completion:(void (^)(void))completion {
  DCHECK_EQ(self.viewController,
            self.baseViewController.presentedViewController);
  SceneState* sceneState = self.browser->GetSceneState();
  DCHECK(sceneState);
  self.viewController.presentationProvider = nil;
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:^{
                                                sceneState.QRScannerVisible =
                                                    NO;
                                                if (completion) {
                                                  completion();
                                                }
                                              }];
  self.viewController = nil;
}

@end
