// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_PROMOS_FEED_SIGN_IN_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_PROMOS_FEED_SIGN_IN_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// The UI that informs the user that they need to sign in to get personalized
// content.
@interface FeedSignInPromoViewController : ConfirmationAlertViewController
@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_PROMOS_FEED_SIGN_IN_PROMO_VIEW_CONTROLLER_H_
