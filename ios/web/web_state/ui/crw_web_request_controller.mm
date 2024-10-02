// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_request_controller.h"

#import <WebKit/WebKit.h>

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/i18n/i18n_constants.h"
#import "base/ios/block_types.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/common/referrer_util.h"
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
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"

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

@end

@implementation CRWWebRequestController

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

  web::NavigationItem* item = self.currentNavItem;
  const GURL currentURL = item ? item->GetURL() : GURL();
  const bool isCurrentURLAppSpecific =
      web::GetWebClient()->IsAppSpecificURL(currentURL);
  // If it's a chrome URL, but not a native one, create the WebUI instance.
  if (isCurrentURLAppSpecific) {
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
  // replace the appropriate view if so.
  if ([self maybeLoadRequestForCurrentNavigationItem]) {
    [_delegate ensureWebViewCreatedForWebViewHandler:self];
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

  // The load data call will replace the current navigation and the webView URL
  // of the navigation will be replaced by `URL`. Set the URL of the
  // navigationItem to keep them synced.
  // Note: it is possible that the URL in item already match `url`. But item can
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
    [_delegate ensureWebViewCreatedForWebViewHandler:self];
    DCHECK(self.webView) << "self.webView null while trying to load HTML";
  }

  DCHECK(HTML.length);

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
    // WebUI uses `loadHTML:forURL:` to feed the content to web view. This
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
                            rendererInitiated:NO];
  }
  context->SetLoadingHtmlString(true);
  context->SetMimeType(@"text/html");
  [self.navigationHandler.navigationStates setContext:std::move(context)
                                        forNavigation:navigation];
}

- (void)reloadWithRendererInitiatedNavigation:(BOOL)rendererInitiated {
  GURL URL = self.currentNavItem->GetURL();

  self.currentNavItem->SetTransitionType(
      ui::PageTransition::PAGE_TRANSITION_RELOAD);
  if (!web::GetWebClient()->IsAppSpecificURL(
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
                rendererInitiated:rendererInitiated];
    [self.navigationHandler.navigationStates
           setContext:std::move(navigationContext)
        forNavigation:navigation];
  } else {
    [self loadCurrentURLWithRendererInitiatedNavigation:rendererInitiated];
  }
}

- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated {
  // Get the navigation type from the last main frame load request, and try to
  // map that to a PageTransition.
  WKNavigationType navigationType =
      self.navigationHandler.pendingNavigationInfo
          ? self.navigationHandler.pendingNavigationInfo.navigationType
          : WKNavigationTypeOther;
  ui::PageTransition transition =
      [self.navigationHandler pageTransitionFromNavigationType:navigationType];

  WKBackForwardListItem* currentItem = self.webView.backForwardList.currentItem;
  if (currentItem && navigationType == WKNavigationTypeBackForward) {
    web::NavigationItem* navigationItem = [[CRWNavigationItemHolder
        holderForBackForwardListItem:self.webView.backForwardList.currentItem]
        navigationItem];
    if (navigationItem) {
      transition = ui::PageTransitionFromInt(
          transition | navigationItem->GetTransitionType());
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
                    rendererInitiated:rendererInitiated];
  context->SetWKNavigationType(navigationType);
  return context;
}

- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)requestURL
                     referrer:(const web::Referrer&)referrer
                   transition:(ui::PageTransition)transition
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated {
  // Transfer time is registered so that further transitions within the time
  // envelope are not also registered as links.
  [_delegate
      userInteractionStateForWebViewHandler:self] -> ResetLastTransferTime();

  // Add or update pending item before any WebStateObserver callbacks.
  // See https://crbug.com/842151 for a scenario where this is important.
  web::NavigationItem* item = self.navigationManagerImpl->GetPendingItem();
  if (item) {
    // Update the existing pending entry.
    // Typically on PAGE_TRANSITION_CLIENT_REDIRECT.
    // Don't update if pending URL has a different origin, because client
    // redirects can not change the origin. It is possible to have more than one
    // pending navigations, so the redirect does not necesserily belong to the
    // pending navigation item.
    // Do not do it for localhost address as this is needed to have
    // pre-rendering in tests.
    if (item->GetURL().DeprecatedGetOriginAsURL() ==
            requestURL.DeprecatedGetOriginAsURL() &&
        !net::IsLocalhost(requestURL)) {
      self.navigationManagerImpl->UpdatePendingItemUrl(requestURL);
    }
  } else {
    BOOL isPostNavigation =
        [self.navigationHandler.pendingNavigationInfo.HTTPMethod
            isEqualToString:@"POST"];
    self.navigationManagerImpl->AddPendingItem(
        requestURL, referrer, transition,
        rendererInitiated ? web::NavigationInitiationType::RENDERER_INITIATED
                          : web::NavigationInitiationType::BROWSER_INITIATED,
        isPostNavigation, /*is_error_navigation=*/false,
        web::HttpsUpgradeType::kNone);
    item = self.navigationManagerImpl->GetPendingItem();
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

  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webState, requestURL, hasUserGesture, transition,
          rendererInitiated);
  context->SetNavigationItemUniqueID(item->GetUniqueID());
  context->SetIsPost([self.navigationHandler isCurrentNavigationItemPOST]);
  context->SetIsSameDocument(sameDocumentNavigation);

  // If WKWebView.loading is used for WebState::IsLoading, do not set it for
  // renderer-initated navigation otherwise WebState::IsLoading will remain true
  // after hash change in the web page.
  if (!rendererInitiated) {
    self.webState->SetIsLoading(true);
  }

  // WKWebView may have multiple pending items. Move pending item ownership from
  // NavigationManager to NavigationContext. NavigationManager owns pending item
  // after navigation was requested and until NavigationContext is created. No
  // need to transfer the ownership for WebUI navigations, because those
  // navigation do not have access to NavigationContext.
  if (self.navigationManagerImpl->GetPendingItemIndex() == -1) {
    context->SetItem(self.navigationManagerImpl->ReleasePendingItem());
  }

  return context;
}

