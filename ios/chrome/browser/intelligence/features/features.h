// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_

#import "base/feature_list.h"

namespace base {
class TimeDelta;
}  // namespace base

class PrefService;

// Feature flag controlling whether enhanced calendar is enabled.
BASE_DECLARE_FEATURE(kEnhancedCalendar);

// Returns true if enhanced calendar is enabled.
bool IsEnhancedCalendarEnabled();

// Feature flag controlling the proactive suggestions framework.
BASE_DECLARE_FEATURE(kProactiveSuggestionsFramework);

// Returns true if the proactive suggestions framework is enabled.
bool IsProactiveSuggestionsFrameworkEnabled();

// Feature flag controlling the page action menu.
BASE_DECLARE_FEATURE(kPageActionMenu);

// Returns true if the page action menu is enabled.
bool IsPageActionMenuEnabled();

// Feature flag controlling the Ask Gemini chip.
BASE_DECLARE_FEATURE(kAskGeminiChip);

// Returns true if the Ask Gemini chip is enabled.
bool IsAskGeminiChipEnabled();

// Returns true if the Ask Gemini chip should be shown without checking the FET
// and time criteria.
bool IsAskGeminiChipIgnoreCriteria();
extern const char kAskGeminiChipIgnoreCriteria[];

// Returns true if a snackbar should be shown when a site is eligible for Ask
// Gemini.
bool IsAskGeminiSnackbarEnabled();
extern const char kAskGeminiChipUseSnackbar[];

// Returns true if the Ask Gemini chip should prepopulate the Gemini Floaty with
// a prompt.
bool IsAskGeminiChipPrepopulateFloatyEnabled();
extern const char kAskGeminiChipPrepopulateFloaty[];

// Feature flag controlling the cross-tab floaty chat persistence.
BASE_DECLARE_FEATURE(kGeminiCrossTab);

// Returns true if the cross-tab chat persistence is enabled for the floaty.
bool IsGeminiCrossTabEnabled();

// Whether the omnibox entry point opens the BWG overlay immediately, skipping
// the AI hub.
bool IsDirectBWGEntryPoint();
extern const char kPageActionMenuDirectEntryPointParam[];

// The BWG session validity duration in minutes.
const base::TimeDelta BWGSessionValidityDuration();
extern const char kBWGSessionValidityDurationParam[];

// Holds the variations of the BWG Promo Consent flow.
enum class BWGPromoConsentVariations {
  kDisabled = 0,
  kSinglePage = 1,
  kDoublePage = 2,
  kSkipConsent = 3,
  kForceFRE = 4,
  kSkipNewUserDelay = 5,
};
extern const char kBWGPromoConsentParams[];

// Returns the variation of the BWG Promo Consent flow.
BWGPromoConsentVariations BWGPromoConsentVariationsParam();

// Returns YES if the promo should be forced.
bool ShouldForceBWGPromo();

// Returns YES if the Chrome FRE recency check should be skipped when evaluating
// whether to show the Gemini on-navigation promo.
bool ShouldSkipBWGPromoNewUserDelay();

// Feature flag to enable BWG Promo Consent.
BASE_DECLARE_FEATURE(kBWGPromoConsent);

extern const char kExplainGeminiEditMenuParams[];

// Holds the position of Explain Gemini button in the EditMenu.
enum class PositionForExplainGeminiEditMenu {
  kDisabled = 0,
  kAfterEdit = 1,
  kAfterSearch = 2,
};

// Returns the position of Explain Gemini in the EditMenu.
PositionForExplainGeminiEditMenu ExplainGeminiEditMenuPosition();

// Feature flag to enable Explain Gemini in Edit Menu.
BASE_DECLARE_FEATURE(kExplainGeminiEditMenu);

// Feature flag to enable Precise Location in BWG Settings Menu.
BASE_DECLARE_FEATURE(kBWGPreciseLocation);

// Returns true if the precise location setting is enabled.
bool IsBWGPreciseLocationEnabled();

// Feature flag controlling the inclusion of anchor tags (links) in Page
// Context.
BASE_DECLARE_FEATURE(kPageContextAnchorTags);

// Returns true if the anchor tags are enabled in Page Context.
bool IsPageContextAnchorTagsEnabled();

// Feature flag controlling whether Gemini is available for managed accounts.
BASE_DECLARE_FEATURE(kGeminiForManagedAccounts);

// Returns true if Gemini is available for managed accounts. If true, can still
// be disabled by an Enterprise policy.
bool IsGeminiAvailableForManagedAccounts();

