// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

enum class UrlLoadStrategy;

// Coordinator that presents Recent Tabs.
@interface RecentTabsCoordinator : ChromeCoordinator
// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_COORDINATOR_H_
