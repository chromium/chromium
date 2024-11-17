// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_DELEGATE_H_

#include <set>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "base/functional/callback.h"
#include "build/blink_buildflags.h"
#import "ios/web/public/navigation/form_warning_type.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state.h"

@protocol CRWResponderInputView;
@class UIViewController;

namespace web {

struct ContextMenuParams;
class JavaScriptDialogPresenter;

// Objects implement this interface to get notified about changes in the
// WebState and to provide necessary functionality.
class WebStateDelegate {
 public:
  WebStateDelegate();

  // Called when `source` wants to open a new window. `url` is the URL of
  // the new window; `opener_url` is the URL of the page which requested a
  // window to be open; `initiated_by_user` is true if action was caused by the
  // user. `source` will not open a window if this method returns nil. This
  // method can not return `source`.
  virtual WebState* CreateNewWebState(WebState* source,
                                      const GURL& url,
                                      const GURL& opener_url,
                                      bool initiated_by_user);

  // Called when the page calls wants to close self by calling window.close()
  // JavaScript API.
  virtual void CloseWebState(WebState* source);

  // Returns the WebState the URL is opened in, or nullptr if the URL wasn't
  // opened immediately.
  virtual WebState* OpenURLFromWebState(WebState* source,
                                        const WebState::OpenURLParams& params);

  // Requests the repost form confirmation dialog. Clients must call `callback`
  // with true to allow repost and with false to cancel the repost. If this
  // method is not implemented then WebState will repost the form.
  virtual void ShowRepostFormWarningDialog(
      WebState* source,
      FormWarningType warning_type,
      base::OnceCallback<void(bool)> callback);

  // Returns a pointer to a service to manage dialogs. May return nullptr in
  // which case dialogs aren't shown.
  // TODO(crbug.com/40473860): Find better place for this method.
  virtual JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(
      WebState* source);

  // Called when web resource requests the user's permission to access
  // `web::Permission`.
  //
  // The delegate should use the `handler` function to answer to the request to
  // grant, deny media permissions or show the default prompt that asks for
  // permissions.
  virtual void HandlePermissionsDecisionRequest(
      WebState* source,
      NSArray<NSNumber*>* permissions,
      WebStatePermissionDecisionHandler handler);

  // Called when a request receives an authentication challenge specified by
  // `protection_space`, and is unable to respond using cached credentials.
  // Clients must call `callback` even if they want to cancel authentication
  // (in which case `username` or `password` should be nil).
  typedef base::OnceCallback<void(NSString* username, NSString* password)>
      AuthCallback;
  virtual void OnAuthRequired(WebState* source,
                              NSURLProtectionSpace* protection_space,
                              NSURLCredential* proposed_credential,
                              AuthCallback callback) = 0;

  // Returns the UIView used to contain the WebView for sizing purposes. Can be
  // nil.
  virtual UIView* GetWebViewContainer(WebState* source);

  // Called when the context menu is triggered and now it is required to
  // provide a UIContextMenuConfiguration to `completion_handler` to generate
  // the context menu.
  virtual void ContextMenuConfiguration(
      WebState* source,
      const ContextMenuParams& params,
      void (^completion_handler)(UIContextMenuConfiguration*));
  // Called when the context menu will commit with animator.
  virtual void ContextMenuWillCommitWithAnimator(
      WebState* source,
      id<UIContextMenuInteractionCommitAnimating> animator);

  // UIResponder Form Input APIs, consult Apple's UIResponder documentation for
  // more info.
  virtual id<CRWResponderInputView> GetResponderInputView(WebState* source);

  // Provides an opportunity to the delegate to react to the creation of the web
  // view.
  virtual void OnNewWebViewCreated(WebState* source);

 protected:
  virtual ~WebStateDelegate();

 private:
  friend class WebStateImpl;
#if BUILDFLAG(USE_BLINK)
  friend class ContentWebState;
#endif

  // Called when `this` becomes the WebStateDelegate for `source`.
  void Attach(WebState* source);

  // Called when `this` is no longer the WebStateDelegate for `source`.
  void Detach(WebState* source);

  // The WebStates for which `this` is currently a delegate.
  std::set<WebState*> attached_states_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_DELEGATE_H_
