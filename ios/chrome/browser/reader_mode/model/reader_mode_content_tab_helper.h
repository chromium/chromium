// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_CONTENT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_CONTENT_TAB_HELPER_H_

#import <memory>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_delegate.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class ReaderModeContentDelegate;

// Tab helper for the WebState rendering Reader mode content. Blocks navigations
// and forwards them to its delegate instead.
class ReaderModeContentTabHelper
    : public web::WebStateUserData<ReaderModeContentTabHelper>,
      public web::WebStatePolicyDecider,
      public web::WebStateObserver {
 public:
  explicit ReaderModeContentTabHelper(web::WebState* web_state);
  ~ReaderModeContentTabHelper() override;

  // Sets the delegate. `delegate` can be `nullptr`.
  void SetDelegate(ReaderModeContentDelegate* delegate);
  // Loads the `content_data` in the WebState using `content_url` as URL. Does
  // nothing if the WebState was destroyed or is being destroyed.
  void LoadContent(GURL content_url, NSData* content_data);
  // Attaches tab helpers that were available in the `original_web_state` to
  // continue supporting a subset of WebState based features.
  void AttachSupportedTabHelpers(web::WebState* web_state);
  // Activates translation on the Reader Mode web state.
  void ActivateTranslateOnPage(const std::string& source_code,
                               const std::string& target_code);

  // WebStatePolicyDecider overrides:
  void ShouldAllowRequest(NSURLRequest* request,
                          RequestInfo request_info,
                          PolicyDecisionCallback callback) override;
  void WebStateDestroyed() override;

  // WebStateObserver overrides:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  // Forwarding class for WebStateDelegate.
  std::unique_ptr<ReaderModeWebStateDelegate> web_state_delegate_;
  // URL of original document.
  GURL content_url_;
  // Whether request to navigate to content URL was allowed.
  bool content_url_request_allowed_ = false;
  // Delegate.
  raw_ptr<ReaderModeContentDelegate> delegate_ = nullptr;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_CONTENT_TAB_HELPER_H_
