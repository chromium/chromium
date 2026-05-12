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
  // TabReopen = 4, // Deprecated, no longer used.
  // Gemini was opened from the AppBar.
  AppBar = 5,
  // Gemini was opened via the image long-press context menu.
  ImageContextMenu = 6,
  // Gemini was opened via tapping the image remix in-product help.
  ImageRemixIPH = 7,
  // Gemini was opened via the edit menu to explain the selection.
  EditMenu = 8,
  // Gemini was opened directly from the omnibox badge, skipping the AI Hub.
  DirectOmniboxBadge = 9,
  // Gemini was opened from the AI Hub after a sign-in flow.
  AIHubSignInSheet = 10,
  // Gemini was opened via an external App Store event.
  ExternalAppStoreEvent = 11,
  kMaxValue = ExternalAppStoreEvent,
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
  GestureIph = 14,
  SearchRelatedPage = 15,
  kMaxValue = SearchRelatedPage,
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

// The type of regenerate option for a Gemini query retry.
// Logged as IOSGeminiRegenerateOptionType enum for the
// IOS.Gemini.RegenerateButton.Tapped histogram.
// LINT.IfChange(RegenerateOptionType)
enum class RegenerateOptionType {
  kUnspecified = 0,
  kNoChanges = 1,
  kWithoutPersonalization = 2,
  kWithPersonalization = 3,
  kWithNanoBananaPro = 4,
  kElaborate = 5,
  kShorten = 6,
  kMaxValue = kShorten,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiRegenerateOptionType)

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

// Current state of the Gemini FRE.
// LINT.IfChange(FREState)
enum class FREState {
  // Initial state, when the flow was never started by the user.
  kPending = 0,
  // The FRE flow was shown to the user but they did not proceed til the end and
  // give their explicit consent to Gemini usage.
  kStarted = 1,
  // The user completed the FRE flow and gave their consent to Gemini usage.
  kCompleted = 2,
  kMaxValue = kCompleted
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFREState)

// Input type for Gemini queries.
// LINT.IfChange(InputType)
enum class InputType {
  // Unknown input type.
  kUnknown = 0,
  // Text input type.
  kText = 1,
  // Summarize input type.
  kSummarize = 2,
  // Check this site input type.
  kCheckThisSite = 3,
  // Find related sites input type.
  kFindRelatedSites = 4,
  // Ask about page input type.
  kAskAboutPage = 5,
  // Create FAQ input type.
  kCreateFaq = 6,
  // Omnibox summarize input type.
  kOmniboxSummarize = 7,
  // Zero state model suggestion input type.
  kZeroStateModelSuggestion = 8,
  // 'What can Gemini do' input type.
  kWhatCanGeminiDo = 9,
  // Discovery card input type.
  kDiscoveryCard = 10,
  // Omnibox prompt input type.
  kOmniboxPrompt = 11,
  // Transition to live input type.
  kTransitionToLive = 12,
  // Onboarding: what can gemini do input type.
  kOnboardingWhatCanGeminiDo = 13,
  // Onboarding: ask about page input type.
  kOnboardingAskAboutPage = 14,
  // Onboarding: summarize input type.
  kOnboardingSummarize = 15,
  // Onboarding: no I am done input type.
  kOnboardingNoIAmDone = 16,
  // Onboarding: keep learning input type.
  kOnboardingKeepLearning = 17,
  // Suggested reply input type.
  kSuggestedReply = 18,
  // Nano Banana: turn this page into a comic strip input type.
  kNanoBananaTurnThisPageIntoAComicStrip = 19,
  // Nano Banana: make a folk art illustration input type.
  kNanoBananaMakeAFolkArtIllustration = 20,
  // Nano Banana: make a custom mini figure input type.
  kNanoBananaMakeACustomMiniFigure = 21,
  // Nano Banana: give me a grunge makeover input type.
  kNanoBananaGiveMeAGrungeMakeover = 22,
  // Nano Banana: turn this image into a vintage postcard input type.
  kNanoBananaTurnThisImageIntoAVintagePostcard = 23,
  // Nano Banana: turn this image into a watercolor painting input type.
  kNanoBananaTurnThisImageIntoAWatercolorPainting = 24,
  // Nano Banana: make this image look like instant film input type.
  kNanoBananaMakeThisImageLookLikeInstantFilm = 25,
  // Input from Helios entry point on the Edit menu when user highlights text
  // Something like: “Explain this to me: <selected text>”
  kEditMenuPrompt = 26,
  kMaxValue = kEditMenuPrompt,
};
// LINT.ThenChange(
//   /ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h:IOSGeminiFirstPromptSubmissionMethod,
//   /tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFirstPromptSubmissionMethod
// )

}  // namespace gemini

// Types of Gemini First Run Experience (FRE).
enum class GeminiFREType {
  kNewUser,
  kLive,
};

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

// Session map dictionary key for the visible URL during the last BWG
// interaction.
extern const char kURLOnLastInteractionDictKey[];

// Links for attributed links.
extern const char kFirstFootnoteLinkURL[];
extern const char kSecondFootnoteLinkURL[];
extern const char kKoreanTermsFootnoteLinkURL[];
extern const char kSecondBoxLinkURLManagedAccount[];
extern const char kSecondBoxLink1URLNonManagedAccount[];
extern const char kSecondBoxLink2URLNonManagedAccount[];
extern const char kLivePrivacyNoticeLinkURL[];
extern const char kLiveLearnMoreLinkURL[];
extern const char kLivePrivacyPolicyLinkURL[];
extern const char kWatchLinkURL[];

// Action identifier on a tap on links in the footnote.
extern NSString* const kGeminiFirstFootnoteLinkAction;
extern NSString* const kGeminiSecondFootnoteLinkAction;
extern NSString* const kGeminiKoreanTermsLinkAction;
extern NSString* const kGeminiSecondBoxLinkActionManagedAccount;
extern NSString* const kGeminiSecondBoxLink1ActionNonManagedAccount;
extern NSString* const kGeminiSecondBoxLink2ActionNonManagedAccount;
extern NSString* const kGeminiLivePrivacyNoticeLinkAction;
extern NSString* const kGeminiLiveLearnMoreLinkAction;
extern NSString* const kGeminiLivePrivacyPolicyLinkAction;
extern NSString* const kGeminiWatchLinkAction;

// The sliding window for displaying a Gemini contextual cue chip. Chips are
// shown within this time range (in hours) relative to the last chip that was
// displayed.
extern const int kGeminiContextualCueChipSlidingWindow;

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_CONSTANTS_H_
