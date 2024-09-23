// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// Container view controller for the Credential Provider Extension promo. Can be
// configured to display the half-sheet or full-screen promo.
@interface CredentialProviderPromoViewController
    : UIViewController <CredentialProviderPromoConsumer>

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_VIEW_CONTROLLER_H_
