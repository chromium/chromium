// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_

#include <string>
#include <vector>

#import <Foundation/Foundation.h>

#import "components/breadcrumbs/ios/breadcrumb_manager_observer_bridge.h"

namespace breadcrumbs {
class BreadcrumbManager;
class BreadcrumbManagerKeyedService;
}

// Combines breadcrumbs from multiple BreadcrumbManagers and sends the merged
// breadcrumb events to breakpad for attachment to crash reports.
@interface CrashReporterBreadcrumbObserver
    : NSObject <BreadcrumbManagerObserving> {
}

// Creates a singleton instance.
+ (CrashReporterBreadcrumbObserver*)uniqueInstance;

// Sets breadcrumb events associated with the previous application session.
- (void)setPreviousSessionEvents:(const std::vector<std::string>&)events;

// Starts collecting breadcrumb events logged to |breadcrumbManager|.
- (void)observeBreadcrumbManager:
    (breadcrumbs::BreadcrumbManager*)breadcrumbManager;

// Stops collecting breadcrumb events logged to |breadcrumbManager|.
- (void)stopObservingBreadcrumbManager:
    (breadcrumbs::BreadcrumbManager*)breadcrumbManager;

// Starts collecting breadcrumb events logged to |breadcrumbManagerService|.
- (void)observeBreadcrumbManagerService:
    (breadcrumbs::BreadcrumbManagerKeyedService*)breadcrumbManagerService;

// Stops collecting breadcrumb events logged to |breadcrumbManagerService|.
- (void)stopObservingBreadcrumbManagerService:
    (breadcrumbs::BreadcrumbManagerKeyedService*)breadcrumbManagerService;

@end

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