- (void)didFinishWithURL:(const GURL&)currentURL
             loadSuccess:(BOOL)loadSuccess
                 context:(nullable const web::NavigationContextImpl*)context {
  DCHECK_EQ(web::WKNavigationState::FINISHED,
            self.navigationHandler.navigationState);
  // Placeholder and restore session URLs are implementation details so should
  // not notify WebStateObservers. If `context` is nullptr, don't skip
  // placeholder URLs because this may be the only opportunity to update
  // `isLoading` for native view reload.

  if (context && context->IsLoadingErrorPage())
    return;

  if (!loadSuccess) {
    // WebStateObserver callbacks will be called for load failure after
    // loading placeholder URL.
    return;
  }

  self.webState->OnPageLoaded(currentURL, YES);

  if (context) {
    if (context->IsRendererInitiated()) {
      UMA_HISTOGRAM_MEDIUM_TIMES("PLT.iOS.RendererInitiatedPageLoadTime2",
                                 context->GetElapsedTimeSinceCreation());
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("PLT.iOS.BrowserInitiatedPageLoadTime2",
                                 context->GetElapsedTimeSinceCreation());
    }

    // Use the Session Restoration User Agent as it is the only way to know if
    // it is automatic or not.
    if (self.webState->GetUserAgentForSessionRestoration() ==
        web::UserAgentType::AUTOMATIC) {
      web::GetWebClient()->LogDefaultUserAgent(self.webState,
                                               context->GetUrl());
    }
  }
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
- (NSDictionary<NSString*, NSString*>*)currentHTTPHeaders {
  web::NavigationItem* currentItem = self.currentNavItem;
  return currentItem ? currentItem->GetHttpRequestHeaders() : nil;
}

#pragma mark - WebUI

- (void)createWebUIForURL:(const GURL&)URL {
  // `CreateWebUI` will do nothing if `URL` is not a WebUI URL and then
  // `HasWebUI` will return false.
  self.webState->CreateWebUI(URL);
}

- (void)clearWebUI {
  self.webState->ClearWebUI();
}

#pragma mark - Private methods

// Checks if a load request of the current navigation item should proceed. If
// this returns `YES`, caller should create a webView and call
// `loadRequestForCurrentNavigationItem`. Otherwise this will set the request
// state to finished and call `didFinishWithURL` with failure.
- (BOOL)maybeLoadRequestForCurrentNavigationItem {
  web::NavigationItem* item = self.currentNavItem;
  GURL targetURL = item ? item->GetVirtualURL() : GURL();
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

// Internal helper method for loadRequestForCurrentNavigationItem.
- (void)defaultNavigationInternal:(NSMutableURLRequest*)request
           sameDocumentNavigation:(BOOL)sameDocumentNavigation {
  web::NavigationItem* item = self.currentNavItem;
  GURL navigationURL = item ? item->GetURL() : GURL();
  GURL virtualURL = item ? item->GetVirtualURL() : GURL();

  // Do not attempt to navigate to file URLs that are typed into the
  // omnibox.
  if (navigationURL.SchemeIsFile() &&
      !web::GetWebClient()->IsAppSpecificURL(virtualURL)) {
    [self.delegate webRequestControllerStopLoading:self];
    return;
  }

  // Set `item` to nullptr here to avoid any use-after-free issues, as it can
  // be cleared by the call to -registerLoadRequestForURL below.
  item = nullptr;
  std::unique_ptr<web::NavigationContextImpl> navigationContext =
      [self registerLoadRequestForURL:navigationURL
                             referrer:self.currentNavItemReferrer
                           transition:self.currentTransition
               sameDocumentNavigation:sameDocumentNavigation
                       hasUserGesture:YES
                    rendererInitiated:NO];

  WKNavigation* navigation = nil;
  if (base::FeatureList::IsEnabled(web::features::kSetRequestAttribution)) {
    request.attribution = NSURLRequestAttributionUser;
  }

  if (navigationURL.SchemeIsFile() &&
      web::GetWebClient()->IsAppSpecificURL(virtualURL)) {
    // file:// URL navigations are allowed for app-specific URLs, which
    // already have elevated privileges.
    [request.URL startAccessingSecurityScopedResource];
    navigation = [self.webView loadFileRequest:request
                       allowingReadAccessToURL:request.URL];
    [request.URL stopAccessingSecurityScopedResource];
  } else {
    navigation = [self.webView loadRequest:request];
  }
  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
      forNavigation:navigation];
  [self.navigationHandler.navigationStates
         setContext:std::move(navigationContext)
      forNavigation:navigation];
}

// Internal helper function for loadRequestForCurrentNavigationItem
- (void)webViewNavigationInternal:(web::WKBackForwardListItemHolder*)holder
           sameDocumentNavigation:(BOOL)sameDocumentNavigation {
  // If the current navigation URL is the same as the URL of the visible
  // page, that means the user requested a reload. `goToBackForwardListItem`
  // will be a no-op when it is passed the current back forward list item,
  // so `reload` must be explicitly called.
  web::NavigationItem* item = self.currentNavItem;
  GURL navigationURL = item ? item->GetURL() : GURL();
  std::unique_ptr<web::NavigationContextImpl> navigationContext =
      [self registerLoadRequestForURL:navigationURL
                             referrer:self.currentNavItemReferrer
                           transition:self.currentTransition
               sameDocumentNavigation:sameDocumentNavigation
                       hasUserGesture:YES
                    rendererInitiated:NO];
  WKNavigation* navigation = nil;
  if (navigationURL == net::GURLWithNSURL(self.webView.URL)) {
    navigation = [self.webView reload];
  } else {
    // `didCommitNavigation:` may not be called for fast navigation, so update
    // the navigation type now as it is already known.
    navigationContext->SetWKNavigationType(WKNavigationTypeBackForward);
    navigationContext->SetMimeType(holder->mime_type());
    holder->set_navigation_type(WKNavigationTypeBackForward);
    navigation =
        [self.webView goToBackForwardListItem:holder->back_forward_list_item()];
  }
  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
      forNavigation:navigation];
  [self.navigationHandler.navigationStates
         setContext:std::move(navigationContext)
      forNavigation:navigation];
}

