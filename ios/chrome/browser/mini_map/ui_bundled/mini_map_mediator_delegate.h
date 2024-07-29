// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_MEDIATOR_DELEGATE_H_

#include "base/ios/block_types.h"

@protocol MiniMapMediatorDelegate

// Show the consent screen.
- (void)showConsentInterstitial;

// Dismiss the consent screen.
- (void)dismissConsentInterstitialWithCompletion:(ProceduralBlock)completion;

// Show the map. If `showIPH` is YES, add the IPH bubble info to the screen.
- (void)showMapWithIPH:(BOOL)showIPH;

@end

#endif  // IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_MEDIATOR_DELEGATE_H_
