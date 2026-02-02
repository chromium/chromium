// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_web_view_navigation_observer.h"

#import "base/check.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/common/features.h"
#import "ios/web/common/url_util.h"
#import "ios/web/navigation/crw_error_page_helper.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/crw_pending_navigation_info.h"
#import "ios/web/navigation/crw_web_view_navigation_observer_delegate.h"
#import "ios/web/navigation/crw_wk_navigation_handler.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/util/wk_web_view_util.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

using web::NavigationManagerImpl;

@interface CRWWebViewNavigationObserver ()

// Dictionary where keys are the names of WKWebView properties and values are
// selector names which should be called when a corresponding property has
// changed. e.g. @{ @"URL" : @"webViewURLDidChange" } means that
// -[self webViewURLDidChange] must be called every time when WKWebView.URL is
// changed.
@property(weak, nonatomic, readonly) NSDictionary* WKWebViewObservers;

@property(nonatomic, assign, readonly) web::WebStateImpl* webStateImpl;

// The WKNavigationDelegate handler class.
@property(nonatomic, readonly, strong)
    CRWWKNavigationHandler* navigationHandler;

// The actual URL of the document object (i.e., the last committed URL).
@property(nonatomic, readonly) const GURL& documentURL;

// The NavigationManagerImpl associated with the web state.
@property(nonatomic, readonly) NavigationManagerImpl* navigationManagerImpl;

@end

@implementation CRWWebViewNavigationObserver {
  // Task runner used to ensure that KVO notifications are handled on the
  // correct sequence (as WebState and CRWWebViewNavigationObserver are
  // sequence-affine, but KVO notifications are sent on the thread where
  // the property is modified).
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Used to enforce the use of the CRWWebViewNavigationObserver on the
  // correct sequence (since this object is sequence-affine).
  SEQUENCE_CHECKER(_sequenceChecker);
}

#pragma mark - NSObject

- (instancetype)init {
  if ((self = [super init])) {
    // Store a refcounted pointer to the current sequence in order to
    // ensure that the KVO notification are executed on that sequence.
    _taskRunner = base::SequencedTaskRunner::GetCurrentDefault();
  }
  return self;
}

#pragma mark - Property

- (void)setWebView:(WKWebView*)webView {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView removeObserver:self forKeyPath:keyPath];
  }

  _webView = webView;

  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView addObserver:self forKeyPath:keyPath options:0 context:nullptr];
  }
}

- (NSDictionary*)WKWebViewObservers {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return @{
    @"estimatedProgress" : @"webViewEstimatedProgressDidChange",
    @"loading" : @"webViewLoadingStateDidChange",
    @"canGoForward" : @"webViewBackForwardStateDidChange",
    @"canGoBack" : @"webViewBackForwardStateDidChange",
    @"URL" : @"webViewURLDidChange",
  };
}

- (NavigationManagerImpl*)navigationManagerImpl {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return self.webStateImpl ? &(self.webStateImpl->GetNavigationManagerImpl())
                           : nil;
}

- (web::WebStateImpl*)webStateImpl {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return [self.delegate webStateImplForWebViewHandler:self];
}

- (CRWWKNavigationHandler*)navigationHandler {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return [self.delegate navigationHandlerForNavigationObserver:self];
}

- (const GURL&)documentURL {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return [self.delegate documentURLForWebViewHandler:self];
}

