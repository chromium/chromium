// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

// A browser agent that stores the position of the omnibox in the given browser
// and allows other objects to observe this.
class OmniboxPositionBrowserAgent
    : public BrowserUserData<OmniboxPositionBrowserAgent> {
 public:
  ~OmniboxPositionBrowserAgent() override;

  bool IsCurrentLayoutBottomOmnibox();

  void SetIsCurrentLayoutBottomOmnibox(bool is_current_layout_bottom_omnibox);

  void AddObserver(OmniboxPositionBrowserAgentObserver* observer);

  void RemoveObserver(OmniboxPositionBrowserAgentObserver* observer);

 private:
  friend class BrowserUserData<OmniboxPositionBrowserAgent>;
  explicit OmniboxPositionBrowserAgent(Browser* browser);

  bool is_current_layout_bottom_omnibox_ = false;

  // List of observers to be notified when the omnibox position changes.
  base::ObserverList<OmniboxPositionBrowserAgentObserver, true> observers_;

  // BrowserUserData key.
  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_H_
