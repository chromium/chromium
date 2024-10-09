// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_handler.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/ios/ns_error_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "components/security_interstitials/core/insecure_form_util.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/net/url_scheme_util.h"
#import "ios/web/common/features.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/download/download_native_task_bridge.h"
#import "ios/web/navigation/crw_error_page_helper.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/crw_pending_navigation_info.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/navigation_manager_util.h"
#import "ios/web/navigation/web_kit_constants.h"
#import "ios/web/navigation/wk_back_forward_list_item_holder.h"
#import "ios/web/navigation/wk_navigation_action_policy_util.h"
#import "ios/web/navigation/wk_navigation_action_util.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/navigation/form_warning_type.h"
#import "ios/web/public/web_client.h"
#import "ios/web/security/crw_cert_verification_controller.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_view/content_type_util.h"
#import "ios/web/web_view/error_translation_util.h"
#import "ios/web/web_view/wk_security_origin_util.h"
#import "ios/web/web_view/wk_web_view_util.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/net_errors.h"
#import "net/cert/x509_util_apple.h"
#import "net/http/http_content_disposition.h"
#import "url/gurl.h"

using web::wk_navigation_util::kReferrerHeaderName;

namespace {
// Maximum number of errors to store in cert verification errors cache.
// Cache holds errors only for pending navigations, so the actual number of
// stored errors is not expected to be high.
const web::CertVerificationErrorsCacheType::size_type kMaxCertErrorsCount = 100;

// Returns true if the navigation was upgraded to HTTPS but failed due to an
// SSL or net error. This can happen when HTTPS-Only Mode feature automatically
// upgrades a navigation to HTTPS.
web::HttpsUpgradeType GetFailedHttpsUpgradeType(
    NSError* error,
    web::NavigationContextImpl* context,
    NSError* cancellationError) {
  if (!context || !context->GetItem() ||
      context->GetItem()->GetHttpsUpgradeType() ==
          web::HttpsUpgradeType::kNone ||
      cancellationError) {
    return web::HttpsUpgradeType::kNone;
  }
  int error_code = 0;
  if (!web::GetNetErrorFromIOSErrorCode(
          error.code, &error_code, net::NSURLWithGURL(context->GetUrl()))) {
    error_code = net::ERR_FAILED;
  }
  if (error_code != net::OK || web::IsWKWebViewSSLCertError(error)) {
    return context->GetItem()->GetHttpsUpgradeType();
  }
  return web::HttpsUpgradeType::kNone;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ErrorPagePresentationFailed {
  kUnknown,
  kWebViewReleased,
  kJavaScriptExceptionOccurred,
  kOtherWKErrorDomain,
  kMaxValue = kOtherWKErrorDomain
};

void LogPresentingErrorPageFailedWithError(NSError* error) {
  ErrorPagePresentationFailed failure_type =
      ErrorPagePresentationFailed::kUnknown;

  if ([WKErrorDomain isEqualToString:error.domain]) {
    if (error.code == WKErrorWebViewInvalidated ||
        error.code == WKErrorWebContentProcessTerminated ||
        error.code == WKErrorJavaScriptResultTypeIsUnsupported) {
      failure_type = ErrorPagePresentationFailed::kWebViewReleased;
    } else if (error.code == WKErrorJavaScriptExceptionOccurred) {
      failure_type = ErrorPagePresentationFailed::kJavaScriptExceptionOccurred;
    } else {
      failure_type = ErrorPagePresentationFailed::kOtherWKErrorDomain;
    }
  }

  base::UmaHistogramEnumeration("IOS.Web.ErrorPagePresentationFailed",
                                failure_type);
}

}  // namespace

@interface CRWWKNavigationHandler () <DownloadNativeTaskBridgeDelegate> {
  // Referrer for the current page; does not include the fragment.
  NSString* _currentReferrerString;

  // CertVerification errors which happened inside
  // `webView:didReceiveAuthenticationChallenge:completionHandler:`.
  // Key is leaf-cert/host pair. This storage is used to carry calculated
  // cert status from `didReceiveAuthenticationChallenge:` to
  // `didFailProvisionalNavigation:` delegate method.
  std::unique_ptr<web::CertVerificationErrorsCacheType> _certVerificationErrors;

  // Used to keep track of newly created and current download
  // task objects created from
  // `webView:navigationAction:didBecomeDownload` and
  // `webView:navigationResponse:didBecomeDownload`. Respectively,
  // DownloadNativeTaskBridge objects will help provide a valid
  // `navigationAction` or `navigationResponse`.
  NSMutableSet<DownloadNativeTaskBridge*>* _nativeTaskBridges;

