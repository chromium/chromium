// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"

class BreadcrumbManager;
class BreadcrumbManagerKeyedService;

// Protocol mirroring BreadcrumbManagerObserver
@protocol BreadcrumbManagerObserving <NSObject>
@optional
- (void)breadcrumbManager:(BreadcrumbManager*)manager
              didAddEvent:(NSString*)string;

- (void)breadcrumbManagerDidRemoveOldEvents:(BreadcrumbManager*)manager;
@end

// A C++ bridge class to handle receiving notifications from the C++ class
// that observes the connection type.
class BreadcrumbManagerObserverBridge
    : public breadcrumbs::BreadcrumbManagerObserver {
 public:
  // Constructs a new bridge instance adding |observer| as an observer of
  // |breadcrumb_manager|.
  BreadcrumbManagerObserverBridge(BreadcrumbManager* breadcrumb_manager,
                                  id<BreadcrumbManagerObserving> observer);

  // Constructs a new bridge instance adding |observer| as an observer of
  // |breadcrumb_manager_service|.
  BreadcrumbManagerObserverBridge(
      BreadcrumbManagerKeyedService* breadcrumb_manager_service,
      id<BreadcrumbManagerObserving> observer);

  ~BreadcrumbManagerObserverBridge() override;

 private:
  BreadcrumbManagerObserverBridge(const BreadcrumbManagerObserverBridge&) =
      delete;
  BreadcrumbManagerObserverBridge& operator=(
      const BreadcrumbManagerObserverBridge&) = delete;

  // BreadcrumbManagerObserver implementation:
  void EventAdded(BreadcrumbManager* manager,
                  const std::string& event) override;
  void OldEventsRemoved(BreadcrumbManager* manager) override;

  BreadcrumbManager* breadcrumb_manager_ = nullptr;
  BreadcrumbManagerKeyedService* breadcrumb_manager_service_ = nullptr;
  __weak id<BreadcrumbManagerObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_OBSERVER_BRIDGE_H_
