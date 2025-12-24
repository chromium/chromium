// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_coordinator.h"

#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_coordinator.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_navigation_mediator.h"
#import "ios/chrome/browser/composebox/public/composebox_animation_base.h"
#import "ios/chrome/browser/composebox/public/composebox_input_plate_position.h"
#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_animation_context.h"
#import "ios/chrome/browser/composebox/ui/composebox_dismiss_animator.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/composebox/ui/composebox_present_animator.h"
#import "ios/chrome/browser/composebox/ui/composebox_view_controller.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/web/public/web_state.h"

@interface ComposeboxCoordinator () <ComposeboxViewControllerDelegate,
                                     ComposeboxNavigationMediatorDelegate,
                                     ComposeboxAnimationContext,
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
  // The base of the composebox animations.
  __weak id<ComposeboxAnimationBase> _animationBase;
  // The holder for the composebox mode.
  ComposeboxModeHolder* _modeHolder;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entrypoint:(ComposeboxEntrypoint)entrypoint
                                     query:(NSString*)query
                   composeboxAnimationBase:
                       (id<ComposeboxAnimationBase>)animationBase {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entrypoint = entrypoint;
    _query = query;
    _animationBase = animationBase;
    _modeHolder = [[ComposeboxModeHolder alloc] init];
  }
  return self;
}

- (void)start {
  ComposeboxTheme* theme = [self createTheme];
  _viewController = [[ComposeboxViewController alloc] initWithTheme:theme];
  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _viewController.transitioningDelegate = self;
  if (self.isOffTheRecord) {
    _viewController.view.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }
  _viewController.delegate = self;

  if ([self.baseViewController
          conformsToProtocol:@protocol(OmniboxPopupPresenterDelegate)]) {
    _viewController.proxiedPresenterDelegate =
        static_cast<id<OmniboxPopupPresenterDelegate>>(self.baseViewController);
  }

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
                       URLLoader:_navigationMediator
                           theme:[self createTheme]
                      modeHolder:_modeHolder];
  _aimComposeboxCoordinator.omniboxPopupPresenterDelegate = _viewController;
  [_aimComposeboxCoordinator start];

  [_viewController
      addInputViewController:_aimComposeboxCoordinator.inputViewController];

  if (theme.useIncognitoViewFallback) {
    [self checkClipboardContent];
  }

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stopAnimatedWithCompletion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  auto dismissComplete = ^{
    [weakSelf cleanup];
    if (completion) {
      completion();
    }
  };

  if (!_viewController.presentingViewController) {
    dismissComplete();
    return;
  }

  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:dismissComplete];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:NO
                                                               completion:nil];
  [self cleanup];
}

- (void)cleanup {
  _viewController = nil;

  [_aimComposeboxCoordinator stop];
  _aimComposeboxCoordinator = nil;

  [_navigationMediator disconnect];
  _navigationMediator = nil;
}

- (BOOL)isPresented {
  return _viewController.presentingViewController != nil;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  ComposeboxPresentAnimator* animator =
      [[ComposeboxPresentAnimator alloc] initWithContext:self
                                           animationBase:_animationBase];
  animator.toggleOnAIM = _entrypoint == ComposeboxEntrypoint::kNTPAIMButton;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  return [[ComposeboxDismissAnimator alloc]
      initWithContextProvider:self
                animationBase:_animationBase];
}

#pragma mark - ComposeboxViewControllerDelegate

- (void)composeboxViewControllerDidTapCloseButton:
    (ComposeboxInputPlateViewController*)viewController {
  [self dismissComposeboxImmediately:NO];
}

#pragma mark - ComposeboxNavigationMediatorDelegate

- (void)navigationMediatorDidFinish:
    (ComposeboxNavigationMediator*)navigationMediator {
  [self dismissComposeboxImmediately:NO];
}

- (void)navigationMediator:(ComposeboxNavigationMediator*)navigationMediator
    wantsToLoadJavaScriptURL:(const GURL&)URL {
  LoadJavaScriptURL(URL, self.browser,
                    self.browser->GetWebStateList()->GetActiveWebState());
}

#pragma mark - OmniboxStateProvider

- (BOOL)isOmniboxFocused {
  // The omnibox is always considered focused while the composebox coordinator
  // is started, which can be proxied by the presence of the input plate
  // coordinator.
  return _aimComposeboxCoordinator != nil;
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

- (ComposeboxTheme*)createTheme {
  BOOL isNTP = NO;
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (activeWebState && IsVisibleURLNewTabPage(activeWebState)) {
    isNTP = YES;
  }

  return [[ComposeboxTheme alloc]
      initWithInputPlatePosition:[self inputPlatePositionPreference]
                       incognito:self.isOffTheRecord
                           isNTP:isNTP];
}

- (ComposeboxInputPlatePosition)inputPlatePositionPreference {
  if (IsComposeboxForceTopEnabled()) {
    return ComposeboxInputPlatePosition::kTop;
  }

  if (IsBottomOmniboxAvailable() &&
      GetApplicationContext()->GetLocalState()->GetBoolean(
          omnibox::kIsOmniboxInBottomPosition)) {
    return ComposeboxInputPlatePosition::kBottom;
  }

  return ComposeboxInputPlatePosition::kTop;
}

#pragma mark - Clipboard checks

- (void)checkClipboardContent {
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  if (!clipboardRecentContent) {
    [self onClipboardMatchedTypesReceived:{}];
    return;
  }

  std::set<ClipboardContentType> desired_types = {ClipboardContentType::URL,
                                                  ClipboardContentType::Text,
                                                  ClipboardContentType::Image};
  __weak __typeof(self) weakSelf = self;
  clipboardRecentContent->HasRecentContentFromClipboard(
      desired_types,
      base::BindOnce(^(std::set<ClipboardContentType> matched_types) {
        [weakSelf onClipboardMatchedTypesReceived:matched_types];
      }));
}

- (void)onClipboardMatchedTypesReceived:
    (std::set<ClipboardContentType>)matchedTypes {
  BOOL hasClipboardContent = !matchedTypes.empty();
  [_viewController setExpectsClipboardSuggestion:hasClipboardContent];
}

#pragma mark - ComposeboxAnimationContext

- (UIView*)inputPlateViewForAnimation {
  return [_aimComposeboxCoordinator
              .inputViewController inputPlateViewForAnimation];
}

- (UIView*)closeButtonForAnimation {
  return _viewController.closeButton;
}

- (UIView*)popupViewForAnimation {
  return _viewController.omniboxPopupContainer;
}

- (UIView*)incognitoViewForAnimation {
  return _viewController.incognitoView;
}

- (void)setComposeboxMode:(ComposeboxMode)mode {
  _modeHolder.mode = mode;
}

- (void)expandInputPlateForDismissal {
  [_viewController expandInputPlateForDismissal];
}

- (BOOL)inputPlateIsCompact {
  return _aimComposeboxCoordinator.inputViewController.compact;
}

@end
