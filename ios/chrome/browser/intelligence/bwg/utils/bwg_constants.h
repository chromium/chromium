// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace bwg {

// The different entrypoints from which Gemini was opened.
// Logged as IOSGeminiEntryPoint enum for the IOS.Gemini.EntryPoint histogram.
// LINT.IfChange(EntryPoint)
enum class EntryPoint {
  // Gemini was opened directly from a Gemini promo.
  Promo = 0,
  // Gemini was opened directly from the overflow menu.
  OverflowMenu = 1,
  // Gemini was opened from the AI Hub.
  AIHub = 2,
  // Gemini was opened directly from the Omnibox chip, skipping the AI Hub.
  OmniboxChip = 3,
  // Gemini was opened via re opening a tab that had Gemini open.
  TabReopen = 4,
  // Gemini was opened from the Diamond prototype.
  Diamond = 5,
  // Gemini was opened via the image long-press context menu.
  ImageContextMenu = 6,
  kMaxValue = ImageContextMenu,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiEntryPoint)

}  // namespace bwg

// BWG UI sheet detent identifier.
extern NSString* const kBWGPromoConsentFullDetentIdentifier;

// BWG UI Lottie Animation name for FRE Banner.
extern NSString* const kLottieAnimationFREBannerName;

// Session map dictionary key for the last interaction timestamp.
extern const char kLastInteractionTimestampDictKey[];

// Session map dictionary key for the server ID.
extern const char kServerIDDictKey[];

// The accessibility ID of the bwg consent FootNote textView.
extern NSString* const kBwgFootNoteTextViewAccessibilityIdentifier;

// The accessibility ID the bwg consent primary button.
extern NSString* const kBwgPrimaryButtonAccessibilityIdentifier;

// The accessibility ID the bwg consent secondary button.
extern NSString* const kBwgSecondaryButtonAccessibilityIdentifier;

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

// Action identifier on a tap on links in the footnote.
extern NSString* const kBwgFirstFootnoteLinkAction;
extern NSString* const kBwgSecondFootnoteLinkAction;
extern NSString* const kBwgFootnoteLinkActionManagedAccount;
extern NSString* const kBwgSecondBoxLinkActionManagedAccount;
extern NSString* const kBwgSecondBoxLink1ActionNonManagedAccount;
extern NSString* const kBwgSecondBoxLink2ActionNonManagedAccount;

// The sliding window for displaying a Gemini contextual cue chip. Chips are
// shown within this time range (in hours) relative to the last chip that was
// displayed.
extern const int kGeminiContextualCueChipSlidingWindow;

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_
