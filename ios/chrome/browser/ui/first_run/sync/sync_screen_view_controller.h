// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// Delegate of sync screen view controller.
@protocol SyncScreenViewControllerDelegate <PromoStyleViewControllerDelegate>

- (void)addConsentStringID:(const int)stringID;

@end

// View controller of sync screen.
@interface SyncScreenViewController
    : PromoStyleViewController <AuthenticationFlowDelegate, SyncScreenConsumer>

@property(nonatomic, weak) id<SyncScreenViewControllerDelegate> delegate;

// True if any data type is managed by policies.
@property(nonatomic, assign) BOOL syncTypesRestricted;

// The ID of the string used to open the settings screen.
@property(nonatomic, assign) int openSettingsStringID;

// The ID of the main button activating sync.
@property(nonatomic, assign) int activateSyncButtonID;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_VIEW_CONTROLLER_H_
