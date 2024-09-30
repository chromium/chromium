// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_UTILS_H_

#import <UIKit/UIKit.h>

class AuthenticationService;
class PrefService;

namespace commerce {
class ShoppingService;
}

// Returns the amount that MagicStack modules are narrower than the ScrollView,
// in order to allow peeking at the sides.
CGFloat ModuleNarrowerWidthToAllowPeekingForTraitCollection(
    UITraitCollection* traitCollection);

// True if the price tracking notification card feature is enabled.
bool IsPriceTrackingPromoCardEnabled(commerce::ShoppingService* service,
                                     AuthenticationService* auth_service,
                                     PrefService* pref_service);

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_UTILS_H_
