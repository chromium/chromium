// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/lens_coordinator.h"

#import "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
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
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  LensConfiguration* configuration = [[LensConfiguration alloc] init];
  configuration.isIncognito = browserState->IsOffTheRecord();
  configuration.ssoService = GetApplicationContext()->GetSSOService();

  if (!configuration.isIncognito) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForBrowserState(browserState);
    configuration.identity = authenticationService->GetPrimaryIdentity(
        ::signin::ConsentLevel::kSignin);
  }

  self.lensController = ios::provider::NewChromeLensController(configuration);
  if (!self.lensController) {
    // Lens is not available.
    return;
  }
  self.lensController.delegate = self;

  self.viewController =
      [self.lensController postCaptureViewControllerForImage:command.image];
  [self.viewController setModalPresentationStyle:UIModalPresentationFullScreen];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - ChromeLensControllerDelegate

- (void)lensControllerDidTapDismissButton {
  // TODO(crbug.com/1234532): Integrate Lens with the browser's navigation
  // stack.
  if (self.baseViewController.presentedViewController == self.viewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }

  self.viewController = nil;
}

- (void)lensControllerDidSelectURL:(NSURL*)URL {
  // Dismiss the Lens view controller.
  if (self.baseViewController.presentedViewController == self.viewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }

  self.viewController = nil;

  // TODO(crbug.com/1234532): Integrate Lens with the browser's navigation
  // stack.
  UrlLoadParams loadParams = UrlLoadParams::InNewTab(net::GURLWithNSURL(URL));
  loadParams.SetInBackground(NO);
  loadParams.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  loadParams.append_to = kCurrentTab;
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  loadingAgent->Load(loadParams);
}

@end
