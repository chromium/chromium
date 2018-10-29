// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_CLOSED_TABS_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_CLOSED_TABS_OBSERVER_BRIDGE_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "components/sessions/core/tab_restore_service_observer.h"

// Objective-C protocol equivalent of the sessions::TabRestoreServiceObserver
// C++ class. Those methods are called through the bridge. The method names are
// the same as the C++ ones.
@protocol ClosedTabsObserving<NSObject>
- (void)tabRestoreServiceChanged:(sessions::TabRestoreService*)service;
- (void)tabRestoreServiceDestroyed:(sessions::TabRestoreService*)service;
@end

namespace recent_tabs {

// Bridge class to forward events from the sessions::TabRestoreService to
// Objective-C protocol ClosedTabsObserving.
class ClosedTabsObserverBridge : public sessions::TabRestoreServiceObserver {
 public:
  explicit ClosedTabsObserverBridge(id<ClosedTabsObserving> owner);
  ~ClosedTabsObserverBridge() override;

  // sessions::TabRestoreServiceObserver implementation.
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

 private:
  __weak id<ClosedTabsObserving> owner_;

  DISALLOW_COPY_AND_ASSIGN(ClosedTabsObserverBridge);
};

}  // namespace recent_tabs

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_CLOSED_TABS_OBSERVER_BRIDGE_H_
