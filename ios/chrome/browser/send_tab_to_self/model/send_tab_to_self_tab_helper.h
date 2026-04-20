// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

// A tab helper that observes WebState and restores the scroll position
// using text fragments when a navigation finishes.
class SendTabToSelfTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<SendTabToSelfTabHelper> {
 public:
  ~SendTabToSelfTabHelper() override;

 private:
  friend class web::WebStateUserData<SendTabToSelfTabHelper>;

  explicit SendTabToSelfTabHelper(web::WebState* web_state);

  // Not copyable or moveable.
  SendTabToSelfTabHelper(const SendTabToSelfTabHelper&) = delete;
  SendTabToSelfTabHelper& operator=(const SendTabToSelfTabHelper&) = delete;

  // WebStateObserver:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState observation.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TAB_HELPER_H_
