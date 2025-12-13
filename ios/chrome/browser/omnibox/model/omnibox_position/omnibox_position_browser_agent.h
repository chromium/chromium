// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_OMNIBOX_POSITION_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_OMNIBOX_POSITION_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent_observer.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_state_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

// A browser agent that stores the position of the omnibox in the given browser
// and allows other objects to observe this.
// Note that this class is referring to the browser's omnibox, omnibox is also
// used in lens overlay.
class OmniboxPositionBrowserAgent
    : public BrowserUserData<OmniboxPositionBrowserAgent> {
 public:
  ~OmniboxPositionBrowserAgent() override;

  /// Whether the omnibox is focused.
  BOOL IsOmniboxFocused() const;

  bool IsCurrentLayoutBottomOmnibox();

  void SetIsCurrentLayoutBottomOmnibox(bool is_current_layout_bottom_omnibox);

  void AddObserver(OmniboxPositionBrowserAgentObserver* observer);

  void RemoveObserver(OmniboxPositionBrowserAgentObserver* observer);

  /// Sets the omnibox state provider.
  void SetOmniboxStateProvider(id<OmniboxStateProvider> state_provider) {
    // This should only be set once.
    CHECK(!omnibox_state_provider_);
    omnibox_state_provider_ = state_provider;
  }

 private:
  friend class BrowserUserData<OmniboxPositionBrowserAgent>;
  explicit OmniboxPositionBrowserAgent(Browser* browser);

  bool is_current_layout_bottom_omnibox_ = false;
  __weak id<OmniboxStateProvider> omnibox_state_provider_;

  // List of observers to be notified when the omnibox position changes.
  base::ObserverList<OmniboxPositionBrowserAgentObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_OMNIBOX_POSITION_BROWSER_AGENT_H_
