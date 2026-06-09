// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/observer_list_types.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

@class TabGridState;

class TabGroup;

// C++ observer interface for TabGridState.
class TabGridStateObserver : public base::CheckedObserver {
 public:
  TabGridStateObserver(const TabGridStateObserver&) = delete;
  TabGridStateObserver& operator=(const TabGridStateObserver&) = delete;
  ~TabGridStateObserver() override = default;
  TabGridStateObserver() = default;

  // Called right before entering the tab grid.
  virtual void WillEnterTabGrid() {}

  // Called right before exiting the tab grid.
  virtual void WillExitTabGrid() {}

  // Called right before the tab grid is changing its page.
  virtual void WillChangePageTo(TabGridPage page) {}

  // Called right before a `group` is shown.
  virtual void WillShowTabGroup(const TabGroup* group) {}

  // Called right before a group is hidden.
  virtual void WillHideTabGroup() {}

  // Called when the tab grid mode changes.
  virtual void TabGridStateModeDidChange(TabGridState* tab_grid_state) {}
};

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_TAB_GRID_STATE_OBSERVER_H_
