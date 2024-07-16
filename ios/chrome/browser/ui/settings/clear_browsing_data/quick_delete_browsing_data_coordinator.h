// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol QuickDeleteBrowsingDataDelegate;

// Coordinator for Quick Delete Browsing Data page.
@interface QuickDeleteBrowsingDataCoordinator : ChromeCoordinator

// Delegate for this coordinator.
@property(nonatomic, weak) id<QuickDeleteBrowsingDataDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_COORDINATOR_H_
