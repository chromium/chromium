// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_web_modal_coordinator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_modal_content_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "url/gurl.h"

@interface GeminiWebModalCoordinator () <
    CRWWebStateDelegate,
    CRWWebStateObserver,
    GeminiModalContentViewControllerDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation GeminiWebModalCoordinator {
  GURL _initialURL;
  std::unique_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegateBridge;
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

  // Used to navigate to links in-modal instead of opening a new web state.
  _webStateDelegateBridge = std::make_unique<web::WebStateDelegateBridge>(self);
  _webState->SetDelegate(_webStateDelegateBridge.get());

  // Used to keep the navigation bar's title and URL in sync with the page.
  _webStateObserverBridge = std::make_unique<web::WebStateObserverBridge>(self);
  _webState->AddObserver(_webStateObserverBridge.get());

  // Disable swiping on edges to navigate back/forward.
  _webState->GetWebViewProxy().allowsBackForwardNavigationGestures = NO;

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
  // Open links in the existing web view, even if they request a new tab.
  [self loadURL:URL];
  return nullptr;
}

- (web::WebState*)webState:(web::WebState*)webState
         openURLWithParams:(const web::WebState::OpenURLParams&)params {
  [self loadURL:params.url];
  return webState;
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

// Navigates the modal's web state to `URL`.
- (void)loadURL:(const GURL&)URL {
  if (!_webState) {
    return;
  }
  _webState->GetNavigationManager()->LoadURLWithParams(
      web::NavigationManager::WebLoadParams(URL));
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
