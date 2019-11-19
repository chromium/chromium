// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPEN_IN_OPEN_IN_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_OPEN_IN_OPEN_IN_TAB_HELPER_H_

#include "base/macros.h"
#import "ios/chrome/browser/open_in/open_in_tab_helper_delegate.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace net {
class HttpResponseHeaders;
}  //  namespace net

@class OpenInController;

// A tab helper that observes WebState and shows open in button for PDF
// documents.
class OpenInTabHelper : public web::WebStateObserver,
                        public web::WebStateUserData<OpenInTabHelper> {
 public:
  ~OpenInTabHelper() override;

  // Creates OpenInTabHelper and attaches to |web_state|. |web_state| must not
  // be null.
  static void CreateForWebState(web::WebState* web_state);

  // Sets the OpenInTabHelper delegate. |delegate| will be in charge of enabling
  // the openIn view. |delegate| is not retained by TabHelper.
  void SetDelegate(id<OpenInTabHelperDelegate> delegate);

 private:
  friend class web::WebStateUserData<OpenInTabHelper>;

  OpenInTabHelper(web::WebState* web_state);

  // Handles exportable files and shows open in button if content mime type is
  // PDF.
  void HandleExportableFile();

  // WebStateObserver implementation.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  // Headers of the last response received for the current navigation.
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  // Used to enable/disable openIn UI.
  __weak id<OpenInTabHelperDelegate> delegate_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(OpenInTabHelper);
};

#endif  // IOS_CHROME_BROWSER_OPEN_IN_OPEN_IN_TAB_HELPER_H_
