// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_LOAD_TIMING_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_LOAD_TIMING_TAB_HELPER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// Updates page load time metric, measured from when the user taps enter in the
// omnibar to when the page is loaded. To make sure the correct interval is
// measured, DidInitiatePageLoad() should only be called on a non-prerender
// web state, after the omnibar action.
class LoadTimingTabHelper : public web::WebStateUserData<LoadTimingTabHelper>,
                            public web::WebStateObserver {
 public:
  explicit LoadTimingTabHelper(web::WebState* web_state);
  ~LoadTimingTabHelper() override;

  // Starts timer.
  void DidInitiatePageLoad();

  // Restarts timer if the prerender is still loading. Otherwise, reports 0 as
  // page load time.
  void DidPromotePrerenderTab();

  // web::WebStateObserver overrides:
  // Reports time elapsed if timer is running. Otherwise, do nothing.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  static const char kOmnibarToPageLoadedMetric[];

 private:
  friend class web::WebStateUserData<LoadTimingTabHelper>;

  void ReportLoadTime(const base::TimeDelta& elapsed);
  void ResetTimer();

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  base::TimeTicks load_start_time_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(LoadTimingTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_LOAD_TIMING_TAB_HELPER_H_
