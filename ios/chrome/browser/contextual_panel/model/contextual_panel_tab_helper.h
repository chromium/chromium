// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_H_

#include "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Tab helper controlling the Contextual Panel feature for a given tab.
class ContextualPanelTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<ContextualPanelTabHelper> {
 public:
  ContextualPanelTabHelper(const ContextualPanelTabHelper&) = delete;
  ContextualPanelTabHelper& operator=(const ContextualPanelTabHelper&) = delete;

  ~ContextualPanelTabHelper() override;

  // WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<ContextualPanelTabHelper>;

  ContextualPanelTabHelper(web::WebState* web_state);

  WEB_STATE_USER_DATA_KEY_DECL();

  // Scoped observation for WebState.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_H_
