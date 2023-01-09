// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/lens_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
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
#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/lens/lens_modal_animator.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LensCoordinator () <ChromeLensControllerDelegate,
                               LensCommands,
                               CRWWebStateObserver,
                               WebStateListObserving>

// A controller that can provide an entrypoint into Lens features.
@property(nonatomic, strong) id<ChromeLensController> lensController;

// The Lens viewController.
@property(nonatomic, strong) UIViewController* viewController;

// The animator for dismissing the Lens view.
@property(nonatomic, strong) LensModalAnimator* transitionAnimator;

// Whether or not a Lens Web page load was triggered from the Lens UI.
@property(nonatomic, assign) BOOL lensWebPageLoadTriggeredFromInputSelection;

// The WebState that is loading a Lens results page, if any.
@property(nonatomic, assign) web::WebState* loadingWebState;

@end

@implementation LensCoordinator {
  // Used to observe the active WebState.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<base::ScopedObservation<web::WebState, web::WebStateObserver>>
      _webStateObservation;

  // Used to observe the WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _webStateListObservation;
}
@synthesize baseViewController = _baseViewController;

// The timeout before the Lens UI is closed, if the Lens Web page
// fails to load.
const base::TimeDelta kCloseLensViewTimeout = base::Seconds(10);

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)start {
  [super start];

  Browser* browser = self.browser;
  DCHECK(browser);

  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensCommands)];

  _webStateListObservation = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserver>>(
      _webStateListObserverBridge.get());

  _webStateObservation = std::make_unique<
      base::ScopedObservation<web::WebState, web::WebStateObserver>>(
      _webStateObserverBridge.get());

  self.loadingWebState = nil;
  self.lensWebPageLoadTriggeredFromInputSelection = NO;
  self.transitionAnimator = [[LensModalAnimator alloc] init];
  _webStateListObservation->Observe(browser->GetWebStateList());
  [self updateLensAvailibilityForExtensions];
}

- (void)stop {
  Browser* browser = self.browser;
  DCHECK(browser);

  [self dismissViewController];
  self.loadingWebState = nullptr;
  self.transitionAnimator = nil;
  self.lensWebPageLoadTriggeredFromInputSelection = NO;

  _webStateListObservation.reset();
  _webStateObservation.reset();

  [browser->GetCommandDispatcher() stopDispatchingToTarget:self];

  [super stop];
}

#pragma mark - Commands

