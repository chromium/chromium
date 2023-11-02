// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_DELEGATE_H_
#define IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_DELEGATE_H_

#import "ios/web/web_state/ui/crw_web_view_handler_delegate.h"

@class CRWWebViewNavigationObserver;
@class CRWWKNavigationHandler;
class GURL;
namespace web {
class NavigationContextImpl;
}

// Delegate for the NavigationObserver.
@protocol CRWWebViewNavigationObserverDelegate <CRWWebViewHandlerDelegate>

// The navigation handler
- (CRWWKNavigationHandler*)navigationHandlerForNavigationObserver:
    (CRWWebViewNavigationObserver*)navigationObserver;

// Notifies the delegate that the URL of the document object changed.
- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
      didChangeDocumentURL:(const GURL&)documentURL
                forContext:(web::NavigationContextImpl*)context;

// Called when the web page has changed document and/or URL, and so the page
// navigation should be reported. `context` contains information about the
// navigation that triggered the document/URL change.
- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
    didChangePageWithContext:(web::NavigationContextImpl*)context;

// Notifies the delegate that the webView has started a new navigation to
// `webViewURL` and whether it `isSameDocumentNavigation`.
- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
                didLoadNewURL:(const GURL&)webViewURL
    forSameDocumentNavigation:(BOOL)isSameDocumentNavigation;

// Notifies the delegate that a non-document-changing URL change occurs.
- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
    URLDidChangeWithoutDocumentChange:(const GURL&)newURL;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_DELEGATE_H_
