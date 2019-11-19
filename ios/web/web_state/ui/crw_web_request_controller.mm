// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_request_controller.h"

#import <WebKit/WebKit.h>

#include "base/feature_list.h"
#include "base/i18n/i18n_constants.h"
#import "base/ios/block_types.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/common/features.h"
#include "ios/web/common/referrer_util.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/crw_pending_navigation_info.h"
#import "ios/web/navigation/crw_wk_navigation_handler.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_back_forward_list_item_holder.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_state/ui/controller/crw_legacy_native_content_controller.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::wk_navigation_util::ExtractTargetURL;
using web::wk_navigation_util::ExtractUrlFromPlaceholderUrl;
using web::wk_navigation_util::IsPlaceholderUrl;
using web::wk_navigation_util::IsRestoreSessionUrl;
using web::wk_navigation_util::IsWKInternalUrl;
using web::wk_navigation_util::kReferrerHeaderName;
using web::wk_navigation_util::URLNeedsUserAgentType;

namespace {
// Values for the histogram that counts slow/fast back/forward navigations.
enum class BackForwardNavigationType {
  // Fast back navigation through WKWebView back-forward list.
  FAST_BACK = 0,
  // Slow back navigation when back-forward list navigation is not possible.
  SLOW_BACK = 1,
  // Fast forward navigation through WKWebView back-forward list.
  FAST_FORWARD = 2,
  // Slow forward navigation when back-forward list navigation is not possible.
  SLOW_FORWARD = 3,
  BACK_FORWARD_NAVIGATION_TYPE_COUNT
};
}

@interface CRWWebRequestController ()

@property(nonatomic, readonly) web::WebStateImpl* webState;

// Returns a web view, if one is associated from delegate.
@property(nonatomic, readonly) WKWebView* webView;

// Returns The WKNavigationDelegate handler class from delegate.
@property(nonatomic, readonly) CRWWKNavigationHandler* navigationHandler;

// Set to YES when [self close] is called.
@property(nonatomic, assign) BOOL beingDestroyed;

// Returns The CRWLegacyNativeContentController handler class from delegate.
@property(nonatomic, readonly)
    CRWLegacyNativeContentController* legacyNativeController;

@end

@implementation CRWWebRequestController

- (instancetype)initWithWebState:(web::WebStateImpl*)webState {
  self = [super init];
  if (self) {
    _webState = webState;
  }
  return self;
}

- (void)close {
  self.beingDestroyed = YES;
  _webState = nullptr;
}

- (void)loadCurrentURLWithRendererInitiatedNavigation:(BOOL)rendererInitiated {
  // Reset current WebUI if one exists.
  [self clearWebUI];

  // Abort any outstanding page load. This ensures the delegate gets informed
  // about the outgoing page, and further messages from the page are suppressed.
  if (self.navigationHandler.navigationState !=
      web::WKNavigationState::FINISHED) {
    [self.webView stopLoading];
    [self.navigationHandler stopLoading];
  }

  self.webState->ClearTransientContent();

  web::NavigationItem* item = self.currentNavItem;
  const GURL currentURL = item ? item->GetURL() : GURL::EmptyGURL();
  const bool isCurrentURLAppSpecific =
      web::GetWebClient()->IsAppSpecificURL(currentURL);
  // If it's a chrome URL, but not a native one, create the WebUI instance.
  if (isCurrentURLAppSpecific &&
      ![self.legacyNativeController shouldLoadURLInNativeView:currentURL]) {
    if (!(item->GetTransitionType() & ui::PAGE_TRANSITION_TYPED ||
          item->GetTransitionType() & ui::PAGE_TRANSITION_AUTO_BOOKMARK) &&
        self.hasOpener) {
      // WebUI URLs can not be opened by DOM to prevent cross-site scripting as
      // they have increased power. WebUI URLs may only be opened when the user
      // types in the URL or use bookmarks.
      self.navigationManagerImpl->DiscardNonCommittedItems();
      return;
    } else {
      [self createWebUIForURL:currentURL];
    }
  }

  // Loading a new url, must check here if it's a native chrome URL and
  // replace the appropriate view if so, or transition back to a web view from
  // a native view.
  if ([self.legacyNativeController shouldLoadURLInNativeView:currentURL]) {
    [self.legacyNativeController
        loadCurrentURLInNativeViewWithRendererInitiatedNavigation:
            rendererInitiated];
  } else if ([self maybeLoadRequestForCurrentNavigationItem]) {
    [_delegate webRequestControllerEnsureWebViewCreated:self];
    [self loadRequestForCurrentNavigationItem];
  }
}

