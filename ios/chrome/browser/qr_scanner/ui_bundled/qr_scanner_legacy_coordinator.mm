// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/qr_scanner/ui_bundled/qr_scanner_legacy_coordinator.h"

#import <ostream>

#import "base/check_op.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/qr_scanner/ui_bundled/qr_scanner_view_controller.h"
#import "ios/chrome/browser/ui/scanner/scanner_presenting.h"

@interface QRScannerLegacyCoordinator () <ScannerPresenting>

@property(nonatomic, readwrite, strong) QRScannerViewController* viewController;

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
  [super stop];
  if (self.baseViewController.presentedViewController == self.viewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
  self.viewController = nil;
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
}

#pragma mark - Commands

- (void)showQRScanner {
  DCHECK(self.browser);
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<OmniboxCommands> handler = HandlerForProtocol(dispatcher, OmniboxCommands);
  [handler cancelOmniboxEdit];
  self.viewController = [[QRScannerViewController alloc]
      initWithPresentationProvider:self
                       queryLoader:static_cast<id<LoadQueryCommands>>(
                                       self.browser->GetCommandDispatcher())];
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
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:^{
                                                sceneState.QRScannerVisible =
                                                    NO;
                                                if (completion)
                                                  completion();
                                              }];
  self.viewController = nil;
}

@end
