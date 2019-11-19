// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_handler.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/timer.h"
#import "ios/net/http_response_headers_util.h"
#include "ios/web/common/features.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/js_messaging/crw_js_injector.h"
#import "ios/web/js_messaging/web_frames_manager_impl.h"
#import "ios/web/navigation/crw_pending_navigation_info.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#include "ios/web/navigation/error_retry_state_machine.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/navigation_manager_util.h"
#import "ios/web/navigation/web_kit_constants.h"
#import "ios/web/navigation/wk_back_forward_list_item_holder.h"
#import "ios/web/navigation/wk_navigation_action_policy_util.h"
#import "ios/web/navigation/wk_navigation_action_util.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/web_client.h"
#import "ios/web/security/crw_cert_verification_controller.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
#import "ios/web/web_state/ui/controller/crw_legacy_native_content_controller.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#include "ios/web/web_view/content_type_util.h"
#import "ios/web/web_view/error_translation_util.h"
#import "ios/web/web_view/wk_web_view_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/cert/x509_util_ios.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::wk_navigation_util::IsPlaceholderUrl;
using web::wk_navigation_util::CreatePlaceholderUrlForUrl;
using web::wk_navigation_util::ExtractUrlFromPlaceholderUrl;
using web::wk_navigation_util::kReferrerHeaderName;
using web::wk_navigation_util::IsRestoreSessionUrl;
using web::wk_navigation_util::IsWKInternalUrl;

namespace {
// Maximum number of errors to store in cert verification errors cache.
// Cache holds errors only for pending navigations, so the actual number of
// stored errors is not expected to be high.
const web::CertVerificationErrorsCacheType::size_type kMaxCertErrorsCount = 100;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OutOfSyncURLAction {
  kNoAction = 0,
  kGoBack = 1,
  kGoForward = 2,
  kMaxValue = kGoForward,
};

void ReportOutOfSyncURLInDidStartProvisionalNavigation(
    OutOfSyncURLAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "WebController.BackForwardListOutOfSyncInProvisionalNavigation", action);
}

}  // namespace

@interface CRWWKNavigationHandler () {
  // Used to poll for a SafeBrowsing warning being displayed. This is created in
  // |decidePolicyForNavigationAction| and destroyed once any of the following
  // happens: 1) a SafeBrowsing warning is detected; 2) any WKNavigationDelegate
  // method is called; 3) |stopLoading| is called.
  base::RepeatingTimer _safeBrowsingWarningDetectionTimer;

  // Referrer for the current page; does not include the fragment.
  NSString* _currentReferrerString;

  // CertVerification errors which happened inside
  // |webView:didReceiveAuthenticationChallenge:completionHandler:|.
  // Key is leaf-cert/host pair. This storage is used to carry calculated
  // cert status from |didReceiveAuthenticationChallenge:| to
  // |didFailProvisionalNavigation:| delegate method.
  std::unique_ptr<web::CertVerificationErrorsCacheType> _certVerificationErrors;
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
// Returns the docuemnt URL from self.delegate.
@property(nonatomic, readonly, assign) GURL documentURL;
// Returns the session controller from self.navigationManagerImpl.
@property(nonatomic, readonly, weak) CRWSessionController* sessionController;
// Returns the js injector from self.delegate.
@property(nonatomic, readonly, weak) CRWJSInjector* JSInjector;
@property(nonatomic, readonly, weak)
    CRWLegacyNativeContentController* legacyNativeContentController;

// Set to YES when [self close] is called.
@property(nonatomic, assign) BOOL beingDestroyed;

@end

@implementation CRWWKNavigationHandler

- (instancetype)initWithDelegate:(id<CRWWKNavigationHandlerDelegate>)delegate {
  if (self = [super init]) {
    _navigationStates = [[CRWWKNavigationStates alloc] init];
    // Load phase when no WebView present is 'loaded' because this represents
    // the idle state.
    _navigationState = web::WKNavigationState::FINISHED;

    _certVerificationErrors =
        std::make_unique<web::CertVerificationErrorsCacheType>(
            kMaxCertErrorsCount);

    _delegate = delegate;
  }
  return self;
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)action
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  [self didReceiveWKNavigationDelegateCallback];

  self.webProcessCrashed = NO;
  if (self.beingDestroyed) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  GURL requestURL = net::GURLWithNSURL(action.request.URL);