- (void)loadData:(NSData*)data
        MIMEType:(NSString*)MIMEType
          forURL:(const GURL&)URL {
  [_delegate webRequestControllerStopLoading:self];
  web::NavigationItemImpl* item =
      self.navigationManagerImpl->GetLastCommittedItemImpl();
  if (!item) {
    return;
  }
  auto navigationContext = web::NavigationContextImpl::CreateNavigationContext(
      self.webState, URL,
      /*has_user_gesture=*/true, item->GetTransitionType(),
      /*is_renderer_initiated=*/false);
  self.navigationHandler.navigationState = web::WKNavigationState::REQUESTED;
  navigationContext->SetNavigationItemUniqueID(item->GetUniqueID());

  item->SetNavigationInitiationType(
      web::NavigationInitiationType::BROWSER_INITIATED);
  // The error_retry_state_machine may still be in the
  // |kDisplayingWebErrorForFailedNavigation| from the navigation that is being
  // replaced. As the navigation is now successful, the error can be cleared.
  item->error_retry_state_machine().SetNoNavigationError();
  // The load data call will replace the current navigation and the webView URL
  // of the navigation will be replaced by |URL|. Set the URL of the
  // navigationItem to keep them synced.
  // Note: it is possible that the URL in item already match |url|. But item can
  // also contain a placeholder URL intended to be replaced.
  item->SetURL(URL);
  navigationContext->SetMimeType(MIMEType);

  WKNavigation* navigation =
      [self.webView loadData:data
                       MIMEType:MIMEType
          characterEncodingName:base::SysUTF8ToNSString(base::kCodepageUTF8)
                        baseURL:net::NSURLWithGURL(URL)];

  [self.navigationHandler.navigationStates
         setContext:std::move(navigationContext)
      forNavigation:navigation];
  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
      forNavigation:navigation];
}

- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL {
  // Web View should not be created for App Specific URLs.
  if (!web::GetWebClient()->IsAppSpecificURL(URL)) {
    [_delegate webRequestControllerEnsureWebViewCreated:self];
    DCHECK(self.webView) << "self.webView null while trying to load HTML";
  }

  DCHECK(HTML.length);
  // Remove the transient content view.
  self.webState->ClearTransientContent();

  self.navigationHandler.navigationState = web::WKNavigationState::REQUESTED;

  WKNavigation* navigation =
      [self.webView loadHTMLString:HTML baseURL:net::NSURLWithGURL(URL)];
  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
      forNavigation:navigation];
  std::unique_ptr<web::NavigationContextImpl> context;
  const ui::PageTransition loadHTMLTransition =
      ui::PageTransition::PAGE_TRANSITION_TYPED;
  if (self.webState->HasWebUI()) {
    // WebUI uses |loadHTML:forURL:| to feed the content to web view. This
    // should not be treated as a navigation, but WKNavigationDelegate callbacks
    // still expect a valid context.
    context = web::NavigationContextImpl::CreateNavigationContext(
        self.webState, URL, /*has_user_gesture=*/true, loadHTMLTransition,
        /*is_renderer_initiated=*/false);
    context->SetNavigationItemUniqueID(self.currentNavItem->GetUniqueID());
    // Transfer pending item ownership to NavigationContext.
    // NavigationManager owns pending item after navigation is requested and
    // until navigation context is created.
    context->SetItem(self.navigationManagerImpl->ReleasePendingItem());
  } else {
    context = [self registerLoadRequestForURL:URL
                                     referrer:web::Referrer()
                                   transition:loadHTMLTransition
                       sameDocumentNavigation:NO
                               hasUserGesture:YES
                            rendererInitiated:NO
                        placeholderNavigation:NO];
  }
  context->SetLoadingHtmlString(true);
  context->SetMimeType(@"text/html");
  [self.navigationHandler.navigationStates setContext:std::move(context)
                                        forNavigation:navigation];
}

