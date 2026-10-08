// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_web_modal_coordinator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_modal_content_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@interface GeminiWebModalCoordinator () <
    CRWWebStateDelegate,
    CRWWebStateObserver,
    CRWWebStatePolicyDecider,
    GeminiModalContentViewControllerDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation GeminiWebModalCoordinator {
  GURL _initialURL;
  std::unique_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegateBridge;
  std::unique_ptr<web::WebStatePolicyDeciderBridge> _policyDeciderBridge;
  GeminiModalContentViewController* _viewController;
  UINavigationController* _navigationController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                       URL:(const GURL&)URL {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _initialURL = URL;
  }
  return self;
}

- (void)dealloc {
  [self detachWebStateBridges];
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  _webState = web::WebState::Create(
      web::WebState::CreateParams(self.browser->GetProfile()));

  AttachTabHelpers(_webState.get(), TabHelperFilter::kGeminiWebModal);
  _webState->SetWebUsageEnabled(true);

  // Used to open links that request a new window in a new tab.
  _webStateDelegateBridge = std::make_unique<web::WebStateDelegateBridge>(self);
  _webState->SetDelegate(_webStateDelegateBridge.get());

  // Used to keep the navigation bar's title and URL in sync with the page.
  _webStateObserverBridge = std::make_unique<web::WebStateObserverBridge>(self);
  _webState->AddObserver(_webStateObserverBridge.get());

  // Used to open links in a new tab instead of navigating in-modal.
  _policyDeciderBridge =
      std::make_unique<web::WebStatePolicyDeciderBridge>(_webState.get(), self);

  // Lets page content extend to the bottom edge of the web view.
  _webState->GetWebViewProxy().scrollViewProxy.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;

  _viewController = [[GeminiModalContentViewController alloc]
      initWithContentView:_webState->GetView()];
  _viewController.delegate = self;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;
  _navigationController.presentationController.delegate = self;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];

  _webState->GetNavigationManager()->LoadURLWithParams(
      web::NavigationManager::WebLoadParams(_initialURL));
  // TODO(crbug.com/41407753): For a newly created WebState, the session
  // will not be restored until LoadIfNecessary call. Remove when fixed.
  _webState->GetNavigationManager()->LoadIfNecessary();
  [self updateNavigationItem];
}

- (void)stop {
  if (_navigationController.presentingViewController &&
      !_navigationController.beingDismissed) {
    [_navigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
  _navigationController = nil;
  _viewController.delegate = nil;
  _viewController = nil;

  [self detachWebStateBridges];
  _policyDeciderBridge.reset();
  _webStateObserverBridge.reset();
  _webStateDelegateBridge.reset();
  _webState.reset();

  [super stop];
}

#pragma mark - CRWWebStateDelegate

- (web::WebState*)webState:(web::WebState*)webState
    createNewWebStateForURL:(const GURL&)URL
                  openerURL:(const GURL&)openerURL
            initiatedByUser:(BOOL)initiatedByUser {
  [self openInNewTab:URL];
  return nullptr;
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  [self updateNavigationItem];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  [self updateNavigationItem];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self updateNavigationItem];
}

#pragma mark - CRWWebStatePolicyDecider

- (void)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(web::WebStatePolicyDecider::RequestInfo)requestInfo
           decisionHandler:(PolicyDecisionHandler)decisionHandler {
  // Once the webState has initially loaded and committed, any subsequent
  // requests should open in a new tab instead of navigating within the modal.
  if (!requestInfo.target_frame_is_main ||
      _webState->GetLastCommittedURL().is_empty()) {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }
  decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Cancel());
  [self openInNewTab:net::GURLWithNSURL(request.URL)];
}

#pragma mark - GeminiModalContentViewControllerDelegate

- (void)geminiModalContentViewControllerDidTapClose:
    (GeminiModalContentViewController*)viewController {
  [self.delegate geminiWebModalCoordinatorDidDismiss:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate geminiWebModalCoordinatorDidDismiss:self];
}

#pragma mark - Private

// Unregisters the observer and delegate bridges from the web state.
- (void)detachWebStateBridges {
  if (!_webState) {
    return;
  }
  if (_webStateObserverBridge) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
  _webState->SetDelegate(nullptr);
}

// Dismisses this modal, minimizes the Floaty and opens `URL` in a new tab.
- (void)openInNewTab:(const GURL&)URL {
  GURL URLCopy = URL;
  dispatch_async(dispatch_get_main_queue(), ^{
    if (!self->_webState) {
      return;
    }
    RecordGeminiWebModalNavigatedToNewTab();
    Browser* browser = self.browser;
    [HandlerForProtocol(browser->GetCommandDispatcher(), GeminiCommands)
        minimizeGeminiIfInvoked];
    [self.delegate geminiWebModalCoordinatorDidDismiss:self];
    UrlLoadParams params = UrlLoadParams::InNewTab(URLCopy);
    params.append_to = OpenPosition::kCurrentTab;
    UrlLoadingBrowserAgent::FromBrowser(browser)->Load(params);
  });
}

// Updates the navigation item to reflect the page's domain and/or title.
- (void)updateNavigationItem {
  NSString* domain =
      base::SysUTF16ToNSString(url_formatter::FormatUrlForSecurityDisplay(
          _webState->GetVisibleURL(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

  if (@available(iOS 26, *)) {
    // Subtitle is available. Show the page title and domain as the
    // navigation item title & subtitle.
    web::NavigationItem* item =
        _webState->GetNavigationManager()->GetVisibleItem();
    _viewController.title =
        item ? base::SysUTF16ToNSString(item->GetTitle()) : nil;
    _viewController.navigationItem.subtitle = domain;
  } else {
    // Subtitle is unavailable. Just show the domain as navigation item title.
    _viewController.title = domain;
  }
}

@end