  // Workaround for a WKWebView bug where the web content loaded using
  // |-loadHTMLString:baseURL| clobbers the next WKBackForwardListItem. It works
  // by detecting back/forward navigation to a clobbered item and replacing the
  // clobberred item and its forward history using a partial session restore in
  // the current web view. There is an unfortunate caveat: if the workaround is
  // triggered in a back navigation to a clobbered item, the restored forward
  // session is inserted after the current item before the back navigation, so
  // it doesn't fully replaces the "bad" history, even though user will be
  // navigated to the expected URL and may not notice the issue until they
  // review the back history by long pressing on "Back" button.
  //
  // TODO(crbug.com/887497): remove this workaround once iOS ships the fix.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      action.targetFrame.mainFrame) {
    GURL webViewURL = net::GURLWithNSURL(webView.URL);
    GURL currentWKItemURL =
        net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
    GURL backItemURL = net::GURLWithNSURL(webView.backForwardList.backItem.URL);
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:webViewURL];
    bool willClobberHistory =
        action.navigationType == WKNavigationTypeBackForward &&
        requestURL == backItemURL && webView.backForwardList.currentItem &&
        requestURL != currentWKItemURL && currentWKItemURL == webViewURL &&
        context &&
        (context->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK);

    UMA_HISTOGRAM_BOOLEAN("IOS.WKWebViewClobberedHistory", willClobberHistory);

    if (willClobberHistory && base::FeatureList::IsEnabled(
                                  web::features::kHistoryClobberWorkaround)) {
      decisionHandler(WKNavigationActionPolicyCancel);
      self.navigationManagerImpl
          ->ApplyWKWebViewForwardHistoryClobberWorkaround();
      return;
    }
  }

  // The page will not be changed until this navigation is committed, so the
  // retrieved state will be pending until |didCommitNavigation| callback.
  [self createPendingNavigationInfoFromNavigationAction:action];

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      action.targetFrame.mainFrame &&
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

  // If this is a placeholder navigation, pass through.
  if (IsPlaceholderUrl(requestURL)) {
    if (action.sourceFrame.mainFrame) {
      // Disallow renderer initiated navigations to placeholder URLs.
      decisionHandler(WKNavigationActionPolicyCancel);
    } else {
      decisionHandler(WKNavigationActionPolicyAllow);
    }
    return;
  }

  ui::PageTransition transition =
      [self pageTransitionFromNavigationType:action.navigationType];
  BOOL isMainFrameNavigationAction = [self isMainFrameNavigationAction:action];
  if (isMainFrameNavigationAction) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:requestURL];
    // Theoretically if |context| can be found here, the navigation should be
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

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // WKBasedNavigationManager doesn't use |loadCurrentURL| for reload or back/
    // forward navigation. So this is the first point where a form repost would
    // be detected. Display the confirmation dialog.
    if ([action.request.HTTPMethod isEqual:@"POST"] &&
        (action.navigationType == WKNavigationTypeFormResubmitted)) {
      self.webStateImpl->ShowRepostFormWarningDialog(
          base::BindOnce(^(bool shouldContinue) {
            if (self.beingDestroyed) {
              decisionHandler(WKNavigationActionPolicyCancel);
            } else if (shouldContinue) {
              decisionHandler(WKNavigationActionPolicyAllow);
            } else {
              decisionHandler(WKNavigationActionPolicyCancel);
              if (action.targetFrame.mainFrame) {
                [self.pendingNavigationInfo setCancelled:YES];
                if (!web::features::UseWKWebViewLoading()) {
                  self.webStateImpl->SetIsLoading(false);
                }
              }
            }
          }));
      return;
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
  }

  // First check if the navigation action should be blocked by the controller
  // and make sure to update the controller in the case that the controller
  // can't handle the request URL. Then use the embedders' policyDeciders to
  // either: 1- Handle the URL it self and return false to stop the controller
  // from proceeding with the navigation if needed. or 2- return true to allow
  // the navigation to be proceeded by the web controller.
  BOOL allowLoad = YES;
  if (web::GetWebClient()->IsAppSpecificURL(requestURL)) {
    allowLoad = [self shouldAllowAppSpecificURLNavigationAction:action
                                                     transition:transition];
    if (allowLoad && !self.webStateImpl->HasWebUI()) {
      [self.delegate navigationHandler:self createWebUIForURL:requestURL];
    }
  }

  BOOL webControllerCanShow =
      web::UrlHasWebScheme(requestURL) ||
      web::GetWebClient()->IsAppSpecificURL(requestURL) ||
      requestURL.SchemeIs(url::kFileScheme) ||
      requestURL.SchemeIs(url::kAboutScheme) ||
      requestURL.SchemeIs(url::kBlobScheme);

  if (allowLoad) {
    BOOL userInteractedWithRequestMainFrame =
        self.userInteractionState->HasUserTappedRecently(webView) &&
        net::GURLWithNSURL(action.request.mainDocumentURL) ==
            self.userInteractionState->LastUserInteraction()->main_document_url;
    web::WebStatePolicyDecider::RequestInfo requestInfo(
        transition, isMainFrameNavigationAction,
        userInteractedWithRequestMainFrame);

    allowLoad =
        self.webStateImpl->ShouldAllowRequest(action.request, requestInfo);
    // The WebState may have been closed in the ShouldAllowRequest callback.
    if (self.beingDestroyed) {
      decisionHandler(WKNavigationActionPolicyCancel);
      return;
    }
  }

  if (!webControllerCanShow) {
    allowLoad = NO;
  }

  if (allowLoad) {
    if ([[action.request HTTPMethod] isEqualToString:@"POST"]) {
      web::NavigationItemImpl* item =
          self.navigationManagerImpl->GetCurrentItemImpl();
      // TODO(crbug.com/570699): Remove this check once it's no longer possible
      // to have no current entries.
      if (item)
        [self cachePOSTDataForRequest:action.request inNavigationItem:item];
    }
  } else {
    if (action.targetFrame.mainFrame) {
      [self.pendingNavigationInfo setCancelled:YES];
      if (self.navigationManagerImpl->GetPendingItemIndex() == -1) {
        // Discard the new pending item to ensure that the current URL is not
        // different from what is displayed on the view. Discard only happens
        // if the last item was not a native view, to avoid ugly animation of
        // inserting the webview. There is no need to reset pending item index
        // for a different pending back-forward navigation.
        [self discardNonCommittedItemsIfLastCommittedWasNotNativeView];
      }

      web::NavigationContextImpl* context =
          [self contextForPendingMainFrameNavigationWithURL:requestURL];
      if (context) {
        // Destroy associated pending item, because this will be the last
        // WKWebView callback for this navigation context.
        context->ReleaseItem();
      }

      if (!self.beingDestroyed &&
          [self shouldClosePageOnNativeApplicationLoad]) {
        if (!web::features::UseWKWebViewLoading()) {
          // Loading was started for user initiated navigations and should be
          // stopped because no other WKWebView callbacks are called.
          // TODO(crbug.com/767092): Loading should not start until
          // webView.loading is changed to YES.
          self.webStateImpl->SetIsLoading(false);
        }
        self.webStateImpl->CloseWebState();
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
      }
    }

    if (!web::features::UseWKWebViewLoading() && !self.beingDestroyed) {
      // Loading was started for user initiated navigations and should be
      // stopped because no other WKWebView callbacks are called.
      // TODO(crbug.com/767092): Loading should not start until webView.loading
      // is changed to YES.
      self.webStateImpl->SetIsLoading(false);
    }
  }

  // Only try to detect a SafeBrowsing warning if one isn't already displayed,
  // since the detection logic won't be able to distinguish between the current
  // warning and a warning for the page that's about to be loaded. Also, since
  // the purpose of running this logic is to ensure that the right URL is
  // displayed in the omnibox, don't try to detect a SafeBrowsing warning for
  // iframe navigations, because the omnibox already shows the correct main
  // frame URL in that case.
  if (allowLoad && isMainFrameNavigationAction &&
      !web::IsSafeBrowsingWarningDisplayedInWebView(webView)) {
    __weak CRWWKNavigationHandler* weakSelf = self;
    __weak WKWebView* weakWebView = webView;
    const base::TimeDelta kDelayUntilSafeBrowsingWarningCheck =
        base::TimeDelta::FromMilliseconds(20);
    _safeBrowsingWarningDetectionTimer.Start(
        FROM_HERE, kDelayUntilSafeBrowsingWarningCheck, base::BindRepeating(^{
          __strong __typeof(weakSelf) strongSelf = weakSelf;
          __strong __typeof(weakWebView) strongWebView = weakWebView;
          if (web::IsSafeBrowsingWarningDisplayedInWebView(strongWebView)) {
            // Extract state from an existing navigation context if one exists.
            // Create a new context rather than just re-using the existing one,
            // since the existing context will continue to be used if the user
            // decides to proceed to the unsafe page. In that case, WebKit
            // continues the navigation with the same WKNavigation* that's
            // associated with the existing context.
            web::NavigationContextImpl* existingContext = [strongSelf
                contextForPendingMainFrameNavigationWithURL:requestURL];
            bool hasUserGesture =
                existingContext ? existingContext->HasUserGesture() : false;
            bool isRendererInitiated =
                existingContext ? existingContext->IsRendererInitiated() : true;
            std::unique_ptr<web::NavigationContextImpl> context =
                web::NavigationContextImpl::CreateNavigationContext(
                    strongSelf.webStateImpl, requestURL, hasUserGesture,
                    transition, isRendererInitiated);
            [strongSelf navigationManagerImpl] -> AddTransientItem(requestURL);
            strongSelf.webStateImpl->OnNavigationStarted(context.get());
            strongSelf.webStateImpl->OnNavigationFinished(context.get());
            strongSelf->_safeBrowsingWarningDetectionTimer.Stop();
            if (!web::features::UseWKWebViewLoading() && !existingContext) {
              // If there's an existing context, observers will already be aware
              // of a load in progress. Otherwise, observers need to be notified
              // here, so that if the user decides to go back to the previous
              // page (stopping the load), observers will be aware of a possible
              // URL change and the URL displayed in the omnibox will get
              // updated.
              DCHECK(strongWebView.loading);
              strongSelf.webStateImpl->SetIsLoading(true);
            }
          }
        }));
  }

  if (!allowLoad) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }
  BOOL isOffTheRecord = self.webStateImpl->GetBrowserState()->IsOffTheRecord();
  decisionHandler(web::GetAllowNavigationActionPolicy(isOffTheRecord));
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse*)WKResponse
                      decisionHandler:
                          (void (^)(WKNavigationResponsePolicy))handler {
  [self didReceiveWKNavigationDelegateCallback];

  // If this is a placeholder navigation, pass through.
  GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  if (IsPlaceholderUrl(responseURL)) {
    handler(WKNavigationResponsePolicyAllow);
    return;
  }

  scoped_refptr<net::HttpResponseHeaders> headers;
  if ([WKResponse.response isKindOfClass:[NSHTTPURLResponse class]]) {
    headers = net::CreateHeadersFromNSHTTPURLResponse(
        static_cast<NSHTTPURLResponse*>(WKResponse.response));
  }

  // The page will not be changed until this navigation is committed, so the
  // retrieved state will be pending until |didCommitNavigation| callback.
  [self updatePendingNavigationInfoFromNavigationResponse:WKResponse
                                              HTTPHeaders:headers];

  BOOL shouldRenderResponse = [self shouldRenderResponse:WKResponse];
  if (!shouldRenderResponse) {
    if (web::UrlHasWebScheme(responseURL)) {
      [self createDownloadTaskForResponse:WKResponse HTTPHeaders:headers.get()];
    } else {
      // DownloadTask only supports web schemes, so do nothing.
    }
    // Discard the pending item to ensure that the current URL is not different
    // from what is displayed on the view.
    [self discardNonCommittedItemsIfLastCommittedWasNotNativeView];
  } else {
    shouldRenderResponse = self.webStateImpl->ShouldAllowResponse(
        WKResponse.response, WKResponse.forMainFrame);
  }

  if (!shouldRenderResponse && WKResponse.canShowMIMEType &&
      WKResponse.forMainFrame) {
    self.pendingNavigationInfo.cancelled = YES;
  }

  if (!web::features::UseWKWebViewLoading()) {
    if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
        !WKResponse.forMainFrame && !webView.loading) {
      // This is the terminal callback for iframe navigation and there is no
      // pending main frame navigation. Last chance to flip IsLoading to false.
      self.webStateImpl->SetIsLoading(false);
    }
  }

  handler(shouldRenderResponse ? WKNavigationResponsePolicyAllow
                               : WKNavigationResponsePolicyCancel);
}

