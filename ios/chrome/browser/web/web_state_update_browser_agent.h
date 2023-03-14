// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_WEB_STATE_UPDATE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_WEB_STATE_UPDATE_BROWSER_AGENT_H_

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/main/browser_user_data.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_delegate.h"
#include "ios/web/public/web_state_observer.h"

// This browser agent monitors the browser's web states (when they become or
// stop being active) and manages web state lifecycle updates.
class WebStateUpdateBrowserAgent
    : public BrowserUserData<WebStateUpdateBrowserAgent>,
      public WebStateListObserver,
      public web::WebStateObserver {
 public:
  ~WebStateUpdateBrowserAgent() override;

  // Not copyable or assignable.
  WebStateUpdateBrowserAgent(const WebStateUpdateBrowserAgent&) = delete;
  WebStateUpdateBrowserAgent& operator=(const WebStateUpdateBrowserAgent&) =
      delete;

 private:
  friend class BrowserUserData<WebStateUpdateBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit WebStateUpdateBrowserAgent(Browser* browser);

  // WebStateListObserver.
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           ActiveWebStateChangeReason reason) override;

  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;

  void WebStateListDestroyed(WebStateList* web_state_list) override;

  WebStateList* web_state_list_ = nullptr;
  // Scoped observations of Browser, WebStateList and WebStates.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_WEB_STATE_UPDATE_BROWSER_AGENT_H_
