// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SELECTION_WEB_SELECTION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_SELECTION_WEB_SELECTION_TAB_HELPER_H_

#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class WebSelectionResponse;

// A tab helper that observes WebState and can retrieve the text selected in the
// page.
class WebSelectionTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<WebSelectionTabHelper> {
 public:
  ~WebSelectionTabHelper() override;

  // Not copyable or moveable.
  WebSelectionTabHelper(const WebSelectionTabHelper&) = delete;
  WebSelectionTabHelper& operator=(const WebSelectionTabHelper&) = delete;

  // Calls the JavaScript to generate to retrieve the selected text. If
  // successful, will invoke `callback` with the selected text (which can be
  // empty). If the selection could not be retrieved, the `response.valid` will
  // be NO.
  void GetSelectedText(
      base::OnceCallback<void(WebSelectionResponse*)> callback);

  // Return whether the JS to retrieve the selected text can be called.
  bool CanRetrieveSelectedText();

 private:
  friend class web::WebStateUserData<WebSelectionTabHelper>;

  explicit WebSelectionTabHelper(web::WebState* web_state);

  // WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_SELECTION_WEB_SELECTION_TAB_HELPER_H_
