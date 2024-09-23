// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/test_session_restoration_observer.h"

void TestSessionRestorationObserver::WillStartSessionRestoration(
    Browser* browser) {
  restore_started_ = true;
}

void TestSessionRestorationObserver::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  restored_web_states_count_ = static_cast<int>(restored_web_states.size());
  session_restoration_call_count_++;
}
