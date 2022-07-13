// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INCOGNITO_INTERSTITIAL_INCOGNITO_INTERSTITIAL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_INCOGNITO_INTERSTITIAL_INCOGNITO_INTERSTITIAL_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"

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

// Whether to ask the tab opener to also dismiss the omnibox before opening a
// new tab.
@property(nonatomic, assign) BOOL shouldDismissOmnibox;

- (instancetype)init NS_UNAVAILABLE;

// Stops the coordinator and dismisses the Incognito interstitial with
// `completion` as a completion.
- (void)stopWithCompletion:(ProceduralBlock)completion;

// Starts the coordinator and shows the Incognito interstitial with `completion`
// as a completion.
- (void)startWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_INCOGNITO_INTERSTITIAL_INCOGNITO_INTERSTITIAL_COORDINATOR_H_
