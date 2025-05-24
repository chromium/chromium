// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"

OmniboxPositionBrowserAgent::OmniboxPositionBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}

OmniboxPositionBrowserAgent::~OmniboxPositionBrowserAgent() = default;

BOOL OmniboxPositionBrowserAgent::IsOmniboxFocused() const {
  return [omnibox_state_provider_ isOmniboxFocused];
}

bool OmniboxPositionBrowserAgent::IsCurrentLayoutBottomOmnibox() {
  return is_current_layout_bottom_omnibox_;
}

void OmniboxPositionBrowserAgent::SetIsCurrentLayoutBottomOmnibox(
    bool is_current_layout_bottom_omnibox) {
  if (is_current_layout_bottom_omnibox_ == is_current_layout_bottom_omnibox) {
    return;
  }
  is_current_layout_bottom_omnibox_ = is_current_layout_bottom_omnibox;
  for (OmniboxPositionBrowserAgentObserver& observer : observers_) {
    observer.OmniboxPositionBrowserAgentHasNewBottomLayout(
        this, is_current_layout_bottom_omnibox_);
  }
}

void OmniboxPositionBrowserAgent::AddObserver(
    OmniboxPositionBrowserAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void OmniboxPositionBrowserAgent::RemoveObserver(
    OmniboxPositionBrowserAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}
