// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_CLEAR_BROWSING_DATA_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_CLEAR_BROWSING_DATA_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_delegate.h"

enum class UrlLoadStrategy;

@protocol HistoryPresentationDelegate;
@protocol HistoryClearBrowsingDataLocalCommands;
@protocol HistoryClearBrowsingDataCoordinatorDelegate;

// Coordinator that presents Clear Browsing Data Table View from History.
// Delegates are hooked up to History coordinator-specific methods.
@interface HistoryClearBrowsingDataCoordinator
    : ChromeCoordinator <ClearBrowsingDataUIDelegate>

// Delegate for this coordinator.
@property(nonatomic, weak) id<HistoryClearBrowsingDataCoordinatorDelegate>
    delegate;

// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;

// Delegate used to make the Tab UI visible.
@property(nonatomic, weak) id<HistoryPresentationDelegate> presentationDelegate;

// Stops this Coordinator then calls `completionHandler`. `completionHandler`
// always will be run.
- (void)stopWithCompletion:(ProceduralBlock)completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_CLEAR_BROWSING_DATA_COORDINATOR_H_