- (void)reloadWithRendererInitiatedNavigation:(BOOL)rendererInitiated {
  GURL URL = self.currentNavItem->GetURL();
  if ([self.legacyNativeController shouldLoadURLInNativeView:URL]) {
    std::unique_ptr<web::NavigationContextImpl> navigationContext = [self
        registerLoadRequestForURL:URL
                         referrer:self.currentNavItemReferrer
                       transition:ui::PageTransition::PAGE_TRANSITION_RELOAD
           sameDocumentNavigation:NO
                   hasUserGesture:YES
                rendererInitiated:rendererInitiated
            placeholderNavigation:NO];
    self.webState->OnNavigationStarted(navigationContext.get());
    [_delegate webRequestControllerDidStartLoading:self];
    self.navigationManagerImpl->CommitPendingItem(
        navigationContext->ReleaseItem());
    [self.legacyNativeController reload];
    navigationContext->SetHasCommitted(true);
    self.webState->OnNavigationFinished(navigationContext.get());
    [_delegate webRequestController:self
         didCompleteLoadWithSuccess:(BOOL)YES
                         forContext:nullptr];
  } else {
    web::NavigationItem* transientItem =
        self.navigationManagerImpl->GetTransientItem();
    if (transientItem) {
      // If there's a transient item, a reload is considered a new navigation to
      // the transient item's URL (as on other platforms).
      web::NavigationManager::WebLoadParams reloadParams(
          transientItem->GetURL());
      reloadParams.transition_type = ui::PAGE_TRANSITION_RELOAD;
      reloadParams.extra_headers =
          [transientItem->GetHttpRequestHeaders() copy];
      self.webState->GetNavigationManager()->LoadURLWithParams(reloadParams);
    } else {
      self.currentNavItem->SetTransitionType(
          ui::PageTransition::PAGE_TRANSITION_RELOAD);
      if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
          !web::GetWebClient()->IsAppSpecificURL(
              net::GURLWithNSURL(self.webView.URL))) {
        // New navigation manager can delegate directly to WKWebView to reload
        // for non-app-specific URLs. The necessary navigation states will be
        // updated in WKNavigationDelegate callbacks.
        WKNavigation* navigation = [self.webView reload];
        [self.navigationHandler.navigationStates
                 setState:web::WKNavigationState::REQUESTED
            forNavigation:navigation];
        std::unique_ptr<web::NavigationContextImpl> navigationContext = [self
            registerLoadRequestForURL:URL
                             referrer:self.currentNavItemReferrer
                           transition:ui::PageTransition::PAGE_TRANSITION_RELOAD
               sameDocumentNavigation:NO
                       hasUserGesture:YES
                    rendererInitiated:rendererInitiated
                placeholderNavigation:NO];
        [self.navigationHandler.navigationStates
               setContext:std::move(navigationContext)
            forNavigation:navigation];
      } else {
        [self loadCurrentURLWithRendererInitiatedNavigation:rendererInitiated];
      }
    }
  }
}

- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated
        placeholderNavigation:(BOOL)placeholderNavigation {
  // Get the navigation type from the last main frame load request, and try to
  // map that to a PageTransition.
  WKNavigationType navigationType =
      self.navigationHandler.pendingNavigationInfo
          ? self.navigationHandler.pendingNavigationInfo.navigationType
          : WKNavigationTypeOther;
  ui::PageTransition transition =
      [self.navigationHandler pageTransitionFromNavigationType:navigationType];

  WKBackForwardListItem* currentItem = self.webView.backForwardList.currentItem;
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() && currentItem) {
    // SlimNav target redirect pages should default to a RELOAD transition,
    // because transition state is not persisted on restore.
    GURL targetURL;
    if (navigationType == WKNavigationTypeOther &&
        IsRestoreSessionUrl(net::GURLWithNSURL(currentItem.URL)) &&
        ExtractTargetURL(net::GURLWithNSURL(currentItem.URL), &targetURL) &&
        targetURL == URL) {
      DCHECK(ui::PageTransitionIsRedirect(transition));
      transition = ui::PAGE_TRANSITION_RELOAD;
    } else if (navigationType == WKNavigationTypeBackForward) {
      web::NavigationItem* currentItem = [[CRWNavigationItemHolder
          holderForBackForwardListItem:self.webView.backForwardList.currentItem]
          navigationItem];
      if (currentItem) {
        transition = ui::PageTransitionFromInt(
            transition | currentItem->GetTransitionType());
      }
    }
  }

  // The referrer is not known yet, and will be updated later.
  const web::Referrer emptyReferrer;
  std::unique_ptr<web::NavigationContextImpl> context =
      [self registerLoadRequestForURL:URL
                             referrer:emptyReferrer
                           transition:transition
               sameDocumentNavigation:sameDocumentNavigation
                       hasUserGesture:hasUserGesture
                    rendererInitiated:rendererInitiated
                placeholderNavigation:placeholderNavigation];
  context->SetWKNavigationType(navigationType);
  return context;
}

- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)requestURL
                     referrer:(const web::Referrer&)referrer
                   transition:(ui::PageTransition)transition
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated
        placeholderNavigation:(BOOL)placeholderNavigation {
  // Transfer time is registered so that further transitions within the time
  // envelope are not also registered as links.
  [_delegate
      webRequestControllerUserInteractionState:self] -> ResetLastTransferTime();

  // Add or update pending item before any WebStateObserver callbacks.
  // See https://crbug.com/842151 for a scenario where this is important.
  web::NavigationItem* item =
      self.navigationManagerImpl->GetPendingItemInCurrentOrRestoredSession();
  if (item) {
    // Update the existing pending entry.
    // Typically on PAGE_TRANSITION_CLIENT_REDIRECT.
    // Don't update if request is a placeholder entry because the pending item
    // should have the original target URL.
    // Don't update if pending URL has a different origin, because client
    // redirects can not change the origin. It is possible to have more than one
    // pending navigations, so the redirect does not necesserily belong to the
    // pending navigation item.
    if (!placeholderNavigation &&
        item->GetURL().GetOrigin() == requestURL.GetOrigin()) {
      self.navigationManagerImpl->UpdatePendingItemUrl(requestURL);
    }
  } else {
    self.navigationManagerImpl->AddPendingItem(
        requestURL, referrer, transition,
        rendererInitiated ? web::NavigationInitiationType::RENDERER_INITIATED
                          : web::NavigationInitiationType::BROWSER_INITIATED,
        web::NavigationManager::UserAgentOverrideOption::INHERIT);
    item =
        self.navigationManagerImpl->GetPendingItemInCurrentOrRestoredSession();
  }

  bool redirect = transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK;
  if (!redirect) {
    // Before changing navigation state, the delegate should be informed that
    // any existing request is being cancelled before completion.
    [self.navigationHandler loadCancelled];
    DCHECK_EQ(web::WKNavigationState::FINISHED,
              self.navigationHandler.navigationState);
  }

  self.navigationHandler.navigationState = web::WKNavigationState::REQUESTED;

  // Record the state of outgoing web view. Do nothing if native controller
  // exists, because in that case recordStateInHistory will record the state
  // of incoming page as native controller is already inserted.
  // TODO(crbug.com/811770) Don't record state under WKBasedNavigationManager
  // because it may incorrectly clobber the incoming page if this is a
  // back/forward navigation. WKWebView restores page scroll state for web view
  // pages anyways so this only impacts user if WKWebView is deleted.
  if (!redirect && ![self.legacyNativeController hasController] &&
      !web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    [_delegate webRequestControllerRecordStateInHistory:self];
  }

  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webState, requestURL, hasUserGesture, transition,
          rendererInitiated);
  context->SetPlaceholderNavigation(placeholderNavigation);

  // TODO(crbug.com/676129): LegacyNavigationManagerImpl::AddPendingItem does
  // not create a pending item in case of reload. Remove this workaround once
  // the bug is fixed or WKBasedNavigationManager is fully adopted.
  if (!item) {
    DCHECK(!web::GetWebClient()->IsSlimNavigationManagerEnabled());
    item = self.navigationManagerImpl->GetLastCommittedItem();
  }

  context->SetNavigationItemUniqueID(item->GetUniqueID());
  context->SetIsPost([self.navigationHandler isCurrentNavigationItemPOST]);
  context->SetIsSameDocument(sameDocumentNavigation);

  // If WKWebView.loading is used for WebState::IsLoading, do not set it for
  // renderer-initated navigation otherwise WebState::IsLoading will remain true
  // after hash change in the web page.
  if (!IsWKInternalUrl(requestURL) && !placeholderNavigation &&
      (!web::features::UseWKWebViewLoading() || !rendererInitiated)) {
    self.webState->SetIsLoading(true);
  }

  // WKWebView may have multiple pending items. Move pending item ownership from
  // NavigationManager to NavigationContext. NavigationManager owns pending item
  // after navigation was requested and until NavigationContext is created.
  // No need to transfer the ownership for NativeContent URLs, because the
  // load of NativeContent is synchronous. No need to transfer the ownership
  // for WebUI navigations, because those navigation do not have access to
  // NavigationContext.
  if (![self.legacyNativeController
          shouldLoadURLInNativeView:context->GetUrl()]) {
    if (self.navigationManagerImpl->GetPendingItemIndex() == -1) {
      context->SetItem(self.navigationManagerImpl->ReleasePendingItem());
    }
  }

  return context;
}