- (void)webView:(WKWebView*)webView
    didStartProvisionalNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];

  GURL webViewURL = net::GURLWithNSURL(webView.URL);

  [self.navigationStates setState:web::WKNavigationState::STARTED
                    forNavigation:navigation];

  if (webViewURL.is_empty()) {
    // May happen on iOS9, however in didCommitNavigation: callback the URL
    // will be "about:blank".
    webViewURL = GURL(url::kAboutBlankURL);
    UMA_HISTOGRAM_BOOLEAN("IOS.WKWebViewStartProvisionalNavigationWithEmptyURL",
                          true);
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

    if (!context->IsPlaceholderNavigation() && !IsWKInternalUrl(webViewURL)) {
      web::NavigationItem* item =
          web::GetItemWithUniqueID(self.navigationManagerImpl, context);
      if (item) {
        web::WKBackForwardListItemHolder* itemHolder =
            web::WKBackForwardListItemHolder::FromNavigationItem(item);
        if (itemHolder->navigation_type() == WKNavigationTypeBackForward &&
            ![webView.backForwardList.currentItem.URL isEqual:webView.URL]) {
          // Sometimes on back/forward navigation, the backforward list is out
          // of sync with the webView. Go back or forward to fix it. See
          // crbug.com/968539.
          if ([webView.backForwardList.backItem.URL isEqual:webView.URL]) {
            ReportOutOfSyncURLInDidStartProvisionalNavigation(
                OutOfSyncURLAction::kGoBack);
            [webView goBack];
            return;
          }
          if ([webView.backForwardList.forwardItem.URL isEqual:webView.URL]) {
            ReportOutOfSyncURLInDidStartProvisionalNavigation(
                OutOfSyncURLAction::kGoForward);
            [webView goForward];
            return;
          }
          ReportOutOfSyncURLInDidStartProvisionalNavigation(
              OutOfSyncURLAction::kNoAction);
        }
      }

      if (context->GetUrl() != webViewURL) {
        // Update last seen URL because it may be changed by WKWebView (f.e.
        // by performing characters escaping).
        if (item) {
          // Item may not exist if navigation was stopped (see
          // crbug.com/969915).
          item->SetURL(webViewURL);
        }
        context->SetUrl(webViewURL);
      }
    }

    self.webStateImpl->OnNavigationStarted(context);
    self.webStateImpl->GetNavigationManagerImpl().OnNavigationStarted(
        webViewURL);
    return;
  }

  // This is renderer-initiated navigation which was not seen before and
  // should be registered.

  // When using WKBasedNavigationManager, renderer-initiated app-specific loads
  // should be allowed in two specific cases:
  // 1) if |backForwardList.currentItem| is a placeholder URL for the
  //    provisional load URL (i.e. webView.URL), then this is an in-progress
  //    app-specific load and should not be restarted.
  // 2) back/forward navigation to an app-specific URL should be allowed.
  bool exemptedAppSpecificLoad = false;
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    bool currentItemIsPlaceholder =
        CreatePlaceholderUrlForUrl(webViewURL) ==
        net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
    bool isBackForward = self.pendingNavigationInfo.navigationType ==
                         WKNavigationTypeBackForward;
    bool isRestoringSession =
        web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
        IsRestoreSessionUrl(self.documentURL);
    exemptedAppSpecificLoad =
        currentItemIsPlaceholder || isBackForward || isRestoringSession;
  }

  if (!web::GetWebClient()->IsAppSpecificURL(webViewURL) ||
      !exemptedAppSpecificLoad) {
    self.webStateImpl->ClearWebUI();
  }

  if (web::GetWebClient()->IsAppSpecificURL(webViewURL) &&
      !exemptedAppSpecificLoad) {
    // Restart app specific URL loads to properly capture state.
    // TODO(crbug.com/546347): Extract necessary tasks for app specific URL
    // navigation rather than restarting the load.

    // Renderer-initiated loads of WebUI can be done only from other WebUI
    // pages. WebUI pages may have increased power and using the same web
    // process (which may potentially be controller by an attacker) is
    // dangerous.
    if (web::GetWebClient()->IsAppSpecificURL(self.documentURL)) {
      [webView stopLoading];
      [self stopLoading];
      web::NavigationManager::WebLoadParams params(webViewURL);
      self.navigationManagerImpl->LoadURLWithParams(params);
    }
    return;
  }

  self.webStateImpl->GetNavigationManagerImpl().OnNavigationStarted(webViewURL);

  // When a client-side redirect occurs while an interstitial warning is
  // displayed, clear the warning and its navigation item, so that a new
  // pending item is created for |context| in |registerLoadRequestForURL|. See
  // crbug.com/861836.
  self.webStateImpl->ClearTransientContent();

  std::unique_ptr<web::NavigationContextImpl> navigationContext =
      [self.delegate navigationHandler:self
             registerLoadRequestForURL:webViewURL
                sameDocumentNavigation:NO
                        hasUserGesture:self.pendingNavigationInfo.hasUserGesture
                     rendererInitiated:YES
                 placeholderNavigation:IsPlaceholderUrl(webViewURL)];
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

  // This callback should never be triggered for placeholder navigations.
  DCHECK(!(web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
           IsPlaceholderUrl(webViewURL)));

  [self.navigationStates setState:web::WKNavigationState::REDIRECTED
                    forNavigation:navigation];

  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];
  [self didReceiveRedirectForNavigation:context withURL:webViewURL];
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self didReceiveWKNavigationDelegateCallback];

  [self.navigationStates setState:web::WKNavigationState::PROVISIONALY_FAILED
                    forNavigation:navigation];

  // Ignore provisional navigation failure if a new navigation has been started,
  // for example, if a page is reloaded after the start of the provisional
  // load but before the load has been committed.
  if (![[self.navigationStates lastAddedNavigation] isEqual:navigation]) {
    return;
  }

  // Handle load cancellation for directly cancelled navigations without
  // handling their potential errors. Otherwise, handle the error.
  if (self.pendingNavigationInfo.cancelled) {
    [self handleCancelledError:error
                 forNavigation:navigation
               provisionalLoad:YES];
  } else if (error.code == NSURLErrorUnsupportedURL &&
             self.webStateImpl->HasWebUI()) {
    // This is a navigation to WebUI page.
    DCHECK(web::GetWebClient()->IsAppSpecificURL(
        net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey])));
  } else {
    // Skip this check when committed interstitials enabled and let the cert
    // error go through handleLoadError: instead. In this case, a generic
    // net error page gets shown for cert errors.
    if (web::IsWKWebViewSSLCertError(error) &&
        !base::FeatureList::IsEnabled(
            web::features::kSSLCommittedInterstitials)) {
      [self handleSSLCertError:error forNavigation:navigation webView:webView];
    } else {
      [self handleLoadError:error
              forNavigation:navigation
                    webView:webView
            provisionalLoad:YES];
    }
  }

  self.webStateImpl->GetWebFramesManagerImpl().RemoveAllWebFrames();
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
  // |webView:didFinishNavigation| before |webView:didCommitNavigation|. If a
  // navigation is already finished, stop processing
  // (https://crbug.com/818796#c2).
  if ([self.navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::FINISHED)
    return;

  BOOL committedNavigation =
      [self.navigationStates isCommittedNavigation:navigation];

  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];
  if (!web::IsWKWebViewSSLCertError(context->GetError())) {
    _certVerificationErrors->Clear();
  }

  // Invariant: Every |navigation| should have a |context|. Note that violation
  // of this invariant is currently observed in production, but the cause is not
  // well understood. This DCHECK is meant to catch such cases in testing if
  // they arise.
  // TODO(crbug.com/864769): Remove nullptr checks on |context| in this method
  // once the root cause of the invariant violation is found.
  DCHECK(context);
  UMA_HISTOGRAM_BOOLEAN("IOS.CommittedNavigationHasContext", context);

  GURL webViewURL = net::GURLWithNSURL(webView.URL);
  GURL currentWKItemURL =
      net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
  UMA_HISTOGRAM_BOOLEAN("IOS.CommittedURLMatchesCurrentItem",
                        webViewURL == currentWKItemURL);

  // TODO(crbug.com/787497): Always use webView.backForwardList.currentItem.URL
  // to obtain lastCommittedURL once loadHTML: is no longer user for WebUI.
  if (webViewURL.is_empty()) {
    // It is possible for |webView.URL| to be nil, in which case
    // webView.backForwardList.currentItem.URL will return the right committed
    // URL (crbug.com/784480).
    webViewURL = currentWKItemURL;
  } else if (context && !context->IsPlaceholderNavigation() &&
             context->GetUrl() == currentWKItemURL) {
    // If webView.backForwardList.currentItem.URL matches |context|, then this
    // is a known edge case where |webView.URL| is wrong.
    // TODO(crbug.com/826013): Remove this workaround.
    webViewURL = currentWKItemURL;
  }

  if (self.pendingNavigationInfo.MIMEType)
    context->SetMimeType(self.pendingNavigationInfo.MIMEType);
  if (self.pendingNavigationInfo.HTTPHeaders)
    context->SetResponseHeaders(self.pendingNavigationInfo.HTTPHeaders);

  // Don't show webview for placeholder navigation to avoid covering the native
  // content, which may have already been shown.
  if (!IsPlaceholderUrl(webViewURL))
    [self.delegate navigationHandlerDisplayWebView:self];

  if (@available(iOS 11.3, *)) {
    // On iOS 11.3 didReceiveServerRedirectForProvisionalNavigation: is not
    // always called. So if URL was unexpectedly changed then it's probably
    // because redirect callback was not called.
    if (@available(iOS 12, *)) {
      // rdar://37547029 was fixed on iOS 12.
    } else if (context && !context->IsPlaceholderNavigation() &&
               context->GetUrl() != webViewURL) {
      [self didReceiveRedirectForNavigation:context withURL:webViewURL];
    }
  }

  // |context| will be nil if this navigation has been already committed and
  // finished.
  if (context) {
    web::NavigationManager* navigationManager =
        self.webStateImpl->GetNavigationManager();
    GURL pendingURL;
    if (navigationManager->GetPendingItemIndex() == -1) {
      if (context->GetItem()) {
        // Item may not exist if navigation was stopped (see
        // crbug.com/969915).
        pendingURL = context->GetItem()->GetURL();
      }
    } else {
      if (navigationManager->GetPendingItem()) {
        pendingURL = navigationManager->GetPendingItem()->GetURL();
      }
    }
    if ((pendingURL == webViewURL) || (context->IsLoadingHtmlString()) ||
        (!web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
         ui::PageTransitionCoreTypeIs(context->GetPageTransition(),
                                      ui::PAGE_TRANSITION_RELOAD) &&
         navigationManager->GetLastCommittedItem())) {
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

  self.webStateImpl->GetWebFramesManagerImpl().RemoveAllWebFrames();

  // This point should closely approximate the document object change, so reset
  // the list of injected scripts to those that are automatically injected.
  // Do not inject window ID if this is a placeholder URL: window ID is not
  // needed for native view. For WebUI, let the window ID be injected when the
  // |loadHTMLString:baseURL| navigation is committed.
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled() ||
      !IsPlaceholderUrl(webViewURL)) {
    [self.JSInjector resetInjectedScriptSet];

    const std::string& mime_type = self.webStateImpl->GetContentsMimeType();
    if (web::IsContentTypeHtml(mime_type) ||
        web::IsContentTypeImage(mime_type) || mime_type.empty()) {
      // In unit tests MIME type will be empty, because loadHTML:forURL: does
      // not notify web view delegate about received response, so web controller
      // does not get a chance to properly update MIME type.
      [self.JSInjector injectWindowID];
      self.webStateImpl->GetWebFramesManagerImpl().RegisterExistingFrames();
    }
  }

  if (committedNavigation) {
    // WKWebView called didCommitNavigation: with incorrect WKNavigation object.
    // Correct WKNavigation object for this navigation was deallocated because
    // WKWebView mistakenly cancelled the navigation and called
    // didFailProvisionalNavigation. As a result web::NavigationContext for this
    // navigation does not exist anymore. Find correct navigation item and make
    // it committed.
    if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
      bool found_correct_navigation_item = false;
      for (size_t i = 0; i < self.sessionController.items.size(); i++) {
        web::NavigationItem* item = self.sessionController.items[i].get();
        found_correct_navigation_item = item->GetURL() == webViewURL;
        if (found_correct_navigation_item) {
          [self.sessionController goToItemAtIndex:i
                         discardNonCommittedItems:NO];
          break;
        }
      }
      DCHECK(found_correct_navigation_item);
    }
    [self resetDocumentSpecificState];
    [self.delegate navigationHandlerDidStartLoading:self];
  } else if (context) {
    // If |navigation| is nil (which happens for windows open by DOM), then it
    // should be the first and the only pending navigation.
    BOOL isLastNavigation =
        !navigation ||
        [[self.navigationStates lastAddedNavigation] isEqual:navigation];
    if (isLastNavigation ||
        self.navigationManagerImpl->GetPendingItemIndex() == -1) {
      [self webPageChangedWithContext:context webView:webView];
    } else if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
      // WKWebView has more than one in progress navigation, and committed
      // navigation was not the latest. Change last committed item to one that
      // corresponds to committed navigation.
      int itemIndex = web::GetCommittedItemIndexWithUniqueID(
          self.navigationManagerImpl, context->GetNavigationItemUniqueID());
      // Do not discard pending entry, because another pending navigation is
      // still in progress and will commit or fail soon.
      [self.sessionController goToItemAtIndex:itemIndex
                     discardNonCommittedItems:NO];
    }
  }

  // No further code relies an existance of pending item, so this navigation can
  // be marked as "committed".
  [self.navigationStates setState:web::WKNavigationState::COMMITTED
                    forNavigation:navigation];

  // This is the point where the document's URL has actually changed.
  [self.delegate navigationHandler:self
                    setDocumentURL:webViewURL
                           context:context];

  if (!committedNavigation && context && !context->IsLoadingErrorPage()) {
    self.webStateImpl->OnNavigationFinished(context);
  }

  // Do not update the HTML5 history state or states of the last committed item
  // for placeholder page because the actual navigation item will not be
  // committed until the native content or WebUI is shown.
  if (context && !context->IsPlaceholderNavigation() &&
      !context->IsLoadingErrorPage() &&
      !context->GetUrl().SchemeIs(url::kAboutScheme) &&
      !IsRestoreSessionUrl(context->GetUrl())) {
    [self.delegate
        navigationHandlerUpdateSSLStatusForCurrentNavigationItem:self];
    [self.delegate navigationHandlerUpdateHTML5HistoryState:self];
    if (!context->IsLoadingErrorPage() && !IsRestoreSessionUrl(webViewURL)) {
      [self setLastCommittedNavigationItemTitle:webView.title];
    }
  }

  // Report cases where SSL cert is missing for a secure connection.
  if (self.documentURL.SchemeIsCryptographic()) {
    scoped_refptr<net::X509Certificate> cert;
    cert = web::CreateCertFromTrust(webView.serverTrust);
    UMA_HISTOGRAM_BOOLEAN("WebController.WKWebViewHasCertForSecureConnection",
                          static_cast<bool>(cert));
  }
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];

  // Sometimes |webView:didFinishNavigation| arrives before
  // |webView:didCommitNavigation|. Explicitly trigger post-commit processing.
  bool navigationCommitted =
      [self.navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::COMMITTED;
  UMA_HISTOGRAM_BOOLEAN("IOS.WKWebViewFinishBeforeCommit",
                        !navigationCommitted);
  if (!navigationCommitted) {
    [self webView:webView didCommitNavigation:navigation];
    DCHECK_EQ(web::WKNavigationState::COMMITTED,
              [self.navigationStates stateForNavigation:navigation]);
  }

  // Sometimes |didFinishNavigation| callback arrives after |stopLoading| has
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

  // Invariant: every |navigation| should have a |context| and a |item|.
  // TODO(crbug.com/899383) Fix invariant violation when a new pending item is
  // created before a placeholder load finishes.
  if (IsPlaceholderUrl(webViewURL)) {
    GURL originalURL = ExtractUrlFromPlaceholderUrl(webViewURL);
    if (self.currentNavItem != item &&
        self.currentNavItem->GetVirtualURL() != originalURL) {
      // The |didFinishNavigation| callback for placeholder navigation can
      // arrive after another navigation has started. Abort in this case.
      return;
    }
  }
  DCHECK(context);
  UMA_HISTOGRAM_BOOLEAN("IOS.FinishedNavigationHasContext", context);
  UMA_HISTOGRAM_BOOLEAN("IOS.FinishedNavigationHasItem", item);

  if (context && item) {
    GURL navigationURL = context->IsPlaceholderNavigation()
                             ? CreatePlaceholderUrlForUrl(context->GetUrl())
                             : context->GetUrl();
    if (navigationURL == currentWKItemURL) {
      // If webView.backForwardList.currentItem.URL matches |context|, then this
      // is a known edge case where |webView.URL| is wrong.
      // TODO(crbug.com/826013): Remove this workaround.
      webViewURL = currentWKItemURL;
    }

    if (!IsWKInternalUrl(currentWKItemURL) && currentWKItemURL == webViewURL &&
        currentWKItemURL != context->GetUrl() &&
        item == self.navigationManagerImpl->GetLastCommittedItem() &&
        item->GetURL().GetOrigin() == currentWKItemURL.GetOrigin()) {
      // WKWebView sometimes changes URL on the same navigation, likely due to
      // location.replace() or history.replaceState in onload handler that does
      // not change the origin. It's safe to update |item| and |context| URL
      // because they are both associated to WKNavigation*, which is a stable ID
      // for the navigation. See https://crbug.com/869540 for a real-world case.
      item->SetURL(currentWKItemURL);
      context->SetUrl(currentWKItemURL);
    }

    if (IsPlaceholderUrl(webViewURL)) {
      if (item->GetURL() == webViewURL) {
        // Current navigation item is restored from a placeholder URL as part
        // of session restoration. It is now safe to update the navigation
        // item URL to the original app-specific URL.
        item->SetURL(ExtractUrlFromPlaceholderUrl(webViewURL));
      }

      if ([self.legacyNativeContentController
              shouldLoadURLInNativeView:item->GetURL()]) {
        [self.legacyNativeContentController
            webViewDidFinishNavigationWithContext:context
                                          andItem:item];
      } else if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
                 item->error_retry_state_machine().state() ==
                     web::ErrorRetryState::kNoNavigationError) {
        // Offline pages can leave the WKBackForwardList current item as a
        // placeholder with no saved content.  In this case, trigger a retry
        // on that navigation with an update |item| url and |context| error.
        item->SetURL(
            ExtractUrlFromPlaceholderUrl(net::GURLWithNSURL(webView.URL)));
        item->SetVirtualURL(item->GetURL());
        context->SetError([NSError
            errorWithDomain:NSURLErrorDomain
                       code:NSURLErrorNetworkConnectionLost
                   userInfo:@{
                     NSURLErrorFailingURLStringErrorKey :
                         base::SysUTF8ToNSString(item->GetURL().spec())
                   }]);
        item->error_retry_state_machine().SetRetryPlaceholderNavigation();
      }
    }

    web::ErrorRetryCommand command =
        item->error_retry_state_machine().DidFinishNavigation(webViewURL);
    [self handleErrorRetryCommand:command
                   navigationItem:item
                navigationContext:context
               originalNavigation:navigation
                          webView:webView];
  }

  [self.navigationStates setState:web::WKNavigationState::FINISHED
                    forNavigation:navigation];

  [self.delegate navigationHandler:self didFinishNavigation:context];

  // Remove the navigation to immediately get rid of pending item. Navigation
  // should not be cleared, however, in the case of a committed interstitial
  // for an SSL error.
  if (web::WKNavigationState::NONE !=
          [self.navigationStates stateForNavigation:navigation] &&
      !web::IsWKWebViewSSLCertError(context->GetError())) {
    [self.navigationStates removeNavigation:navigation];
  }
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self didReceiveWKNavigationDelegateCallback];

  [self.navigationStates setState:web::WKNavigationState::FAILED
                    forNavigation:navigation];

  [self handleLoadError:error
          forNavigation:navigation
                webView:webView
        provisionalLoad:NO];
  self.webStateImpl->GetWebFramesManagerImpl().RemoveAllWebFrames();
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
  if ([authMethod isEqual:NSURLAuthenticationMethodHTTPBasic] ||
      [authMethod isEqual:NSURLAuthenticationMethodNTLM] ||
      [authMethod isEqual:NSURLAuthenticationMethodHTTPDigest]) {
    [self handleHTTPAuthForChallenge:challenge
                   completionHandler:completionHandler];
    return;
  }

  if (![authMethod isEqual:NSURLAuthenticationMethodServerTrust]) {
    completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    return;
  }

  SecTrustRef trust = challenge.protectionSpace.serverTrust;
  base::ScopedCFTypeRef<SecTrustRef> scopedTrust(trust,
                                                 base::scoped_policy::RETAIN);
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

- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
  [self didReceiveWKNavigationDelegateCallback];

  _certVerificationErrors->Clear();
  self.webProcessCrashed = YES;
  self.webStateImpl->GetWebFramesManagerImpl().RemoveAllWebFrames();

  [self.delegate navigationHandlerWebProcessDidCrash:self];
}

#pragma mark - Private methods

- (web::NavigationManagerImpl*)navigationManagerImpl {
  return &(self.webStateImpl->GetNavigationManagerImpl());
}

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForNavigationHandler:self];
}

- (web::UserInteractionState*)userInteractionState {
  return [self.delegate userInteractionStateForNavigationHandler:self];
}

- (CRWJSInjector*)JSInjector {
  return [self.delegate JSInjectorForNavigationHandler:self];
}

- (CRWCertVerificationController*)certVerificationController {
  return [self.delegate certVerificationControllerForNavigationHandler:self];
}

- (CRWLegacyNativeContentController*)legacyNativeContentController {
  return [self.delegate legacyNativeContentControllerForNavigationHandler:self];
}

- (GURL)documentURL {
  return [self.delegate navigationHandlerDocumentURL:self];
}

- (web::NavigationItemImpl*)currentNavItem {
  return self.navigationManagerImpl
             ? self.navigationManagerImpl->GetCurrentItemImpl()
             : nullptr;
}

