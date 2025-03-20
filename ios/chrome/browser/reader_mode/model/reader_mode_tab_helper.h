// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_

#import "base/memory/weak_ptr.h"
#import "base/timer/timer.h"
#import "components/prefs/pref_service.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Observes changes to the web state to perform reader mode operations.
class ReaderModeTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<ReaderModeTabHelper> {
 public:
  ReaderModeTabHelper(web::WebState* web_state);
  ReaderModeTabHelper(const ReaderModeTabHelper&) = delete;
  ReaderModeTabHelper& operator=(const ReaderModeTabHelper&) = delete;

  ~ReaderModeTabHelper() override;

  // web::WebStateObserver overrides:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<ReaderModeTabHelper>;

  // Determine if the web state is eligible for triggering the reader mode
  // heuristic.
  bool CanTriggerReaderModeHeuristic();

  // Trigger the heuristic to determine reader mode eligibility.
  void TriggerReaderModeHeuristic();

  raw_ptr<web::WebState> web_state_ = nullptr;
  base::OneShotTimer trigger_reader_mode_timer_;
  base::WeakPtrFactory<ReaderModeTabHelper> weak_ptr_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_
