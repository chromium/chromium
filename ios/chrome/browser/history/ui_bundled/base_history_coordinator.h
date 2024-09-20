// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator_delegate.h"
#import "ios/chrome/browser/history/ui_bundled/history_table_view_controller_delegate.h"
#import "ios/chrome/browser/history/ui_bundled/public/history_presentation_delegate.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

enum class UrlLoadStrategy;

@protocol HistoryCoordinatorDelegate;
@protocol HistoryPresentationDelegate;

// Coordinator that presents History.
@interface BaseHistoryCoordinator : ChromeCoordinator

// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;
// Delegate used to make the Tab UI visible.
@property(nonatomic, weak) id<HistoryPresentationDelegate> presentationDelegate;
// The delegate handling coordinator dismissal.
@property(nonatomic, weak) id<HistoryCoordinatorDelegate> delegate;

// Dismisses this Coordinator then calls `completionHandler`.
- (void)dismissWithCompletion:(ProceduralBlock)completionHandler;
@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_BASE_HISTORY_COORDINATOR_H_
