// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_NAVIGATIONS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_NAVIGATIONS_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// A helper object that tracks which NavigationItems were created because of
// voice search queries.
class VoiceSearchNavigationTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<VoiceSearchNavigationTabHelper> {
 public:
  VoiceSearchNavigationTabHelper(const VoiceSearchNavigationTabHelper&) =
      delete;
  VoiceSearchNavigationTabHelper& operator=(
      const VoiceSearchNavigationTabHelper&) = delete;

  // Notifies that the next navigation is the result of a voice
  // search.
  void WillLoadVoiceSearchResult();

  // Returns whether the next committed navigation item is the result of a voice
  // search.
  bool IsExpectingVoiceSearch() const;

 private:
  friend class web::WebStateUserData<VoiceSearchNavigationTabHelper>;

  // Private constructor.
  explicit VoiceSearchNavigationTabHelper(web::WebState* web_state);

  // WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Whether a voice search navigation is expected.
  bool will_navigate_to_voice_search_result_ = false;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_VOICE_SEARCH_NAVIGATIONS_TAB_HELPER_H_
