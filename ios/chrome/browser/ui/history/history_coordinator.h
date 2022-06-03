// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

enum class UrlLoadStrategy;

@protocol HistoryPresentationDelegate;

// Coordinator that presents History.
@interface HistoryCoordinator : ChromeCoordinator

// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;
// Delegate used to make the Tab UI visible.
@property(nonatomic, weak) id<HistoryPresentationDelegate> presentationDelegate;

// Stops this Coordinator then calls |completionHandler|.
- (void)stopWithCompletion:(ProceduralBlock)completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_COORDINATOR_H_
