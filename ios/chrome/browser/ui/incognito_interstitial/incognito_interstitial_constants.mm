// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kIncognitoInterstitialAccessibilityIdentifier =
    @"incognitoInterstitialAccessibilityIdentifier";

NSString* const kIncognitoInterstitialURLLabelAccessibilityIdentifier =
    @"incognitoInterstitialURLLabelAccessibilityIdentifier";

NSString* const kIncognitoInterstitialCancelButtonAccessibilityIdentifier =
    @"incognitoInterstitialCancelButtonAccessibilityIdentifier";

const char kIncognitoInterstitialActionsHistogram[] =
    "IOS.IncognitoInterstitial";
const char kIncognitoInterstitialSettingsActionsHistogram[] =
    "IOS.IncognitoInterstitial.Settings";
