// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_UI_CONTEXTUAL_DEFAULT_BROWSER_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_UI_CONTEXTUAL_DEFAULT_BROWSER_PROMO_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/default_browser/promo/contextual/ui/contextual_default_browser_promo_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// View controller for the contextual default browser promo half sheet.
@interface ContextualDefaultBrowserPromoViewController
    : ConfirmationAlertViewController <ContextualDefaultBrowserPromoConsumer>

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_UI_CONTEXTUAL_DEFAULT_BROWSER_PROMO_VIEW_CONTROLLER_H_