- (void)didFinishWithURL:(const GURL&)currentURL
             loadSuccess:(BOOL)loadSuccess
                 context:(nullable const web::NavigationContextImpl*)context {
  DCHECK_EQ(web::WKNavigationState::FINISHED,
            self.navigationHandler.navigationState);

  [_delegate webRequestControllerRestoreStateFromHistory:self];

  // Placeholder and restore session URLs are implementation details so should
  // not notify WebStateObservers. If |context| is nullptr, don't skip
  // placeholder URLs because this may be the only opportunity to update
  // |isLoading| for native view reload.

  if (context && context->IsPlaceholderNavigation())
    return;

  if (context && IsRestoreSessionUrl(context->GetUrl()))
    return;

  if (IsRestoreSessionUrl(net::GURLWithNSURL(self.webView.URL)))
    return;

  if (context && context->IsLoadingErrorPage())
    return;

  if (!loadSuccess) {
    // WebStateObserver callbacks will be called for load failure after
    // loading placeholder URL.
    return;
  }
  if (!web::features::UseWKWebViewLoading()) {
    if (![self.navigationHandler.navigationStates
                lastNavigationWithPendingItemInNavigationContext]) {
      self.webState->SetIsLoading(false);
    } else {
      // There is another pending navigation, so the state is still loading.
    }
  }

  self.webState->OnPageLoaded(currentURL, YES);

  if (context) {
    if (context->IsRendererInitiated()) {
      UMA_HISTOGRAM_TIMES("PLT.iOS.RendererInitiatedPageLoadTime",
                          context->GetElapsedTimeSinceCreation());
    } else {
      UMA_HISTOGRAM_TIMES("PLT.iOS.BrowserInitiatedPageLoadTime",
                          context->GetElapsedTimeSinceCreation());
    }
  }
}

// Reports Navigation.IOSWKWebViewSlowFastBackForward UMA. No-op if pending
// navigation is not back forward navigation.
- (void)reportBackForwardNavigationTypeForFastNavigation:(BOOL)isFast {
  web::NavigationManager* navigationManager = self.navigationManagerImpl;
  int pendingIndex = navigationManager->GetPendingItemIndex();
  if (pendingIndex == -1) {
    // Pending navigation is not a back forward navigation.
    return;
  }

  BOOL isBack = pendingIndex < navigationManager->GetLastCommittedItemIndex();
  BackForwardNavigationType type = BackForwardNavigationType::FAST_BACK;
  if (isBack) {
    type = isFast ? BackForwardNavigationType::FAST_BACK
                  : BackForwardNavigationType::SLOW_BACK;
  } else {
    type = isFast ? BackForwardNavigationType::FAST_FORWARD
                  : BackForwardNavigationType::SLOW_FORWARD;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.IOSWKWebViewSlowFastBackForward", type,
      BackForwardNavigationType::BACK_FORWARD_NAVIGATION_TYPE_COUNT);
}

#pragma mark Navigation and Session Information

// Returns the associated NavigationManagerImpl.
- (web::NavigationManagerImpl*)navigationManagerImpl {
  return self.webState ? &(self.webState->GetNavigationManagerImpl()) : nil;
}

// Returns the navigation item for the current page.
- (web::NavigationItemImpl*)currentNavItem {
  return self.navigationManagerImpl
             ? self.navigationManagerImpl->GetCurrentItemImpl()
             : nullptr;
}

// Returns the current transition type.
- (ui::PageTransition)currentTransition {
  if (self.currentNavItem)
    return self.currentNavItem->GetTransitionType();
  else
    return ui::PageTransitionFromInt(0);
}