- (CRWSessionController*)sessionController {
  return self.navigationManagerImpl
             ? self.navigationManagerImpl->GetSessionController()
             : nil;
}

// This method should be called on receiving WKNavigationDelegate callbacks. It
// will log a metric if the callback occurs after the reciever has already been
// closed. It also stops the SafeBrowsing warning detection timer, since after
// this point it's too late for a SafeBrowsing warning to be displayed for the
// navigation for which the timer was started.
- (void)didReceiveWKNavigationDelegateCallback {
  if (self.beingDestroyed) {
    UMA_HISTOGRAM_BOOLEAN("Renderer.WKWebViewCallbackAfterDestroy", true);
  }
  _safeBrowsingWarningDetectionTimer.Stop();
}

// Extracts navigation info from WKNavigationAction and sets it as a pending.
// Some pieces of navigation information are only known in
// |decidePolicyForNavigationAction|, but must be in a pending state until
// |didgo/Navigation| where it becames current.
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
// |decidePolicyForNavigationResponse|, but must be in a pending state until
// |didCommitNavigation| where it becames current.
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
  // |sourceFrame| is the mainFrame.
  return action.sourceFrame.mainFrame;
}

// Returns YES if the given |action| should be allowed to continue for app
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

  if (!ui::PageTransitionIsNewNavigation(pageTransition)) {
    // Allow reloads and back-forward navigations.
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

  // If the session is being restored, allow the navigation.
  if (IsRestoreSessionUrl(self.documentURL)) {
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
    // According to NSURLRequest documentation, |-valueForHTTPHeaderField:| is
    // case insensitive, so it's enough to test the lower case only.
    if ([request valueForHTTPHeaderField:cookieHeaderName]) {
      // Case insensitive search in |headers|.
      NSSet* cookieKeys = [item->GetHttpRequestHeaders()
          keysOfEntriesPassingTest:^(id key, id obj, BOOL* stop) {
            NSString* header = (NSString*)key;
            const BOOL found =
                [header caseInsensitiveCompare:cookieHeaderName] ==
                NSOrderedSame;
            *stop = found;
            return found;
          }];
      DCHECK_EQ(1u, [cookieKeys count]);
      item->RemoveHttpRequestHeaderForKey([cookieKeys anyObject]);
    }
  }
}

// Discards non committed items, only if the last committed URL was not loaded
// in native view. But if it was a native view, no discard will happen to avoid
// an ugly animation where the web view is inserted and quickly removed.
- (void)discardNonCommittedItemsIfLastCommittedWasNotNativeView {
  GURL lastCommittedURL = self.webStateImpl->GetLastCommittedURL();
  BOOL previousItemWasLoadedInNativeView =
      [self.delegate navigationHandler:self
             shouldLoadURLInNativeView:lastCommittedURL];
  if (!previousItemWasLoadedInNativeView)
    self.navigationManagerImpl->DiscardNonCommittedItems();
}

// If YES, the page should be closed if it successfully redirects to a native
// application, for example if a new tab redirects to the App Store.
- (BOOL)shouldClosePageOnNativeApplicationLoad {
  // The page should be closed if it was initiated by the DOM and there has been
  // no user interaction with the page since the web view was created, or if
  // the page has no navigation items, as occurs when an App Store link is
  // opened from another application.
  BOOL rendererInitiatedWithoutInteraction =
      self.webStateImpl->HasOpener() &&
      !self.userInteractionState
           ->UserInteractionRegisteredSinceWebViewCreated();
  BOOL noNavigationItems = !(self.navigationManagerImpl->GetItemCount());
  return rendererInitiatedWithoutInteraction || noNavigationItems;
}

// Returns YES if response should be rendered in WKWebView.
- (BOOL)shouldRenderResponse:(WKNavigationResponse*)WKResponse {
  if (!WKResponse.canShowMIMEType) {
    return NO;
  }

  GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  if (responseURL.SchemeIs(url::kDataScheme) && WKResponse.forMainFrame) {
    // Block rendering data URLs for renderer-initiated navigations in main
    // frame to prevent abusive behavior (crbug.com/890558).
    web::NavigationContext* context =
        [self contextForPendingMainFrameNavigationWithURL:responseURL];
    if (context->IsRendererInitiated()) {
      return NO;
    }
  }

  return YES;
}

// Creates DownloadTask for the given navigation response. Headers are passed
// as argument to avoid extra NSDictionary -> net::HttpResponseHeaders
// conversion.
- (void)createDownloadTaskForResponse:(WKNavigationResponse*)WKResponse
                          HTTPHeaders:(net::HttpResponseHeaders*)headers {
  const GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  const int64_t contentLength = WKResponse.response.expectedContentLength;
  const std::string MIMEType =
      base::SysNSStringToUTF8(WKResponse.response.MIMEType);

  std::string contentDisposition;
  if (headers) {
    headers->GetNormalizedHeader("content-disposition", &contentDisposition);
  }

  ui::PageTransition transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  if (WKResponse.forMainFrame) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:responseURL];
    context->SetIsDownload(true);
    context->ReleaseItem();
    // Navigation callbacks can only be called for the main frame.
    self.webStateImpl->OnNavigationFinished(context);
    transition = context->GetPageTransition();
    bool transitionIsLink = ui::PageTransitionTypeIncludingQualifiersIs(
        transition, ui::PAGE_TRANSITION_LINK);
    if (transitionIsLink && !context->HasUserGesture()) {
      // Link click is not possible without user gesture, so this transition
      // was incorrectly classified and should be "client redirect" instead.
      // TODO(crbug.com/549301): Remove this workaround when transition
      // detection is fixed.
      transition = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
    }
  }
  web::DownloadController::FromBrowserState(
      self.webStateImpl->GetBrowserState())
      ->CreateDownloadTask(self.webStateImpl, [NSUUID UUID].UUIDString,
                           responseURL, contentDisposition, contentLength,
                           MIMEType, transition);
}

