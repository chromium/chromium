// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent.h"

BROWSER_USER_DATA_KEY_IMPL(OmniboxPositionBrowserAgent)

OmniboxPositionBrowserAgent::OmniboxPositionBrowserAgent(Browser* browser) {}

OmniboxPositionBrowserAgent::~OmniboxPositionBrowserAgent() = default;

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
