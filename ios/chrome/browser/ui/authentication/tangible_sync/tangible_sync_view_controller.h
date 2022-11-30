// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_consumer.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_view_controller_delegate.h"
#import "ios/chrome/browser/ui/elements/instruction_view.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol TangibleSyncViewControllerDelegate;

// View controller for tangible sync.
@interface TangibleSyncViewController
    : PromoStyleViewController <InstructionLineTappedListener,
                                TangibleSyncConsumer>

@property(nonatomic, weak) id<TangibleSyncViewControllerDelegate> delegate;

// The ID of the main button activating sync.
@property(nonatomic, readonly, assign) int activateSyncButtonID;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_VIEW_CONTROLLER_H_
