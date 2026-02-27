// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_

#import "base/feature_list.h"
#import "ios/chrome/browser/intelligence/actuation/actuation_util.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace optimization_guide::proto {
class Action;
}  // namespace optimization_guide::proto

class PrefService;

// Feature flag controlling whether enhanced calendar is enabled.
BASE_DECLARE_FEATURE(kEnhancedCalendar);

// Returns true if enhanced calendar is enabled.
bool IsEnhancedCalendarEnabled();

// Feature flag controlling the proactive suggestions framework.
BASE_DECLARE_FEATURE(kProactiveSuggestionsFramework);

// Returns true if the proactive suggestions framework is enabled.
bool IsProactiveSuggestionsFrameworkEnabled();

// Returns true if the popup blocker feature is enabled within the proactive
// suggestions framework.
bool IsProactiveSuggestionsFrameworkPopupBlockerEnabled();
extern const char kProactiveSuggestionsFrameworkPopupBlocker[];

// Page action menu feature flag, used to roll out and toggle in chrome://flags.
BASE_DECLARE_FEATURE(kPageActionMenu);

// Gemini killswitch, used to disable the feature in any locale, including
// launched ones.
BASE_DECLARE_FEATURE(kGeminiKillSwitch);

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

// Returns true if the Ask Gemini chip should prepopulate the Gemini Floaty with
// a prompt.
bool IsAskGeminiChipPrepopulateFloatyEnabled();
extern const char kAskGeminiChipPrepopulateFloaty[];

// A variation that combines `kAskGeminiChipIgnoreCriteria` and
// `kAskGeminiChipPrepopulateFloaty`.
extern const char kAskGeminiChipPrepopulateAndIgnoreCriteria[];

// Returns true if the Ask Gemini chip should allow non-consented users.
bool IsAskGeminiChipAllowNonconsentedUsersEnabled();
extern const char kAskGeminiChipAllowNonconsentedUsers[];

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

// Feature flag to enable Explain Gemini in Edit Menu.
BASE_DECLARE_FEATURE(kExplainGeminiEditMenu);

extern const char kExplainGeminiEditMenuParams[];

// Holds the position of Explain Gemini button in the EditMenu.
enum class PositionForExplainGeminiEditMenu {
  kDisabled = 0,
  kAfterEdit = 1,
  kAfterSearch = 2,
};

// Returns the position of Explain Gemini in the EditMenu.
PositionForExplainGeminiEditMenu ExplainGeminiEditMenuPosition();

// Feature flag to enable Precise Location in BWG Settings Menu.
BASE_DECLARE_FEATURE(kBWGPreciseLocation);

// Returns true if the precise location setting is enabled.
bool IsBWGPreciseLocationEnabled();

// Feature flag to show the AI Hub new badge.
BASE_DECLARE_FEATURE(kAIHubNewBadge);
bool IsAIHubNewBadgeEnabled();

// Whether the Gemini consent pref should be deleted on account change.
bool ShouldDeleteGeminiConsentPref();

// Feature flag to delete the Gemini consent pref.
BASE_DECLARE_FEATURE(kDeleteGeminiConsentPref);

// Feature flag to enable smart tab grouping.
BASE_DECLARE_FEATURE(kSmartTabGrouping);

// Returns true if smart tab grouping is enabled.
bool IsSmartTabGroupingEnabled();

// Feature parameter determining the storage backend used for persisting tab
// contexts.
extern const char kPersistTabContextStorageParam[];

// Feature parameter detirmining the event(s) that trigger page context
// extraction.
extern const char kPersistTabContextExtractionTimingParam[];

// Feature parameter detirmining what page content data is persisted.
extern const char kPersistTabContextDataParam[];

// Defines the storage backend used for persisting tab contexts.
enum class PersistTabStorageType {
  kFileSystem = 0,
  kSQLite = 1,
};

// Defines the event(s) that trigger page context extraction.
enum class PersistTabExtractionTiming {
  kOnWasHidden = 0,
  kOnWasHiddenAndPageLoad = 1,
};

// Defines what page content data is persisted.
enum class PersistTabDataExtracted {
  kApcAndInnerText = 0,
  kInnerTextOnly = 1,
};

// Returns true if tab context persisting is enabled.
bool IsPersistTabContextEnabled();

// Returns the configured persistent tab context storage type.
PersistTabStorageType GetPersistTabContextStorageType();

// Returns the configured persistent tab context extraction timing.
PersistTabExtractionTiming GetPersistTabContextExtractionTiming();

// Returns the configured persistent tab context data extracted.
PersistTabDataExtracted GetPersistTabContextDataExtracted();

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

// Feature flag to use the new refactored version of the page context extractor.
// Acts as a killswitch where the feature is enabled by default.
BASE_DECLARE_FEATURE(kPageContextExtractorRefactored);