  // Stores navigation policy state of download task to indicate if a download
  // should be performed.
  BOOL _shouldPerformDownload;
}

@property(nonatomic, weak) id<CRWWKNavigationHandlerDelegate> delegate;

// Returns the WebStateImpl from self.delegate.
@property(nonatomic, readonly, assign) web::WebStateImpl* webStateImpl;
// Returns the NavigationManagerImpl from self.webStateImpl.
@property(nonatomic, readonly, assign)
    web::NavigationManagerImpl* navigationManagerImpl;
// Returns the UserInteractionState from self.delegate.
@property(nonatomic, readonly, assign)
    web::UserInteractionState* userInteractionState;
// Returns the CRWCertVerificationController from self.delegate.
@property(nonatomic, readonly, weak)
    CRWCertVerificationController* certVerificationController;
// Returns the document URL from self.delegate.
@property(nonatomic, readonly, assign) GURL documentURL;

@end

@implementation CRWWKNavigationHandler

- (instancetype)initWithDelegate:(id<CRWWKNavigationHandlerDelegate>)delegate {
  if ((self = [super init])) {
    _navigationStates = [[CRWWKNavigationStates alloc] init];
    // Load phase when no WebView present is 'loaded' because this represents
    // the idle state.
    _navigationState = web::WKNavigationState::FINISHED;

    _certVerificationErrors =
        std::make_unique<web::CertVerificationErrorsCacheType>(
            kMaxCertErrorsCount);

    _nativeTaskBridges = [[NSMutableSet alloc] init];

    _shouldPerformDownload = NO;

    _delegate = delegate;
  }
  return self;
}

- (void)dealloc {
  for (DownloadNativeTaskBridge* bridge in _nativeTaskBridges) {
    [bridge cancel];
  }
  _nativeTaskBridges = nil;
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)action
                        preferences:(WKWebpagePreferences*)preferences
                    decisionHandler:(void (^)(WKNavigationActionPolicy,
                                              WKWebpagePreferences*))handler {
  // Check if OS lockdown mode is enabled and update the preference value.
  if (!self.beingDestroyed) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
      if (@available(iOS 16.0, *)) {
        web::GetWebClient()->SetOSLockdownModeEnabled(
            preferences.lockdownModeEnabled);
      }
    });
  }

  GURL requestURL = net::GURLWithNSURL(action.request.URL);
  const web::UserAgentType userAgentType =
      [self userAgentForNavigationAction:action webView:webView];

  if (userAgentType != web::UserAgentType::NONE) {
    if (action.navigationType == WKNavigationTypeBackForward &&
        self.webStateImpl->GetUserAgentForSessionRestoration() !=
            web::UserAgentType::AUTOMATIC) {
      // When navigating back to a page with a UserAgent that wasn't automatic,
      // let's reuse this user agent for next navigations.
      self.webStateImpl->SetUserAgent(userAgentType);
    }

    GURL URLForUserAgent = requestURL;
    if ([CRWErrorPageHelper isErrorPageFileURL:URLForUserAgent]) {
      URLForUserAgent = [CRWErrorPageHelper
          failedNavigationURLFromErrorPageFileURL:URLForUserAgent];
    }

    if (action.navigationType == WKNavigationTypeReload &&
        web::wk_navigation_util::URLNeedsUserAgentType(URLForUserAgent) &&
        webView.backForwardList.currentItem) {
      // When reloading the page, the UserAgent will be updated to the one for
      // the new page.
      web::NavigationItem* item = [[CRWNavigationItemHolder
          holderForBackForwardListItem:webView.backForwardList.currentItem]
          navigationItem];
      if (item) {
        item->SetUserAgentType(userAgentType);
      }
    }

    NSString* userAgentString = base::SysUTF8ToNSString(
        web::GetWebClient()->GetUserAgent(userAgentType));
    if (![webView.customUserAgent isEqualToString:userAgentString]) {
      webView.customUserAgent = userAgentString;
    }
  }

  const WKContentMode contentMode = userAgentType == web::UserAgentType::DESKTOP
                                        ? WKContentModeDesktop
                                        : WKContentModeMobile;
  BOOL isMainFrameNavigationAction = [self isMainFrameNavigationAction:action];
  auto decisionHandler = ^(WKNavigationActionPolicy policy) {
    preferences.preferredContentMode = contentMode;
    if (@available(iOS 16.0, *)) {
      if ((policy == WKNavigationActionPolicyAllow) &&
          isMainFrameNavigationAction) {
        UMA_HISTOGRAM_BOOLEAN("IOS.MainFrameNavigationIsInLockdownMode",
                              preferences.lockdownModeEnabled);
      }

      if (!self.beingDestroyed) {
        bool browser_lockdown_mode_enabled =
            web::GetWebClient()->IsBrowserLockdownModeEnabled();
        if ((policy == WKNavigationActionPolicyAllow) &&
            isMainFrameNavigationAction) {
          UMA_HISTOGRAM_BOOLEAN(
              "IOS.MainFrameNavigationIsInBrowserLockdownMode",
              browser_lockdown_mode_enabled);
        }
        if (browser_lockdown_mode_enabled) {
          preferences.lockdownModeEnabled = true;
        }
      }
    }
    handler(policy, preferences);
  };

  [self didReceiveWKNavigationDelegateCallback];

  BOOL forceBlockUniversalLinks = self.blockUniversalLinksOnNextDecidePolicy;
  self.blockUniversalLinksOnNextDecidePolicy = NO;

  _webProcessCrashed = NO;
  if (self.beingDestroyed) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  // The page will not be changed until this navigation is committed, so the
  // retrieved state will be pending until `didCommitNavigation` callback.
  [self createPendingNavigationInfoFromNavigationAction:action];

  if (action.targetFrame.mainFrame &&
      action.navigationType == WKNavigationTypeBackForward) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:requestURL];
    if (context) {
      // Context is null for renderer-initiated navigations.
      int index = web::GetCommittedItemIndexWithUniqueID(
          self.navigationManagerImpl, context->GetNavigationItemUniqueID());
      self.navigationManagerImpl->SetPendingItemIndex(index);
    }
  }

  // If this is a error navigation, pass through.
  if ([CRWErrorPageHelper isErrorPageFileURL:requestURL]) {
    if (action.sourceFrame.mainFrame) {
      // Disallow renderer initiated navigations to error URLs.
      decisionHandler(WKNavigationActionPolicyCancel);
    } else {
      decisionHandler(WKNavigationActionPolicyAllow);
    }
    return;
  }

  // Always disallow navigations to fido URLs. See crbug.com/371929521.
  constexpr char kFidoScheme[] = "fido";
  if (requestURL.SchemeIs(kFidoScheme)) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  ui::PageTransition transition =
      [self pageTransitionFromNavigationType:action.navigationType];
  if (isMainFrameNavigationAction) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:requestURL];
    // Theoretically if `context` can be found here, the navigation should be
    // either user-initiated or JS back/forward. The second part in the "if"
    // condition used to be a DCHECK, but it would fail in this case:
    // 1. Multiple render-initiated navigation with the same URL happens at the
    //    same time;
    // 2. One of these navigations gets the "didStartProvisonalNavigation"
    //    callback and creates a NavigationContext;
    // 3. Another navigation reaches here and retrieves that NavigationContext
    //    by matching URL.
    // The DCHECK is now turned into a "if" condition, but can be reverted if a
    // more reliable way of matching NavigationContext with WKNavigationAction
    // is found.
    if (context &&
        (!context->IsRendererInitiated() ||
         (context->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK))) {
      transition = context->GetPageTransition();
      if (context->IsLoadingErrorPage()) {
        // loadHTMLString: navigation which loads error page into WKWebView.
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
      }
    }
  }

  // Invalid URLs should not be loaded.
  if (!requestURL.is_valid()) {
    // The HTML5 spec indicates that window.open with an invalid URL should open
    // about:blank.
    BOOL isFirstLoadInOpenedWindow =
        self.webStateImpl->HasOpener() &&
        !self.webStateImpl->GetNavigationManager()->GetLastCommittedItem();
    BOOL isMainFrame = action.targetFrame.mainFrame;
    if (isFirstLoadInOpenedWindow && isMainFrame) {
      decisionHandler(WKNavigationActionPolicyCancel);
      GURL aboutBlankURL(url::kAboutBlankURL);
      web::NavigationManager::WebLoadParams loadParams(aboutBlankURL);
      loadParams.referrer = self.currentReferrer;

      self.webStateImpl->GetNavigationManager()->LoadURLWithParams(loadParams);
      return;
    }
    // On iOS 12, we allow the navigation since cancelling it here causes
    // crbug.com/965067. The underlying issue is a WebKit bug that converts
    // valid URLs into invalid ones. This issue is fixed in iOS 13.
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  // First check if the navigation action should be blocked by the controller
  // and make sure to update the controller in the case that the controller
  // can't handle the request URL. Then use the embedders' policyDeciders to
  // either: 1- Handle the URL it self and return false to stop the controller
  // from proceeding with the navigation if needed. or 2- return true to allow
  // the navigation to be proceeded by the web controller.
  web::WebStatePolicyDecider::PolicyDecision policyDecision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  if (web::GetWebClient()->IsAppSpecificURL(requestURL)) {
    // `policyDecision` is initialized above this conditional to allow loads, so
    // it only needs to be overwritten if the load should be cancelled.
    if (![self shouldAllowAppSpecificURLNavigationAction:action
                                              transition:transition]) {
      policyDecision = web::WebStatePolicyDecider::PolicyDecision::Cancel();
    }
    if (policyDecision.ShouldAllowNavigation()) {
      [self.delegate navigationHandler:self createWebUIForURL:requestURL];
    }
  }

  const BOOL webControllerCanShow =
      web::UrlHasWebScheme(requestURL) ||
      web::GetWebClient()->IsAppSpecificURL(requestURL) ||
      requestURL.SchemeIs(url::kFileScheme) ||
      requestURL.SchemeIs(url::kAboutScheme) ||
      requestURL.SchemeIs(url::kBlobScheme);

  _shouldPerformDownload = NO;
  _shouldPerformDownload = action.shouldPerformDownload;

  __weak CRWWKNavigationHandler* weakSelf = self;
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        __strong CRWWKNavigationHandler* strongSelf = weakSelf;
        // The WebState may have been closed in the ShouldAllowRequest callback.
        if (!strongSelf || strongSelf.beingDestroyed) {
          decisionHandler(WKNavigationActionPolicyCancel);
          return;
        }

        if (!webControllerCanShow) {
          decision = web::WebStatePolicyDecider::PolicyDecision::Cancel();
        }

        [strongSelf answerDecisionHandler:decisionHandler
                      forNavigationAction:action
                       withPolicyDecision:decision
                                  webView:webView
                 forceBlockUniversalLinks:forceBlockUniversalLinks];
      });

  if (!policyDecision.ShouldAllowNavigation()) {
    std::move(callback).Run(policyDecision);
    return;
  }

  BOOL isUserInitiated = [self.delegate isUserInitiatedAction:action];
  BOOL hasTappedRecently =
      self.userInteractionState->HasUserTappedRecently(webView);

  BOOL isCrossOriginTargetFrame = NO;
  if (action.sourceFrame && action.targetFrame &&
      action.sourceFrame.webView == action.targetFrame.webView &&
      action.sourceFrame != action.targetFrame) {
    isCrossOriginTargetFrame = !url::IsSameOriginWith(
        web::GURLOriginWithWKSecurityOrigin(action.sourceFrame.securityOrigin),
        web::GURLOriginWithWKSecurityOrigin(action.targetFrame.securityOrigin));
  }

  BOOL isCrossOriginCrossWindow = NO;
  if (action.sourceFrame && action.targetFrame &&
      action.sourceFrame.webView != action.targetFrame.webView) {
    GURL sourceOrigin =
        web::GURLOriginWithWKSecurityOrigin(action.sourceFrame.securityOrigin);
    GURL targetOrigin =
        web::GURLOriginWithWKSecurityOrigin(action.targetFrame.securityOrigin);
    isCrossOriginCrossWindow =
        !url::IsSameOriginWith(sourceOrigin, targetOrigin);
  }

  // Ref: crbug.com/1408799
  if (base::FeatureList::IsEnabled(
          web::features::kPreventNavigationWithoutUserInteraction) &&
      isMainFrameNavigationAction && isCrossOriginTargetFrame &&
      !isUserInitiated) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  const web::WebStatePolicyDecider::RequestInfo requestInfo(
      transition, isMainFrameNavigationAction, isCrossOriginTargetFrame,
      isCrossOriginCrossWindow, isUserInitiated, hasTappedRecently);

  self.webStateImpl->ShouldAllowRequest(action.request, requestInfo,
                                        std::move(callback));
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse*)WKResponse
                      decisionHandler:
                          (void (^)(WKNavigationResponsePolicy))handler {
  [self didReceiveWKNavigationDelegateCallback];

  // If this is a error navigation, pass through.
  GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  if ([CRWErrorPageHelper isErrorPageFileURL:responseURL]) {
    handler(WKNavigationResponsePolicyAllow);
    // Set the mime type for error pages. This allows them to be treated as
    // HTML.
    [self updatePendingNavigationInfoFromNavigationResponse:WKResponse
                                                HTTPHeaders:nullptr];
    return;
  }

  if (self.pendingNavigationInfo.unsafeRedirect) {
    self.pendingNavigationInfo.cancelled = YES;
    self.pendingNavigationInfo.cancellationError =
        [NSError errorWithDomain:net::kNSErrorDomain
                            code:net::ERR_UNSAFE_REDIRECT
                        userInfo:nil];
    handler(WKNavigationResponsePolicyCancel);
    return;
  }

  scoped_refptr<net::HttpResponseHeaders> headers;
  if ([WKResponse.response isKindOfClass:[NSHTTPURLResponse class]]) {
    headers = net::CreateHeadersFromNSHTTPURLResponse(
        static_cast<NSHTTPURLResponse*>(WKResponse.response));
  }

  // The page will not be changed until this navigation is committed, so the
  // retrieved state will be pending until `didCommitNavigation` callback.
  [self updatePendingNavigationInfoFromNavigationResponse:WKResponse
                                              HTTPHeaders:headers];

  __weak CRWPendingNavigationInfo* weakPendingNavigationInfo =
      self.pendingNavigationInfo;
  auto callback = base::BindOnce(
      ^(web::WebStatePolicyDecider::PolicyDecision policyDecision) {
        if (policyDecision.ShouldCancelNavigation() &&
            WKResponse.canShowMIMEType && WKResponse.forMainFrame) {
          weakPendingNavigationInfo.cancelled = YES;
          weakPendingNavigationInfo.cancellationError =
              policyDecision.GetDisplayError();
        }

        handler(policyDecision.ShouldAllowNavigation()
                    ? WKNavigationResponsePolicyAllow
                    : WKNavigationResponsePolicyCancel);
      });

  if ([self shouldRenderResponse:WKResponse HTTPHeaders:headers.get()]) {
    const web::WebStatePolicyDecider::ResponseInfo response_info(
        WKResponse.forMainFrame);
    self.webStateImpl->ShouldAllowResponse(WKResponse.response, response_info,
                                           std::move(callback));
    return;
  }

  handler(WKNavigationResponsePolicyDownload);
}

- (void)webView:(WKWebView*)webView
    didStartProvisionalNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];

  GURL webViewURL = net::GURLWithNSURL(webView.URL);

  [self.navigationStates setState:web::WKNavigationState::STARTED
                    forNavigation:navigation];

  if (webViewURL.is_empty()) {
    // URL starts empty for window.open(""), by didCommitNavigation: callback
    // the URL will be "about:blank".
    webViewURL = GURL(url::kAboutBlankURL);
  }

  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];

  if (context) {
    // This is already seen and registered navigation.

    if (context->IsLoadingErrorPage()) {
      // This is loadHTMLString: navigation to display error page in web view.
      self.navigationState = web::WKNavigationState::REQUESTED;
      return;
    }

    if (![CRWErrorPageHelper isErrorPageFileURL:webViewURL] &&
        context->GetUrl() != webViewURL) {
      web::NavigationItem* item =
          web::GetItemWithUniqueID(self.navigationManagerImpl, context);

      // Update last seen URL because it may be changed by WKWebView (f.e. by
      // performing characters escaping).
      if (item) {
        // Item may not exist if navigation was stopped (see crbug.com/969915).
        item->SetURL(webViewURL);
        if ([CRWErrorPageHelper isErrorPageFileURL:webViewURL]) {
          item->SetVirtualURL([CRWErrorPageHelper
              failedNavigationURLFromErrorPageFileURL:webViewURL]);
        }
      }
      context->SetUrl(webViewURL);
    }

    self.webStateImpl->OnNavigationStarted(context);
    return;
  }

  // This is renderer-initiated navigation which was not seen before and
  // should be registered.

  // Renderer-initiated app-specific loads should only be allowed in these
  // specific cases:
  // 1) back/forward navigation to an app-specific URL should be allowed.
  // 2) navigation to an app-specific URL should be allowed from other
  //    app-specific URLs
  bool exemptedAppSpecificLoad = false;
  bool isBackForward =
      self.pendingNavigationInfo.navigationType == WKNavigationTypeBackForward;
  exemptedAppSpecificLoad = isBackForward || self.webStateImpl->HasWebUI();

  if (!web::GetWebClient()->IsAppSpecificURL(webViewURL) ||
      !exemptedAppSpecificLoad) {
    self.webStateImpl->ClearWebUI();
  }

  std::unique_ptr<web::NavigationContextImpl> navigationContext =
      [self.delegate navigationHandler:self
             registerLoadRequestForURL:webViewURL
                sameDocumentNavigation:NO
                        hasUserGesture:self.pendingNavigationInfo.hasUserGesture
                     rendererInitiated:YES];
  web::NavigationContextImpl* navigationContextPtr = navigationContext.get();

  // GetPendingItem which may be called inside OnNavigationStarted relies on
  // association between NavigationContextImpl and WKNavigation.
  [self.navigationStates setContext:std::move(navigationContext)
                      forNavigation:navigation];
  self.webStateImpl->OnNavigationStarted(navigationContextPtr);
  DCHECK_EQ(web::WKNavigationState::REQUESTED, self.navigationState);
}

