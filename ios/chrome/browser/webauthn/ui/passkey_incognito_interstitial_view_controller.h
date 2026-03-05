// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_UI_PASSKEY_INCOGNITO_INTERSTITIAL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_UI_PASSKEY_INCOGNITO_INTERSTITIAL_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// Delegate for the PasskeyIncognitoInterstitialViewController.
@protocol PasskeyIncognitoInterstitialViewControllerDelegate <NSObject>
- (void)passkeyIncognitoInterstitialViewDidDisappear;
@end

// The accessibility identifier of incognito interstitial bottom sheet.
extern NSString* const kPasskeyIncognitoInterstitialViewID;

// View controller for the incognito passkey interstitial.
@interface PasskeyIncognitoInterstitialViewController
    : ConfirmationAlertViewController

// Delegate to handle lifecycle events for this view controller.
@property(nonatomic, weak)
    id<PasskeyIncognitoInterstitialViewControllerDelegate>
        delegate;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_UI_PASSKEY_INCOGNITO_INTERSTITIAL_VIEW_CONTROLLER_H_
