// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_coordinator.h"

#import "ios/chrome/browser/composebox/coordinator/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_coordinator.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_navigation_mediator.h"
#import "ios/chrome/browser/composebox/ui/composebox_dismiss_animator.h"
#import "ios/chrome/browser/composebox/ui/composebox_present_animator.h"
#import "ios/chrome/browser/composebox/ui/composebox_view_controller.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/web/public/web_state.h"

@interface ComposeboxCoordinator () <ComposeboxViewControllerDelegate,
                                     ComposeboxNavigationMediatorDelegate,
                                     UIViewControllerTransitioningDelegate>

@end

@implementation ComposeboxCoordinator {
  // The coordinator for the composebox.
  ComposeboxInputPlateCoordinator* _aimComposeboxCoordinator;
  // The mediator for the web navigation.
  ComposeboxNavigationMediator* _navigationMediator;
  // The entrypoint that triggered the composebox.
  ComposeboxEntrypoint _entrypoint;
  // An optional query to pre-fill the omnibox.
  NSString* _query;
  // The container view controller.
  ComposeboxViewController* _viewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entrypoint:(ComposeboxEntrypoint)entrypoint
                                     query:(NSString*)query {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entrypoint = entrypoint;
    _query = query;
  }
  return self;
}

- (void)start {
  _viewController = [[ComposeboxViewController alloc] init];
  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _viewController.transitioningDelegate = self;
  _viewController.delegate = self;

  UrlLoadingBrowserAgent* urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  web::WebState::CreateParams params =
      web::WebState::CreateParams(self.profile);
  _navigationMediator = [[ComposeboxNavigationMediator alloc]
      initWithUrlLoadingBrowserAgent:urlLoadingBrowserAgent
                      webStateParams:params];
  _navigationMediator.consumer = _viewController;
  _navigationMediator.delegate = self;

  _aimComposeboxCoordinator = [[ComposeboxInputPlateCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      entrypoint:_entrypoint
                           query:_query
                       URLLoader:_navigationMediator];
  _aimComposeboxCoordinator.omniboxPopupPresenterDelegate = _viewController;
  [_aimComposeboxCoordinator start];

  [_viewController
      addInputViewController:_aimComposeboxCoordinator.inputViewController];

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;

  [_aimComposeboxCoordinator stop];
  _aimComposeboxCoordinator = nil;

  [_navigationMediator disconnect];
  _navigationMediator = nil;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  ComposeboxPresentAnimator* animator = [[ComposeboxPresentAnimator alloc]
      initWithContextProvider:_aimComposeboxCoordinator.contextProvider];
  animator.toggleOnAIM = _entrypoint == ComposeboxEntrypoint::kNTPAIMButton;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  return [[ComposeboxDismissAnimator alloc]
      initWithContextProvider:_aimComposeboxCoordinator.contextProvider];
}

#pragma mark - ComposeboxViewControllerDelegate

- (void)composeboxViewControllerDidTapCloseButton:
    (ComposeboxInputPlateViewController*)viewController {
  [self dismissComposeboxImmediately:YES];
}

#pragma mark - ComposeboxNavigationMediatorDelegate

- (void)navigationMediatorDidFinish:
    (ComposeboxNavigationMediator*)navigationMediator {
  [self dismissComposeboxImmediately:NO];
}

#pragma mark - Private

// Sends the command to get the composebox dismissed. If not `immediately`,
// stop the prototoype on the next run loop as this might be called while the
// prototype's omnibox is loading a query.
- (void)dismissComposeboxImmediately:(BOOL)immediately {
  id<BrowserCoordinatorCommands> commands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [commands hideComposeboxImmediately:immediately];
}

@end