- (void)webView:(WKWebView*)webView
    didReceiveServerRedirectForProvisionalNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];

  GURL webViewURL = net::GURLWithNSURL(webView.URL);

  [self.navigationStates setState:web::WKNavigationState::REDIRECTED
                    forNavigation:navigation];

  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];
  if (!context)
    return;

  // Redirecting to a data url is always unsafe.
  if (webViewURL.SchemeIs(url::kDataScheme) ||
      // Block redirects to JavaScript schemes. Ref: crbug.com/1509267
      webViewURL.SchemeIs(url::kJavaScriptScheme)) {
    self.pendingNavigationInfo.unsafeRedirect = YES;
  } else {
    context->SetUrl(webViewURL);
  }
  web::NavigationItemImpl* item =
      web::GetItemWithUniqueID(self.navigationManagerImpl, context);

  // Associated item can be a pending item, previously discarded by another
  // navigation. WKWebView allows multiple provisional navigations, while
  // Navigation Manager has only one pending navigation.
  if (item) {
    if (!self.pendingNavigationInfo.unsafeRedirect) {
      item->SetVirtualURL(webViewURL);
      item->SetURL(webViewURL);
    }
    // Redirects (3xx response code), must change POST requests to GETs.
    item->SetPostData(nil);
    item->ResetHttpRequestHeaders();
  }

  self.userInteractionState->ResetLastTransferTime();
  self.webStateImpl->OnNavigationRedirected(context);
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self didReceiveWKNavigationDelegateCallback];

  BOOL wasRedirected = [self.navigationStates stateForNavigation:navigation] ==
                       web::WKNavigationState::REDIRECTED;

  [self.navigationStates setState:web::WKNavigationState::PROVISIONALY_FAILED
                    forNavigation:navigation];

  // Ignore provisional navigation failure if a new navigation has been started,
  // for example, if a page is reloaded after the start of the provisional
  // load but before the load has been committed.
  if (![[self.navigationStates lastAddedNavigation] isEqual:navigation]) {
    return;
  }

  web::NavigationContextImpl* navigationContext =
      [self.navigationStates contextForNavigation:navigation];
  if (wasRedirected && navigationContext) {
    // If there was a redirect, change the URL to have the URL of the first
    // page.
    NSMutableDictionary* userInfo = [error.userInfo mutableCopy];
    userInfo[NSURLErrorFailingURLStringErrorKey] =
        base::SysUTF8ToNSString(navigationContext->GetUrl().spec());
    error = [NSError errorWithDomain:error.domain
                                code:error.code
                            userInfo:userInfo];
  }

  // Handle load cancellation for directly cancelled navigations without
  // handling their potential errors. Otherwise, handle the error.
  if (self.pendingNavigationInfo.cancelled) {
    if (self.pendingNavigationInfo.cancellationError) {
      // If the navigation was cancelled for a CancelAndDisplayError() policy
      // decision, load the error in the failed navigation.
      [self handleLoadError:error
              forNavigation:navigation
                    webView:webView
            provisionalLoad:YES];
    } else {
      [self handleCancelledError:error
                   forNavigation:navigation
                 provisionalLoad:YES];
    }
  } else if (error.code == NSURLErrorUnsupportedURL &&
             self.webStateImpl->HasWebUI()) {
    // This is a navigation to WebUI page.
    DCHECK(web::GetWebClient()->IsAppSpecificURL(
        net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey])));
  } else {
    [self handleLoadError:error
            forNavigation:navigation
                  webView:webView
          provisionalLoad:YES];
  }

  self.webStateImpl->RemoveAllWebFrames();
  // This must be reset at the end, since code above may need information about
  // the pending load.
  self.pendingNavigationInfo = nil;
  if (!web::IsWKWebViewSSLCertError(error)) {
    _certVerificationErrors->Clear();
  }

  // Remove the navigation to immediately get rid of pending item.
  if (web::WKNavigationState::NONE !=
      [self.navigationStates stateForNavigation:navigation]) {
    [self.navigationStates removeNavigation:navigation];
  }
}

- (void)webView:(WKWebView*)webView
    didCommitNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];

  // For reasons not yet fully understood, sometimes WKWebView triggers
  // `webView:didFinishNavigation` before `webView:didCommitNavigation`. If a
  // navigation is already finished, stop processing
  // (https://crbug.com/818796#c2).
  if ([self.navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::FINISHED)
    return;

  BOOL committedNavigation =
      [self.navigationStates isCommittedNavigation:navigation];

  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];
  if (context && !web::IsWKWebViewSSLCertError(context->GetError())) {
    _certVerificationErrors->Clear();
  }

  // Invariant: Every `navigation` should have a `context`. Note that violation
  // of this invariant is currently observed in production, but the cause is not
  // well understood. Based on the current frequency with which this invariant
  // fails to hold, removing null-checks on `context` would lead to a top-crash.
  UMA_HISTOGRAM_BOOLEAN("IOS.CommittedNavigationHasContext", context);

  GURL webViewURL = net::GURLWithNSURL(webView.URL);
  GURL currentWKItemURL =
      net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
  UMA_HISTOGRAM_BOOLEAN("IOS.CommittedURLMatchesCurrentItem",
                        webViewURL == currentWKItemURL);

  // TODO(crbug.com/41356827): Always use
  // webView.backForwardList.currentItem.URL to obtain lastCommittedURL once
  // loadHTML: is no longer user for WebUI.
  if (webViewURL.is_empty()) {
    // It is possible for `webView.URL` to be nil, in which case
    // webView.backForwardList.currentItem.URL will return the right committed
    // URL (crbug.com/784480).
    webViewURL = currentWKItemURL;
  } else if (context &&
             context->GetUrl() == currentWKItemURL) {
    // If webView.backForwardList.currentItem.URL matches `context`, then this
    // is a known edge case where `webView.URL` is wrong.
    // TODO(crbug.com/41379040): Remove this workaround.
    webViewURL = currentWKItemURL;
  }

  if (context) {
    if (self.pendingNavigationInfo.HTTPHeaders)
      context->SetResponseHeaders(self.pendingNavigationInfo.HTTPHeaders);
  }

  [self.delegate navigationHandlerDisplayWebView:self];

  // `context` will be nil if this navigation has been already committed and
  // finished.
  if (context) {
    web::NavigationManager* navigationManager =
        self.webStateImpl->GetNavigationManager();
    GURL pendingURL;
    web::NavigationItem* pendingItem = nullptr;
    if (navigationManager->GetPendingItemIndex() == -1) {
      // Item may not exist if navigation was stopped (see
      // crbug.com/969915).
      pendingItem = context->GetItem();
    } else {
      pendingItem = navigationManager->GetPendingItem();
    }

    if (pendingItem) {
      pendingURL = pendingItem->GetURL();
    }

    if (self.pendingNavigationInfo.MIMEType) {
      context->SetMimeType(self.pendingNavigationInfo.MIMEType);
    } else if (pendingItem && !context->GetMimeType()) {
      // For navigations handled directly by WebKit's back/forward cache, such
      // as swipe-triggered navigations, `pendingNavigationInfo` will not get
      // populated since there is no `decidePolicyForNavigationResponse`
      // callback. The context's MIME type will also not get set when creating
      // the navigation, since swipe-triggered navigations are directly
      // initiated by WebKit. In this case, use the MIME type stored with the
      // navigation item.
      context->SetMimeType(
          web::WKBackForwardListItemHolder::FromNavigationItem(pendingItem)
              ->mime_type());
    }

    if ((pendingURL == webViewURL) || (context->IsLoadingHtmlString())) {
      // Commit navigation if at least one of these is true:
      //  - Navigation has pending item (this should always be true, but
      //    pending item may not exist due to crbug.com/925304).
      //  - Navigation is loadHTMLString:baseURL: navigation, which does not
      //    create a pending item, but modifies committed item instead.
      //  - Transition type is reload with Legacy Navigation Manager (Legacy
      //    Navigation Manager does not create pending item for reload due to
      //    crbug.com/676129)
      context->SetHasCommitted(true);
    }
    self.webStateImpl->SetContentsMimeType(
        base::SysNSStringToUTF8(context->GetMimeType()));
  }

  [self commitPendingNavigationInfoInWebView:webView];

  self.webStateImpl->RemoveAllWebFrames();

  // This point should closely approximate the document object change, so reset
  // the list of injected scripts to those that are automatically injected.  For
  // WebUI, let the window ID be injected when the `loadHTMLString:baseURL`
  // navigation is committed.
  const std::string& mime_type = self.webStateImpl->GetContentsMimeType();
  if (web::IsContentTypeHtml(mime_type) || web::IsContentTypeImage(mime_type) ||
      mime_type.empty()) {
    // In unit tests MIME type will be empty, because loadHTML:forURL: does
    // not notify web view delegate about received response, so web controller
    // does not get a chance to properly update MIME type.
    self.webStateImpl->RetrieveExistingFrames();
  }

  if (committedNavigation) {
    // WKWebView called didCommitNavigation: with incorrect WKNavigation object.
    // Correct WKNavigation object for this navigation was deallocated because
    // WKWebView mistakenly cancelled the navigation and called
    // didFailProvisionalNavigation. As a result web::NavigationContext for this
    // navigation does not exist anymore. Find correct navigation item and make
    // it committed.
    [self resetDocumentSpecificState];
    [self.delegate navigationHandlerDidStartLoading:self];
  } else if (context) {
    // If `navigation` is nil (which happens for windows open by DOM), then it
    // should be the first and the only pending navigation.
    BOOL isLastNavigation =
        !navigation ||
        [[self.navigationStates lastAddedNavigation] isEqual:navigation];
    if (isLastNavigation ||
        self.navigationManagerImpl->GetPendingItemIndex() == -1) {
      [self webPageChangedWithContext:context webView:webView];
    }
  }

  // The WebView URL can't always be trusted when multiple pending navigations
  // are occuring, as a navigation could commit after a new navigation has
  // started (and thus the WebView URL will be the URL of the new navigation).
  // See crbug.com/1127025.
  BOOL hasMultiplePendingNavigations =
      [self.navigationStates pendingNavigations].count > 1;

  // When loading an error page, the context has the correct URL whereas the
  // webView has the file URL.
  BOOL isErrorPage = [CRWErrorPageHelper isErrorPageFileURL:webViewURL];

  BOOL shouldUseContextURL =
      context && (isErrorPage || hasMultiplePendingNavigations);
  GURL documentURL = shouldUseContextURL ? context->GetUrl() : webViewURL;

  // This is the point where the document's URL has actually changed.
  [self.delegate navigationHandler:self
                    setDocumentURL:documentURL
                           context:context];

  // No further code relies an existance of pending item, so this navigation can
  // be marked as "committed".
  [self.navigationStates setState:web::WKNavigationState::COMMITTED
                    forNavigation:navigation];

  if (!committedNavigation && context && !context->IsLoadingErrorPage()) {
    self.webStateImpl->OnNavigationFinished(context);
  }

  // The actual navigation item will not be committed until the native content
  // or WebUI is shown.
  if (context && !context->GetUrl().SchemeIs(url::kAboutScheme)) {
    [self.delegate webViewHandlerUpdateSSLStatusForCurrentNavigationItem:self];
    if (!context->IsLoadingErrorPage()) {
      [self setLastCommittedNavigationItemTitle:webView.title];
    }
  }
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];

  // Sometimes `webView:didFinishNavigation` arrives before
  // `webView:didCommitNavigation`. Explicitly trigger post-commit processing.
  bool navigationCommitted =
      [self.navigationStates isCommittedNavigation:navigation];
  UMA_HISTOGRAM_BOOLEAN("IOS.WKWebViewFinishBeforeCommit",
                        !navigationCommitted);
  if (!navigationCommitted) {
    [self webView:webView didCommitNavigation:navigation];
    DCHECK_EQ(web::WKNavigationState::COMMITTED,
              [self.navigationStates stateForNavigation:navigation]);
  }

  // Sometimes `didFinishNavigation` callback arrives after `stopLoading` has
  // been called. Abort in this case.
  if ([self.navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::NONE) {
    return;
  }

  GURL webViewURL = net::GURLWithNSURL(webView.URL);
  GURL currentWKItemURL =
      net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
  UMA_HISTOGRAM_BOOLEAN("IOS.FinishedURLMatchesCurrentItem",
                        webViewURL == currentWKItemURL);

  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];
  web::NavigationItemImpl* item =
      context ? web::GetItemWithUniqueID(self.navigationManagerImpl, context)
              : nullptr;
  // Item may not exist if navigation was stopped (see crbug.com/969915).
  UMA_HISTOGRAM_BOOLEAN("IOS.FinishedNavigationHasContext", context);
  UMA_HISTOGRAM_BOOLEAN("IOS.FinishedNavigationHasItem", item);

  if (context && item) {
    if (context->GetUrl() == currentWKItemURL) {
      // If webView.backForwardList.currentItem.URL matches `context`, then this
      // is a known edge case where `webView.URL` is wrong.
      // TODO(crbug.com/41379040): Remove this workaround.
      webViewURL = currentWKItemURL;
    }

    if (currentWKItemURL == webViewURL &&
        currentWKItemURL != context->GetUrl() &&
        item == self.navigationManagerImpl->GetLastCommittedItem() &&
        item->GetURL().DeprecatedGetOriginAsURL() ==
            currentWKItemURL.DeprecatedGetOriginAsURL()) {
      // WKWebView sometimes changes URL on the same navigation, likely due to
      // location.replace() or history.replaceState in onload handler that does
      // not change the origin. It's safe to update `item` and `context` URL
      // because they are both associated to WKNavigation*, which is a stable ID
      // for the navigation. See https://crbug.com/869540 for a real-world case.
      item->SetURL(currentWKItemURL);
      context->SetUrl(currentWKItemURL);
    }

    if (context->GetError()) {
      [self loadErrorPageForNavigationItem:item
                         navigationContext:navigation
                                   webView:webView];
    }
  }

  [self updateStateForNavigation:navigation toFinishedWithContext:context];
}

