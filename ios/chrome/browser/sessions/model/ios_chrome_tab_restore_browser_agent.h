// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_BROWSER_AGENT_H_

#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

namespace web::proto {
class WebStateStorage;
}  // namespace web::proto

// A browser agent that is responsible for observing tab closures and
// informing the IOSChromeTabRestoreService about those events.
class IOSChromeTabRestoreBrowserAgent
    : public BrowserUserData<IOSChromeTabRestoreBrowserAgent>,
      public WebStateListObserver {
 public:
  ~IOSChromeTabRestoreBrowserAgent() override;

 private:
  friend class BrowserUserData<IOSChromeTabRestoreBrowserAgent>;

  explicit IOSChromeTabRestoreBrowserAgent(Browser* browser);

  // Records history for a given non-incognito WebState and does not record
  // history if the tab has no navigation or has only presented the NTP or the
  // bookmark UI.
  void RecordHistoryForWebState(int index, web::WebState* web_state);

  // Records history for a given unrealized WebState after loading its state
  // from storage.
  void RecordHistoryFromStorage(int index, web::proto::WebStateStorage storage);

  // WebStateListObserver implementation.
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;

  // Observation of the Browser's WebStateList.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  // Weak pointer factory.
  base::WeakPtrFactory<IOSChromeTabRestoreBrowserAgent> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_BROWSER_AGENT_H_
