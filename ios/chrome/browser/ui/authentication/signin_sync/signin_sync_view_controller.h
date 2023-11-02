// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_view_controller_delegate.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@interface SigninSyncViewController
    : PromoStyleViewController <SigninSyncConsumer>

@property(nonatomic, weak) id<SigninSyncViewControllerDelegate> delegate;

@property(nonatomic, assign)
    EnterpriseSignInRestrictions enterpriseSignInRestrictions;

// The ID of the main button activating sync.
@property(nonatomic, readonly) int activateSyncButtonID;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_VIEW_CONTROLLER_H_
