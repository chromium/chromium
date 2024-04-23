// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_CLOSING_WEB_STATE_OBSERVER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_CLOSING_WEB_STATE_OBSERVER_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"

namespace web::proto {
class WebStateStorage;
}

// TODO(crbug.com/40715295): more cleanly separate the responsibilities of this
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

  // Records history for a given unrealized WebState after loading its state
  // from storage.
  void RecordHistoryFromStorage(int index, web::proto::WebStateStorage storage);

  // BrowserObserver methods.
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver implementation.
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  raw_ptr<Browser> browser_;

  base::WeakPtrFactory<ClosingWebStateObserverBrowserAgent> weak_ptr_factory_{
      this};
};
#endif  // IOS_CHROME_BROWSER_TABS_MODEL_CLOSING_WEB_STATE_OBSERVER_BROWSER_AGENT_H_
