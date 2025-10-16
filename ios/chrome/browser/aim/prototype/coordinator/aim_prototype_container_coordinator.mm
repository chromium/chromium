// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_container_coordinator.h"

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_composebox_coordinator.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_entrypoint.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_navigation_mediator.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_container_view_controller.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_dismiss_animator.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_present_animator.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/web/public/web_state.h"

@interface AIMPrototypeContainerCoordinator () <
    AIMPrototypeContainerViewControllerDelegate,
    AIMPrototypeNavigationMediatorDelegate,
    UIViewControllerTransitioningDelegate>

@end

@implementation AIMPrototypeContainerCoordinator {
  // The coordinator for the composebox.
  AIMPrototypeComposeboxCoordinator* _aimComposeboxCoordinator;
  // The mediator for the web navigation.
  AIMPrototypeNavigationMediator* _navigationMediator;
  // The entrypoint that triggered the AIM prototype.
  AIMPrototypeEntrypoint _entrypoint;
  // An optional query to pre-fill the omnibox.
  NSString* _query;
  // The container view controller.
  AIMPrototypeContainerViewController* _viewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entrypoint:(AIMPrototypeEntrypoint)entrypoint
                                     query:(NSString*)query {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entrypoint = entrypoint;
    _query = query;
  }
  return self;
}

- (void)start {
  _viewController = [[AIMPrototypeContainerViewController alloc] init];
  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _viewController.transitioningDelegate = self;
  _viewController.delegate = self;

  UrlLoadingBrowserAgent* urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  web::WebState::CreateParams params =
      web::WebState::CreateParams(self.profile);
  _navigationMediator = [[AIMPrototypeNavigationMediator alloc]
      initWithUrlLoadingBrowserAgent:urlLoadingBrowserAgent
                      webStateParams:params];
  _navigationMediator.consumer = _viewController;
  _navigationMediator.delegate = self;

  _aimComposeboxCoordinator = [[AIMPrototypeComposeboxCoordinator alloc]
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
  AIMPrototypePresentAnimator* animator = [[AIMPrototypePresentAnimator alloc]
      initWithContextProvider:_aimComposeboxCoordinator.contextProvider];
  animator.toggleOnAIM = _entrypoint == AIMPrototypeEntrypoint::kNTPAIMButton;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  return [[AIMPrototypeDismissAnimator alloc]
      initWithContextProvider:_aimComposeboxCoordinator.contextProvider];
}

#pragma mark - AIMPrototypeContainerViewControllerDelegate

- (void)aimPrototypeContainerViewControllerDidTapCloseButton:
    (AIMPrototypeComposeboxViewController*)viewController {
  [self dismissAIMPrototype];
}

#pragma mark - AIMPrototypeNavigationMediatorDelegate

- (void)navigationMediatorDidFinish:
    (AIMPrototypeNavigationMediator*)navigationMediator {
  [self dismissAIMPrototype];
}

#pragma mark - Private

// Sends the command to get the AIM prototype dismissed.
- (void)dismissAIMPrototype {
  id<BrowserCoordinatorCommands> commands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [commands hideAIMPrototype];
}

@end
