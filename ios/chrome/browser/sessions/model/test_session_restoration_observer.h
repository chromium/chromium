// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_TEST_SESSION_RESTORATION_OBSERVER_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_TEST_SESSION_RESTORATION_OBSERVER_H_

#import "ios/chrome/browser/sessions/model/session_restoration_observer.h"

// An implementation of SessionRestorationObserver recording the method
// calls and their parameters for use in test.
class TestSessionRestorationObserver : public SessionRestorationObserver {
 public:
  bool restore_started() const { return restore_started_; }
  int restored_web_states_count() const { return restored_web_states_count_; }
  int session_restoration_call_count() const {
    return session_restoration_call_count_;
  }

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) override;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) override;

 private:
  bool restore_started_ = false;
  int restored_web_states_count_ = -1;
  int session_restoration_call_count_ = 0;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_TEST_SESSION_RESTORATION_OBSERVER_H_
