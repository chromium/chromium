// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_navigation_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/task/sequenced_task_runner.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_navigation_consumer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

@interface ComposeboxNavigationMediator () <CRWWebStateDelegate,
                                            CRWWebStateObserver,
                                            CRWWebStatePolicyDecider>
@end

@implementation ComposeboxNavigationMediator {
  // The URL loading browser agent.
  raw_ptr<UrlLoadingBrowserAgent> _urlLoadingBrowserAgent;
  // The WebState for AI Mode SRP.
  std::unique_ptr<web::WebState> _webState;
  // The WebState delegate.
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegateBridge;
  // The observer bridge for WebState observing.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  // The WebState policy decider.
  std::unique_ptr<web::WebStatePolicyDeciderBridge> _policyDeciderBridge;
}

- (instancetype)initWithUrlLoadingBrowserAgent:
                    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent
                                webStateParams:
                                    (const web::WebState::CreateParams&)params {
  self = [super init];
  if (self) {
    _urlLoadingBrowserAgent = urlLoadingBrowserAgent;
    if (base::FeatureList::IsEnabled(kComposeboxImmersiveSRP)) {
      _webStateDelegateBridge =
          std::make_unique<web::WebStateDelegateBridge>(self);
      _webStateObserverBridge =
          std::make_unique<web::WebStateObserverBridge>(self);
      [self attachWebState:web::WebState::Create(params)];
    }
  }
  return self;
}

- (void)disconnect {
  _urlLoadingBrowserAgent = nullptr;
  if (_webState) {
    [self detachWebState];
  }
  _webStateObserverBridge.reset();
  _webStateDelegateBridge.reset();
}

- (void)setConsumer:(id<ComposeboxNavigationConsumer>)consumer {
  _consumer = consumer;
  if (_webState) {
    _webState->SetWebUsageEnabled(true);
    [_consumer setWebView:_webState->GetView()];
  }
}

#pragma mark - ComposeboxURLLoader

- (void)loadURLParams:(const UrlLoadParams&)URLLoadParams {
  if (_webState) {
    // Request an SRP without an input plate.
    GURL webStateURL = net::AppendOrReplaceQueryParameter(
        URLLoadParams.web_params.url, "gsc", "2");
    web::NavigationManager::WebLoadParams webParams =
        web::NavigationManager::WebLoadParams(webStateURL);
    webParams.transition_type = ui::PAGE_TRANSITION_GENERATED;
    _webState->GetNavigationManager()->LoadURLWithParams(webParams);
  } else {
    if (URLLoadParams.web_params.url.SchemeIs(url::kJavaScriptScheme)) {
      [self.delegate navigationMediator:self
               wantsToLoadJavaScriptURL:URLLoadParams.web_params.url];
    } else {
      _urlLoadingBrowserAgent->Load(URLLoadParams);
    }
    [self dismissComposebox];
  }
}

#pragma mark - CRWWebStatePolicyDecider

- (void)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(web::WebStatePolicyDecider::RequestInfo)requestInfo
           decisionHandler:(PolicyDecisionHandler)decisionHandler {
  GURL URL = net::GURLWithNSURL(request.URL);

  if (requestInfo.target_frame_is_main &&
      !google_util::IsGoogleSearchUrl(URL)) {
    // Don't load within the embedded web view.
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Cancel());
    _urlLoadingBrowserAgent->Load(UrlLoadParams::InCurrentTab(URL));
    [self dismissComposebox];
  } else {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
  }
}

#pragma mark - Private

// Creates a special WebState for AIM.
- (void)attachWebState:(std::unique_ptr<web::WebState>)webState {
  CHECK(!_webState) << "Detach the current WebState before attaching a new one";
  CHECK(!_policyDeciderBridge);
  _webState = std::move(webState);
  _webState->SetDelegate(_webStateDelegateBridge.get());
  _webState->AddObserver(_webStateObserverBridge.get());
  _policyDeciderBridge =
      std::make_unique<web::WebStatePolicyDeciderBridge>(_webState.get(), self);
  // TODO(crbug.com/445918427): Make an AIM tab helper filter.
  AttachTabHelpers(_webState.get(), TabHelperFilter::kLensOverlay);
  id<CRWWebViewProxy> webViewProxy = _webState->GetWebViewProxy();
  webViewProxy.allowsBackForwardNavigationGestures = NO;
  // Allow the scrollView to cover the safe area.
  webViewProxy.scrollViewProxy.clipsToBounds = NO;

  if (self.consumer) {
    _webState->SetWebUsageEnabled(true);
    [self.consumer setWebView:_webState->GetView()];
  }
}

// Removes the AIM WebState and returns it.
- (std::unique_ptr<web::WebState>)detachWebState {
  CHECK(_webState);
  _policyDeciderBridge.reset();
  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webState->SetDelegate(nullptr);
  return std::move(_webState);
}

// Asks the delegate to dismiss the composebox.
- (void)dismissComposebox {
  [self.delegate navigationMediatorDidFinish:self];
}

@end
