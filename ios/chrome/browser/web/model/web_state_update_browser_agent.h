// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_UPDATE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_UPDATE_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
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

  // Translates all web states' offset so web states from other tabs are also
  // updated.
  void UpdateWebStateScrollViewOffset(CGFloat toolbar_height);

 private:
  friend class BrowserUserData<WebStateUpdateBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit WebStateUpdateBrowserAgent(Browser* browser);

  // WebStateListObserver.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  void WebStateListDestroyed(WebStateList* web_state_list) override;

  raw_ptr<WebStateList> web_state_list_ = nullptr;
  // Scoped observations of Browser, WebStateList and WebStates.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_UPDATE_BROWSER_AGENT_H_
