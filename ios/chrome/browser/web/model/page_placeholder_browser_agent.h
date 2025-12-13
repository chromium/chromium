// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_PAGE_PLACEHOLDER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_PAGE_PLACEHOLDER_BROWSER_AGENT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/shared/model/browser/browser_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

// Browser agent used to add or cancel a page placeholder for next navigation.
class PagePlaceholderBrowserAgent final
    : public BrowserUserData<PagePlaceholderBrowserAgent>,
      public web::WebStateObserver,
      public WebStateListObserver {
 public:
  ~PagePlaceholderBrowserAgent() final;

  // Returns whether a page placeholder will be displayed for WebState.
  static bool IsPagePlaceholderPlannedForWebState(web::WebState* web_state);

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // web::WebStateObserver implementation.
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class BrowserUserData<PagePlaceholderBrowserAgent>;

  explicit PagePlaceholderBrowserAgent(Browser* browser);

  // Helper called when a WebState is inserted in the WebStateList.
  void WebStateInserted(web::WebState* web_state, bool force_placeholder);

  // Helper called when a WebState is removed from the WebStateList.
  void WebStateRemoved(web::WebState* web_state);

  // Adds placeholder for next navigation to WebState.
  void AddPlaceholderToWebState(web::WebState* web_state);

  // Observation of the WebStateList.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  // Observation for unrealized WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_PAGE_PLACEHOLDER_BROWSER_AGENT_H_
