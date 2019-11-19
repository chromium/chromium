// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_DELEGATE_H_
#define IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_DELEGATE_H_

@class CRWWebViewNavigationObserver;
@class CRWWKNavigationHandler;
class GURL;
namespace web {
class NavigationContextImpl;
class WebStateImpl;
}

// Delegate for the NavigationObserver.
@protocol CRWWebViewNavigationObserverDelegate

// The WebState.
- (web::WebStateImpl*)webStateImplForNavigationObserver:
    (CRWWebViewNavigationObserver*)navigationObserver;

// The navigation handler
- (CRWWKNavigationHandler*)navigationHandlerForNavigationObserver:
    (CRWWebViewNavigationObserver*)navigationObserver;

// The actual URL of the document object.
- (const GURL&)documentURLForNavigationObserver:
    (CRWWebViewNavigationObserver*)navigationObserver;

// Notifies the delegate that the SSL status of the web view changed.
- (void)navigationObserverDidChangeSSLStatus:
    (CRWWebViewNavigationObserver*)navigationObserver;

// Notifies the delegate that the navigation has finished. Navigation is
// considered complete when the document has finished loading, or when other
// page load mechanics are completed on a non-document-changing URL change.
- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
       didFinishNavigation:(web::NavigationContextImpl*)context;

// Notifies the delegate that the URL of the document object changed.
- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
      didChangeDocumentURL:(const GURL&)documentURL
                forContext:(web::NavigationContextImpl*)context;

// Called when the web page has changed document and/or URL, and so the page
// navigation should be reported. |context| contains information about the
// navigation that triggered the document/URL change.
- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
    didChangePageWithContext:(web::NavigationContextImpl*)context;

// Notifies the delegate that the webView has started a new navigation to
// |webViewURL| and whether it |isSameDocumentNavigation|.
- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
                didLoadNewURL:(const GURL&)webViewURL
    forSameDocumentNavigation:(BOOL)isSameDocumentNavigation;

// Notifies the delegate that a non-document-changing URL change occurs.
- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
    URLDidChangeWithoutDocumentChange:(const GURL&)newURL;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_DELEGATE_H_
