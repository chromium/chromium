// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVER_H_

#include "base/memory/weak_ptr.h"
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
      std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
          item_configurations) {}

  // The given ContextualPanelTabHelper is being destroyed, give a chance to
  // observers to disconnect.
  virtual void ContextualPanelTabHelperDestroyed(
      ContextualPanelTabHelper* tab_helper) {}

  // The given ContextualPanelTabHelper has opened its panel UI.
  virtual void ContextualPanelOpened(ContextualPanelTabHelper* tab_helper) {}

  // The given ContextualPanelTabHelper has closed its panel UI.
  virtual void ContextualPanelClosed(ContextualPanelTabHelper* tab_helper) {}
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVER_H_