- (void)updateStateForNavigation:(WKNavigation*)navigation
           toFinishedWithContext:(web::NavigationContextImpl*)context {
  [self.navigationStates setState:web::WKNavigationState::FINISHED
                    forNavigation:navigation];

  [self.delegate webViewHandler:self didFinishNavigation:context];

  // Remove the navigation to immediately get rid of pending item. Navigation
  // should not be cleared, however, in the case of a committed interstitial
  // for an SSL error.
  if (web::WKNavigationState::NONE !=
          [self.navigationStates stateForNavigation:navigation] &&
      !(context && web::IsWKWebViewSSLCertError(context->GetError()))) {
    [self.navigationStates removeNavigation:navigation];
  }
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self didReceiveWKNavigationDelegateCallback];

  // `webView:didFailNavigation:withError:` may be called after the document has
  // already loaded which should be ignored. This can happen when navigating
  // back to a page which loads from the back forward cache. See
  // crbug.com/1249735 for more details. This will also be called when the user
  // cancels an active page load.
  if (self.navigationState == web::WKNavigationState::FINISHED &&
      error.code == NSURLErrorCancelled &&
      [error.domain isEqualToString:NSURLErrorDomain]) {
    _certVerificationErrors->Clear();
    return;
  }

  // `webView:didFailNavigation:withError:` may be called when rendering an
  // office document which should be ignored because the `webView` will already
  // be displaying its own error. Additionally, in these cases, JavaScript can
  // not be run on these pages (in order to display our own error message). See
  // crbug.com/1489167 for more details.
  if ([error.domain isEqualToString:@"OfficeImportErrorDomain"]) {
    [self.navigationStates setState:web::WKNavigationState::FINISHED
                      forNavigation:navigation];
    self.webStateImpl->RemoveAllWebFrames();
    _certVerificationErrors->Clear();
    return;
  }

  if ([error.domain isEqualToString:@(web::kWebKitErrorDomain)] &&
      error.code == web::kWebKitErrorPlugInLoadFailed) {
    // In cases where a Plug-in handles the load, mark the navigation as
    // successful even though it is reported as a failed navigation.
    web::NavigationContextImpl* navigationContext =
        [self.navigationStates contextForNavigation:navigation];
    [self updateStateForNavigation:navigation
             toFinishedWithContext:navigationContext];
    return;
  }

  [self.navigationStates setState:web::WKNavigationState::FAILED
                    forNavigation:navigation];

  [self handleLoadError:error
          forNavigation:navigation
                webView:webView
        provisionalLoad:NO];
  self.webStateImpl->RemoveAllWebFrames();
  _certVerificationErrors->Clear();
  [self forgetNullWKNavigation:navigation];
}

- (void)webView:(WKWebView*)webView
    didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge*)challenge
                    completionHandler:
                        (void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler {
  [self didReceiveWKNavigationDelegateCallback];

  NSString* authMethod = challenge.protectionSpace.authenticationMethod;
  if ([authMethod isEqualToString:NSURLAuthenticationMethodHTTPBasic] ||
      [authMethod isEqualToString:NSURLAuthenticationMethodNTLM] ||
      [authMethod isEqualToString:NSURLAuthenticationMethodHTTPDigest]) {
    [self handleHTTPAuthForChallenge:challenge
                   completionHandler:completionHandler];
    return;
  }

  if (![authMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
    completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    return;
  }

  SecTrustRef trust = challenge.protectionSpace.serverTrust;
  base::apple::ScopedCFTypeRef<SecTrustRef> scopedTrust(
      trust, base::scoped_policy::RETAIN);
  __weak CRWWKNavigationHandler* weakSelf = self;
  [self.certVerificationController
      decideLoadPolicyForTrust:scopedTrust
                          host:challenge.protectionSpace.host
             completionHandler:^(web::CertAcceptPolicy policy,
                                 net::CertStatus status) {
               CRWWKNavigationHandler* strongSelf = weakSelf;
               if (!strongSelf) {
                 completionHandler(
                     NSURLSessionAuthChallengeRejectProtectionSpace, nil);
                 return;
               }
               [strongSelf processAuthChallenge:challenge
                            forCertAcceptPolicy:policy
                                     certStatus:status
                              completionHandler:completionHandler];
             }];
}

- (void)webView:(WKWebView*)webView
     authenticationChallenge:(NSURLAuthenticationChallenge*)challenge
    shouldAllowDeprecatedTLS:(void (^)(BOOL))decisionHandler {
  [self didReceiveWKNavigationDelegateCallback];
  DCHECK(challenge);
  DCHECK(decisionHandler);

  // TLS 1.0/1.1 is no longer supported in Chrome. When a connection occurs that
  // uses deprecated TLS versions, reject it. For navigations (which are user
  // visible), cancel the navigation and set the error code to
  // ERR_SSL_VERSION_OR_CIPHER_MISMATCH.
  if (self.pendingNavigationInfo) {
    self.pendingNavigationInfo.cancelled = YES;
    self.pendingNavigationInfo.cancellationError =
        [NSError errorWithDomain:net::kNSErrorDomain
                            code:net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH
                        userInfo:nil];
  }
  decisionHandler(NO);
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
  [self didReceiveWKNavigationDelegateCallback];

  _certVerificationErrors->Clear();
  _webProcessCrashed = YES;
  self.webStateImpl->RemoveAllWebFrames();

  [self.delegate navigationHandlerWebProcessDidCrash:self];
}

- (void)webView:(WKWebView*)webView
     navigationAction:(WKNavigationAction*)navigationAction
    didBecomeDownload:(WKDownload*)WKDownload {
  // As Chromium never return WKNavigationResponsePolicyDownload
  // when deciding the policy for an action, WebKit should never
  // invoke this delegate method.
  NOTREACHED_IN_MIGRATION();
}

- (void)webView:(WKWebView*)webView
    navigationResponse:(WKNavigationResponse*)navigationResponse
     didBecomeDownload:(WKDownload*)WKDownload {
  // Send navigation callback if the download occurs in the main frame.
  if (navigationResponse.forMainFrame) {
    const GURL responseURL =
        net::GURLWithNSURL(navigationResponse.response.URL);
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:responseURL];

    // Context lookup can fail in rare cases (e.g. after certain redirects,
    // see https://crbug.com/820375 for details). In that case, it's not
    // possible to locate the correct context to call OnNavigationFinished().
    // Not sending this event does not cause any major issue, so do nothing
    // if `context` cannot be found (i.e. this is not a security issue).
    if (context) {
      context->SetIsDownload(true);
      context->ReleaseItem();
      self.webStateImpl->OnNavigationFinished(context);
    }
  }

  // Discard the pending item to ensure that the current URL is not different
  // from what is displayed on the view.
  self.navigationManagerImpl->DiscardNonCommittedItems();

  [_nativeTaskBridges
      addObject:[[DownloadNativeTaskBridge alloc] initWithDownload:WKDownload
                                                          delegate:self]];
}

// Used to set response url, content length, mimetype and http response headers
// in CRWWkNavigationHandler so method can interact with WKWebView. Returns NO
// if the download cannot be started.
- (BOOL)onDownloadNativeTaskBridgeReadyForDownload:
    (DownloadNativeTaskBridge*)bridge {
  NS_VALID_UNTIL_END_OF_SCOPE
  DownloadNativeTaskBridge* nativeTaskBridge = bridge;
  [_nativeTaskBridges removeObject:bridge];
  if (!self.webStateImpl)
    return NO;

  const GURL responseURL = net::GURLWithNSURL(bridge.response.URL);
  const int64_t contentLength = bridge.response.expectedContentLength;
  const std::string MIMEType =
      base::SysNSStringToUTF8(bridge.response.MIMEType);

  std::string contentDisposition;
  scoped_refptr<net::HttpResponseHeaders> headers;
  if ([bridge.response isKindOfClass:[NSHTTPURLResponse class]]) {
    headers = net::CreateHeadersFromNSHTTPURLResponse(
        static_cast<NSHTTPURLResponse*>(bridge.response));
    if (headers) {
      headers->GetNormalizedHeader("content-disposition", &contentDisposition);
    }
  }

  NSString* HTTPMethod = bridge.download.originalRequest.HTTPMethod;
  web::DownloadController::FromBrowserState(
      self.webStateImpl->GetBrowserState())
      ->CreateNativeDownloadTask(self.webStateImpl, [NSUUID UUID].UUIDString,
                                 responseURL, HTTPMethod, contentDisposition,
                                 contentLength, MIMEType, bridge);
  return YES;
}

- (void)resumeDownloadNativeTask:(NSData*)data
               completionHandler:(void (^)(WKDownload*))completionHandler {
  [self.delegate resumeDownloadWithData:data
                      completionHandler:completionHandler];
}

#pragma mark - Private methods

// Returns the UserAgent that needs to be used for the `navigationAction` from
// the `webView`.
- (web::UserAgentType)userAgentForNavigationAction:
                          (WKNavigationAction*)navigationAction
                                           webView:(WKWebView*)webView {
  web::NavigationItem* item = nullptr;
  web::UserAgentType userAgentType = web::UserAgentType::NONE;
  if (navigationAction.navigationType == WKNavigationTypeBackForward) {
    // Use the item associated with the back/forward item to have the same user
    // agent as the one used the first time.
    item = [[CRWNavigationItemHolder
        holderForBackForwardListItem:webView.backForwardList.currentItem]
        navigationItem];
    // In some cases, the associated item isn't found. In that case, follow the
    // code path for the non-backforward navigations. See crbug.com/1121950.
    if (item)
      userAgentType = item->GetUserAgentType();
  }
  if (!item) {
    // Get the visible item. There is no guarantee that the pending item belongs
    // to this navigation but it is very likely that it is the case. If there is
    // no pending item, it is probably a render initiated navigation. Use the
    // UserAgent of the previous navigation. This will also return the
    // navigation item of the restoration if a restoration occurs. Request the
    // pending item explicitly as the visible item might be the committed item
    // if the pending navigation isn't user triggered.
    item = self.navigationManagerImpl->GetPendingItem();
    if (!item)
      item = self.navigationManagerImpl->GetVisibleItem();

    if (item && item->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK) {
      // When navigating forward to a restored page, the WKNavigationAction is
      // of type reload and not BackForward. The item is correctly set a
      // back/forward, so it is possible to use it.
      userAgentType = item->GetUserAgentType();
    } else {
      userAgentType = self.webStateImpl->GetUserAgentForNextNavigation(
          net::GURLWithNSURL(navigationAction.request.URL));
    }
  }

  if (item && web::GetWebClient()->IsAppSpecificURL(item->GetVirtualURL())) {
    // In case of app specific URL, no specificUser Agent needs to be used.
    // However, to have a custom User Agent and a WKContentMode, use mobile.
    userAgentType = web::UserAgentType::MOBILE;
  }
  return userAgentType;
}

- (web::NavigationManagerImpl*)navigationManagerImpl {
  return &(self.webStateImpl->GetNavigationManagerImpl());
}

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForWebViewHandler:self];
}