#pragma mark - KVO Observation

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  // As https://crbug.com/477494757 demonstrates, KVO will invoke this method
  // on the thread where the observed property is modified, and WebKit does
  // change properties of the WKWebView on background thread.
  //
  // The WebState is sequence-affine, and by extension the current object is
  // also sequence-affine. So, if the observation happens on a background
  // thread, post a task to the correct sequence via _taskRunner. This is safe
  // as _taskRunner is unmodified after the object is initialized, and all the
  // Objective-C pointers are reference counted (thus the object cannot be
  // deallocated while KVO is sending the notification).
  //
  // Note that when going through the _taskRunner, the WebView may be destroyed
  // or the observation unregistered by the time the task is executed. Thus it
  // must correctly handle those cases.
  if (!_taskRunner->RunsTasksInCurrentSequence()) {
    __weak __typeof(self) weakSelf = self;
    _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                            [weakSelf observeValueForKeyPath:keyPath
                                                    ofObject:object
                                                      change:change
                                                     context:context];
                          }));
    return;
  }

  // Do not move this before the PostTask(...).
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.beingDestroyed) {
    return;
  }

  NSString* dispatcherSelectorName = self.WKWebViewObservers[keyPath];
  DCHECK(dispatcherSelectorName);
  if (dispatcherSelectorName) {
    // With ARC memory management, it is not known what a method called
    // via a selector will return. If a method returns a retained value
    // (e.g. NS_RETURNS_RETAINED) that returned object will leak as ARC is
    // unable to property insert the correct release calls for it.
    // All selectors used here return void and take no parameters so it's safe
    // to call a function mapping to the method implementation manually.
    SEL selector = NSSelectorFromString(dispatcherSelectorName);
    IMP methodImplementation = [self methodForSelector:selector];
    if (methodImplementation) {
      void (*methodCallFunction)(id, SEL) =
          reinterpret_cast<void (*)(id, SEL)>(methodImplementation);
      methodCallFunction(self, selector);
    }
  }
}

// Called when WKWebView estimatedProgress has been changed.
- (void)webViewEstimatedProgressDidChange {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  self.webStateImpl->SendChangeLoadProgress(self.webView.estimatedProgress);
}

// Called when WKWebView loading state has been changed.
- (void)webViewLoadingStateDidChange {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  self.webStateImpl->SetIsLoading(self.webView.loading);

  if (self.webView.loading) {
    return;
  }

  GURL webViewURL = net::GURLWithNSURL(self.webView.URL);

  if (![self.navigationHandler isCurrentNavigationBackForward]) {
    [self.delegate webViewHandlerUpdateSSLStatusForCurrentNavigationItem:self];
    return;
  }

  // For failed navigations, WKWebView will sometimes revert to the previous URL
  // before committing the current navigation or resetting the web view's
  // `isLoading` property to NO.  If this is the first navigation for the web
  // view, this will result in an empty URL.
  BOOL navigationWasCommitted = self.navigationHandler.navigationState !=
                                web::WKNavigationState::REQUESTED;
  if (!navigationWasCommitted &&
      (webViewURL.is_empty() || webViewURL == self.documentURL)) {
    return;
  }

  web::NavigationContextImpl* existingContext = [self.navigationHandler
      contextForPendingMainFrameNavigationWithURL:webViewURL];
  if (!navigationWasCommitted &&
      !self.navigationHandler.pendingNavigationInfo.cancelled) {
    // A fast back-forward navigation does not call `didCommitNavigation:`, so
    // signal page change explicitly.
    BOOL isSameDocumentNavigation =
        [self isKVOChangePotentialSameDocumentNavigationToURL:webViewURL];

    [self.delegate navigationObserver:self
                 didChangeDocumentURL:webViewURL
                           forContext:existingContext];
    if (!existingContext) {
      // This URL was not seen before, so register new load request.
      [self.delegate navigationObserver:self
                          didLoadNewURL:webViewURL
              forSameDocumentNavigation:isSameDocumentNavigation];
    } else {
      // Same document navigation does not contain response headers.
      if (isSameDocumentNavigation) {
        existingContext->SetResponseHeaders(nullptr);
      }
      existingContext->SetIsSameDocument(isSameDocumentNavigation);
      existingContext->SetHasCommitted(!isSameDocumentNavigation);
      self.webStateImpl->OnNavigationStarted(existingContext);
      [self.delegate navigationObserver:self
               didChangePageWithContext:existingContext];
      self.webStateImpl->OnNavigationFinished(existingContext);
    }
  }

  [self.delegate webViewHandlerUpdateSSLStatusForCurrentNavigationItem:self];
  [self.delegate webViewHandler:self didFinishNavigation:existingContext];
}