// Updates URL for navigation context and navigation item.
- (void)didReceiveRedirectForNavigation:(web::NavigationContextImpl*)context
                                withURL:(const GURL&)URL {
  context->SetUrl(URL);
  web::NavigationItemImpl* item =
      web::GetItemWithUniqueID(self.navigationManagerImpl, context);

  // Associated item can be a pending item, previously discarded by another
  // navigation. WKWebView allows multiple provisional navigations, while
  // Navigation Manager has only one pending navigation.
  if (item) {
    if (!IsWKInternalUrl(URL)) {
      item->SetVirtualURL(URL);
      item->SetURL(URL);
    }
    // Redirects (3xx response code), must change POST requests to GETs.
    item->SetPostData(nil);
    item->ResetHttpRequestHeaders();
  }

  self.userInteractionState->ResetLastTransferTime();
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
    // cert verification result in |_certVerificationErrors|, so that it can
    // later be reused inside |didFailProvisionalNavigation:|.
    // The leaf cert is used as the key, because the chain provided by
    // |didFailProvisionalNavigation:| will differ (it is the server-supplied
    // chain), thus if intermediates were considered, the keys would mismatch.
    scoped_refptr<net::X509Certificate> leafCert =
        net::x509_util::CreateX509CertificateFromSecCertificate(
            SecTrustGetCertificateAtIndex(trust, 0),
            std::vector<SecCertificateRef>());
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
  DCHECK(
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodHTTPBasic] ||
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodNTLM] ||
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodHTTPDigest]);

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

// Called when a load ends in an SSL error and certificate chain.
- (void)handleSSLCertError:(NSError*)error
             forNavigation:(WKNavigation*)navigation
                   webView:(WKWebView*)webView {
  CHECK(web::IsWKWebViewSSLCertError(error));

  net::SSLInfo info;
  web::GetSSLInfoFromWKWebViewSSLCertError(error, &info);

  if (!info.cert) {
    // |info.cert| can be null if certChain in NSError is empty or can not be
    // parsed, in this case do not ask delegate if error should be allowed, it
    // should not be.
    [self handleLoadError:error
            forNavigation:navigation
                  webView:webView
          provisionalLoad:YES];
    return;
  }

  // Retrieve verification results from _certVerificationErrors cache to avoid
  // unnecessary recalculations. Verification results are cached for the leaf
  // cert, because the cert chain in |didReceiveAuthenticationChallenge:| is
  // the OS constructed chain, while |chain| is the chain from the server.
  NSArray* chain = error.userInfo[web::kNSErrorPeerCertificateChainKey];
  NSURL* requestURL = error.userInfo[web::kNSErrorFailingURLKey];
  NSString* host = requestURL.host;
  scoped_refptr<net::X509Certificate> leafCert;
  bool recoverable = false;
  if (chain.count && host.length) {
    // The complete cert chain may not be available, so the leaf cert is used
    // as a key to retrieve _certVerificationErrors, as well as for storing the
    // cert decision.
    leafCert = web::CreateCertFromChain(@[ chain.firstObject ]);
    if (leafCert) {
      auto error = _certVerificationErrors->Get(
          {leafCert, base::SysNSStringToUTF8(host)});
      bool cacheHit = error != _certVerificationErrors->end();
      if (cacheHit) {
        recoverable = error->second.is_recoverable;
        info.cert_status = error->second.status;
      }
      UMA_HISTOGRAM_BOOLEAN("WebController.CertVerificationErrorsCacheHit",
                            cacheHit);
    }
  }

  // If the current navigation item is in error state, update the error retry
  // state machine to indicate that SSL interstitial error will be displayed to
  // make sure subsequent back/forward navigation to this item starts with the
  // correct error retry state.
  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];
  if (context) {
    // This NavigationContext will be destroyed, so return pending item
    // ownership to NavigationManager. NavigationContext can only own pending
    // item until the navigation has committed or aborted.
    self.navigationManagerImpl->SetPendingItem(context->ReleaseItem());
    web::NavigationItemImpl* item =
        web::GetItemWithUniqueID(self.navigationManagerImpl, context);
    if (item && item->error_retry_state_machine().state() ==
                    web::ErrorRetryState::kRetryFailedNavigationItem) {
      item->error_retry_state_machine().SetDisplayingWebError();
    }
  }

  // Ask web client if this cert error should be allowed.
  web::GetWebClient()->AllowCertificateError(
      self.webStateImpl, net::MapCertStatusToNetError(info.cert_status), info,
      net::GURLWithNSURL(requestURL), recoverable, context->GetNavigationId(),
      base::BindRepeating(^(bool proceed) {
        if (proceed) {
          DCHECK(recoverable);
          [self.certVerificationController allowCert:leafCert
                                             forHost:host
                                              status:info.cert_status];
          self.webStateImpl->GetSessionCertificatePolicyCacheImpl()
              .RegisterAllowedCertificate(
                  leafCert, base::SysNSStringToUTF8(host), info.cert_status);
          // New navigation is a different navigation from the original one.
          // The new navigation is always browser-initiated and happens when
          // the browser allows to proceed with the load.
          [self.delegate navigationHandler:self
              loadCurrentURLWithRendererInitiatedNavigation:NO];
        } else {
          [self.legacyNativeContentController handleSSLError];
        }
      }));

  [self loadCancelled];
}

// Called when a load ends in an error.
- (void)handleLoadError:(NSError*)error
          forNavigation:(WKNavigation*)navigation
                webView:(WKWebView*)webView
        provisionalLoad:(BOOL)provisionalLoad {
  if (error.code == NSURLErrorCancelled) {
    [self handleCancelledError:error
                 forNavigation:navigation
               provisionalLoad:provisionalLoad];
    if (@available(iOS 13, *)) {
      // The bug has been fixed on iOS 13. The workaround is only needed for
      // other versions.
    } else if (@available(iOS 12.2, *)) {
      if (![webView.backForwardList.currentItem.URL isEqual:webView.URL] &&
          [self isCurrentNavigationItemPOST]) {
        UMA_HISTOGRAM_BOOLEAN("WebController.BackForwardListOutOfSync", true);
        // Sometimes on error the backForward list is out of sync with the
        // webView, go back or forward to fix it. See crbug.com/951880.
        if ([webView.backForwardList.backItem.URL isEqual:webView.URL]) {
          [webView goBack];
        } else if ([webView.backForwardList.forwardItem.URL
                       isEqual:webView.URL]) {
          [webView goForward];
        }
      }
    }
    // NSURLErrorCancelled errors that aren't handled by aborting the load will
    // automatically be retried by the web view, so early return in this case.
    return;
  }

  web::NavigationContextImpl* navigationContext =
      [self.navigationStates contextForNavigation:navigation];

  if (@available(iOS 13, *)) {
  } else {
    if (provisionalLoad && !navigationContext &&
        web::RequiresProvisionalNavigationFailureWorkaround()) {
      // It is likely that |navigationContext| is null because
      // didStartProvisionalNavigation: was not called with this WKNavigation
      // object. Log UMA to know when this workaround can be removed and
      // do not call OnNavigationFinished() to avoid crash on null pointer
      // dereferencing. See crbug.com/973653 and crbug.com/1004634 for details.
      UMA_HISTOGRAM_BOOLEAN(
          "Navigation.IOSNullContextInDidFailProvisionalNavigation", true);
      return;
    }
  }

  navigationContext->SetError(error);
  navigationContext->SetIsPost([self isCurrentNavigationItemPOST]);
  // TODO(crbug.com/803631) DCHECK that self.currentNavItem is the navigation
  // item associated with navigationContext.

  if ([error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)]) {
    if (error.code == web::kWebKitErrorPlugInLoadFailed) {
      // In cases where a Plug-in handles the load do not take any further
      // action.
      return;
    }

    ui::PageTransition transition = navigationContext->GetPageTransition();
    if (error.code == web::kWebKitErrorUrlBlockedByContentFilter) {
      DCHECK(provisionalLoad);
      if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
        // If URL is blocked due to Restriction, do not take any further
        // action as WKWebView will show a built-in error.
        if (!web::RequiresContentFilterBlockingWorkaround()) {
          // On iOS13, immediately following this navigation, WebKit will
          // navigate to an internal failure page. Unfortunately, due to how
          // SlimNav session restoration works with same document navigations,
          // this page blocked by a content filter puts WebKit into a state
          // where all further restoration same-document navigations are 'stuck'
          // on this failure page.  Instead, avoid restoring this page
          // completely.  Consider revisiting this if and when a proper session
          // restoration API is provided by WKWebView.
          self.navigationManagerImpl->SetWKWebViewNextPendingUrlNotSerializable(
              navigationContext->GetUrl());
          return;
        } else if (!PageTransitionIsNewNavigation(transition)) {
          if (!web::features::UseWKWebViewLoading() &&
              transition & ui::PAGE_TRANSITION_RELOAD) {
            self.webStateImpl->SetIsLoading(false);
          }
          return;
        }
      } else {
        if (transition & ui::PAGE_TRANSITION_RELOAD &&
            !(transition & ui::PAGE_TRANSITION_FORWARD_BACK)) {
          // There is no pending item for reload (see crbug.com/676129). So
          // the is nothing to do.
          DCHECK(!self.navigationManagerImpl->GetPendingItem());
        } else {
          // A new or back-forward navigation, which requires navigation item
          // commit.
          DCHECK(self.navigationManagerImpl->GetPendingItem());
          DCHECK(transition & ui::PAGE_TRANSITION_FORWARD_BACK ||
                 PageTransitionIsNewNavigation(transition));
          self.navigationManagerImpl->CommitPendingItem(
              navigationContext->ReleaseItem());
        }
        // WKWebView will show the error page, so no further action is required.
        return;
      }
    }

    if (error.code == web::kWebKitErrorFrameLoadInterruptedByPolicyChange) {
      // This method should not be called if the navigation was cancelled by
      // embedder.
      DCHECK(self.pendingNavigationInfo &&
             !self.pendingNavigationInfo.cancelled);

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
      if (!web::features::UseWKWebViewLoading()) {
        self.webStateImpl->SetIsLoading(false);
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
        if (!web::features::UseWKWebViewLoading()) {
          self.webStateImpl->SetIsLoading(false);
        }
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

  if (item) {
    GURL errorURL =
        net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey]);
    web::ErrorRetryCommand command = web::ErrorRetryCommand::kDoNothing;
    if (provisionalLoad) {
      command = item->error_retry_state_machine().DidFailProvisionalNavigation(
          net::GURLWithNSURL(webView.URL), errorURL);
    } else {
      command = item->error_retry_state_machine().DidFailNavigation(
          net::GURLWithNSURL(webView.URL));
    }
    [self handleErrorRetryCommand:command
                   navigationItem:item
                navigationContext:navigationContext
               originalNavigation:navigation
                          webView:webView];
  }

  // Don't commit the pending item or call OnNavigationFinished until the
  // placeholder navigation finishes loading.
}

