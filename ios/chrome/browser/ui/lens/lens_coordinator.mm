// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/lens_coordinator.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LensCoordinator () <ChromeLensControllerDelegate>

// A controller that can provide an entrypoint into Lens features.
@property(nonatomic, strong) id<ChromeLensController> lensController;

// The Lens viewController.
@property(nonatomic, strong) UIViewController* viewController;

@end

@implementation LensCoordinator
@synthesize viewController = _viewController;

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forSelector:@selector(searchImageWithLens:)];

  [super start];
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

- (void)searchImageWithLens:(SearchImageWithLensCommand*)command {
  const bool isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();
  [self openWebLoadParams:ios::provider::GenerateLensLoadParamsForImage(
                              command.image, LensEntrypoint::ContextMenu,
                              isIncognito)];
}

#pragma mark - ChromeLensControllerDelegate

- (void)lensControllerDidTapDismissButton {
  if (self.baseViewController.presentedViewController == self.viewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }

  self.viewController = nil;
}

- (void)lensControllerDidGenerateLoadParams:
    (const web::NavigationManager::WebLoadParams&)params {
  [self openWebLoadParams:params];
}

#pragma mark - Private

- (void)openWebLoadParams:(const web::NavigationManager::WebLoadParams&)params {
  if (!self.browser)
    return;
  UrlLoadParams loadParams = UrlLoadParams::InNewTab(params);
  loadParams.SetInBackground(NO);
  loadParams.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  loadParams.append_to = kCurrentTab;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(loadParams);
}

@end
