// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/history/ui_bundled/base_history_coordinator.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator_delegate.h"

enum class UrlLoadStrategy;

@protocol HistoryCoordinatorDelegate;

// Coordinator that presents History.
@interface HistoryCoordinator : BaseHistoryCoordinator

// Optional: If provided, search terms to filter the displayed history items.
@property(nonatomic, copy) NSString* searchTerms;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_COORDINATOR_H_