- (web::UserInteractionState*)userInteractionState {
  return [self.delegate userInteractionStateForWebViewHandler:self];
}

- (CRWCertVerificationController*)certVerificationController {
  return [self.delegate certVerificationControllerForNavigationHandler:self];
}

- (GURL)documentURL {
  return [self.delegate documentURLForWebViewHandler:self];
}

- (web::NavigationItemImpl*)currentNavItem {
  return self.navigationManagerImpl
             ? self.navigationManagerImpl->GetCurrentItemImpl()
             : nullptr;
}

// This method should be called on receiving WKNavigationDelegate callbacks.
- (void)didReceiveWKNavigationDelegateCallback {
  DCHECK(!self.beingDestroyed);
}

// Extracts navigation info from WKNavigationAction and sets it as a pending.
// Some pieces of navigation information are only known in
// `decidePolicyForNavigationAction`, but must be in a pending state until
// `didgo/Navigation` where it becames current.
- (void)createPendingNavigationInfoFromNavigationAction:
    (WKNavigationAction*)action {
  if (action.targetFrame.mainFrame) {
    self.pendingNavigationInfo = [[CRWPendingNavigationInfo alloc] init];
    self.pendingNavigationInfo.referrer =
        [action.request valueForHTTPHeaderField:kReferrerHeaderName];
    self.pendingNavigationInfo.navigationType = action.navigationType;
    self.pendingNavigationInfo.HTTPMethod = action.request.HTTPMethod;
    self.pendingNavigationInfo.hasUserGesture =
        web::GetNavigationActionInitiationType(action) ==
        web::NavigationActionInitiationType::kUserInitiated;
  }
}

// Extracts navigation info from WKNavigationResponse and sets it as a pending.
// Some pieces of navigation information are only known in
// `decidePolicyForNavigationResponse`, but must be in a pending state until
// `didCommitNavigation` where it becames current.
- (void)
    updatePendingNavigationInfoFromNavigationResponse:
        (WKNavigationResponse*)response
                                          HTTPHeaders:
                                              (const scoped_refptr<
                                                  net::HttpResponseHeaders>&)
                                                  headers {
  if (response.isForMainFrame) {
    if (!self.pendingNavigationInfo) {
      self.pendingNavigationInfo = [[CRWPendingNavigationInfo alloc] init];
    }
    self.pendingNavigationInfo.MIMEType = response.response.MIMEType;
    self.pendingNavigationInfo.HTTPHeaders = headers;
  }
}

// Returns YES if the navigation action is associated with a main frame request.
- (BOOL)isMainFrameNavigationAction:(WKNavigationAction*)action {
  if (action.targetFrame) {
    return action.targetFrame.mainFrame;
  }
  // According to WKNavigationAction documentation, in the case of a new window
  // navigation, target frame will be nil. In this case check if the
  // `sourceFrame` is the mainFrame.
  return action.sourceFrame.mainFrame;
}

