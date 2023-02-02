// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_consumer.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// Container view controller for the Credential Provider Extension promo. Can be
// configured to display the half-sheet or full-screen promo.
@interface CredentialProviderPromoViewController
    : UIViewController <CredentialProviderPromoConsumer>

// Child view controller used to display the alert screen for the half-screen
// and full-screen promos.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;

@end

#endif  // IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_VIEW_CONTROLLER_H_
