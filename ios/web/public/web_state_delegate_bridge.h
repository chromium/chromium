// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_DELEGATE_BRIDGE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/web/public/web_state_delegate.h"

@class UIViewController;

// Objective-C interface for web::WebStateDelegate.
@protocol CRWWebStateDelegate <NSObject>
@optional

// Called when `webState` wants to open a new window. `url` is the URL of
// the new window; `opener_url` is the URL of the page which requested a
// window to be open; `initiated_by_user` is true if action was caused by the
// user. `webState` will not open a window if this method returns nil. This
// method can not return `webState`.
- (web::WebState*)webState:(web::WebState*)webState
    createNewWebStateForURL:(const GURL&)URL
                  openerURL:(const GURL&)openerURL
            initiatedByUser:(BOOL)initiatedByUser;

// Called when the page calls wants to close self by calling window.close()
// JavaScript API.
- (void)closeWebState:(web::WebState*)webState;

// Returns the WebState the URL is opened in, or nullptr if the URL wasn't
// opened immediately.
- (web::WebState*)webState:(web::WebState*)webState
         openURLWithParams:(const web::WebState::OpenURLParams&)params;

// Requests the repost form confirmation dialog. Clients must call `handler`
// with YES to allow repost and with NO to cancel the repost. If this method is
// not implemented then WebState will repost the form.
- (void)webState:(web::WebState*)webState
    runRepostFormDialogWithCompletionHandler:(void (^)(BOOL))handler;

// Returns a pointer to a service to manage dialogs. May return null in which
// case dialogs aren't shown.
- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState;

// Called when the media permission is requested and to acquire the decision
// handler needed to process the user's decision to grant, deny media
// permissions or show the default prompt that asks for permissions.
- (void)webState:(web::WebState*)webState
    handlePermissions:(NSArray<NSNumber*>*)permissions
      decisionHandler:(web::WebStatePermissionDecisionHandler)decisionHandler;

// Called when a request receives an authentication challenge specified by
// `protectionSpace`, and is unable to respond using cached credentials.
// Clients must call `handler` even if they want to cancel authentication
// (in which case `username` or `password` should be nil).
- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler;

// Called to know the size of the view containing the WebView.
- (UIView*)webViewContainerForWebState:(web::WebState*)webState;

// Called when the context menu is triggered and now it is required to provide a
// UIContextMenuConfiguration to `completion_handler` to generate the context
// menu.
- (void)webState:(web::WebState*)webState
    contextMenuConfigurationForParams:(const web::ContextMenuParams&)params
                    completionHandler:(void (^)(UIContextMenuConfiguration*))
                                          completionHandler;

// Called when the context menu will commit with animator.
- (void)webState:(web::WebState*)webState
    contextMenuWillCommitWithAnimator:
        (id<UIContextMenuInteractionCommitAnimating>)animator;

// This API can be used to show custom input views in the web view.
- (id<CRWResponderInputView>)webStateInputViewProvider:(web::WebState*)webState;

// Provides an opportunity to the delegate to react to the creation of the web
// view.
- (void)webStateDidCreateWebView:(web::WebState*)webState;

@end

namespace web {

// Adapter to use an id<CRWWebStateDelegate> as a web::WebStateDelegate.
class WebStateDelegateBridge : public web::WebStateDelegate {
 public:
  explicit WebStateDelegateBridge(id<CRWWebStateDelegate> delegate);

  WebStateDelegateBridge(const WebStateDelegateBridge&) = delete;
  WebStateDelegateBridge& operator=(const WebStateDelegateBridge&) = delete;

  ~WebStateDelegateBridge() override;

  // web::WebStateDelegate methods.
  WebState* CreateNewWebState(WebState* source,
                              const GURL& url,
                              const GURL& opener_url,
                              bool initiated_by_user) override;
  void CloseWebState(WebState* source) override;
  WebState* OpenURLFromWebState(WebState*,
                                const WebState::OpenURLParams&) override;
  void ShowRepostFormWarningDialog(
      WebState* source,
      FormWarningType warning_type,
      base::OnceCallback<void(bool)> callback) override;
  JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(
      WebState* source) override;
  void HandlePermissionsDecisionRequest(
      WebState* source,
      NSArray<NSNumber*>* permissions,
      WebStatePermissionDecisionHandler handler) override;
  void OnAuthRequired(WebState* source,
                      NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      AuthCallback callback) override;
  UIView* GetWebViewContainer(WebState* source) override;
  void ContextMenuConfiguration(
      WebState* source,
      const ContextMenuParams& params,
      void (^completion_handler)(UIContextMenuConfiguration*)) override;
  void ContextMenuWillCommitWithAnimator(
      WebState* source,
      id<UIContextMenuInteractionCommitAnimating> animator) override;

  id<CRWResponderInputView> GetResponderInputView(WebState* source) override;

  void OnNewWebViewCreated(WebState* source) override;

 private:
  // CRWWebStateDelegate which receives forwarded calls.
  __weak id<CRWWebStateDelegate> delegate_ = nil;
};

}  // web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_DELEGATE_BRIDGE_H_
