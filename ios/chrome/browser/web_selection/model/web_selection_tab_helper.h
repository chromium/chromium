// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/web_selection/model/web_selection_java_script_feature_observer.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class WebSelectionResponse;

// A tab helper that observes WebState and can retrieve the text selected in the
// page.
class WebSelectionTabHelper
    : public web::WebStateObserver,
      public WebSelectionJavaScriptFeatureObserver,
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
  // Note: If there is no selection in the page, the callback will be called
  // after a timeout (currently 1s).
  void GetSelectedText(
      base::OnceCallback<void(WebSelectionResponse*)> callback);

  // Return whether the JS to retrieve the selected text can be called.
  bool CanRetrieveSelectedText();

  // WebSelectionJavaScriptFeatureObserver methods.
  void OnSelectionRetrieved(web::WebState* web_state,
                            WebSelectionResponse* response) override;

 private:
  friend class web::WebStateUserData<WebSelectionTabHelper>;

  explicit WebSelectionTabHelper(web::WebState* web_state);

  // WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  // Called when the selection retrieval times out.
  void Timeout();

  // Call `final_callback_` with `response` asynchronously.
  // It is possible that `final_callback_` will eventually cause a new selection
  // fetch which may cause reentrancy problem. To avoid this, post the task
  // instead of calling it synchronously.
  void SendResponse(WebSelectionResponse* response);

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // The callback to call when the selection is finally retrieved.
  base::OnceCallback<void(WebSelectionResponse*)> final_callback_;

  // A timer to limit the time taken to retrieve the selection.
  base::OneShotTimer time_out_callback_;

  WEB_STATE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<WebSelectionTabHelper> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_TAB_HELPER_H_
