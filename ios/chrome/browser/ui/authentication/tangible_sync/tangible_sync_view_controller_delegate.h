// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_VIEW_CONTROLLER_DELEGATE_H_

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// View controller for tangible sync.
@protocol TangibleSyncViewControllerDelegate <PromoStyleViewControllerDelegate>

// Adds string ID in the consent string list.
- (void)addConsentStringID:(const int)stringID;

// Logs scrollability metric when the view appears.
- (void)logScrollButtonVisible:(BOOL)scrollButtonVisible;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_VIEW_CONTROLLER_DELEGATE_H_
