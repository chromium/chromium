// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_PAGELOAD_FOREGROUND_DURATION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_PAGELOAD_FOREGROUND_DURATION_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Tracks the time spent on pages visible on the screen and logs them as UKMs.
class PageloadForegroundDurationTabHelper
    : public web::WebStateUserData<PageloadForegroundDurationTabHelper>,
      public web::WebStateObserver {
 public:
  ~PageloadForegroundDurationTabHelper() override;

 private:
  WEB_STATE_USER_DATA_KEY_DECL();
  friend class web::WebStateUserData<PageloadForegroundDurationTabHelper>;
  explicit PageloadForegroundDurationTabHelper(web::WebState* web_state);

  // web::WebStateObserver override.
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void WebStateRealized(web::WebState* web_state) override;

  // Helper used to create notification observers.
  void CreateNotificationObservers();

  // Indicates to this tab helper that the app has entered a foreground state.
  void UpdateForAppWillForeground();
  // Indicates to this tab helper that the app has entered a background state.
  void UpdateForAppDidBackground();

  // End recording and log a UKM if necessary.
  void RecordUkmIfInForeground();

  // Whether recording is happening for time spent on the current page.
  bool currently_recording_ = false;
  // Last time when recording started.
  base::TimeTicks last_time_shown_;
  // WebState reference.
  raw_ptr<web::WebState> web_state_ = nullptr;
  // Scoped observer that facilitates observing the WebState.
  base::ScopedObservation<web::WebState, WebStateObserver> scoped_observation_{
      this};
  // Holds references to foreground NSNotification callback observer.
  id foreground_notification_observer_;
  // Holds references to background NSNotification callback observer.
  id background_notification_observer_;

  base::WeakPtrFactory<PageloadForegroundDurationTabHelper> weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_PAGELOAD_FOREGROUND_DURATION_TAB_HELPER_H_