// Returns YES if the given `action` should be allowed to continue for app
// specific URL. If this returns NO, the navigation should be cancelled.
// App specific pages have elevated privileges and WKWebView uses the same
// renderer process for all page frames. With that Chromium does not allow
// running App specific pages in the same process as a web site from the
// internet. Allows navigation to app specific URL in the following cases:
//   - last committed URL is app specific
//   - navigation not a new navigation (back-forward or reload)
//   - navigation is typed, generated or bookmark
//   - navigation is performed in iframe and main frame is app-specific page
- (BOOL)shouldAllowAppSpecificURLNavigationAction:(WKNavigationAction*)action
                                       transition:
                                           (ui::PageTransition)pageTransition {
  GURL requestURL = net::GURLWithNSURL(action.request.URL);
  DCHECK(web::GetWebClient()->IsAppSpecificURL(requestURL));
  if (web::GetWebClient()->IsAppSpecificURL(
          self.webStateImpl->GetLastCommittedURL())) {
    // Last committed page is also app specific and navigation should be
    // allowed.
    return YES;
  }

  if (pageTransition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    // Allow back-forward navigations.
    return YES;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(pageTransition,
                                                  ui::PAGE_TRANSITION_TYPED)) {
    return YES;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(
          pageTransition, ui::PAGE_TRANSITION_GENERATED)) {
    return YES;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(
          pageTransition, ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    return YES;
  }

  // Allow navigation to WebUI pages from error pages.
  if ([CRWErrorPageHelper isErrorPageFileURL:self.documentURL]) {
    return YES;
  }

  GURL mainDocumentURL = net::GURLWithNSURL(action.request.mainDocumentURL);
  if (web::GetWebClient()->IsAppSpecificURL(mainDocumentURL) &&
      !action.sourceFrame.mainFrame) {
    // AppSpecific URLs are allowed inside iframe if the main frame is also
    // app specific page.
    return YES;
  }

  return NO;
}

// Caches request POST data in the given session entry.
- (void)cachePOSTDataForRequest:(NSURLRequest*)request
               inNavigationItem:(web::NavigationItemImpl*)item {
  NSUInteger maxPOSTDataSizeInBytes = 4096;
  NSString* cookieHeaderName = @"cookie";

  DCHECK(item);
  const bool shouldUpdateEntry =
      ui::PageTransitionCoreTypeIs(item->GetTransitionType(),
                                   ui::PAGE_TRANSITION_FORM_SUBMIT) &&
      ![request HTTPBodyStream] &&  // Don't cache streams.
      !item->HasPostData() &&
      item->GetURL() == net::GURLWithNSURL([request URL]);
  const bool belowSizeCap =
      [[request HTTPBody] length] < maxPOSTDataSizeInBytes;
  DLOG_IF(WARNING, shouldUpdateEntry && !belowSizeCap)
      << "Data in POST request exceeds the size cap (" << maxPOSTDataSizeInBytes
      << " bytes), and will not be cached.";

  if (shouldUpdateEntry && belowSizeCap) {
    item->SetPostData([request HTTPBody]);
    item->ResetHttpRequestHeaders();
    item->AddHttpRequestHeaders([request allHTTPHeaderFields]);
    // Don't cache the "Cookie" header.
    // According to NSURLRequest documentation, `-valueForHTTPHeaderField:` is
    // case insensitive, so it's enough to test the lower case only.
    if ([request valueForHTTPHeaderField:cookieHeaderName]) {
      // Case insensitive search in `headers`.
      NSSet<NSString*>* cookieKeys = [item->GetHttpRequestHeaders()
          keysOfEntriesPassingTest:^(NSString* key, NSString* obj, BOOL* stop) {
            const BOOL found =
                [key caseInsensitiveCompare:cookieHeaderName] == NSOrderedSame;
            *stop = found;
            return found;
          }];
      DCHECK_EQ(1u, [cookieKeys count]);
      item->RemoveHttpRequestHeaderForKey([cookieKeys anyObject]);
    }
  }
}

// If YES, the page should be closed if it successfully redirects to a native
// application, for example if a new tab redirects to the App Store.
- (BOOL)shouldClosePageOnNativeApplicationLoad {
  // The page should be closed if it was initiated by the DOM and there has been
  // no user interaction with the page since the web view was created, or if the
  // page has no navigation items. An exception to that when an URL redirect to
  // an application was initiated from another application (intent), in that
  // case a prompt will show and page initiating the redirect needs to stay
  // open to make sure that a prompt is properly owned and to give the user
  // context about that prompt.
  BOOL rendererInitiatedWithoutInteraction =
      self.webStateImpl->HasOpener() &&
      !self.userInteractionState
           ->UserInteractionRegisteredSinceWebViewCreated();
  BOOL noNavigationItems = !(self.navigationManagerImpl->GetItemCount());
  BOOL isIntent = !self.webStateImpl->HasOpener() && noNavigationItems;
  return !isIntent &&
         (rendererInitiatedWithoutInteraction || noNavigationItems);
}

// Returns YES if response should be rendered in WKWebView.
- (BOOL)shouldRenderResponse:(WKNavigationResponse*)WKResponse
                 HTTPHeaders:(net::HttpResponseHeaders*)headers {
  if (headers) {
    std::string contentDisposition;
    headers->GetNormalizedHeader("content-disposition", &contentDisposition);
    net::HttpContentDisposition parsedContentDisposition(contentDisposition,
                                                         std::string());
    if (parsedContentDisposition.is_attachment()) {
      return NO;
    }
  }

  if (_shouldPerformDownload) {
    return NO;
  }

  if (!WKResponse.canShowMIMEType) {
    return NO;
  }

  // TODO(crbug.com/40219220): Remove this when `canShowMIMEType` is fixed.
  // On iOS 15 `canShowMIMEType` returns true for AR files although WebKit is
  // not capable of displaying them natively.
  NSString* MIMEType = WKResponse.response.MIMEType;
  if ([MIMEType isEqualToString:@"model/vnd.pixar.usd"] ||
      [MIMEType isEqualToString:@"model/usd"] ||
      [MIMEType isEqualToString:@"model/vnd.usdz+zip"] ||
      [MIMEType isEqualToString:@"model/vnd.pixar.usd"] ||
      [MIMEType isEqualToString:@"model/vnd.reality"]) {
    return NO;
  }

  GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  if (responseURL.SchemeIs(url::kDataScheme) && WKResponse.forMainFrame) {
    // Block rendering data URLs for renderer-initiated navigations in main
    // frame to prevent abusive behavior (crbug.com/890558).
    web::NavigationContext* context =
        [self contextForPendingMainFrameNavigationWithURL:responseURL];
    if (!context) {
      // If the data URL is originally data://foo/bar instead of data:foo/bar,
      // then the URL is transformed to data:///bar. Considering that the "foo"
      // part of the URL is lost, it doesn't really make sense to try to match
      // the URL as it would only work for text.
      return NO;
    }
    // If the server is doing a redirect on a user reload, the navigation is
    // treated as a reload instead of a redirect. See crbug.com/1165654.
    web::NavigationItem* lastCommittedItem =
        self.navigationManagerImpl->GetLastCommittedItem();
    BOOL isFakeReload = PageTransitionCoreTypeIs(context->GetPageTransition(),
                                                 ui::PAGE_TRANSITION_RELOAD) &&
                        lastCommittedItem &&
                        lastCommittedItem->GetURL() != responseURL;
    if (context->IsRendererInitiated() || isFakeReload) {
      return NO;
    }
  }

  return YES;
}

// WKNavigation objects are used as a weak key to store web::NavigationContext.
// WKWebView manages WKNavigation lifetime and destroys them after the
// navigation is finished. However for window opening navigations WKWebView
// passes null WKNavigation to WKNavigationDelegate callbacks and strong key is
// used to store web::NavigationContext. Those "null" navigations have to be
// cleaned up manually by calling this method.
- (void)forgetNullWKNavigation:(WKNavigation*)navigation {
  if (!navigation)
    [self.navigationStates removeNavigation:navigation];
}

// Returns the warning type to be shown for <form> posts, or kNone if no warning
// should be shown.
- (web::FormWarningType)formWarningType:(WKNavigationAction*)action {
  if (action.navigationType == WKNavigationTypeFormResubmitted) {
    return web::FormWarningType::kRepost;
  }
  if (web::GetWebClient()->IsInsecureFormWarningEnabled(
          self.webStateImpl->GetBrowserState()) &&
      action.navigationType == WKNavigationTypeFormSubmitted) {
    if (action.sourceFrame) {
      GURL source_url = web::GURLOriginWithWKSecurityOrigin(
          action.sourceFrame.securityOrigin);
      GURL form_action_url = net::GURLWithNSURL(action.request.URL);
      if (security_interstitials::IsInsecureFormActionOnSecureSource(
              source_url, form_action_url)) {
        return web::FormWarningType::kInsecureForm;
      }
    }
  }
  return web::FormWarningType::kNone;
}

// This method should be called on deciding policy for navigation action. It
// Answers the `decisionHandler` with a final decision caculated with passed
// `policyDecision`. The passed `policyDecision` should be determined by some
// conditions and policy deciders
- (void)answerDecisionHandler:
            (void (^)(WKNavigationActionPolicy))decisionHandler
          forNavigationAction:(WKNavigationAction*)action
           withPolicyDecision:
               (web::WebStatePolicyDecider::PolicyDecision)policyDecision
                      webView:(WKWebView*)webView
     forceBlockUniversalLinks:(BOOL)forceBlockUniversalLinks {
  if (policyDecision.ShouldAllowNavigation()) {
    if ([[action.request HTTPMethod] isEqualToString:@"POST"]) {
      web::FormWarningType warning_type = [self formWarningType:action];
      // Display the confirmation dialog if a form repost is detected.
      if (warning_type != web::FormWarningType::kNone) {
        self.webStateImpl->ShowRepostFormWarningDialog(
            warning_type, base::BindOnce(^(bool shouldContinue) {
              if (self.beingDestroyed) {
                decisionHandler(WKNavigationActionPolicyCancel);
              } else if (shouldContinue) {
                decisionHandler(WKNavigationActionPolicyAllow);
              } else {
                decisionHandler(WKNavigationActionPolicyCancel);
                if (action.targetFrame.mainFrame) {
                  [self.pendingNavigationInfo setCancelled:YES];
                }
              }
            }));
        return;
      }

      web::NavigationItemImpl* item =
          self.navigationManagerImpl->GetCurrentItemImpl();
      // TODO(crbug.com/40449786): Remove this check once it's no longer
      // possible to have no current entries.
      if (item)
        [self cachePOSTDataForRequest:action.request inNavigationItem:item];
    }
  } else {
    if (action.targetFrame.mainFrame) {
      if (!self.beingDestroyed && policyDecision.ShouldDisplayError()) {
        DCHECK(policyDecision.GetDisplayError());

        // Navigation was blocked by `ShouldProvisionallyFailRequest`. Cancel
        // load of page.
        decisionHandler(WKNavigationActionPolicyCancel);

        ui::PageTransition transition =
            [self pageTransitionFromNavigationType:action.navigationType];

        [self displayError:policyDecision.GetDisplayError()
            forCancelledNavigationToURL:action.request.URL
                              inWebView:webView
                         withTransition:transition];
        return;
      }

      [self.pendingNavigationInfo setCancelled:YES];
      if (self.navigationManagerImpl->GetPendingItemIndex() == -1) {
        // Discard the new pending item to ensure that the current URL is not
        // different from what is displayed on the view. There is no need to
        // reset pending item index for a different pending back-forward
        // navigation.
        self.navigationManagerImpl->DiscardNonCommittedItems();
      }

      web::NavigationContextImpl* context = [self
          contextForPendingMainFrameNavigationWithURL:net::GURLWithNSURL(
                                                          action.request.URL)];
      if (context) {
        // Destroy associated pending item, because this will be the last
        // WKWebView callback for this navigation context.
        context->ReleaseItem();
      }

      if (!self.beingDestroyed &&
          [self shouldClosePageOnNativeApplicationLoad]) {
        self.webStateImpl->CloseWebState();
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
      }
    }
  }

  if (policyDecision.ShouldCancelNavigation()) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  BOOL isOffTheRecord = self.webStateImpl->GetBrowserState()->IsOffTheRecord();
  decisionHandler(web::GetAllowNavigationActionPolicy(
      isOffTheRecord || forceBlockUniversalLinks));
}

#pragma mark - Auth Challenge

// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to
// reply with NSURLSessionAuthChallengeDisposition and credentials.
- (void)processAuthChallenge:(NSURLAuthenticationChallenge*)challenge
         forCertAcceptPolicy:(web::CertAcceptPolicy)policy
                  certStatus:(net::CertStatus)certStatus
           completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                       NSURLCredential*))completionHandler {
  SecTrustRef trust = challenge.protectionSpace.serverTrust;
  if (policy == web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_ACCEPTED_BY_USER) {
    // Cert is invalid, but user agreed to proceed, override default behavior.
    completionHandler(NSURLSessionAuthChallengeUseCredential,
                      [NSURLCredential credentialForTrust:trust]);
    return;
  }

  if (policy != web::CERT_ACCEPT_POLICY_ALLOW &&
      SecTrustGetCertificateCount(trust)) {
    // The cert is invalid and the user has not agreed to proceed. Cache the
    // cert verification result in `_certVerificationErrors`, so that it can
    // later be reused inside `didFailProvisionalNavigation:`.
    // The leaf cert is used as the key, because the chain provided by
    // `didFailProvisionalNavigation:` will differ (it is the server-supplied
    // chain), thus if intermediates were considered, the keys would mismatch.

    scoped_refptr<net::X509Certificate> leafCert = nil;
    base::apple::ScopedCFTypeRef<CFArrayRef> certificateChain(
        SecTrustCopyCertificateChain(trust));
    SecCertificateRef secCertificate =
        base::apple::CFCastStrict<SecCertificateRef>(
            CFArrayGetValueAtIndex(certificateChain.get(), 0));
    leafCert = net::x509_util::CreateX509CertificateFromSecCertificate(
        base::apple::ScopedCFTypeRef<SecCertificateRef>(
            secCertificate, base::scoped_policy::RETAIN),
        {});

    if (leafCert) {
      bool is_recoverable =
          policy == web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_UNDECIDED_BY_USER;
      std::string host =
          base::SysNSStringToUTF8(challenge.protectionSpace.host);
      _certVerificationErrors->Put(
          web::CertHostPair(leafCert, host),
          web::CertVerificationError(is_recoverable, certStatus));
    }
  }
  completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
}

// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to reply
// with NSURLSessionAuthChallengeDisposition and credentials.
- (void)handleHTTPAuthForChallenge:(NSURLAuthenticationChallenge*)challenge
                 completionHandler:
                     (void (^)(NSURLSessionAuthChallengeDisposition,
                               NSURLCredential*))completionHandler {
  NSURLProtectionSpace* space = challenge.protectionSpace;
  DCHECK([space.authenticationMethod
             isEqualToString:NSURLAuthenticationMethodHTTPBasic] ||
         [space.authenticationMethod
             isEqualToString:NSURLAuthenticationMethodNTLM] ||
         [space.authenticationMethod
             isEqualToString:NSURLAuthenticationMethodHTTPDigest]);

  self.webStateImpl->OnAuthRequired(
      space, challenge.proposedCredential,
      base::BindRepeating(^(NSString* user, NSString* password) {
        [CRWWKNavigationHandler processHTTPAuthForUser:user
                                              password:password
                                     completionHandler:completionHandler];
      }));
}

// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to reply
// with NSURLSessionAuthChallengeDisposition and credentials.
+ (void)processHTTPAuthForUser:(NSString*)user
                      password:(NSString*)password
             completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                         NSURLCredential*))completionHandler {
  DCHECK_EQ(user == nil, password == nil);
  if (!user || !password) {
    // Embedder cancelled authentication.
    completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    return;
  }
  completionHandler(
      NSURLSessionAuthChallengeUseCredential,
      [NSURLCredential
          credentialWithUser:user
                    password:password
                 persistence:NSURLCredentialPersistenceForSession]);
}