// Feature flag to show the AI Hub new badge.
BASE_DECLARE_FEATURE(kAIHubNewBadge);

// Whether the Gemini consent pref should be deleted on account change.
bool ShouldDeleteGeminiConsentPref();

// Feature flag to delete the Gemini consent pref.
BASE_DECLARE_FEATURE(kDeleteGeminiConsentPref);

// Feature flag to enable smart tab grouping.
BASE_DECLARE_FEATURE(kSmartTabGrouping);

// Returns true if smart tab grouping is enabled.
bool IsSmartTabGroupingEnabled();

// Returns true if tab context persisting is enabled.
bool IsPersistTabContextEnabled();

// Feature flag to persist tab context.
BASE_DECLARE_FEATURE(kPersistTabContext);

// Returns the effective Time-To-Live (TTL) for persisted tab contexts.
// This is the minimum of the `ttl_days` Finch parameter (with a default
// fallback if it is invalid) and the TTL defined by the Inactive Tabs feature.
base::TimeDelta GetPersistedContextEffectiveTTL(PrefService* prefs);

// Acts as a killswitch (enabled by default) for the
// PersistTabContextBrowserAgent.
BASE_DECLARE_FEATURE(kCleanupPersistedTabContexts);

// Returns true if persisted tab contexts cleanup is enabled.
bool IsCleanupPersistedTabContextsEnabled();

// Feature flag for the automatic Gemini promo shown on navigation.
BASE_DECLARE_FEATURE(kGeminiNavigationPromo);

// Returns true if the Gemini navigation promo is enabled.
bool IsGeminiNavigationPromoEnabled();

// Feature flag to enable zero-state suggestions.
BASE_DECLARE_FEATURE(kZeroStateSuggestions);

// Returns true if zero-state suggestions are enabled.
bool IsZeroStateSuggestionsEnabled();

// Parameter names for the zero-state suggestions placement.
extern const char kZeroStateSuggestionsPlacementAIHub[];
extern const char kZeroStateSuggestionsPlacementAskGemini[];

// Returns true if zero-state suggestions should be executed in the AI Hub.
bool IsZeroStateSuggestionsAIHubEnabled();

// Returns true if zero-state suggestions should be executed in the Ask Gemini
// overlay.
bool IsZeroStateSuggestionsAskGeminiEnabled();

// Feature flag for showing full chat history in the floaty.
BASE_DECLARE_FEATURE(kGeminiFullChatHistory);
bool IsGeminiFullChatHistoryEnabled();

// Feature flag for the redesigned loading state UI.
BASE_DECLARE_FEATURE(kGeminiLoadingStateRedesign);
bool IsGeminiLoadingStateRedesignEnabled();

// Feature flag for the floaty latency improvements.
BASE_DECLARE_FEATURE(kGeminiLatencyImprovement);
bool IsGeminiLatencyImprovementEnabled();

// Feature flag for showing the Gemini floaty immediately.
//
// This feature exists so the overlay can open without having to wait for the
// page to finish loading.
BASE_DECLARE_FEATURE(kGeminiImmediateOverlay);
bool IsGeminiImmediateOverlayEnabled();

// Feature flag for the discovery onboarding cards.
BASE_DECLARE_FEATURE(kGeminiOnboardingCards);
bool IsGeminiOnboardingCardsEnabled();

// Feature flag to use the new refactored version of the page context extractor.
// Acts as a killswitch where the feature is enabled by default.
BASE_DECLARE_FEATURE(kPageContextExtractorRefactored);

// Feature flag for displaying a sheet which shows the web page's self-reported
// important images. Experimental.
BASE_DECLARE_FEATURE(kWebPageReportedImagesSheet);
bool IsWebPageReportedImagesSheetEnabled();

// Feature flag for enabling passing an image from the long-press context menu
// to Gemini.
BASE_DECLARE_FEATURE(kImageContextMenuGeminiEntryPoint);
bool IsImageContextMenuGeminiEntryPointEnabled();

// Feature flag for enabling the Gemini eligibility ablation experiment.
BASE_DECLARE_FEATURE(kGeminiEligibilityAblation);
bool IsGeminiEligibilityAblationEnabled();

// Feature flag for Gemini Live.
BASE_DECLARE_FEATURE(kGeminiLive);
bool IsGeminiLiveEnabled();

// Feature flag for Gemini Personalization.
BASE_DECLARE_FEATURE(kGeminiPersonalization);
bool IsGeminiPersonalizationEnabled();

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
