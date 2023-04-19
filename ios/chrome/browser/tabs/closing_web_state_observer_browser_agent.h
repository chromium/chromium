// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_CLOSING_WEB_STATE_OBSERVER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_CLOSING_WEB_STATE_OBSERVER_BROWSER_AGENT_H_

#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

namespace sessions {
class TabRestoreService;
}

// TODO(crbug.com/1121120): more cleanly separate the responsibilities of this
// class: There should be a different object to be responsible for cleaning up
// snapshots.
// A browser agent that is responsible for handling WebStateList
// events about closing WebState, like requesting deletion of the current page
// snapshot from disk and memory. This class also records of history for
// non-incognito Browser's WebStates.
class ClosingWebStateObserverBrowserAgent
    : public BrowserObserver,
      public BrowserUserData<ClosingWebStateObserverBrowserAgent>,
      public WebStateListObserver {
 public:
  ClosingWebStateObserverBrowserAgent();
  ~ClosingWebStateObserverBrowserAgent() override;

 private:
  friend class BrowserUserData<ClosingWebStateObserverBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit ClosingWebStateObserverBrowserAgent(Browser* browser);

  // Records history for a given non-incognito WebState and does not record
  // history if the tab has no navigation or has only presented the NTP or the
  // bookmark UI.
  void RecordHistoryForWebStateAtIndex(web::WebState* web_state, int index);

  // BrowserObserver methods.
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver implementation.
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;

  void WillCloseWebStateAt(WebStateList* web_state_list,
                           web::WebState* web_state,
                           int index,
                           bool user_action) override;

  sessions::TabRestoreService* restore_service_ = nullptr;
};
#endif  // IOS_CHROME_BROWSER_TABS_CLOSING_WEB_STATE_OBSERVER_BROWSER_AGENT_H_
