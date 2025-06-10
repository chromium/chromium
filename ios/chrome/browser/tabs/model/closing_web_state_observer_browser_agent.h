// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_CLOSING_WEB_STATE_OBSERVER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_CLOSING_WEB_STATE_OBSERVER_BROWSER_AGENT_H_

#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"

// A browser agent that record the IOS.ClosedTabIsAboutBlank histogram.
class ClosingWebStateObserverBrowserAgent
    : public BrowserUserData<ClosingWebStateObserverBrowserAgent>,
      public WebStateListObserver {
 public:
  ~ClosingWebStateObserverBrowserAgent() override;

 private:
  friend class BrowserUserData<ClosingWebStateObserverBrowserAgent>;

  explicit ClosingWebStateObserverBrowserAgent(Browser* browser);

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // Observation of the Browser's WebStateList.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_CLOSING_WEB_STATE_OBSERVER_BROWSER_AGENT_H_
