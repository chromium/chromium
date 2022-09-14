// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_TAILORED_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_TAILORED_PROMO_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

#import "ios/chrome/browser/ui/default_promo/tailored_promo_util.h"

@interface TailoredPromoViewController
    : ConfirmationAlertViewController <TailoredPromoConsumer>
@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_TAILORED_PROMO_VIEW_CONTROLLER_H_
