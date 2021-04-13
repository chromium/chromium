// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SECURITY_WEB_INTERSTITIAL_IMPL_H_
#define IOS_WEB_SECURITY_WEB_INTERSTITIAL_IMPL_H_

#import <UIKit/UIKit.h>

#import "ios/web/common/crw_content_view.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#include "ios/web/public/security/web_interstitial.h"
#include "ios/web/public/web_state_observer.h"
#include "url/gurl.h"

@protocol WKNavigationDelegate;
@class WKWebView;

namespace web {

class NavigationManagerImpl;
class WebInterstitialDelegate;
class WebInterstitialImpl;
class WebStateImpl;

// May be implemented in tests to run JavaScript on interstitials. This function
// has access to private ExecuteJavaScript method to be used for testing.
void ExecuteScriptForTesting(WebInterstitialImpl*,
                             NSString*,
                             void (^)(id, NSError*));

// An abstract subclass of WebInterstitial that exposes the views necessary to
// embed the interstitial into a WebState.
class WebInterstitialImpl : public WebInterstitial, public WebStateObserver {
 public:
  WebInterstitialImpl(WebStateImpl* web_state,
                      bool new_navigation,
                      const GURL& url,
                      std::unique_ptr<WebInterstitialDelegate> delegate);
  ~WebInterstitialImpl() override;

  // Returns the transient content view used to display interstitial content.
  virtual CRWContentView* GetContentView() const;

  // Returns the url corresponding to this interstitial.
  const GURL& GetUrl() const;

  // WebInterstitial implementation:
  void Show() override;
  void Hide() override;
  void DontProceed() override;
  void Proceed() override;

  // WebStateObserver implementation:
  void WebStateDestroyed(WebState* web_state) override;

  // Called by |web_view_controller_delegate_| when |web_view_controller_|
  // receives a JavaScript command. This method forwards the command to
  // WebInterstitialDelegate::CommandReceived.
  void CommandReceivedFromWebView(NSString* command);

  // Executes the given |script| on interstitial's web view if there is one.
  // Calls |completionHandler| with results of the evaluation.
  // The |completionHandler| can be nil. Must be used only for testing.
  virtual void ExecuteJavaScript(NSString* script,
                                 void (^completion_handler)(id, NSError*));

 private:
  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  WebStateImpl* web_state_ = nullptr;

  // The navigation manager corresponding to the WebState the interstiatial was
  // created for.
  NavigationManagerImpl* navigation_manager_;
  // The URL corresponding to the page that resulted in this interstitial.
  GURL url_;
  // Whether or not to create a new transient entry on display.
  bool new_navigation_;
  // Whether or not either Proceed() or DontProceed() has been called.
  bool action_taken_;

  std::unique_ptr<WebInterstitialDelegate> delegate_;
  // The |web_view_|'s delegate.  Used to forward JavaScript commands
  // resulting from user interaction with the interstitial content.
  id<WKNavigationDelegate> web_view_delegate_;
  // The web view used to show the content. View needs to be resized by the
  // caller.
  WKWebView* web_view_;
  // View that encapsulates interstitial's web view and scroll view.
  CRWContentView* content_view_;

  // Must be implemented only for testing purposes.
  friend void web::ExecuteScriptForTesting(
      WebInterstitialImpl*,
      NSString*,
      void (^completion_handler)(id, NSError*));
};

}  // namespace web

#endif  // IOS_WEB_SECURITY_WEB_INTERSTITIAL_IMPL_H_