// Called when a load ends in an error.
- (void)handleLoadError:(NSError*)error
          forNavigation:(WKNavigation*)navigation
                webView:(WKWebView*)webView
        provisionalLoad:(BOOL)provisionalLoad {
  NSError* policyDecisionCancellationError =
      self.pendingNavigationInfo.cancellationError;
  if (!policyDecisionCancellationError && error.code == NSURLErrorCancelled) {
    [self handleCancelledError:error
                 forNavigation:navigation
               provisionalLoad:provisionalLoad];
    // NSURLErrorCancelled errors that aren't handled by aborting the load will
    // automatically be retried by the web view, so early return in this case.
    return;
  }

  NSError* contextError = web::NetErrorFromError(error);
  if (policyDecisionCancellationError) {
    contextError = base::ios::ErrorWithAppendedUnderlyingError(
        contextError, policyDecisionCancellationError);
  }

  web::NavigationContextImpl* navigationContext =
      [self.navigationStates contextForNavigation:navigation];
  web::HttpsUpgradeType failed_upgrade_type = GetFailedHttpsUpgradeType(
      error, navigationContext, policyDecisionCancellationError);
  if (failed_upgrade_type != web::HttpsUpgradeType::kNone) {
    navigationContext->SetError(contextError);
    navigationContext->SetFailedHttpsUpgradeType(failed_upgrade_type);
    [self handleCancelledError:error
                 forNavigation:navigation
               provisionalLoad:provisionalLoad];
    return;
  }

  navigationContext->SetError(contextError);
  navigationContext->SetIsPost([self isCurrentNavigationItemPOST]);
  // TODO(crbug.com/41365797) DCHECK that self.currentNavItem is the navigation
  // item associated with navigationContext.

  if ([error.domain
          isEqualToString:base::SysUTF8ToNSString(web::kWebKitErrorDomain)]) {
    if (error.code == web::kWebKitErrorUrlBlockedByContentFilter) {
      DCHECK(provisionalLoad);
      // If URL is blocked due to Restriction, do not take any further
      // action as WKWebView will show a built-in error.

      // On iOS13, immediately following this navigation, WebKit will
      // navigate to an internal failure page. Unfortunately, due to how
      // session restoration works with same document navigations, this page
      // blocked by a content filter puts WebKit into a state where all
      // further restoration same-document navigations are 'stuck' on this
      // failure page.  Instead, avoid restoring this page completely.
      // Consider revisiting this once WKWebView's interactionSate is used
      // for session restoration everywhere.
      self.navigationManagerImpl->SetWKWebViewNextPendingUrlNotSerializable(
          navigationContext->GetUrl());
      return;
    }

    if (error.code == web::kWebKitErrorFrameLoadInterruptedByPolicyChange &&
        !policyDecisionCancellationError) {
      // Handle Frame Load Interrupted errors from WebView. This block is
      // executed when web controller rejected the load inside
      // decidePolicyForNavigationResponse: to handle download or WKWebView
      // opened a Universal Link.
      if (!navigationContext->IsDownload()) {
        // Non-download navigation was cancelled because WKWebView has opened a
        // Universal Link and called webView:didFailProvisionalNavigation:.
        self.navigationManagerImpl->DiscardNonCommittedItems();
        [self.navigationStates removeNavigation:navigation];
      }
      return;
    }

    if (error.code == web::kWebKitErrorCannotShowUrl) {
      if (!navigationContext->GetUrl().is_valid()) {
        // It won't be possible to load an error page for invalid URL, because
        // WKWebView will revert the url to about:blank. Simply discard pending
        // item and fail the navigation.
        navigationContext->ReleaseItem();
        self.webStateImpl->OnNavigationFinished(navigationContext);
        self.webStateImpl->OnPageLoaded(navigationContext->GetUrl(), false);
        return;
      }
    }
  }

  web::NavigationManager* navManager =
      self.webStateImpl->GetNavigationManager();
  web::NavigationItem* lastCommittedItem = navManager->GetLastCommittedItem();
  if (lastCommittedItem && !web::IsWKWebViewSSLCertError(error)) {
    // Reset SSL status for last committed navigation to avoid showing security
    // status for error pages.
    if (!lastCommittedItem->GetSSL().Equals(web::SSLStatus())) {
      lastCommittedItem->GetSSL() = web::SSLStatus();
      self.webStateImpl->DidChangeVisibleSecurityState();
    }
  }

  web::NavigationItemImpl* item =
      web::GetItemWithUniqueID(self.navigationManagerImpl, navigationContext);

  if (!item) {
    return;
  }

  WKNavigation* errorNavigation =
      [self displayErrorPageWithError:error
                            inWebView:webView
                    isProvisionalLoad:provisionalLoad];

  std::unique_ptr<web::NavigationContextImpl> originalContext =
      [self.navigationStates removeNavigation:navigation];
  originalContext->SetLoadingErrorPage(true);
  [self.navigationStates setContext:std::move(originalContext)
                      forNavigation:errorNavigation];
}

// Displays an error page with details from `error` in `webView`. The error page
// is presented with `transition` and associated with `blockedNSURL`.
- (void)displayError:(NSError*)error
    forCancelledNavigationToURL:(NSURL*)blockedNSURL
                      inWebView:(WKWebView*)webView
                 withTransition:(ui::PageTransition)transition {
  const GURL blockedURL = net::GURLWithNSURL(blockedNSURL);

  // Error page needs the URL string in the error's userInfo for proper
  // display.
  if (!error.userInfo[NSURLErrorFailingURLStringErrorKey]) {
    NSMutableDictionary* updatedUserInfo = [[NSMutableDictionary alloc] init];
    [updatedUserInfo addEntriesFromDictionary:error.userInfo];
    [updatedUserInfo setObject:blockedNSURL.absoluteString
                        forKey:NSURLErrorFailingURLStringErrorKey];

    error = [NSError errorWithDomain:error.domain
                                code:error.code
                            userInfo:updatedUserInfo];
  }

  WKNavigation* errorNavigation = [self displayErrorPageWithError:error
                                                        inWebView:webView
                                                isProvisionalLoad:YES];

  // Create pending item.
  self.navigationManagerImpl->AddPendingItem(
      blockedURL, web::Referrer(), transition,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/true,
      web::HttpsUpgradeType::kNone);

  // Create context.
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webStateImpl, blockedURL,
          /*has_user_gesture=*/true, transition,
          /*is_renderer_initiated=*/false);
  std::unique_ptr<web::NavigationItemImpl> item =
      self.navigationManagerImpl->ReleasePendingItem();
  CHECK(item);

  context->SetNavigationItemUniqueID(item->GetUniqueID());
  context->SetItem(std::move(item));
  context->SetError(error);
  context->SetLoadingErrorPage(true);

  self.webStateImpl->OnNavigationStarted(context.get());

  [self.navigationStates setContext:std::move(context)
                      forNavigation:errorNavigation];
}

// Creates and returns a new WKNavigation to load an error page displaying
// details of `error` inside `webView`. `provisionalLoad` should be set
// according to whether or not the error occurred during a provisionalLoad.
- (WKNavigation*)displayErrorPageWithError:(NSError*)error
                                 inWebView:(WKWebView*)webView
                         isProvisionalLoad:(BOOL)provisionalLoad {
  CRWErrorPageHelper* errorPage =
      [[CRWErrorPageHelper alloc] initWithError:error];
  WKBackForwardListItem* backForwardItem = webView.backForwardList.currentItem;
  GURL backForwardGURL = net::GURLWithNSURL(backForwardItem.URL);
  GURL failedURL = [CRWErrorPageHelper
      failedNavigationURLFromErrorPageFileURL:backForwardGURL];
  bool isSameURLFromWebClient = web::GetWebClient()->IsPointingToSameDocument(
      failedURL, net::GURLWithNSURL(errorPage.failedNavigationURL));
  // There are 3 possible scenarios here:
  //   1. Current nav item is an error page for failed URL;
  //   2. Current nav item has a failed URL. This may happen when
  //      back/forward/refresh on a loaded page;
  //   3. Current nav item is an irrelevant page.
  // For 1 and 2, load an empty string to remove existing JS code. The URL is
  // also updated to the URL of the page that failed to allow back/forward
  // navigations even on navigations originating from pushstate. See
  // crbug.com/1153261.
  // For 3, load error page file to create a new nav item.
  // The actual error HTML will be loaded in didFinishNavigation callback.
  WKNavigation* errorNavigation = nil;
  if (provisionalLoad &&
      ![errorPage
          isErrorPageFileURLForFailedNavigationURL:backForwardItem.URL] &&
      !isSameURLFromWebClient &&
      ![backForwardItem.URL isEqual:errorPage.failedNavigationURL]) {
    errorNavigation = [webView loadFileURL:errorPage.errorPageFileURL
                   allowingReadAccessToURL:errorPage.errorPageFileURL];
  } else {
    errorNavigation = [webView loadHTMLString:@""
                                      baseURL:errorPage.failedNavigationURL];
  }
  [self.navigationStates setState:web::WKNavigationState::REQUESTED
                    forNavigation:errorNavigation];

  return errorNavigation;
}

// Handles cancelled load in WKWebView (error with NSURLErrorCancelled code).
- (void)handleCancelledError:(NSError*)error
               forNavigation:(WKNavigation*)navigation
             provisionalLoad:(BOOL)provisionalLoad {
  web::HttpsUpgradeType failed_upgrade_type = GetFailedHttpsUpgradeType(
      error, [self.navigationStates contextForNavigation:navigation],
      self.pendingNavigationInfo.cancellationError);
  if (failed_upgrade_type == web::HttpsUpgradeType::kNone &&
      ![self shouldCancelLoadForCancelledError:error
                               provisionalLoad:provisionalLoad]) {
    return;
  }
  std::unique_ptr<web::NavigationContextImpl> navigationContext =
      [self.navigationStates removeNavigation:navigation];
  [self loadCancelled];
  web::NavigationItemImpl* item =
      navigationContext ? web::GetItemWithUniqueID(self.navigationManagerImpl,
                                                   navigationContext.get())
                        : nullptr;
  if (self.navigationManagerImpl->GetPendingItem() == item) {
    self.navigationManagerImpl->DiscardNonCommittedItems();
  }

    if (provisionalLoad) {
      // TODO(crbug.com/40631880): Remove this workaround when WebKit bug is
      // fixed.
      if (!navigationContext) {
        // It is likely that `navigationContext` is null because
        // didStartProvisionalNavigation: was not called with this WKNavigation
        // object. Do not call OnNavigationFinished() to avoid crash on null
        // pointer dereferencing. See crbug.com/973653 for details.
      } else {
        self.webStateImpl->OnNavigationFinished(navigationContext.get());
      }
    }
}

// Used to decide whether a load that generates errors with the
// NSURLErrorCancelled code should be cancelled.
- (BOOL)shouldCancelLoadForCancelledError:(NSError*)error
                          provisionalLoad:(BOOL)provisionalLoad {
  DCHECK(error.code == NSURLErrorCancelled ||
         error.code == web::kWebKitErrorFrameLoadInterruptedByPolicyChange);
  // Do not cancel the load if it is for an app specific URL, as such errors
  // are produced during the app specific URL load process.
  const GURL errorURL =
      net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey]);
  if (web::GetWebClient()->IsAppSpecificURL(errorURL))
    return NO;

  return provisionalLoad;
}

