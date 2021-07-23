// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_consumer.h"

// Delegate of sync screen view controller.
@protocol
    SyncScreenViewControllerDelegate <FirstRunScreenViewControllerDelegate>

// Called when the user taps to see sync settings.
- (void)showSyncSettings;

- (void)addConsentStringID:(const int)stringID;

@end

// View controller of sync screen.
@interface SyncScreenViewController
    : FirstRunScreenViewController <AuthenticationFlowDelegate,
                                    SyncScreenConsumer>

@property(nonatomic, weak) id<SyncScreenViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_VIEW_CONTROLLER_H_