// Returns true if the refactored page context extractor is enabled.
bool IsPageContextExtractorRefactoredEnabled();

// Feature flag to enable the refactored FRE flow (Gemini architecture).
BASE_DECLARE_FEATURE(kGeminiRefactoredFRE);

// Returns true if the Gemini refactored FRE is enabled.
bool IsGeminiRefactoredFREEnabled();

// Feature flag to enable the updated eligibility checks for Gemini.
BASE_DECLARE_FEATURE(kGeminiUpdatedEligibility);

// Returns true if the updated eligibiliy checks for Gemini are enabled.
bool IsGeminiUpdatedEligibilityEnabled();

// Feature flag for enabling the image remixing tool in the Gemini floaty.
BASE_DECLARE_FEATURE(kGeminiImageRemixTool);
bool IsGeminiImageRemixToolEnabled();

// Returns true if the Gemini FRE should show the image remix row.
bool IsGeminiImageRemixToolShowFRERowEnabled();
extern const char kGeminiImageRemixToolShowFRERow[];

// Returns true if the image remix tool should appear above
// search image with Google (entry point will be in that same section).
bool IsGeminiImageRemixToolShowAboveSearchImageEnabled();
extern const char kGeminiImageRemixToolShowAboveSearchImage[];

// Returns true if the image remix tool should appear below
// search image with Google (entry point will be in that same section).
bool IsGeminiImageRemixToolShowBelowSearchImageEnabled();
extern const char kGeminiImageRemixToolShowBelowSearchImage[];

// Feature flag for enabling the Gemini eligibility ablation experiment.
BASE_DECLARE_FEATURE(kGeminiEligibilityAblation);
bool IsGeminiEligibilityAblationEnabled();

// Feature flag for Gemini Live.
BASE_DECLARE_FEATURE(kGeminiLive);
bool IsGeminiLiveEnabled();

// Feature flag for Gemini Personalization.
BASE_DECLARE_FEATURE(kGeminiPersonalization);
bool IsGeminiPersonalizationEnabled();

// Feature flag for Gemini Copresence.
BASE_DECLARE_FEATURE(kGeminiCopresence);
bool IsGeminiCopresenceEnabled();

// The threshold interval for displaying the response ready state in seconds.
extern const char kGeminiCopresenceResponseReadyInterval[];
double GetGeminiCopresenceResponseReadyInterval();

// Returns true if the zero state with chat history is enabled.
bool IsGeminiCopresenceZeroStateWithChatHistoryEnabled();
extern const char kGeminiCopresenceZeroStateWithChatHistory[];

// Feature flag for Gemini Dynamic Resizing.
BASE_DECLARE_FEATURE(kGeminiResponseViewDynamicResizing);

// Returns true if Gemini Dynamic Resizing is enabled.
bool IsGeminiResponseViewDynamicResizingEnabled();

// Feature flag for Gemini Dynamic Settings.
BASE_DECLARE_FEATURE(kGeminiDynamicSettings);
bool IsGeminiDynamicSettingsEnabled();

// Feature flag for Actuation tools.
BASE_DECLARE_FEATURE(kActuationTools);
bool IsActuationEnabled();

// Returns true if the specified tool is disabled via the "DisabledTools"
// feature parameter of the `kActuationTools` feature.
bool IsToolDisabled(optimization_guide::proto::Action::ActionCase tool);

// Feature flag for Model based page classification experiment.
BASE_DECLARE_FEATURE(kModelBasedPageClassification);

// Returns true if Model based page classification is enabled.
bool IsModelBasedPageClassificationEnabled();

// Returns the execution rate (0-100) for the classification experiment.
int GetModelBasedPageClassificationExecutionRate();

extern const char kModelBasedPageClassificationExecutionRateParam[];

// Enables the PageActionMenuIcon feature.
BASE_DECLARE_FEATURE(kPageActionMenuIcon);

// Param for the page action menu icon variations.
extern const char kPageActionMenuIconParams[];

// Icon to use for the page action menu entry point.
enum class PageActionMenuIconVariations {
  kDefault = 0,
  kSparkles1 = 1,
  kSparkles2 = 2,
};

PageActionMenuIconVariations GetPageActionMenuIcon();

// Feature flag for enabling Gemini backend migration.
BASE_DECLARE_FEATURE(kGeminiBackendMigration);
bool IsGeminiBackendMigrationEnabled();

// Feature flag for enabling Gemini actor.
BASE_DECLARE_FEATURE(kGeminiActor);
bool IsGeminiActorEnabled();

// Feature flag for enabling rich APC (v2) extraction for Gemini.
BASE_DECLARE_FEATURE(kGeminiRichAPCExtraction);
bool IsGeminiRichAPCExtractionEnabled();

// Feature flag to enable Gemini Floaty on all pages.
BASE_DECLARE_FEATURE(kGeminiFloatyAllPages);
bool IsGeminiFloatyAllPagesEnabled();

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
