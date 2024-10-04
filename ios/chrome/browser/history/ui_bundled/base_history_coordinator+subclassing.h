// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_COORDINATOR_SUBCLASSING_H_

#import "ios/chrome/browser/history/ui_bundled/base_history_coordinator.h"
#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller.h"
#import "ios/chrome/browser/history/ui_bundled/history_menu_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"

@interface BaseHistoryCoordinator (Subclassing) <
    BrowserObserving,
    HistoryMenuProvider,
    HistoryTableViewControllerDelegate>

@property(nonatomic, strong) BaseHistoryViewController* viewController;
@property(nonatomic, readonly) MenuScenarioHistogram scenario;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_COORDINATOR_SUBCLASSING_H_
