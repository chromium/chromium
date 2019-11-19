// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_DELEGATE_H_

#include <set>

#import <Foundation/Foundation.h>

#include "base/callback.h"
#import "ios/web/public/web_state.h"

@class UIViewController;

namespace web {

struct ContextMenuParams;
class JavaScriptDialogPresenter;

// Objects implement this interface to get notified about changes in the
// WebState and to provide necessary functionality.
class WebStateDelegate {
 public:
  WebStateDelegate();

  // Called when |source| wants to open a new window. |url| is the URL of
  // the new window; |opener_url| is the URL of the page which requested a
  // window to be open; |initiated_by_user| is true if action was caused by the
  // user. |source| will not open a window if this method returns nil. This
  // method can not return |source|.
  virtual WebState* CreateNewWebState(WebState* source,
                                      const GURL& url,
                                      const GURL& opener_url,
                                      bool initiated_by_user);

  // Called when the page calls wants to close self by calling window.close()
  // JavaScript API.
  virtual void CloseWebState(WebState* source);

  // Returns the WebState the URL is opened in, or nullptr if the URL wasn't
  // opened immediately.
  virtual WebState* OpenURLFromWebState(WebState*,
                                        const WebState::OpenURLParams&);

  // Notifies the delegate that the user triggered the context menu with the
  // given |ContextMenuParams|. If the delegate does not implement this method,
  // no context menu will be displayed.
  virtual void HandleContextMenu(WebState* source,
                                 const ContextMenuParams& params);

  // Requests the repost form confirmation dialog. Clients must call |callback|
  // with true to allow repost and with false to cancel the repost. If this
  // method is not implemented then WebState will repost the form.
  virtual void ShowRepostFormWarningDialog(
      WebState* source,
      base::OnceCallback<void(bool)> callback);

  // Returns a pointer to a service to manage dialogs. May return nullptr in
  // which case dialogs aren't shown.
  // TODO(crbug.com/622084): Find better place for this method.
  virtual JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(
      WebState* source);

  // Called when a request receives an authentication challenge specified by
  // |protection_space|, and is unable to respond using cached credentials.
  // Clients must call |callback| even if they want to cancel authentication
  // (in which case |username| or |password| should be nil).
  typedef base::Callback<void(NSString* username, NSString* password)>
      AuthCallback;
  virtual void OnAuthRequired(WebState* source,
                              NSURLProtectionSpace* protection_space,
                              NSURLCredential* proposed_credential,
                              const AuthCallback& callback) = 0;

  // Determines whether the given link with |link_url| should show a preview on
  // force touch.
  virtual bool ShouldPreviewLink(WebState* source, const GURL& link_url);
  // Called when the user performs a peek action on a link with |link_url| with
  // force touch. Returns a view controller shown as a pop-up. Uses Webkit's
  // default preview behavior when it returns nil.
  virtual UIViewController* GetPreviewingViewController(WebState* source,
                                                        const GURL& link_url);
  // Called when the user performs a pop action on the preview on force touch.
  // |previewing_view_controller| is the view controller that is popped.
  // It should display |previewing_view_controller| inside the app.
  virtual void CommitPreviewingViewController(
      WebState* source,
      UIViewController* previewing_view_controller);

 protected:
  virtual ~WebStateDelegate();

 private:
  friend class WebStateImpl;

  // Called when |this| becomes the WebStateDelegate for |source|.
  void Attach(WebState* source);

  // Called when |this| is no longer the WebStateDelegate for |source|.
  void Detach(WebState* source);

  // The WebStates for which |this| is currently a delegate.
  std::set<WebState*> attached_states_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_DELEGATE_H_
