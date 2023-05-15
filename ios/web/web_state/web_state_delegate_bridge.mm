// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state_delegate_bridge.h"

#import "ios/web/public/ui/context_menu_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebStateDelegateBridge::WebStateDelegateBridge(id<CRWWebStateDelegate> delegate)
    : delegate_(delegate) {}

WebStateDelegateBridge::~WebStateDelegateBridge() {}

WebState* WebStateDelegateBridge::CreateNewWebState(WebState* source,
                                                    const GURL& url,
                                                    const GURL& opener_url,
                                                    bool initiated_by_user) {
  SEL selector =
      @selector(webState:createNewWebStateForURL:openerURL:initiatedByUser:);
  if ([delegate_ respondsToSelector:selector]) {
    return [delegate_ webState:source
        createNewWebStateForURL:url
                      openerURL:opener_url
                initiatedByUser:initiated_by_user];
  }
  return nullptr;
}

void WebStateDelegateBridge::CloseWebState(WebState* source) {
  if ([delegate_ respondsToSelector:@selector(closeWebState:)]) {
    [delegate_ closeWebState:source];
  }
}

WebState* WebStateDelegateBridge::OpenURLFromWebState(
    WebState* source,
    const WebState::OpenURLParams& params) {
  if ([delegate_ respondsToSelector:@selector(webState:openURLWithParams:)])
    return [delegate_ webState:source openURLWithParams:params];
  return nullptr;
}

void WebStateDelegateBridge::ShowRepostFormWarningDialog(
    WebState* source,
    base::OnceCallback<void(bool)> callback) {
  SEL selector = @selector(webState:runRepostFormDialogWithCompletionHandler:);
  if ([delegate_ respondsToSelector:selector]) {
    __block base::OnceCallback<void(bool)> block_callback = std::move(callback);
    [delegate_ webState:source
        runRepostFormDialogWithCompletionHandler:^(BOOL should_continue) {
          std::move(block_callback).Run(should_continue);
        }];
  } else {
    std::move(callback).Run(true);
  }
}

JavaScriptDialogPresenter* WebStateDelegateBridge::GetJavaScriptDialogPresenter(
    WebState* source) {
  SEL selector = @selector(javaScriptDialogPresenterForWebState:);
  if ([delegate_ respondsToSelector:selector]) {
    return [delegate_ javaScriptDialogPresenterForWebState:source];
  }
  return nullptr;
}

void WebStateDelegateBridge::HandlePermissionsDecisionRequest(
    WebState* source,
    NSArray<NSNumber*>* permissions,
    WebStatePermissionDecisionHandler handler) API_AVAILABLE(ios(15.0)) {
  if ([delegate_ respondsToSelector:@selector(webState:
                                        handlePermissions:decisionHandler:)]) {
    [delegate_ webState:source
        handlePermissions:permissions
          decisionHandler:handler];
  } else {
    handler(PermissionDecisionShowDefaultPrompt);
  }
}

void WebStateDelegateBridge::OnAuthRequired(
    WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    AuthCallback callback) {
  if ([delegate_
          respondsToSelector:@selector(webState:
                                 didRequestHTTPAuthForProtectionSpace:
                                                   proposedCredential:
                                                    completionHandler:)]) {
    __block AuthCallback local_callback = std::move(callback);
    [delegate_ webState:source
        didRequestHTTPAuthForProtectionSpace:protection_space
                          proposedCredential:proposed_credential
                           completionHandler:^(NSString* username,
                                               NSString* password) {
                             std::move(local_callback).Run(username, password);
                           }];
  } else {
    std::move(callback).Run(nil, nil);
  }
}

UIView* WebStateDelegateBridge::GetWebViewContainer(WebState* source) {
  if ([delegate_ respondsToSelector:@selector(webViewContainerForWebState:)]) {
    return [delegate_ webViewContainerForWebState:source];
  }
  return nil;
}

void WebStateDelegateBridge::ContextMenuConfiguration(
    WebState* source,
    const ContextMenuParams& params,
    void (^completion_handler)(UIContextMenuConfiguration*)) {
  if ([delegate_ respondsToSelector:@selector
                 (webState:
                     contextMenuConfigurationForParams:completionHandler:)]) {
    [delegate_ webState:source
        contextMenuConfigurationForParams:params
                        completionHandler:completion_handler];
  } else {
    completion_handler(nil);
  }
}

void WebStateDelegateBridge::ContextMenuWillCommitWithAnimator(
    WebState* source,
    id<UIContextMenuInteractionCommitAnimating> animator) {
  if ([delegate_ respondsToSelector:@selector(webState:
                                        contextMenuWillCommitWithAnimator:)]) {
    [delegate_ webState:source contextMenuWillCommitWithAnimator:animator];
  }
}

id<CRWResponderInputView> WebStateDelegateBridge::GetResponderInputView(
    WebState* source) {
  if ([delegate_ respondsToSelector:@selector(webStateInputViewProvider:)]) {
    return [delegate_ webStateInputViewProvider:source];
  }
  return nil;
}

}  // web
