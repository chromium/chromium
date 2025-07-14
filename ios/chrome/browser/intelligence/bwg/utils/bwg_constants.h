// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace bwg {

// The different entrypoints from which BWG was opened.
// Logged as IOSBWGEntryPoint enum for the IOS.BWG.EntryPoint histogram.
// LINT.IfChange(IOSBWGEntryPoint)
enum class EntryPoint {
  // BWG was opened directly from a BWG promo.
  Promo = 0,
  // BWG was opened directly from the overflow menu.
  OverflowMenu = 1,
  // BWG was opened from the AI Hub.
  AIHub = 2,
  // BWG was opened directly from the Omnibox chip, skipping the AI Hub.
  OmniboxChip = 3,
  // BWG was opened via re opening a tab that had BWG open.
  TabReopen = 4,
  // BWG was opened from the Diamond prototype.
  Diamond = 5,
  kMaxValue = Diamond,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSBWGEntryPoint)

}  // namespace bwg

// BWG UI sheet detent identifier.
extern NSString* const kBWGPromoConsentFullDetentIdentifier;

// BWG UI Lottie Animation name for FRE Banner.
extern NSString* const kLottieAnimationFREBannerName;

// Session map dictionary key for the last interaction timestamp.
extern const char kLastInteractionTimestampDictKey[];

// Session map dictionary key for the server ID.
extern const char kServerIDDictKey[];

// Session map dictionary key for the visible URL during the last BWG
// interaction.
extern const char kURLOnLastInteractionDictKey[];

// Links for attributed links.
extern const char kFirstFootnoteLinkURL[];
extern const char kSecondFootnoteLinkURL[];
extern const char kFootnoteLinkURLManagedAccount[];
extern const char kSecondBoxLinkURLManagedAccount[];
extern const char kSecondBoxLink1URLNonManagedAccount[];
extern const char kSecondBoxLink2URLNonManagedAccount[];

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_