// Handles cancelled load in WKWebView (error with NSURLErrorCancelled code).
- (void)handleCancelledError:(NSError*)error
               forNavigation:(WKNavigation*)navigation
             provisionalLoad:(BOOL)provisionalLoad {
  if ([self shouldCancelLoadForCancelledError:error
                              provisionalLoad:provisionalLoad]) {
    std::unique_ptr<web::NavigationContextImpl> navigationContext =
        [self.navigationStates removeNavigation:navigation];
    [self loadCancelled];
    web::NavigationItemImpl* item = web::GetItemWithUniqueID(
        self.navigationManagerImpl, navigationContext.get());
    if (self.navigationManagerImpl->GetPendingItem() == item) {
      self.navigationManagerImpl->DiscardNonCommittedItems();
    }

    [self.legacyNativeContentController
        handleCancelledErrorForContext:navigationContext.get()];

    if (provisionalLoad) {
      if (!navigationContext &&
          web::RequiresProvisionalNavigationFailureWorkaround()) {
        // It is likely that |navigationContext| is null because
        // didStartProvisionalNavigation: was not called with this WKNavigation
        // object. Log UMA to know when this workaround can be removed and
        // do not call OnNavigationFinished() to avoid crash on null pointer
        // dereferencing. See crbug.com/973653 for details.
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.IOSNullContextInDidFailProvisionalNavigation", true);
      } else {
        self.webStateImpl->OnNavigationFinished(navigationContext.get());
      }
    }
  } else if (!provisionalLoad) {
    web::NavigationContextImpl* navigationContext =
        [self.navigationStates contextForNavigation:navigation];
    web::NavigationItemImpl* item =
        web::GetItemWithUniqueID(self.navigationManagerImpl, navigationContext);
    if (item) {
      // Since the navigation has already been committed, it will retain its
      // back / forward item even though the load has been cancelled. Update the
      // error state machine so that if future loads of this item fail, the same
      // item will be reused for the error view rather than loading a
      // placeholder URL into a new navigation item, since the latter would
      // destroy the forward list.
      item->error_retry_state_machine().SetNoNavigationError();
    }
  }
}

