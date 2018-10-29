// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/primary_toolbar_coordinator.h"

@protocol ToolbarCoordinatorDelegate;
@protocol UrlLoader;

// Coordinator for the primary part, the one containing the omnibox, of the
// adaptive toolbar.
@interface PrimaryToolbarCoordinator
    : AdaptiveToolbarCoordinator<PrimaryToolbarCoordinator>

// Delegate for this coordinator.
// TODO(crbug.com/799446): Change this.
@property(nonatomic, weak) id<ToolbarCoordinatorDelegate> delegate;
// URL loader for the toolbar.
// TODO(crbug.com/799446): Remove this.
@property(nonatomic, weak) id<UrlLoader> URLLoader;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_COORDINATOR_H_
