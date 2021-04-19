// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer_bridge.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BreadcrumbManagerObserverBridge::BreadcrumbManagerObserverBridge(
    breadcrumbs::BreadcrumbManager* breadcrumb_manager,
    id<BreadcrumbManagerObserving> observer)
    : breadcrumb_manager_(breadcrumb_manager), observer_(observer) {
  DCHECK(observer_);
  breadcrumb_manager_->AddObserver(this);
}

BreadcrumbManagerObserverBridge::BreadcrumbManagerObserverBridge(
    breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_manager_service,
    id<BreadcrumbManagerObserving> observer)
    : breadcrumb_manager_service_(breadcrumb_manager_service),
      observer_(observer) {
  DCHECK(observer_);
  breadcrumb_manager_service_->AddObserver(this);
}

BreadcrumbManagerObserverBridge::~BreadcrumbManagerObserverBridge() {
  if (breadcrumb_manager_) {
    breadcrumb_manager_->RemoveObserver(this);
  }
  if (breadcrumb_manager_service_) {
    breadcrumb_manager_service_->RemoveObserver(this);
  }
}

void BreadcrumbManagerObserverBridge::EventAdded(
    breadcrumbs::BreadcrumbManager* manager,
    const std::string& event) {
  if ([observer_ respondsToSelector:@selector(breadcrumbManager:
                                                    didAddEvent:)]) {
    [observer_ breadcrumbManager:manager
                     didAddEvent:base::SysUTF8ToNSString(event)];
  }
}

void BreadcrumbManagerObserverBridge::OldEventsRemoved(
    breadcrumbs::BreadcrumbManager* manager) {
  if ([observer_
          respondsToSelector:@selector(breadcrumbManagerDidRemoveOldEvents:)]) {
    [observer_ breadcrumbManagerDidRemoveOldEvents:manager];
  }
}
