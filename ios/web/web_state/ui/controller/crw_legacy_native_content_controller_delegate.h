// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CONTROLLER_CRW_LEGACY_NATIVE_CONTENT_CONTROLLER_DELEGATE_H_
#define IOS_WEB_WEB_STATE_UI_CONTROLLER_CRW_LEGACY_NATIVE_CONTENT_CONTROLLER_DELEGATE_H_

#include "ios/web/public/navigation/referrer.h"
#include "ui/base/page_transition_types.h"

namespace web {
class NavigationContextImpl;
}
@protocol CRWNativeContent;
@class CRWLegacyNativeContentController;
class GURL;

// Delegate for the CRWLegacyNativeContentController.
@protocol CRWLegacyNativeContentControllerDelegate

// Whether the web usage is enabled.
- (BOOL)legacyNativeContentControllerWebUsageEnabled:
    (CRWLegacyNativeContentController*)contentController;

// Asks the delegate to remove the web view.
- (void)legacyNativeContentControllerRemoveWebView:
    (CRWLegacyNativeContentController*)contentController;

// Called when a page (native or web) has actually started loading (i.e., for
// a web page the document has actually changed), or after the load request has
// been registered for a non-document-changing URL change. Updates internal
// state not specific to web pages.
- (void)legacyNativeContentControllerDidStartLoading:
    (CRWLegacyNativeContentController*)contentController;

// Loads a blank page directly into WKWebView as a placeholder for a Native View
// or WebUI URL. This page has the URL about:blank?for=<encoded original URL>.
// If |originalContext| is provided, reuse it for the placeholder navigation
// instead of creating a new one. See "Handling App-specific URLs"
// section of go/bling-navigation-experiment for details.
- (web::NavigationContextImpl*)
     legacyNativeContentController:
         (CRWLegacyNativeContentController*)contentController
    loadPlaceholderInWebViewForURL:(const GURL&)originalURL
                 rendererInitiated:(BOOL)rendererInitiated
                        forContext:(std::unique_ptr<web::NavigationContextImpl>)
                                       originalContext;

// Prepares web controller and delegates for anticipated page change.
// Allows several methods to invoke webWill/DidAddPendingURL on anticipated page
// change, using the same cached request and calculated transition types.
// Returns navigation context for this request.
- (std::unique_ptr<web::NavigationContextImpl>)
    legacyNativeContentController:
        (CRWLegacyNativeContentController*)contentController
        registerLoadRequestForURL:(const GURL&)requestURL
                         referrer:(const web::Referrer&)referrer
                       transition:(ui::PageTransition)transition
           sameDocumentNavigation:(BOOL)sameDocumentNavigation
                   hasUserGesture:(BOOL)hasUserGesture
                rendererInitiated:(BOOL)rendererInitiated
            placeholderNavigation:(BOOL)placeholderNavigation;

// Set the title of the native content.
- (void)legacyNativeContentController:
            (CRWLegacyNativeContentController*)contentController
                setNativeContentTitle:(NSString*)title;

// Notifies the delegate that the native content did change.
- (void)legacyNativeContentController:
            (CRWLegacyNativeContentController*)contentController
               nativeContentDidChange:
                   (id<CRWNativeContent>)previousNativeController;

// Notifies the delegate that the native controller finished loading the URL.
- (void)legacyNativeContentController:
            (CRWLegacyNativeContentController*)contentController
    nativeContentLoadDidFinishWithURL:(const GURL&)targetURL
                              context:(web::NavigationContextImpl*)context;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CONTROLLER_CRW_LEGACY_NATIVE_CONTENT_CONTROLLER_DELEGATE_H_
