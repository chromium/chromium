// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/crw_fake_web_state_delegate.h"

#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"

@implementation CRWFakeWebStateDelegate {
  // Backs up the property with the same name.
  std::unique_ptr<web::WebState::OpenURLParams> _openURLParams;
  // Backs up the property with the same name.
  BOOL _javaScriptDialogPresenterRequested;
}

@synthesize webState = _webState;
@synthesize webStateCreationRequested = _webStateCreationRequested;
@synthesize webStateClosingRequested = _webStateClosingRequested;
@synthesize repostFormWarningRequested = _repostFormWarningRequested;
@synthesize permissionsRequestHandled = _permissionsRequestHandled;
@synthesize authenticationRequested = _authenticationRequested;
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
    handlePermissions:(NSArray<NSNumber*>*)permissions
      decisionHandler:(web::WebStatePermissionDecisionHandler)decisionHandler {
  _webState = webState;
  _permissionsRequestHandled = YES;
  decisionHandler(web::PermissionDecisionGrant);
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

- (BOOL)javaScriptDialogPresenterRequested {
  return _javaScriptDialogPresenterRequested;
}

- (BOOL)isAppLaunchingAllowedForWebState:(web::WebState*)webState {
  _webState = webState;
  return _isAppLaunchingAllowedForWebStateReturnValue;
}

@end