// Called when WKWebView canGoForward/canGoBack state has been changed.
- (void)webViewBackForwardStateDidChange {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Don't trigger for LegacyNavigationManager because its back/foward state
  // doesn't always match that of WKWebView.
  self.webStateImpl->OnBackForwardStateChanged();
}

// Called when WKWebView URL has been changed.
- (void)webViewURLDidChange {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // TODO(crbug.com/41460688): Determine if there are any cases where this still
  // happens, and if so whether anything should be done when it does.
  if (self.webView.URL.absoluteString.length == 0) {
    DVLOG(1) << "Received nil/empty URL callback";
    base::UmaHistogramBoolean("IOS.Web.URLDidChangeToEmptyURL", true);
    return;
  }
  GURL URL(net::GURLWithNSURL(self.webView.URL));
  // URL changes happen at four points:
  // 1) When a load starts; at this point, the load is provisional, and
  //    it should be ignored until it's committed, since the document/window
  //    objects haven't changed yet.
  // 2) When a non-document-changing URL change happens (hash change,
  //    history.pushState, etc.). This URL change happens instantly, so should
  //    be reported.
  // 3) When a navigation error occurs after provisional navigation starts,
  //    the URL reverts to the previous URL without triggering a new navigation.
  // 4) When the user is reloading an error page.
  //
  // If `isLoading` is NO, then it must be case 2, 3, or 4. If the last
  // committed URL (_documentURL) matches the current URL, assume that it is
  // case 3. If the URL does not match, assume it is a non-document-changing
  // URL change, and handle accordingly.
  //
  // If `isLoading` is YES, then it could either be case 1, or it could be case
  // 2 on a page that hasn't finished loading yet. If it's possible that it
  // could be a same-page navigation (in which case there may not be any other
  // callback about the URL having changed), then check the actual page URL via
  // JavaScript. If the origin of the new URL matches the last committed URL,
  // then check window.location.href, and if it matches, trust it. The origin
  // check ensures that if a site somehow corrupts window.location.href it can't
  // do a redirect to a slow-loading target page while it is still loading to
  // spoof the origin. On a document-changing URL change, the
  // window.location.href will match the previous URL at this stage, not the web
  // view's current URL.
  if (!self.webView.loading) {
    if ([CRWErrorPageHelper isErrorPageFileURL:URL] &&
        self.documentURL ==
            [CRWErrorPageHelper failedNavigationURLFromErrorPageFileURL:URL]) {
      // Case 4: reloading an error page.
      return;
    }
    if (self.documentURL == URL) {
      return;
    }

    // At this point, self.webView, self.webView.backForwardList.currentItem and
    // its associated NavigationItem should all have the same URL, except in two
    // edge cases:
    // 1. location.replace that only changes hash: WebKit updates
    // self.webView.URL
    //    and currentItem.URL, and NavigationItem URL must be synced.
    // 2. location.replace to about: URL: a WebKit bug causes only
    // self.webView.URL,
    //    but not currentItem.URL to be updated. NavigationItem URL should be
    //    synced to self.webView.URL.
    // This needs to be done before `URLDidChangeWithoutDocumentChange` so any
    // WebStateObserver callbacks will see the updated URL.
    // TODO(crbug.com/41368944) use currentItem.URL instead of self.webView.URL
    // to update NavigationItem URL.
    const GURL webViewURL = net::GURLWithNSURL(self.webView.URL);
    web::NavigationItem* currentItem = nullptr;
    if (self.webView.backForwardList.currentItem) {
      currentItem = [[CRWNavigationItemHolder
          holderForBackForwardListItem:self.webView.backForwardList.currentItem]
          navigationItem];
    } else {
      // `WKBackForwardList.currentItem` may be nil in a corner case when
      // `location.replace` is called with `about:blank#hash` in an empty window
      // open tab. See crbug.com/866142. It may also be nil when the initial
      // load is a failed navigation, such as when the user navigates to a
      // an unresolvable hostname.
      currentItem = self.navigationManagerImpl->GetLastCommittedItem();
    }

    if (currentItem && webViewURL != currentItem->GetURL()) {
      currentItem->SetURL(webViewURL);
    }

    [self.delegate navigationObserver:self
        URLDidChangeWithoutDocumentChange:URL];
  } else if ([self isKVOChangePotentialSameDocumentNavigationToURL:URL]) {
    WKNavigation* navigation =
        [self.navigationHandler.navigationStates lastAddedNavigation];
    [self.webView
        evaluateJavaScript:@"window.location.href"
         completionHandler:^(id result, NSError* error) {
           // If the web view has gone away, or the location
           // couldn't be retrieved, abort.
           DCHECK_CALLED_ON_VALID_SEQUENCE(self->_sequenceChecker);
           if (!self.webView || ![result isKindOfClass:[NSString class]]) {
             return;
           }
           GURL JSURL(base::SysNSStringToUTF8(result));
           // Check that window.location matches the new URL. If
           // it does not, this is a document-changing URL change as
           // the window location would not have changed to the new
           // URL when the script was called.
           BOOL windowLocationMatchesNewURL = JSURL == URL;
           // Re-check origin in case navigaton has occurred since
           // start of JavaScript evaluation.
           BOOL newURLOriginMatchesDocumentURLOrigin =
               self.documentURL.DeprecatedGetOriginAsURL() ==
               URL.DeprecatedGetOriginAsURL();
           // Check that the web view URL still matches the new URL.
           // TODO(crbug.com/41224497): webViewURLMatchesNewURL check
           // may drop same document URL changes if pending URL
           // change occurs immediately after. Revisit heuristics to
           // prevent this.
           BOOL webViewURLMatchesNewURL =
               net::GURLWithNSURL(self.webView.URL) == URL;
           // Check that the new URL is different from the current
           // document URL. If not, URL change should not be reported.
           BOOL URLDidChangeFromDocumentURL = URL != self.documentURL;
           // Check if a new different document navigation started before the JS
           // completion block fires. Check WKNavigationState to make sure this
           // navigation has started in WKWebView. If so, don't run the block to
           // avoid clobbering global states. See crbug.com/788452.
           // TODO(crbug.com/40551549): simplify hisgtory state handling to
           // avoid this hack.
           WKNavigation* last_added_navigation =
               [self.navigationHandler.navigationStates lastAddedNavigation];
           BOOL differentDocumentNavigationStarted =
               navigation != last_added_navigation &&
               [self.navigationHandler.navigationStates
                   stateForNavigation:last_added_navigation] >=
                   web::WKNavigationState::STARTED;
           if (windowLocationMatchesNewURL &&
               newURLOriginMatchesDocumentURLOrigin &&
               webViewURLMatchesNewURL && URLDidChangeFromDocumentURL &&
               !differentDocumentNavigationStarted) {
             [self.delegate navigationObserver:self
                 URLDidChangeWithoutDocumentChange:URL];
           }
         }];
  }
}

#pragma mark - Private

// Returns YES if a KVO change to `newURL` could be a 'navigation' within the
// document (hash change, pushState/replaceState, etc.). This should only be
// used in the context of a URL KVO callback firing, and only if `isLoading` is
// YES for the web view (since if it's not, no guesswork is needed).
- (BOOL)isKVOChangePotentialSameDocumentNavigationToURL:(const GURL&)newURL {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // If the origin changes, it can't be same-document.
  if (const GURL originAsURL = self.documentURL.DeprecatedGetOriginAsURL();
      originAsURL.is_empty() ||
      originAsURL != newURL.DeprecatedGetOriginAsURL()) {
    return NO;
  }
  if (self.navigationHandler.navigationState ==
      web::WKNavigationState::REQUESTED) {
    // Normally LOAD_REQUESTED indicates that this is a regular, pending
    // navigation, but it can also happen during a fast-back navigation across
    // a hash change, so that case is potentially a same-document navigation.
    return web::GURLByRemovingRefFromGURL(newURL) ==
           web::GURLByRemovingRefFromGURL(self.documentURL);
  }
  // If it passes all the checks above, it might be (but there's no guarantee
  // that it is).
  return YES;
}

@end