// Returns the referrer for current navigation item. May be empty.
- (web::Referrer)currentNavItemReferrer {
  web::NavigationItem* currentItem = self.currentNavItem;
  return currentItem ? currentItem->GetReferrer() : web::Referrer();
}

// The HTTP headers associated with the current navigation item. These are nil
// unless the request was a POST.
- (NSDictionary*)currentHTTPHeaders {
  web::NavigationItem* currentItem = self.currentNavItem;
  return currentItem ? currentItem->GetHttpRequestHeaders() : nil;
}

#pragma mark - WebUI

- (void)createWebUIForURL:(const GURL&)URL {
  // |CreateWebUI| will do nothing if |URL| is not a WebUI URL and then
  // |HasWebUI| will return false.
  self.webState->CreateWebUI(URL);
}

- (void)clearWebUI {
  self.webState->ClearWebUI();
}

#pragma mark - Private methods

// Checks if a load request of the current navigation item should proceed. If
// this returns |YES|, caller should create a webView and call
// |loadRequestForCurrentNavigationItem|. Otherwise this will set the request
// state to finished and call |didFinishWithURL| with failure.
- (BOOL)maybeLoadRequestForCurrentNavigationItem {
  web::NavigationItem* item = self.currentNavItem;
  GURL targetURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
  // Load the url. The UIWebView delegate callbacks take care of updating the
  // session history and UI.
  if (!targetURL.is_valid()) {
    [self didFinishWithURL:targetURL loadSuccess:NO context:nullptr];
    self.webState->SetIsLoading(false);
    self.webState->OnPageLoaded(targetURL, NO);
    return NO;
  }

  // JavaScript should never be evaluated here. User-entered JS should be
  // evaluated via stringByEvaluatingUserJavaScriptFromString.
  DCHECK(!targetURL.SchemeIs(url::kJavaScriptScheme));

  return YES;
}

