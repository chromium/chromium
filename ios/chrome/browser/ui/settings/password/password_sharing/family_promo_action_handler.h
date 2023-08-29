// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PROMO_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PROMO_ACTION_HANDLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// Handles user interactions in the family promo view.
@protocol FamilyPromoActionHandler <ConfirmationAlertActionHandler>

// Handles taps on the link to create a family group.
- (void)createFamilyGroupLinkWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PROMO_ACTION_HANDLER_H_
