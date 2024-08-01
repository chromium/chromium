// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

@protocol IncognitoInterstitialCoordinatorDelegate;
@protocol TabOpening;

// The coordinator for the Incognito interstitial. It manages a view controller
// with the actual interstitial UI.
@interface IncognitoInterstitialCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<IncognitoInterstitialCoordinatorDelegate>
    delegate;

// Tab opener to be used to open a new tab.
@property(nonatomic, weak) id<TabOpening> tabOpener;

// URL load parameters associated with the external intent.
@property(nonatomic, assign) UrlLoadParams urlLoadParams;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_COORDINATOR_H_