// Loads request for the URL of the current navigation item. Subclasses may
// choose to build a new NSURLRequest and call
// `loadRequestForCurrentNavigationItem` on the underlying web view, or use
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

  web::WKBackForwardListItemHolder* holder =
      self.navigationHandler.currentBackForwardListItemHolder;

  BOOL repostedForm =
      [holder->http_method() isEqualToString:@"POST"] &&
      (holder->navigation_type() == WKNavigationTypeFormResubmitted ||
       holder->navigation_type() == WKNavigationTypeFormSubmitted);
  web::NavigationItemImpl* currentItem = self.currentNavItem;
  NSData* POSTData = currentItem->GetPostData();
  NSMutableURLRequest* request = [self requestForCurrentNavigationItem];

  // If the request has POST data and is not a repost form, configure the POST
  // request.
  if (POSTData.length && !repostedForm) {
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:POSTData];
    [request setAllHTTPHeaderFields:self.currentHTTPHeaders];
  }

  BOOL sameDocumentNavigation = currentItem->IsCreatedFromHashChange();

  // If there is no corresponding WKBackForwardListItem, or the item is not
  // in the current WKWebView's back-forward list, navigating using WKWebView
  // API is not possible. In this case, fall back to the default navigation
  // mechanism.
  if (!holder->back_forward_list_item() ||
      ![self isBackForwardListItemValid:holder->back_forward_list_item()]) {
    [self defaultNavigationInternal:request
             sameDocumentNavigation:sameDocumentNavigation];
    return;
  }

  [self webViewNavigationInternal:holder
           sameDocumentNavigation:sameDocumentNavigation];
}

// Returns a NSMutableURLRequest that represents the current NavigationItem.
- (NSMutableURLRequest*)requestForCurrentNavigationItem {
  web::NavigationItem* item = self.currentNavItem;
  const GURL currentNavigationURL = item ? item->GetURL() : GURL();
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

  // If there are headers in the current session entry add them to `request`.
  // Headers that would overwrite fields already present in `request` are
  // skipped.
  NSDictionary<NSString*, NSString*>* headers = self.currentHTTPHeaders;
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

- (web::WebStateImpl*)webState {
  return [_delegate webStateImplForWebViewHandler:self];
}

- (WKWebView*)webView {
  return [_delegate webViewForWebViewHandler:self];
}

- (CRWWKNavigationHandler*)navigationHandler {
  return [_delegate webRequestControllerNavigationHandler:self];
}

// Whether the associated WebState has an opener.
- (BOOL)hasOpener {
  return self.webState ? self.webState->HasOpener() : NO;
}

@end
