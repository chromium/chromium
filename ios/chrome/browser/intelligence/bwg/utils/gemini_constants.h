// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

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
  // Gemini was opened via the edit menu to explain the selection.
  EditMenu = 8,
  kMaxValue = EditMenu,
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
  Keyboard = 13,
  kMaxValue = Keyboard,
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

// Settings for Gemini integration.
enum class SettingsPolicy {
  kAllowed = 0,
  kNotAllowed = 1,
};

// Default policy values for generative AI features.
enum class GenAiDefaultSettingsPolicy {
  //  Allow GenAI features and improve AI models by using relevant data.
  kAllowedImprovingModels = 0,
  //  Allow GenAI features without improving AI models.
  kAllowedWithoutImprovingModels = 1,
  // Do not allow GenAI features.
  kNotAllowed = 2,
};
}  // namespace gemini

// Set of parameters for starting a Gemini session.
@interface GeminiStartupState : NSObject

// The entry point that triggered the Gemini session.
@property(nonatomic, assign) gemini::EntryPoint entryPoint;

// An optional image to attach to the query.
@property(nonatomic, strong) UIImage* imageAttachment;

// An optional text prompt to prepopulate the input field.
@property(nonatomic, copy) NSString* prepopulatedPrompt;

// Initializes with the given entry point.
- (instancetype)initWithEntryPoint:(gemini::EntryPoint)entryPoint;

@end

// Gemini UI sheet detent identifier.
extern NSString* const kGeminiPromoConsentFullDetentIdentifier;

// Gemini UI Lottie Animation name for FRE Banner.
extern NSString* const kLottieAnimationFREBannerName;

// Session map dictionary key for the last interaction timestamp.
extern const char kLastInteractionTimestampDictKey[];

// The accessibility ID of the Gemini consent FootNote textView.
extern NSString* const kGeminiFootNoteTextViewAccessibilityIdentifier;

// The accessibility ID the Gemini consent primary button.
extern NSString* const kGeminiPrimaryButtonAccessibilityIdentifier;

// The accessibility ID the Gemini consent secondary button.
extern NSString* const kGeminiSecondaryButtonAccessibilityIdentifier;

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
extern NSString* const kGeminiFirstFootnoteLinkAction;
extern NSString* const kGeminiSecondFootnoteLinkAction;
extern NSString* const kGeminiFootnoteLinkActionManagedAccount;
extern NSString* const kGeminiSecondBoxLinkActionManagedAccount;
extern NSString* const kGeminiSecondBoxLink1ActionNonManagedAccount;
extern NSString* const kGeminiSecondBoxLink2ActionNonManagedAccount;

// The sliding window for displaying a Gemini contextual cue chip. Chips are
// shown within this time range (in hours) relative to the last chip that was
// displayed.
extern const int kGeminiContextualCueChipSlidingWindow;

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_CONSTANTS_H_