// Loads the error page.
- (void)loadErrorPageForNavigationItem:(web::NavigationItemImpl*)item
                     navigationContext:(WKNavigation*)navigation
                               webView:(WKWebView*)webView {
  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];
  NSError* error = context->GetError();
  DCHECK(error);
  DCHECK_EQ(item->GetUniqueID(), context->GetNavigationItemUniqueID());

  net::SSLInfo info;
  std::optional<net::SSLInfo> ssl_info = std::nullopt;

  if (web::IsWKWebViewSSLCertError(error)) {
    web::GetSSLInfoFromWKWebViewSSLCertError(error, &info);
    if (info.cert) {
      // Retrieve verification results from _certVerificationErrors cache to
      // avoid unnecessary recalculations. Verification results are cached for
      // the leaf cert, because the cert chain in
      // `didReceiveAuthenticationChallenge:` is the OS constructed chain, while
      // `chain` is the chain from the server.
      NSArray* chain = error.userInfo[web::kNSErrorPeerCertificateChainKey];
      NSURL* requestURL = error.userInfo[web::kNSErrorFailingURLKey];
      NSString* host = requestURL.host;
      scoped_refptr<net::X509Certificate> leafCert;
      if (chain.count && host.length) {
        // The complete cert chain may not be available, so the leaf cert is
        // used as a key to retrieve _certVerificationErrors, as well as for
        // storing the cert decision.
        leafCert = web::CreateCertFromChain(@[ chain.firstObject ]);
        if (leafCert) {
          auto verificationError = _certVerificationErrors->Get(
              {leafCert, base::SysNSStringToUTF8(host)});
          bool cacheHit = verificationError != _certVerificationErrors->end();
          if (cacheHit) {
            info.is_fatal_cert_error = verificationError->second.is_recoverable;
            info.cert_status = verificationError->second.status;
          }
          UMA_HISTOGRAM_BOOLEAN("WebController.CertVerificationErrorsCacheHit",
                                cacheHit);
        }
      }
      ssl_info = info;
    }
  }
  NSString* failingURLString =
      error.userInfo[NSURLErrorFailingURLStringErrorKey];
  GURL failingURL(base::SysNSStringToUTF8(failingURLString));
  GURL itemURL = item->GetURL();
  if (itemURL != failingURL)
    item->SetVirtualURL(failingURL);
  web::GetWebClient()->PrepareErrorPage(
      self.webStateImpl, failingURL, error, context->IsPost(),
      self.webStateImpl->GetBrowserState()->IsOffTheRecord(), ssl_info,
      context->GetNavigationId(), base::BindOnce(^(NSString* errorHTML) {
        if (self.navigationStates.lastAddedNavigation != navigation) {
          // Do not show the stale error page if the page has since navigated.
          return;
        }
        if (errorHTML) {
          CRWErrorPageHelper* errorPageHelper =
              [[CRWErrorPageHelper alloc] initWithError:context->GetError()];
          [webView
              evaluateJavaScript:[errorPageHelper
                                     scriptForInjectingHTML:errorHTML
                                         addAutomaticReload:YES]
               completionHandler:^(id result, NSError* nserror) {
                 if (nserror) {
                   LogPresentingErrorPageFailedWithError(nserror);
                   // WKErrorJavaScriptResultTypeIsUnsupported can be received
                   // if the WKWebView is released during this call.
                   DCHECK(nserror.code == WKErrorWebViewInvalidated ||
                          nserror.code == WKErrorWebContentProcessTerminated ||
                          nserror.code ==
                              WKErrorJavaScriptResultTypeIsUnsupported)
                       << "Error injecting error page HTML: "
                       << base::SysNSStringToUTF8(nserror.description);
                 }
               }];
        }

        // TODO(crbug.com/41464714): This is a workaround because `item` might
        // get released after
        // `self.navigationManagerImpl->
        // CommitPendingItem(context->ReleaseItem()`.
        // Remove this once navigation refactor is done.
        web::NavigationContextImpl* navContext =
            [self.navigationStates contextForNavigation:navigation];
        self.navigationManagerImpl->CommitPendingItem(
            navContext->ReleaseItem());
        [self.delegate navigationHandler:self
                          setDocumentURL:itemURL
                                 context:navContext];

        // Rewrite the context URL to actual URL and trigger the deferred
        // `OnNavigationFinished` callback.
        navContext->SetUrl(failingURL);
        navContext->SetHasCommitted(true);
        self.webStateImpl->OnNavigationFinished(navContext);

        // For SSL cert error pages, SSLStatus needs to be set manually because
        // the placeholder navigation for the error page is committed and
        // there is no server trust (since there's no network navigation), which
        // is required to create a cert in CRWSSLStatusUpdater.
        if (web::IsWKWebViewSSLCertError(navContext->GetError()) && info.cert) {
          web::SSLStatus& SSLStatus =
              self.navigationManagerImpl->GetLastCommittedItem()->GetSSL();
          SSLStatus.cert_status = info.cert_status;
          SSLStatus.certificate = info.cert;
          SSLStatus.security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
          self.webStateImpl->DidChangeVisibleSecurityState();
        }

        [self.delegate navigationHandler:self
              didCompleteLoadWithSuccess:NO
                              forContext:navContext];
        self.webStateImpl->OnPageLoaded(failingURL, NO);
      }));
}

// Resets any state that is associated with a specific document object (e.g.,
// page interaction tracking).
- (void)resetDocumentSpecificState {
  self.userInteractionState->SetLastUserInteraction(nullptr);
  self.userInteractionState->SetTapInProgress(false);
}

#pragma mark - Public methods

- (void)stopLoading {
  self.pendingNavigationInfo.cancelled = YES;
  [self loadCancelled];
  _certVerificationErrors->Clear();
}

- (void)loadCancelled {
  if (self.navigationState != web::WKNavigationState::FINISHED) {
    self.navigationState = web::WKNavigationState::FINISHED;
    if (!self.beingDestroyed) {
      self.webStateImpl->SetIsLoading(false);
    }
  }
}

// Returns context for pending navigation that has `URL`. null if there is no
// matching pending navigation.
- (web::NavigationContextImpl*)contextForPendingMainFrameNavigationWithURL:
    (const GURL&)URL {
  // Here the enumeration variable `navigation` is __strong to allow setting it
  // to nil.
  for (__strong id navigation in [self.navigationStates pendingNavigations]) {
    if (navigation == [NSNull null]) {
      // null is a valid navigation object passed to WKNavigationDelegate
      // callbacks and represents window opening action.
      navigation = nil;
    }

    web::NavigationContextImpl* context =
        [self.navigationStates contextForNavigation:navigation];
    if (context && context->GetUrl() == URL) {
      return context;
    }
  }
  return nullptr;
}

- (BOOL)isCurrentNavigationBackForward {
  if (!self.currentNavItem)
    return NO;
  WKNavigationType currentNavigationType =
      self.currentBackForwardListItemHolder->navigation_type();
  return currentNavigationType == WKNavigationTypeBackForward;
}

- (BOOL)isCurrentNavigationItemPOST {
  // `self.navigationHandler.pendingNavigationInfo` will be nil if the
  // decidePolicy* delegate methods were not called.
  NSString* HTTPMethod =
      self.pendingNavigationInfo
          ? self.pendingNavigationInfo.HTTPMethod
          : self.currentBackForwardListItemHolder->http_method();
  if ([HTTPMethod isEqualToString:@"POST"]) {
    return YES;
  }
  if (!self.currentNavItem) {
    return NO;
  }
  return self.currentNavItem->HasPostData();
}

// Returns the WKBackForwardListItemHolder for the current navigation item.
- (web::WKBackForwardListItemHolder*)currentBackForwardListItemHolder {
  web::NavigationItem* item = self.currentNavItem;
  DCHECK(item);
  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(item);
  DCHECK(holder);
  return holder;
}

// Updates current state with any pending information. Should be called when a
// navigation is committed.
- (void)commitPendingNavigationInfoInWebView:(WKWebView*)webView {
  if (self.pendingNavigationInfo.referrer) {
    _currentReferrerString = [self.pendingNavigationInfo.referrer copy];
  }
  [self updateCurrentBackForwardListItemHolderInWebView:webView];

  self.pendingNavigationInfo = nil;
}

// Updates the WKBackForwardListItemHolder navigation item.
- (void)updateCurrentBackForwardListItemHolderInWebView:(WKWebView*)webView {
  if (!self.currentNavItem) {
    // TODO(crbug.com/41437377): Pending item (which stores the holder) should
    // be owned by NavigationContext object. Pending item should never be null.
    return;
  }

  web::WKBackForwardListItemHolder* holder =
      self.currentBackForwardListItemHolder;

  WKNavigationType navigationType =
      self.pendingNavigationInfo ? self.pendingNavigationInfo.navigationType
                                 : WKNavigationTypeOther;
  holder->set_back_forward_list_item(webView.backForwardList.currentItem);
  holder->set_navigation_type(navigationType);
  holder->set_http_method(self.pendingNavigationInfo.HTTPMethod);

  // Only update the MIME type in the holder if there was MIME type information
  // as part of this pending load. It will be nil when doing a fast
  // back/forward navigation, for instance, because the callback that would
  // populate it is not called in that flow.
  if (self.pendingNavigationInfo.MIMEType)
    holder->set_mime_type(self.pendingNavigationInfo.MIMEType);
}

- (web::Referrer)currentReferrer {
  // Referrer string doesn't include the fragment, so in cases where the
  // previous URL is equal to the current referrer plus the fragment the
  // previous URL is returned as current referrer.
  NSString* referrerString = _currentReferrerString;

  // In case of an error evaluating the JavaScript simply return empty string.
  if (referrerString.length == 0)
    return web::Referrer();

  web::NavigationItem* item = self.currentNavItem;
  GURL navigationURL = item ? item->GetVirtualURL() : GURL();
  NSString* previousURLString = base::SysUTF8ToNSString(navigationURL.spec());
  // Check if the referrer is equal to the previous URL minus the hash symbol.
  // L'#' is used to convert the char '#' to a unichar.
  if ([previousURLString length] > referrerString.length &&
      [previousURLString hasPrefix:referrerString] &&
      [previousURLString characterAtIndex:referrerString.length] == L'#') {
    referrerString = previousURLString;
  }
  // Since referrer is being extracted from the destination page, the correct
  // policy from the origin has *already* been applied. Since the extracted URL
  // is the post-policy value, and the source policy is no longer available,
  // the policy is set to Always so that whatever WebKit decided to send will be
  // re-sent when replaying the entry.
  // TODO(crbug.com/41004475): When possible, get the real referrer and policy
  // in advance and use that instead.
  return web::Referrer(GURL(base::SysNSStringToUTF8(referrerString)),
                       web::ReferrerPolicyAlways);
}

- (void)setLastCommittedNavigationItemTitle:(NSString*)title {
  DCHECK(title);
  web::NavigationItem* item =
      self.navigationManagerImpl->GetLastCommittedItem();
  if (!item)
    return;

  item->SetTitle(base::SysNSStringToUTF16(title));
  self.webStateImpl->OnTitleChanged();
}

- (ui::PageTransition)pageTransitionFromNavigationType:
    (WKNavigationType)navigationType {
  switch (navigationType) {
    case WKNavigationTypeLinkActivated:
      return ui::PAGE_TRANSITION_LINK;
    case WKNavigationTypeFormSubmitted:
    case WKNavigationTypeFormResubmitted:
      return ui::PAGE_TRANSITION_FORM_SUBMIT;
    case WKNavigationTypeBackForward:
      return ui::PAGE_TRANSITION_FORWARD_BACK;
    case WKNavigationTypeReload:
      return ui::PAGE_TRANSITION_RELOAD;
    case WKNavigationTypeOther:
      // The "Other" type covers a variety of very different cases, which may
      // or may not be the result of user actions. For now, guess based on
      // whether there's been an interaction since the last URL change.
      // TODO(crbug.com/41213462): See if this heuristic can be improved.
      return self.userInteractionState
                     ->UserInteractionRegisteredSinceLastUrlChange()
                 ? ui::PAGE_TRANSITION_LINK
                 : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  }
}

- (void)webPageChangedWithContext:(web::NavigationContextImpl*)context
                          webView:(WKWebView*)webView {
  web::Referrer referrer = self.currentReferrer;
  // If no referrer was known in advance, record it now. (If there was one,
  // keep it since it will have a more accurate URL and policy than what can
  // be extracted from the landing page.)
  web::NavigationItem* currentItem = self.currentNavItem;

  // TODO(crbug.com/41437377): Pending item (which should be used here) should
  // be owned by NavigationContext object. Pending item should never be null.
  if (currentItem && !currentItem->GetReferrer().url.is_valid()) {
    currentItem->SetReferrer(referrer);
  }

  // TODO(crbug.com/40624624): This shouldn't be called for push/replaceState.
  [self resetDocumentSpecificState];

  [self.delegate navigationHandlerDidStartLoading:self];
  self.navigationManagerImpl->CommitPendingItem(context->ReleaseItem());
  if (context->IsLoadingHtmlString()) {
    self.navigationManagerImpl->GetLastCommittedItem()->SetURL(
        context->GetUrl());
  }
}

@end
