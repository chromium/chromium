// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVER_H_

#include "base/observer_list_types.h"

struct ContextualPanelItemConfiguration;
class ContextualPanelTabHelper;

class ContextualPanelTabHelperObserver : public base::CheckedObserver {
 public:
  // The given ContextualPanelTabHelper has a new set of active items. The
  // vector of item configurations will be ordered based on relevance to the
  // user, and the first one should be the item displayed in any entry point.
  virtual void ContextualPanelHasNewData(
      ContextualPanelTabHelper* tab_helper,
      std::vector<ContextualPanelItemConfiguration> item_configurations) {}
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVER_H_
