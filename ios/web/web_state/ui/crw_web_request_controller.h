// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_REQUEST_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_REQUEST_CONTROLLER_H_

#import <WebKit/WebKit.h>

#include <memory>

#include "ios/web/public/navigation/referrer.h"
#import "ios/web/web_state/ui/crw_web_view_handler.h"
#import "ios/web/web_state/ui/crw_web_view_handler_delegate.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class CRWWebRequestController;
@class CRWWKNavigationHandler;

namespace web {
class NavigationContextImpl;
}  // namespace web

@protocol CRWWebRequestControllerDelegate <CRWWebViewHandlerDelegate>

// The delegate should stop loading the page.
- (void)webRequestControllerStopLoading:
    (CRWWebRequestController*)requestController;

// The delegate is called when a page has actually started loading.
- (void)webRequestControllerDidStartLoading:
    (CRWWebRequestController*)requestController;

- (CRWWKNavigationHandler*)webRequestControllerNavigationHandler:
    (CRWWebRequestController*)requestController;

@end

// Controller in charge of preparing and handling web requests for the delegate,
// which should be `CRWWebController`.
@interface CRWWebRequestController : CRWWebViewHandler

@property(nonatomic, weak) id<CRWWebRequestControllerDelegate> delegate;

// Checks if a load request of the current navigation item should proceed. If
// this returns `YES`, caller should create a webView and call
// `loadRequestForCurrentNavigationItem`. Otherwise this will set the request
// state to finished and call `didFinishWithURL` with failure.
- (BOOL)maybeLoadRequestForCurrentNavigationItem;

// Sets up WebUI for URL.
- (void)createWebUIForURL:(const GURL&)URL;

// Clears WebUI, if one exists.
- (void)clearWebUI;

// Loads the URL indicated by current session state.
- (void)loadCurrentURLWithRendererInitiatedNavigation:(BOOL)rendererInitiated;

// Should be called by owner component after URL is finished loading and
// self.navigationHandler.navigationState is set to FINISHED. `context` contains
// information about the navigation associated with the URL. It is nil if
// currentURL is invalid.
- (void)didFinishWithURL:(const GURL&)currentURL
             loadSuccess:(BOOL)loadSuccess
                 context:(const web::NavigationContextImpl*)context;

// Calls `registerLoadRequestForURL` with empty referrer and link or client
// redirect transition based on user interaction state. Returns navigation
// context for this request.
- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated;

// Creates a page change request and registers it with the navigation handler.
// Returns navigation context for this request.
- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)requestURL
                     referrer:(const web::Referrer&)referrer
                   transition:(ui::PageTransition)transition
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated;

// Loads `data` of type `MIMEType` and replaces last committed URL with the
// given `URL`.
- (void)loadData:(NSData*)data
        MIMEType:(NSString*)MIMEType
          forURL:(const GURL&)URL;

// Loads `HTML` into the page and use `URL` to resolve relative URLs within the
// document.
- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL;

// Reloads web view. `rendererInitiated` is YES for renderer-initiated
// navigation. `rendererInitiated` is NO for browser-initiated navigation.
- (void)reloadWithRendererInitiatedNavigation:(BOOL)rendererInitiated;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_REQUEST_CONTROLLER_H_
