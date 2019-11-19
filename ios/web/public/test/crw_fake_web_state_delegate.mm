// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/crw_fake_web_state_delegate.h"

#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWFakeWebStateDelegate {
  // Backs up the property with the same name.
  std::unique_ptr<web::WebState::OpenURLParams> _openURLParams;
  // Backs up the property with the same name.
  std::unique_ptr<web::ContextMenuParams> _contextMenuParams;
  // Backs up the property with the same name.
  BOOL _javaScriptDialogPresenterRequested;
}

@synthesize webState = _webState;
@synthesize webStateCreationRequested = _webStateCreationRequested;
@synthesize webStateClosingRequested = _webStateClosingRequested;
@synthesize repostFormWarningRequested = _repostFormWarningRequested;
@synthesize authenticationRequested = _authenticationRequested;
@synthesize shouldPreviewLinkWithURLReturnValue =
    _shouldPreviewLinkWithURLReturnValue;
@synthesize linkURL = _linkURL;
@synthesize previewingViewControllerForLinkWithURLReturnValue =
    _previewingViewControllerForLinkWithURLReturnValue;
@synthesize previewingViewController = _previewingViewController;
@synthesize commitPreviewingViewControllerRequested =
    _commitPreviewingViewControllerRequested;
@synthesize isAppLaunchingAllowedForWebStateReturnValue =
    _isAppLaunchingAllowedForWebStateReturnValue;

- (web::WebState*)webState:(web::WebState*)webState
    createNewWebStateForURL:(const GURL&)URL
                  openerURL:(const GURL&)openerURL
            initiatedByUser:(BOOL)initiatedByUser {
  _webState = webState;
  _webStateCreationRequested = YES;
  return nil;
}

- (void)closeWebState:(web::WebState*)webState {
  _webState = webState;
  _webStateClosingRequested = YES;
}

- (web::WebState*)webState:(web::WebState*)webState
         openURLWithParams:(const web::WebState::OpenURLParams&)params {
  _webState = webState;
  _openURLParams.reset(new web::WebState::OpenURLParams(params));
  return webState;
}

- (void)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  _webState = webState;
  _contextMenuParams.reset(new web::ContextMenuParams(params));
}

- (void)webState:(web::WebState*)webState
    runRepostFormDialogWithCompletionHandler:(void (^)(BOOL))handler {
  _webState = webState;
  _repostFormWarningRequested = YES;
}

- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState {
  _webState = webState;
  _javaScriptDialogPresenterRequested = YES;
  return nil;
}

- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler {
  _webState = webState;
  _authenticationRequested = YES;
}

- (const web::WebState::OpenURLParams*)openURLParams {
  return _openURLParams.get();
}

- (web::ContextMenuParams*)contextMenuParams {
  return _contextMenuParams.get();
}

- (BOOL)javaScriptDialogPresenterRequested {
  return _javaScriptDialogPresenterRequested;
}

- (BOOL)webState:(web::WebState*)webState
    shouldPreviewLinkWithURL:(const GURL&)linkURL {
  _webState = webState;
  _linkURL = linkURL;
  return _shouldPreviewLinkWithURLReturnValue;
}

- (UIViewController*)webState:(web::WebState*)webState
    previewingViewControllerForLinkWithURL:(const GURL&)linkURL {
  _webState = webState;
  _linkURL = linkURL;
  return _previewingViewControllerForLinkWithURLReturnValue;
}

- (void)webState:(web::WebState*)webState
    commitPreviewingViewController:(UIViewController*)previewingViewController {
  _webState = webState;
  _previewingViewController = previewingViewController;
  _commitPreviewingViewControllerRequested = YES;
}

- (BOOL)isAppLaunchingAllowedForWebState:(web::WebState*)webState {
  _webState = webState;
  return _isAppLaunchingAllowedForWebStateReturnValue;
}

@end