// Loads request for the URL of the current navigation item. Subclasses may
// choose to build a new NSURLRequest and call
// |loadRequestForCurrentNavigationItem| on the underlying web view, or use
// native web view navigation where possible (for example, going back and
// forward through the history stack).
- (void)loadRequestForCurrentNavigationItem {
  DCHECK(self.webView);
  DCHECK(self.currentNavItem);
  // If a load is kicked off on a WKWebView with a frame whose size is {0, 0} or
  // that has a negative dimension for a size, rendering issues occur that
  // manifest in erroneous scrolling and tap handling (crbug.com/574996,
  // crbug.com/577793).
  DCHECK_GT(CGRectGetWidth(self.webView.frame), 0.0);
  DCHECK_GT(CGRectGetHeight(self.webView.frame), 0.0);

  // If the current item uses a different user agent from that is currently used
  // in the web view, update |customUserAgent| property, which will be used by
  // the next request sent by this web view.
  web::UserAgentType itemUserAgentType =
      self.currentNavItem->GetUserAgentType();
  if (itemUserAgentType != web::UserAgentType::NONE) {
    NSString* userAgentString = base::SysUTF8ToNSString(
        web::GetWebClient()->GetUserAgent(itemUserAgentType));
    if (![self.webView.customUserAgent isEqualToString:userAgentString]) {
      self.webView.customUserAgent = userAgentString;
    }
  }

  web::WKBackForwardListItemHolder* holder =
      self.navigationHandler.currentBackForwardListItemHolder;

  BOOL repostedForm =
      [holder->http_method() isEqual:@"POST"] &&
      (holder->navigation_type() == WKNavigationTypeFormResubmitted ||
       holder->navigation_type() == WKNavigationTypeFormSubmitted);
  web::NavigationItemImpl* currentItem = self.currentNavItem;
  NSData* POSTData = currentItem->GetPostData();
  NSMutableURLRequest* request = [self requestForCurrentNavigationItem];

  BOOL sameDocumentNavigation = currentItem->IsCreatedFromPushState() ||
                                currentItem->IsCreatedFromHashChange();

  if (holder->back_forward_list_item()) {
    // Check if holder's WKBackForwardListItem still correctly represents
    // navigation item. With LegacyNavigationManager, replaceState operation
    // creates a new navigation item, leaving the old item committed. That
    // old committed item will be associated with WKBackForwardListItem whose
    // state was replaced. So old item won't have correct WKBackForwardListItem.
    if (net::GURLWithNSURL(holder->back_forward_list_item().URL) !=
        currentItem->GetURL()) {
      // The state was replaced for this item. The item should not be a part of
      // committed items, but it's too late to remove the item. Cleaup
      // WKBackForwardListItem and mark item with "state replaced" flag.
      currentItem->SetHasStateBeenReplaced(true);
      holder->set_back_forward_list_item(nil);
    }
  }

  // If the request has POST data and is not a repost form, configure the POST
  // request.
  if (POSTData.length && !repostedForm) {
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:POSTData];
    [request setAllHTTPHeaderFields:self.currentHTTPHeaders];
  }

  ProceduralBlock defaultNavigationBlock = ^{
    web::NavigationItem* item = self.currentNavItem;
    GURL navigationURL = item ? item->GetURL() : GURL::EmptyGURL();
    GURL virtualURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();

    // Do not attempt to navigate to file URLs that are typed into the
    // omnibox.
    if (navigationURL.SchemeIsFile() &&
        !web::GetWebClient()->IsAppSpecificURL(virtualURL) &&
        !IsRestoreSessionUrl(navigationURL)) {
      [_delegate webRequestControllerStopLoading:self];
      return;
    }

    // Set |item| to nullptr here to avoid any use-after-free issues, as it can
    // be cleared by the call to -registerLoadRequestForURL below.
    item = nullptr;
    GURL contextURL = IsPlaceholderUrl(navigationURL)
                          ? ExtractUrlFromPlaceholderUrl(navigationURL)
                          : navigationURL;
    std::unique_ptr<web::NavigationContextImpl> navigationContext =
        [self registerLoadRequestForURL:contextURL
                               referrer:self.currentNavItemReferrer
                             transition:self.currentTransition
                 sameDocumentNavigation:sameDocumentNavigation
                         hasUserGesture:YES
                      rendererInitiated:NO
                  placeholderNavigation:IsPlaceholderUrl(navigationURL)];

    if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
        self.navigationManagerImpl->IsRestoreSessionInProgress()) {
      if (self.navigationManagerImpl->ShouldBlockUrlDuringRestore(
              navigationURL)) {
        return;
      }
      [_delegate
          webRequestControllerDisableNavigationGesturesUntilFinishNavigation:
              self];
    }

    WKNavigation* navigation = nil;
    if (navigationURL.SchemeIsFile() &&
        web::GetWebClient()->IsAppSpecificURL(virtualURL)) {
      // file:// URL navigations are allowed for app-specific URLs, which
      // already have elevated privileges.
      NSURL* navigationNSURL = net::NSURLWithGURL(navigationURL);
      navigation = [self.webView loadFileURL:navigationNSURL
                     allowingReadAccessToURL:navigationNSURL];
    } else {
      navigation = [self.webView loadRequest:request];
    }
    [self.navigationHandler.navigationStates
             setState:web::WKNavigationState::REQUESTED
        forNavigation:navigation];
    [self.navigationHandler.navigationStates
           setContext:std::move(navigationContext)
        forNavigation:navigation];
    [self reportBackForwardNavigationTypeForFastNavigation:NO];
  };

  // When navigating via WKBackForwardListItem to pages created or updated by
  // calls to pushState() and replaceState(), sometimes web_bundle.js is not
  // injected correctly.  This means that calling window.history navigation
  // functions will invoke WKWebView's non-overridden implementations, causing a
  // mismatch between the WKBackForwardList and NavigationManager.
  // TODO(crbug.com/659816): Figure out how to prevent web_bundle.js injection
  // flake.
  if (currentItem->HasStateBeenReplaced() ||
      currentItem->IsCreatedFromPushState()) {
    defaultNavigationBlock();
    return;
  }

  // If there is no corresponding WKBackForwardListItem, or the item is not in
  // the current WKWebView's back-forward list, navigating using WKWebView API
  // is not possible. In this case, fall back to the default navigation
  // mechanism.
  if (!holder->back_forward_list_item() ||
      ![self isBackForwardListItemValid:holder->back_forward_list_item()]) {
    defaultNavigationBlock();
    return;
  }

  ProceduralBlock webViewNavigationBlock = ^{
    // If the current navigation URL is the same as the URL of the visible
    // page, that means the user requested a reload. |goToBackForwardListItem|
    // will be a no-op when it is passed the current back forward list item,
    // so |reload| must be explicitly called.
    web::NavigationItem* item = self.currentNavItem;
    GURL navigationURL = item ? item->GetURL() : GURL::EmptyGURL();
    std::unique_ptr<web::NavigationContextImpl> navigationContext =
        [self registerLoadRequestForURL:navigationURL
                               referrer:self.currentNavItemReferrer
                             transition:self.currentTransition
                 sameDocumentNavigation:sameDocumentNavigation
                         hasUserGesture:YES
                      rendererInitiated:NO
                  placeholderNavigation:NO];
    WKNavigation* navigation = nil;
    if (navigationURL == net::GURLWithNSURL(self.webView.URL)) {
      navigation = [self.webView reload];
    } else {
      // |didCommitNavigation:| may not be called for fast navigation, so update
      // the navigation type now as it is already known.
      navigationContext->SetWKNavigationType(WKNavigationTypeBackForward);
      navigationContext->SetMimeType(holder->mime_type());
      holder->set_navigation_type(WKNavigationTypeBackForward);
      navigation = [self.webView
          goToBackForwardListItem:holder->back_forward_list_item()];
      [self reportBackForwardNavigationTypeForFastNavigation:YES];
    }
    [self.navigationHandler.navigationStates
             setState:web::WKNavigationState::REQUESTED
        forNavigation:navigation];
    [self.navigationHandler.navigationStates
           setContext:std::move(navigationContext)
        forNavigation:navigation];
  };

  // If the request is not a form submission or resubmission, or the user
  // doesn't need to confirm the load, then continue right away.

  if (!repostedForm || currentItem->ShouldSkipRepostFormConfirmation()) {
    webViewNavigationBlock();
    return;
  }

  // If the request is form submission or resubmission, then prompt the
  // user before proceeding.
  DCHECK(repostedForm);
  DCHECK(!web::GetWebClient()->IsSlimNavigationManagerEnabled());
  self.webState->ShowRepostFormWarningDialog(
      base::BindOnce(^(bool shouldContinue) {
        if (self.beingDestroyed)
          return;

        if (shouldContinue)
          webViewNavigationBlock();
        else
          [_delegate webRequestControllerStopLoading:self];
      }));
}

