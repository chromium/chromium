// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_
#define IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import <memory>

#import "ios/web/security/cert_verification_error.h"
#import "ios/web/web_state/ui/crw_web_view_handler.h"
#import "ios/web/web_state/ui/crw_web_view_handler_delegate.h"
#include "ui/base/page_transition_types.h"

@class CRWWKNavigationHandler;
@class CRWPendingNavigationInfo;
@class CRWWKNavigationStates;
@class CRWCertVerificationController;
class GURL;
namespace web {
enum class WKNavigationState;
enum class ErrorRetryCommand;
struct Referrer;
class NavigationContextImpl;
class WKBackForwardListItemHolder;
}

// CRWWKNavigationHandler uses this protocol to interact with its owner.
@protocol CRWWKNavigationHandlerDelegate <CRWWebViewHandlerDelegate>

// Returns whether `action` was user initiated.
- (BOOL)isUserInitiatedAction:(WKNavigationAction*)action;

// Returns associated certificate verificatio controller.
- (CRWCertVerificationController*)
    certVerificationControllerForNavigationHandler:
        (CRWWKNavigationHandler*)navigationHandler;

// Sets document URL to newURL, and updates any relevant state information.
- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
           setDocumentURL:(const GURL&)newURL
                  context:(web::NavigationContextImpl*)context;

// Sets up WebUI for URL.
- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
        createWebUIForURL:(const GURL&)URL;

- (std::unique_ptr<web::NavigationContextImpl>)
            navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)renderedInitiated;

// Instructs the delegate to display the webView.
- (void)navigationHandlerDisplayWebView:
    (CRWWKNavigationHandler*)navigationHandler;

// Notifies the delegate that the page has actually started loading.
- (void)navigationHandlerDidStartLoading:
    (CRWWKNavigationHandler*)navigationHandler;

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

// Resumes download using `webView`
- (void)resumeDownloadWithData:(NSData*)data
             completionHandler:(void (^)(WKDownload*))completionHandler;

@end

// Handler class for WKNavigationDelegate, deals with navigation callbacks from
// WKWebView and maintains page loading state.
@interface CRWWKNavigationHandler : CRWWebViewHandler <WKNavigationDelegate>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDelegate:(id<CRWWKNavigationHandlerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Indicates if the webview reported a crash.
@property(nonatomic, assign, readonly) BOOL webProcessCrashed;

// Indicates if the next call to decidePolicyForNavigationAction will block
// universal links.  This is useful for native session restore's navigation.
@property(nonatomic, assign, readwrite)
    BOOL blockUniversalLinksOnNextDecidePolicy;

// Pending information for an in-progress page navigation. The lifetime of
// this object starts at `decidePolicyForNavigationAction` where the info is
// extracted from the request, and ends at either `didCommitNavigation` or
// `didFailProvisionalNavigation`.
@property(nonatomic, strong) CRWPendingNavigationInfo* pendingNavigationInfo;

// Holds all WKNavigation objects and their states which are currently in
// flight.
@property(nonatomic, readonly, strong) CRWWKNavigationStates* navigationStates;

// The current page loading phase.
// TODO(crbug.com/40624624): Remove this once refactor is done.
@property(nonatomic, readwrite, assign) web::WKNavigationState navigationState;

// Returns the WKBackForwardlistItemHolder of current navigation item.
@property(nonatomic, readonly, assign)
    web::WKBackForwardListItemHolder* currentBackForwardListItemHolder;

// Returns the referrer for the current page.
@property(nonatomic, readonly, assign) web::Referrer currentReferrer;

// Instructs this handler to stop loading.
- (void)stopLoading;

// Informs this handler that any outstanding load operations are cancelled.
- (void)loadCancelled;

// Returns context for pending navigation that has `URL`. null if there is no
// matching pending navigation.
- (web::NavigationContextImpl*)contextForPendingMainFrameNavigationWithURL:
    (const GURL&)URL;

// Returns YES if current navigation item is WKNavigationTypeBackForward.
- (BOOL)isCurrentNavigationBackForward;

// Returns YES if the current navigation item corresponds to a web page
// loaded by a POST request.
- (BOOL)isCurrentNavigationItemPOST;

// Sets last committed NavigationItem's title to the given `title`, which can
// not be nil.
- (void)setLastCommittedNavigationItemTitle:(NSString*)title;

// Maps WKNavigationType to ui::PageTransition.
- (ui::PageTransition)pageTransitionFromNavigationType:
    (WKNavigationType)navigationType;

// Called when the web page has changed document and/or URL, and so the page
// navigation should be reported to the delegate, and internal state updated to
// reflect the fact that the navigation has occurred. `context` contains
// information about the navigation that triggered the document/URL change.
- (void)webPageChangedWithContext:(web::NavigationContextImpl*)context
                          webView:(WKWebView*)webView;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_HANDLER_H_
