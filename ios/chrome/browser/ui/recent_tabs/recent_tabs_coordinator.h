// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

enum class UrlLoadStrategy;

@protocol RecentTabsCoordinatorDelegate;
@class TabGridModeHolder;

// Coordinator that presents Recent Tabs.
@interface RecentTabsCoordinator : ChromeCoordinator
// The delegate for this coordinator.
@property(nonatomic, weak) id<RecentTabsCoordinatorDelegate> delegate;

// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;

// Holder for the current Tab Grid mode.
@property(nonatomic, strong) TabGridModeHolder* tabGridModeHolder;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_COORDINATOR_H_
