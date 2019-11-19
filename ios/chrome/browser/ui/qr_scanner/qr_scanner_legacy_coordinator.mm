// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_legacy_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_view_controller.h"
#import "ios/chrome/browser/ui/scanner/scanner_presenting.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface QRScannerLegacyCoordinator () <ScannerPresenting>

@property(nonatomic, readwrite, strong) QRScannerViewController* viewController;

@end

@implementation QRScannerLegacyCoordinator
@synthesize dispatcher = _dispatcher;
@synthesize viewController = _viewController;

#pragma mark - ChromeCoordinator

- (void)stop {
  [super stop];
  if (self.baseViewController.presentedViewController == self.viewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
  self.viewController = nil;
  self.dispatcher = nil;
}

#pragma mark - Public

- (void)setDispatcher:(CommandDispatcher*)dispatcher {
  if (dispatcher == self.dispatcher) {
    return;
  }

  if (self.dispatcher) {
    [self.dispatcher stopDispatchingToTarget:self];
  }

  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(showQRScanner)];
  _dispatcher = dispatcher;
}

#pragma mark - Commands

- (void)showQRScanner {
  DCHECK(self.dispatcher);
  [static_cast<id<OmniboxFocuser>>(self.dispatcher) cancelOmniboxEdit];
  self.viewController = [[QRScannerViewController alloc]
      initWithPresentationProvider:self
                       queryLoader:static_cast<id<LoadQueryCommands>>(
                                       self.dispatcher)];
  self.viewController.modalPresentationStyle = UIModalPresentationFullScreen;

  [self.baseViewController
      presentViewController:[self.viewController getViewControllerToPresent]
                   animated:YES
                 completion:nil];
}

#pragma mark - QRScannerPresenting

- (void)dismissScannerViewController:(UIViewController*)controller
                          completion:(void (^)(void))completion {
  DCHECK_EQ(self.viewController,
            self.baseViewController.presentedViewController);
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:completion];
  self.viewController = nil;
}

@end