// Returns a NSMutableURLRequest that represents the current NavigationItem.
- (NSMutableURLRequest*)requestForCurrentNavigationItem {
  web::NavigationItem* item = self.currentNavItem;
  const GURL currentNavigationURL = item ? item->GetURL() : GURL::EmptyGURL();
  NSMutableURLRequest* request = [NSMutableURLRequest
      requestWithURL:net::NSURLWithGURL(currentNavigationURL)];
  const web::Referrer referrer(self.currentNavItemReferrer);
  if (referrer.url.is_valid()) {
    std::string referrerValue =
        web::ReferrerHeaderValueForNavigation(currentNavigationURL, referrer);
    if (!referrerValue.empty()) {
      [request setValue:base::SysUTF8ToNSString(referrerValue)
          forHTTPHeaderField:kReferrerHeaderName];
    }
  }

  // If there are headers in the current session entry add them to |request|.
  // Headers that would overwrite fields already present in |request| are
  // skipped.
  NSDictionary* headers = self.currentHTTPHeaders;
  for (NSString* headerName in headers) {
    if (![request valueForHTTPHeaderField:headerName]) {
      [request setValue:[headers objectForKey:headerName]
          forHTTPHeaderField:headerName];
    }
  }

  return request;
}

// Returns YES if the given WKBackForwardListItem is valid to use for
// navigation.
- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item {
  // The current back-forward list item MUST be in the WKWebView's back-forward
  // list to be valid.
  WKBackForwardList* list = self.webView.backForwardList;
  return list.currentItem == item ||
         [list.forwardList indexOfObject:item] != NSNotFound ||
         [list.backList indexOfObject:item] != NSNotFound;
}

#pragma mark - Private properties

- (WKWebView*)webView {
  return [_delegate webRequestControllerWebView:self];
}

- (CRWWKNavigationHandler*)navigationHandler {
  return [_delegate webRequestControllerNavigationHandler:self];
}

- (CRWLegacyNativeContentController*)legacyNativeController {
  return [_delegate webRequestControllerLegacyNativeContentController:self];
}

// Whether the associated WebState has an opener.
- (BOOL)hasOpener {
  return self.webState ? self.webState->HasOpener() : NO;
}

@end