- (void)searchImageWithLens:(SearchImageWithLensCommand*)command {
  const bool isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();
  __weak LensCoordinator* weakSelf = self;

  LensQuery* lensQuery = [LensQuery alloc];
  lensQuery.image = command.image;
  lensQuery.isIncognito = isIncognito;
  lensQuery.entrypoint = command.entryPoint;
  ios::provider::GenerateLensLoadParamsAsync(
      lensQuery,
      base::BindOnce(^(const web::NavigationManager::WebLoadParams params) {
        [weakSelf openWebLoadParams:params];
      }));
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
    id<SystemIdentity> identity = authenticationService->GetPrimaryIdentity(
        ::signin::ConsentLevel::kSignin);
    configuration.identity = identity;
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

  // Set the transitioning delegate of the view controller to customize
  // modal dismiss animations.
  const LensModalAnimator* transitionAnimator = self.transitionAnimator;
  DCHECK(transitionAnimator);
  [viewController setTransitioningDelegate:transitionAnimator];

  [viewController
      setModalPresentationStyle:UIModalPresentationOverCurrentContext];

  [self.baseViewController presentViewController:viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - ChromeLensControllerDelegate

- (void)lensControllerDidTapDismissButton {
  self.lensWebPageLoadTriggeredFromInputSelection = NO;
  web::WebState* loadingWebState = self.loadingWebState;
  // If there is a webstate loading Lens results underneath the Lens UI,
  // close it so we return the user to the initial state.
  if (loadingWebState) {
    const int index =
        self.browser->GetWebStateList()->GetIndexOfWebState(loadingWebState);
    self.loadingWebState = nil;
    if (index != WebStateList::kInvalidIndex) {
      self.browser->GetWebStateList()->CloseWebStateAt(
          index, WebStateList::CLOSE_USER_ACTION);
    }
  }

  [self dismissViewController];
}

- (void)lensControllerDidGenerateLoadParams:
    (const web::NavigationManager::WebLoadParams&)params {
  const __weak UIViewController* lensViewController = self.viewController;
  if (!lensViewController) {
    // If the coordinator view controller is nil, simply open the params and
    // return early.
    [self openWebLoadParams:params];
    return;
  }

  // Prepare the coordinator for dismissing the presented view controller.
  // Since we are opening Lens Web, mark the page load as triggered.
  self.lensWebPageLoadTriggeredFromInputSelection = YES;
  [self openWebLoadParams:params];

  // This function will be called when the user selects an image in Lens.
  // we should continue to display the Lens UI until the search results
  // for that image have loaded, or a timeout occurs.
  // Fallback to close the preview if the page never loads beneath.
  __weak LensCoordinator* weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        // Only dismiss the Lens view if the displayed view controller is the
        // same as the one that was displayed when the load params were
        // initially generated.
        if (weakSelf.viewController == lensViewController) {
          weakSelf.lensWebPageLoadTriggeredFromInputSelection = NO;
          [weakSelf dismissViewController];
        }
      }),
      kCloseLensViewTimeout);
}

- (void)lensControllerDidSelectURL:(NSURL*)url {
  // This method is called when the user selects a URL within the Lens UI
  // and should be treated as a link press.
  web::NavigationManager::WebLoadParams params =
      web_navigation_util::CreateWebLoadParams(
          net::GURLWithNSURL(url), ui::PAGE_TRANSITION_LINK, nullptr);
  [self openWebLoadParams:params];
  [self dismissViewController];
}

#pragma mark - WebStateListObserving methods

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  if (self.lensWebPageLoadTriggeredFromInputSelection) {
    self.loadingWebState = newWebState;
  }
}

#pragma mark - CRWWebStateObserver methods

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(webState, self.loadingWebState);
  // If the loaded page is a Lens Web page and we are expecting a Lens Web page
  // load, dismiss the Lens UI.
  if (self.lensWebPageLoadTriggeredFromInputSelection &&
      ios::provider::IsLensWebResultsURL(webState->GetLastCommittedURL())) {
    self.lensWebPageLoadTriggeredFromInputSelection = NO;
    self.loadingWebState = nil;
    [self dismissViewController];

    // As this was a successful Lens Web results page load, trigger the toolbar
    // slide-in animation.
    CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
    id<ToolbarCommands> toolbarCommandsHandler =
        HandlerForProtocol(dispatcher, ToolbarCommands);
    [toolbarCommandsHandler triggerToolbarSlideInAnimation];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(webState, self.loadingWebState);
  self.loadingWebState = nil;
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

- (void)dismissViewController {
  if (self.baseViewController.presentedViewController == self.viewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }

  self.viewController = nil;
}

- (void)setLoadingWebState:(web::WebState*)webState {
  DCHECK(_webStateObservation);
  _webStateObservation->Reset();
  _loadingWebState = webState;
  if (_loadingWebState) {
    _webStateObservation->Observe(_loadingWebState);
  }
}

// Sets the visibility of the Lens replacement for the QR code scanner in the
// home screen widget.
- (void)updateLensAvailibilityForExtensions {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSString* enableLensInWidgetKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupEnableLensInWidget);

  const bool enableLensInWidget =
      ios::provider::IsLensSupported() &&
      base::FeatureList::IsEnabled(kEnableLensInHomeScreenWidget);
  [sharedDefaults setBool:enableLensInWidget forKey:enableLensInWidgetKey];
}

@end
