// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_
#define IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import <memory>

#import "ios/web/security/cert_verification_error.h"
#include "ui/base/page_transition_types.h"

@class CRWWKNavigationHandler;
@class CRWPendingNavigationInfo;
@class CRWWKNavigationStates;
@class CRWJSInjector;
@class CRWLegacyNativeContentController;
@class CRWCertVerificationController;
class GURL;
namespace web {
enum class WKNavigationState;
enum class ErrorRetryCommand;
struct Referrer;
class WebStateImpl;
class NavigationContextImpl;
class UserInteractionState;
class WKBackForwardListItemHolder;
}

// CRWWKNavigationHandler uses this protocol to interact with its owner.
@protocol CRWWKNavigationHandlerDelegate <NSObject>

// Returns associated WebStateImpl.
- (web::WebStateImpl*)webStateImplForNavigationHandler:
    (CRWWKNavigationHandler*)navigationHandler;

// Returns associated UserInteractionState.
- (web::UserInteractionState*)userInteractionStateForNavigationHandler:
    (CRWWKNavigationHandler*)navigationHandler;

// Returns associated certificate verificatio controller.
- (CRWCertVerificationController*)
    certVerificationControllerForNavigationHandler:
        (CRWWKNavigationHandler*)navigationHandler;

// Returns the associated js injector.
- (CRWJSInjector*)JSInjectorForNavigationHandler:
    (CRWWKNavigationHandler*)navigationHandler;

// Returns the associated legacy native content controller.
- (CRWLegacyNativeContentController*)
    legacyNativeContentControllerForNavigationHandler:
        (CRWWKNavigationHandler*)navigationHandler;

// Returns the actual URL of the document object (i.e., the last committed URL
// of the main frame).
- (GURL)navigationHandlerDocumentURL:(CRWWKNavigationHandler*)navigationHandler;

// Sets document URL to newURL, and updates any relevant state information.
- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
           setDocumentURL:(const GURL&)newURL
                  context:(web::NavigationContextImpl*)context;

// Sets up WebUI for URL.
- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
        createWebUIForURL:(const GURL&)URL;

// Returns YES if |url| should be loaded in a native view.
- (BOOL)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    shouldLoadURLInNativeView:(const GURL&)url;

// Requires that the next load rebuild the web view. This is expensive, and
// should be used only in the case where something has changed that the web view
// only checks on creation, such that the whole object needs to be rebuilt.
- (void)navigationHandlerRequirePageReconstruction:
    (CRWWKNavigationHandler*)navigationHandler;

- (std::unique_ptr<web::NavigationContextImpl>)
            navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)renderedInitiated
        placeholderNavigation:(BOOL)placeholderNavigation;

// Instructs the delegate to display the webView.
- (void)navigationHandlerDisplayWebView:
    (CRWWKNavigationHandler*)navigationHandler;

// Notifies the delegate that the page has actually started loading.
- (void)navigationHandlerDidStartLoading:
    (CRWWKNavigationHandler*)navigationHandler;

// Instructs the delegate to update the SSL status for the current navigation
// item.
- (void)navigationHandlerUpdateSSLStatusForCurrentNavigationItem:
    (CRWWKNavigationHandler*)navigationHandler;

// Instructs the delegate to update the HTML5 history state of the page using
// the current NavigationItem.
- (void)navigationHandlerUpdateHTML5HistoryState:
    (CRWWKNavigationHandler*)navigationHandler;

// Notifies the delegate that navigation has finished.
- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
      didFinishNavigation:(web::NavigationContextImpl*)context;

// Notifies the delegate that web process has crashed.
- (void)navigationHandlerWebProcessDidCrash:
    (CRWWKNavigationHandler*)navigationHandler;

// Instructs the delegate to load current URL.
- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    loadCurrentURLWithRendererInitiatedNavigation:(BOOL)rendererInitiated;

// Notifies the delegate that load has completed.
- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    didCompleteLoadWithSuccess:(BOOL)loadSuccess
                    forContext:(web::NavigationContextImpl*)context;

// Instructs the delegate to create a web view if it's not yet created.
- (WKWebView*)navigationHandlerEnsureWebViewCreated:
    (CRWWKNavigationHandler*)navigationHandler;

@end

// Handler class for WKNavigationDelegate, deals with navigation callbacks from
// WKWebView and maintains page loading state.
@interface CRWWKNavigationHandler : NSObject <WKNavigationDelegate>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDelegate:(id<CRWWKNavigationHandlerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// TODO(crbug.com/956511): Change this to readonly when
// |webViewWebProcessDidCrash| is moved to CRWWKNavigationHandler.
@property(nonatomic, assign) BOOL webProcessCrashed;

// Pending information for an in-progress page navigation. The lifetime of
// this object starts at |decidePolicyForNavigationAction| where the info is
// extracted from the request, and ends at either |didCommitNavigation| or
// |didFailProvisionalNavigation|.
@property(nonatomic, strong) CRWPendingNavigationInfo* pendingNavigationInfo;

// Holds all WKNavigation objects and their states which are currently in
// flight.
@property(nonatomic, readonly, strong) CRWWKNavigationStates* navigationStates;

// The current page loading phase.
// TODO(crbug.com/956511): Remove this once refactor is done.
@property(nonatomic, readwrite, assign) web::WKNavigationState navigationState;

// Returns the WKBackForwardlistItemHolder of current navigation item.
@property(nonatomic, readonly, assign)
    web::WKBackForwardListItemHolder* currentBackForwardListItemHolder;

// Returns the referrer for the current page.
@property(nonatomic, readonly, assign) web::Referrer currentReferrer;

// Instructs this handler to close.
- (void)close;

// Instructs this handler to stop loading.
- (void)stopLoading;

// Informs this handler that any outstanding load operations are cancelled.
- (void)loadCancelled;

// Returns context for pending navigation that has |URL|. null if there is no
// matching pending navigation.
- (web::NavigationContextImpl*)contextForPendingMainFrameNavigationWithURL:
    (const GURL&)URL;

// Returns YES if current navigation item is WKNavigationTypeBackForward.
- (BOOL)isCurrentNavigationBackForward;

// Returns YES if the current navigation item corresponds to a web page
// loaded by a POST request.
- (BOOL)isCurrentNavigationItemPOST;

// Sets last committed NavigationItem's title to the given |title|, which can
// not be nil.
- (void)setLastCommittedNavigationItemTitle:(NSString*)title;

// Maps WKNavigationType to ui::PageTransition.
- (ui::PageTransition)pageTransitionFromNavigationType:
    (WKNavigationType)navigationType;

// Loads a blank page directly into WKWebView as a placeholder to create a new
// back forward item (f.e. for error page). This page has the URL
// about:blank?for=<encoded original URL>. If |originalContext| is provided,
// reuse it for the placeholder navigation instead of creating a new one.
- (web::NavigationContextImpl*)
    loadPlaceholderInWebViewForURL:(const GURL&)originalURL
                 rendererInitiated:(BOOL)rendererInitiated
                        forContext:(std::unique_ptr<web::NavigationContextImpl>)
                                       originalContext;

// Called when the web page has changed document and/or URL, and so the page
// navigation should be reported to the delegate, and internal state updated to
// reflect the fact that the navigation has occurred. |context| contains
// information about the navigation that triggered the document/URL change.
- (void)webPageChangedWithContext:(web::NavigationContextImpl*)context
                          webView:(WKWebView*)webView;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_
