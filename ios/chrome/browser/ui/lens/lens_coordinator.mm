// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/lens_coordinator.h"

#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/lens_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
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

@interface LensCoordinator () <ChromeLensControllerDelegate, LensCommands>

// A controller that can provide an entrypoint into Lens features.
@property(nonatomic, strong) id<ChromeLensController> lensController;

// The Lens viewController.
@property(nonatomic, strong) UIViewController* viewController;

@end

@implementation LensCoordinator
@synthesize baseViewController = _baseViewController;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  DCHECK(self.browser);
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensCommands)];

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

- (void)openInputSelectionForEntrypoint:(LensEntrypoint)entrypoint {
  // Cancel any omnibox editing.
  Browser* browser = self.browser;
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<OmniboxCommands> omniboxCommandsHandler =
      HandlerForProtocol(dispatcher, OmniboxCommands);
  [omniboxCommandsHandler cancelOmniboxEdit];

  // Early return if Lens is not available.
  if (!ios::provider::IsLensSupported()) {
    return;
  }

  // Create a Lens configuration for this request.
  ChromeBrowserState* browserState = browser->GetBrowserState();
  const bool isIncognito = browserState->IsOffTheRecord();
  LensConfiguration* configuration = [[LensConfiguration alloc] init];
  configuration.isIncognito = isIncognito;
  configuration.ssoService = GetApplicationContext()->GetSSOService();
  configuration.entrypoint = entrypoint;

  if (!isIncognito) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForBrowserState(browserState);
    ChromeIdentity* chromeIdentity = authenticationService->GetPrimaryIdentity(
        ::signin::ConsentLevel::kSignin);
    configuration.identity = chromeIdentity;
  }

  // Set the controller.
  id<ChromeLensController> lensController =
      ios::provider::NewChromeLensController(configuration);
  DCHECK(lensController);

  self.lensController = lensController;
  lensController.delegate = self;

  // Create an input selection UIViewController and present it modally.
  CGRect contentArea = [UIScreen mainScreen].bounds;

  id<LensPresentationDelegate> delegate = self.delegate;
  if (delegate) {
    contentArea = [delegate webContentAreaForLensCoordinator:self];
  }

  UIViewController* viewController = [lensController
      inputSelectionViewControllerWithWebContentFrame:contentArea];

  // TODO(crbug.com/1353430): the returned UIViewController
  // must not be nil, remove this check once the internal
  // implementation of the method is complete.
  if (!viewController) {
    return;
  }

  self.viewController = viewController;

  [viewController
      setModalPresentationStyle:UIModalPresentationOverCurrentContext];

  [self.baseViewController presentViewController:viewController
                                        animated:YES
                                      completion:nil];
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
