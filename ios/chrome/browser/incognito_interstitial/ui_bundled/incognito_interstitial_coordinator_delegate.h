// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_COORDINATOR_DELEGATE_H_

#import "base/ios/block_types.h"

@class IncognitoInterstitialCoordinator;

@protocol IncognitoInterstitialCoordinatorDelegate <NSObject>

// Callback for the delegate to know it should call stop the interstitial
// coordinator.
- (void)shouldStopIncognitoInterstitial:
    (IncognitoInterstitialCoordinator*)incognitoInterstitial;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_COORDINATOR_DELEGATE_H_