// Executes the command specified by the ErrorRetryStateMachine.
- (void)handleErrorRetryCommand:(web::ErrorRetryCommand)command
                 navigationItem:(web::NavigationItemImpl*)item
              navigationContext:(web::NavigationContextImpl*)context
             originalNavigation:(WKNavigation*)originalNavigation
                        webView:(WKWebView*)webView {
  if (command == web::ErrorRetryCommand::kDoNothing)
    return;

  DCHECK_EQ(item->GetUniqueID(), context->GetNavigationItemUniqueID());
  switch (command) {
    case web::ErrorRetryCommand::kLoadPlaceholder: {
      // This case only happens when a new request failed in provisional
      // navigation. Disassociate the navigation context from the original
      // request and resuse it for the placeholder navigation.
      std::unique_ptr<web::NavigationContextImpl> originalContext =
          [self.navigationStates removeNavigation:originalNavigation];
      [self loadPlaceholderInWebViewForURL:item->GetURL()
                         rendererInitiated:context->IsRendererInitiated()
                                forContext:std::move(originalContext)];
    } break;

    case web::ErrorRetryCommand::kLoadError:
      [self loadErrorPageForNavigationItem:item
                         navigationContext:originalNavigation
                                   webView:webView];
      break;

    case web::ErrorRetryCommand::kReload:
      [webView reload];
      break;

    case web::ErrorRetryCommand::kRewriteToWebViewURL: {
      std::unique_ptr<web::NavigationContextImpl> navigationContext =
          [self.delegate navigationHandler:self
                 registerLoadRequestForURL:item->GetURL()
                    sameDocumentNavigation:NO
                            hasUserGesture:NO
                         rendererInitiated:context->IsRendererInitiated()
                     placeholderNavigation:NO];
      WKNavigation* navigation =
          [webView loadHTMLString:@""
                          baseURL:net::NSURLWithGURL(item->GetURL())];
      navigationContext->SetError(context->GetError());
      navigationContext->SetIsPost(context->IsPost());
      [self.navigationStates setContext:std::move(navigationContext)
                          forNavigation:navigation];
    } break;

    case web::ErrorRetryCommand::kRewriteToPlaceholderURL: {
      std::unique_ptr<web::NavigationContextImpl> originalContext =
          [self.navigationStates removeNavigation:originalNavigation];
      originalContext->SetPlaceholderNavigation(YES);
      GURL placeholderURL = CreatePlaceholderUrlForUrl(item->GetURL());

      WKNavigation* navigation =
          [webView loadHTMLString:@""
                          baseURL:net::NSURLWithGURL(placeholderURL)];
      [self.navigationStates setContext:std::move(originalContext)
                          forNavigation:navigation];
    } break;

    case web::ErrorRetryCommand::kDoNothing:
      NOTREACHED();
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
  base::Optional<net::SSLInfo> ssl_info = base::nullopt;

  if (web::IsWKWebViewSSLCertError(error) &&
      base::FeatureList::IsEnabled(web::features::kSSLCommittedInterstitials)) {
    web::GetSSLInfoFromWKWebViewSSLCertError(error, &info);
    if (info.cert) {
      // Retrieve verification results from _certVerificationErrors cache to
      // avoid unnecessary recalculations. Verification results are cached for
      // the leaf cert, because the cert chain in
      // |didReceiveAuthenticationChallenge:| is the OS constructed chain, while
      // |chain| is the chain from the server.
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
          auto error = _certVerificationErrors->Get(
              {leafCert, base::SysNSStringToUTF8(host)});
          bool cacheHit = error != _certVerificationErrors->end();
          if (cacheHit) {
            info.is_fatal_cert_error = error->second.is_recoverable;
            info.cert_status = error->second.status;
          }
          UMA_HISTOGRAM_BOOLEAN("WebController.CertVerificationErrorsCacheHit",
                                cacheHit);
        }
      }
    }
    ssl_info = base::make_optional<net::SSLInfo>(info);
  } else if (web::IsWKWebViewSSLCertError(error) &&
             !base::FeatureList::IsEnabled(
                 web::features::kSSLCommittedInterstitials)) {
    // This could happen only if certificate is absent or could not be parsed.
    error = web::NetErrorFromError(error, net::ERR_SSL_SERVER_CERT_BAD_FORMAT);
#if defined(DEBUG)
    net::SSLInfo info;
    web::GetSSLInfoFromWKWebViewSSLCertError(error, &info);
    CHECK(!error.cert);
#endif
  } else {
    error = web::NetErrorFromError(error);
  }
  NSString* failingURLString =
      error.userInfo[NSURLErrorFailingURLStringErrorKey];
  GURL failingURL(base::SysNSStringToUTF8(failingURLString));
  GURL itemURL = item->GetURL();
  int itemID = item->GetUniqueID();
  web::GetWebClient()->PrepareErrorPage(
      self.webStateImpl, failingURL, error, context->IsPost(),
      self.webStateImpl->GetBrowserState()->IsOffTheRecord(), ssl_info,
      context->GetNavigationId(), base::BindOnce(^(NSString* errorHTML) {
        if (errorHTML) {
          WKNavigation* navigation =
              [webView loadHTMLString:errorHTML
                              baseURL:net::NSURLWithGURL(failingURL)];
          auto loadHTMLContext =
              web::NavigationContextImpl::CreateNavigationContext(
                  self.webStateImpl, failingURL,
                  /*has_user_gesture=*/false, ui::PAGE_TRANSITION_FIRST,
                  /*is_renderer_initiated=*/false);
          loadHTMLContext->SetLoadingErrorPage(true);
          loadHTMLContext->SetNavigationItemUniqueID(itemID);

          [self.navigationStates setContext:std::move(loadHTMLContext)
                              forNavigation:navigation];
          [self.navigationStates setState:web::WKNavigationState::REQUESTED
                            forNavigation:navigation];
        }

        // TODO(crbug.com/803503): only call these for placeholder navigation
        // because they should have already been triggered during navigation
        // commit for failures that happen after commit.
        [self.delegate navigationHandlerDidStartLoading:self];
        // TODO(crbug.com/973765): This is a workaround because |item| might get
        // released after
        // |self.navigationManagerImpl->CommitPendingItem(context->ReleaseItem()|.
        // Remove this once navigation refactor is done.
        web::NavigationContextImpl* context =
            [self.navigationStates contextForNavigation:navigation];
        self.navigationManagerImpl->CommitPendingItem(context->ReleaseItem());
        if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
          [self.delegate navigationHandler:self
                            setDocumentURL:itemURL
                                   context:context];
        } else {
          web::NavigationItem* lastCommittedItem =
              self.navigationManagerImpl->GetLastCommittedItem();
          DCHECK_EQ(itemURL, lastCommittedItem->GetURL());
          [self.delegate navigationHandler:self
                            setDocumentURL:lastCommittedItem->GetURL()
                                   context:context];
        }

        // If |context| is a placeholder navigation, this is the second part of
        // the error page load for a provisional load failure. Rewrite the
        // context URL to actual URL and trigger the deferred
        // |OnNavigationFinished| callback. This is also needed if |context| is
        // not yet committed, which can happen on a reload/back/forward load
        // that failed in provisional navigation.
        if (context->IsPlaceholderNavigation() || !context->HasCommitted()) {
          context->SetUrl(itemURL);
          context->SetPlaceholderNavigation(false);
          context->SetHasCommitted(true);
          self.webStateImpl->OnNavigationFinished(context);
        }

        // For SSL cert error pages, SSLStatus needs to be set manually because
        // the placeholder navigation for the error page is committed and
        // there is no server trust (since there's no network navigation), which
        // is required to create a cert in CRWSSLStatusUpdater.
        if (web::IsWKWebViewSSLCertError(context->GetError())) {
          web::SSLStatus& SSLStatus =
              self.navigationManagerImpl->GetLastCommittedItem()->GetSSL();
          SSLStatus.cert_status = info.cert_status;
          SSLStatus.certificate = info.cert;
          SSLStatus.security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
          self.webStateImpl->DidChangeVisibleSecurityState();
        }

        [self.delegate navigationHandler:self
              didCompleteLoadWithSuccess:NO
                              forContext:context];
        if (!web::features::UseWKWebViewLoading()) {
          self.webStateImpl->SetIsLoading(false);
        }
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

- (void)close {
  self.beingDestroyed = YES;
}

- (void)stopLoading {
  self.pendingNavigationInfo.cancelled = YES;
  _safeBrowsingWarningDetectionTimer.Stop();
  [self loadCancelled];
  _certVerificationErrors->Clear();
}

- (void)loadCancelled {
  // TODO(crbug.com/821995):  Check if this function should be removed.
  if (self.navigationState != web::WKNavigationState::FINISHED) {
    self.navigationState = web::WKNavigationState::FINISHED;
    if (!self.beingDestroyed) {
      self.webStateImpl->SetIsLoading(false);
    }
  }
}

// Returns context for pending navigation that has |URL|. null if there is no
// matching pending navigation.
- (web::NavigationContextImpl*)contextForPendingMainFrameNavigationWithURL:
    (const GURL&)URL {
  // Here the enumeration variable |navigation| is __strong to allow setting it
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
  // |self.navigationHandler.pendingNavigationInfo| will be nil if the
  // decidePolicy* delegate methods were not called.
  NSString* HTTPMethod =
      self.pendingNavigationInfo
          ? self.pendingNavigationInfo.HTTPMethod
          : self.currentBackForwardListItemHolder->http_method();
  if ([HTTPMethod isEqual:@"POST"]) {
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
    // TODO(crbug.com/925304): Pending item (which stores the holder) should be
    // owned by NavigationContext object. Pending item should never be null.
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
  GURL navigationURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
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
  // TODO(crbug.com/227769): When possible, get the real referrer and policy in
  // advance and use that instead.
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
      // TODO(crbug.com/549301): See if this heuristic can be improved.
      return self.userInteractionState
                     ->UserInteractionRegisteredSinceLastUrlChange()
                 ? ui::PAGE_TRANSITION_LINK
                 : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  }
}

- (web::NavigationContextImpl*)
    loadPlaceholderInWebViewForURL:(const GURL&)originalURL
                 rendererInitiated:(BOOL)rendererInitiated
                        forContext:(std::unique_ptr<web::NavigationContextImpl>)
                                       originalContext {
  GURL placeholderURL = CreatePlaceholderUrlForUrl(originalURL);
  // TODO(crbug.com/956511): Remove this code when NativeContent support is
  // removed.
  WKWebView* webView =
      [self.delegate navigationHandlerEnsureWebViewCreated:self];

  NSURLRequest* request =
      [NSURLRequest requestWithURL:net::NSURLWithGURL(placeholderURL)];
  WKNavigation* navigation = [webView loadRequest:request];

  NSError* error = originalContext ? originalContext->GetError() : nil;
  if (web::RequiresContentFilterBlockingWorkaround() &&
      [error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)] &&
      error.code == web::kWebKitErrorUrlBlockedByContentFilter) {
    GURL currentWKItemURL =
        net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
    if (currentWKItemURL.SchemeIs(url::kAboutScheme)) {
      // WKWebView will pass nil WKNavigation objects to WKNavigationDelegate
      // callback for this navigation. TODO(crbug.com/954332): Remove the
      // workaround when https://bugs.webkit.org/show_bug.cgi?id=196930 is
      // fixed.
      navigation = nil;
    }
  }

  [self.navigationStates setState:web::WKNavigationState::REQUESTED
                    forNavigation:navigation];
  std::unique_ptr<web::NavigationContextImpl> navigationContext;
  if (originalContext) {
    navigationContext = std::move(originalContext);
    navigationContext->SetPlaceholderNavigation(YES);
  } else {
    navigationContext = [self.delegate navigationHandler:self
                               registerLoadRequestForURL:originalURL
                                  sameDocumentNavigation:NO
                                          hasUserGesture:NO
                                       rendererInitiated:rendererInitiated
                                   placeholderNavigation:YES];
  }
  [self.navigationStates setContext:std::move(navigationContext)
                      forNavigation:navigation];
  return [self.navigationStates contextForNavigation:navigation];
}

- (void)webPageChangedWithContext:(web::NavigationContextImpl*)context
                          webView:(WKWebView*)webView {
  web::Referrer referrer = self.currentReferrer;
  // If no referrer was known in advance, record it now. (If there was one,
  // keep it since it will have a more accurate URL and policy than what can
  // be extracted from the landing page.)
  web::NavigationItem* currentItem = self.currentNavItem;

  // TODO(crbug.com/925304): Pending item (which should be used here) should be
  // owned by NavigationContext object. Pending item should never be null.
  if (currentItem && !currentItem->GetReferrer().url.is_valid()) {
    currentItem->SetReferrer(referrer);
  }

  // TODO(crbug.com/956511): This shouldn't be called for hash state or
  // push/replaceState.
  [self resetDocumentSpecificState];

  [self.delegate navigationHandlerDidStartLoading:self];
  // Do not commit pending item in the middle of loading a placeholder URL. The
  // item will be committed when the native content or webUI is displayed.
  if (!context->IsPlaceholderNavigation()) {
    self.navigationManagerImpl->CommitPendingItem(context->ReleaseItem());
    if (context->IsLoadingHtmlString()) {
      self.navigationManagerImpl->GetLastCommittedItem()->SetURL(
          context->GetUrl());
    }
    // If a SafeBrowsing warning is currently displayed, the user has tapped
    // the button on the warning page to proceed to the site, the site has
    // started loading, and the warning is about to be removed. In this case,
    // the transient item for the warning needs to be removed too.
    if (web::IsSafeBrowsingWarningDisplayedInWebView(webView))
      self.navigationManagerImpl->DiscardNonCommittedItems();
  }
}

@end
