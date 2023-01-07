// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POST_RESTORE_SIGNIN_POST_RESTORE_SIGNIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_POST_RESTORE_SIGNIN_POST_RESTORE_SIGNIN_VIEW_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@interface PostRestoreSignInViewController : PromoStyleViewController

// Initializes the Post Restore Sign-in promo and displays the passed-in user
// info.
- (instancetype)initWithAccountInfo:(AccountInfo)accountInfo;

@end

#endif  // IOS_CHROME_BROWSER_UI_POST_RESTORE_SIGNIN_POST_RESTORE_SIGNIN_VIEW_CONTROLLER_H_
