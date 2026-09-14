// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_PUBLIC_CONTEXTUAL_DEFAULT_BROWSER_PROMO_CONSTANTS_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_PUBLIC_CONTEXTUAL_DEFAULT_BROWSER_PROMO_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Enum defining the available contextual default browser promo types.
// LINT.IfChange(IOSDefaultBrowserContextualPromoType)
enum class ContextualDefaultBrowserPromoType {
  kGemini = 0,
  kMaxValue = kGemini,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSDefaultBrowserContextualPromoType)

// Accessibility identifier for the contextual default browser promo view.
extern NSString* const kContextualDefaultBrowserPromoAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_PUBLIC_CONTEXTUAL_DEFAULT_BROWSER_PROMO_CONSTANTS_H_
