// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_OBSERVER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_OBSERVER_H_

#include "base/observer_list_types.h"

class OmniboxPositionBrowserAgent;

// Observation class to observe new data from the OmniboxPositionBrowserAgent.
class OmniboxPositionBrowserAgentObserver : public base::CheckedObserver {
 public:
  virtual void OmniboxPositionBrowserAgentHasNewBottomLayout(
      OmniboxPositionBrowserAgent* browser_agent,
      bool is_current_layout_bottom_omnibox) {}
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_OBSERVER_H_
