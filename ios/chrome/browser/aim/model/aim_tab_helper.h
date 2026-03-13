// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_AIM_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_AIM_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Observes a WebState for navigations to AIM-eligible URLs and triggers
// eligibility fetches.
class AimTabHelper : public web::WebStateObserver,
                     public web::WebStateUserData<AimTabHelper> {
 public:
  AimTabHelper(const AimTabHelper&) = delete;
  AimTabHelper& operator=(const AimTabHelper&) = delete;

  ~AimTabHelper() override;

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<AimTabHelper>;

  AimTabHelper(web::WebState* web_state);

  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_AIM_TAB_HELPER_H_
