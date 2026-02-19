// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace gemini {

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
  // Gemini was opened from the AppBar.
  AppBar = 5,
  // Gemini was opened via the image long-press context menu.
  ImageContextMenu = 6,
  // Gemini was opened via tapping the image remix in-product help.
  ImageRemixIPH = 7,
  kMaxValue = ImageRemixIPH,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiEntryPoint)

// The different update sources for the Gemini floaty.
// Logged as IOSGeminiFloatyUpdateSource enum for the
// IOS.Gemini.Floaty.HiddenFromSource and IOS.Gemini.Floaty.ShownFromSource
// histogram.
// LINT.IfChange(FloatyUpdateSource)
enum class FloatyUpdateSource {
  Unknown = 0,
  ViewTransition = 1,
  WebNavigation = 2,
  TabGrid = 3,
  ContextMenu = 4,
  WebContextMenu = 5,
  ForcedFromScroll = 6,
  Overlay = 7,
  IneligibleSite = 8,
  ForcedFromQueryResponse = 9,
  Snackbar = 10,
  Alert = 11,
  Banner = 12,
  kMaxValue = Banner,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFloatyUpdateSource)

// The action button type for an image in Gemini.
// Logged as IOSGeminiImageActionButtonType enum for the
// IOS.Gemini.ImageActionButton.Tapped histogram.
// LINT.IfChange(ImageActionButtonType)
enum class ImageActionButtonType {
  kUnknown = 0,
  kCopy = 1,
  kDownload = 2,
  kShare = 3,
  kMaxValue = kShare,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiImageActionButtonType)

// The input plate view attachment option for Gemini queries.
// Logged as IOSGeminiInputPlateAttachmentOption enum for the
// IOS.Gemini.InputPlateAttachmentOption.Tapped histogram.
// LINT.IfChange(InputPlateAttachmentOption)
enum class InputPlateAttachmentOption {
  kUnknown = 0,
  kCamera = 1,
  kGallery = 2,
  kCreateImageDeselected = 3,
  kCreateImageSelected = 4,
  kMaxValue = kCreateImageSelected,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiInputPlateAttachmentOption)

}  // namespace gemini

// BWG UI sheet detent identifier.
extern NSString* const kBWGPromoConsentFullDetentIdentifier;

// BWG UI Lottie Animation name for FRE Banner.
extern NSString* const kLottieAnimationFREBannerName;

// Session map dictionary key for the last interaction timestamp.
extern const char kLastInteractionTimestampDictKey[];

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
