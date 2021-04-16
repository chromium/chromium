// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller.h"

// Delegate of sync screen view controller.
@protocol SyncScreenViewControllerDelegate <NSObject>

// Called when the user wants to continue with sync.
- (void)continueWithSync;

// Called when the user wants to continue without sync.
- (void)continueWithoutSync;

// Called when the user taps "Customize Sync Settings".
- (void)showSyncSettings;

@end

// View controller of sync screen.
@interface SyncScreenViewController : FirstRunScreenViewController

@property(nonatomic, weak) id<SyncScreenViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SCREEN_VIEW_CONTROLLER_H_
