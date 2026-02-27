// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import <optional>

#import "base/check.h"
#import "base/metrics/field_trial_params.h"
#import "base/strings/string_split.h"
#import "base/strings/string_util.h"
#import "base/time/time.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/service/variations_service_utils.h"
#import "ios/chrome/browser/intelligence/actuation/actuation_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"

BASE_FEATURE(kEnhancedCalendar, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsEnhancedCalendarEnabled() {
  return base::FeatureList::IsEnabled(kEnhancedCalendar);
}

// Launched in en-US, but remains disabled by default for other locales.
BASE_FEATURE(kPageActionMenu, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGeminiKillSwitch, base::FEATURE_DISABLED_BY_DEFAULT);

const char kPageActionMenuDirectEntryPointParam[] =
    "PageActionMenuDirectEntryPoint";

bool IsPageActionMenuEnabled() {
  // Checks the killswtich, allowing to disable the feature for any user
  // including those in launched locales.
  bool is_killswitch_enabled = base::FeatureList::IsEnabled(kGeminiKillSwitch);
  if (is_killswitch_enabled) {
    return false;
  }

  // Launched in en-US. Checks for the country (US) and locale (en-US).
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  bool is_launched_country =
      variations_service &&
      base::ToLowerASCII(variations_service->GetStoredPermanentCountry()) ==
          "us";

  ApplicationLocaleStorage* locale_storage =
      GetApplicationContext()->GetApplicationLocaleStorage();
  bool is_launched_locale =
      locale_storage && base::ToLowerASCII(locale_storage->Get()) == "en-us";

  if (is_launched_country && is_launched_locale) {
    return true;
  }

  // Allows for the feature to be enabled through Finch or chrome://flags.
  return base::FeatureList::IsEnabled(kPageActionMenu);
}

BASE_FEATURE(kProactiveSuggestionsFramework, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsProactiveSuggestionsFrameworkEnabled() {
  return IsPageActionMenuEnabled() &&
         base::FeatureList::IsEnabled(kProactiveSuggestionsFramework);
}

const char kProactiveSuggestionsFrameworkPopupBlocker[] = "PopupBlocker";

bool IsProactiveSuggestionsFrameworkPopupBlockerEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kProactiveSuggestionsFramework,
      kProactiveSuggestionsFrameworkPopupBlocker, false);
}

BASE_FEATURE(kAskGeminiChip, base::FEATURE_DISABLED_BY_DEFAULT);

const char kAskGeminiChipIgnoreCriteria[] = "AskGeminiChipIgnoreCriteria";

const char kAskGeminiChipPrepopulateFloaty[] = "AskGeminiChipPrepopulateFloaty";

const char kAskGeminiChipPrepopulateAndIgnoreCriteria[] =
    "AskGeminiChipPrepopulateAndIgnoreCriteria";

const char kAskGeminiChipAllowNonconsentedUsers[] =
    "AskGeminiChipAllowNonconsentedUsers";

bool IsAskGeminiChipEnabled() {
  return base::FeatureList::IsEnabled(kAskGeminiChip);
}

bool IsAskGeminiChipIgnoreCriteria() {
  if (base::GetFieldTrialParamByFeatureAsBool(
          kAskGeminiChip, kAskGeminiChipPrepopulateAndIgnoreCriteria, false)) {
    return true;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kAskGeminiChip, kAskGeminiChipIgnoreCriteria, false);
}

bool IsAskGeminiChipPrepopulateFloatyEnabled() {
  if (base::GetFieldTrialParamByFeatureAsBool(
          kAskGeminiChip, kAskGeminiChipPrepopulateAndIgnoreCriteria, false)) {
    return true;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kAskGeminiChip, kAskGeminiChipPrepopulateFloaty, false);
}

bool IsAskGeminiChipAllowNonconsentedUsersEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kAskGeminiChip, kAskGeminiChipAllowNonconsentedUsers, false);
}

bool IsDirectBWGEntryPoint() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kPageActionMenu, kPageActionMenuDirectEntryPointParam, false);
}

const char kBWGSessionValidityDurationParam[] = "BWGSessionValidityDuration";

BASE_FEATURE_PARAM(int,
                   kBWGSessionValidityDurationFeatureParam,
                   &kPageActionMenu,
                   kBWGSessionValidityDurationParam,
                   30);

const base::TimeDelta BWGSessionValidityDuration() {
  return base::Minutes(kBWGSessionValidityDurationFeatureParam.Get());
}

const char kBWGPromoConsentParams[] = "BWGPromoConsentVariations";

BASE_FEATURE_PARAM(int,
                   kBWGPromoConsentFeatureParam,
                   &kBWGPromoConsent,
                   kBWGPromoConsentParams,
                   0);

BWGPromoConsentVariations BWGPromoConsentVariationsParam() {
  int param = kBWGPromoConsentFeatureParam.Get();
  if (!IsPageActionMenuEnabled()) {
    return BWGPromoConsentVariations::kDisabled;
  }
  if (param == 1) {
    return BWGPromoConsentVariations::kSinglePage;
  }
  if (param == 2) {
    return BWGPromoConsentVariations::kDoublePage;
  }
  if (param == 3) {
    return BWGPromoConsentVariations::kSkipConsent;
  }
  if (param == 4) {
    return BWGPromoConsentVariations::kForceFRE;
  }
  if (param == 5) {
    return BWGPromoConsentVariations::kSkipNewUserDelay;
  }
  return BWGPromoConsentVariations::kDisabled;
}

bool ShouldForceBWGPromo() {
  return BWGPromoConsentVariationsParam() ==
         BWGPromoConsentVariations::kForceFRE;
}

bool ShouldSkipBWGPromoNewUserDelay() {
  return BWGPromoConsentVariationsParam() ==
         BWGPromoConsentVariations::kSkipNewUserDelay;
}

BASE_FEATURE(kBWGPromoConsent, base::FEATURE_DISABLED_BY_DEFAULT);

const char kExplainGeminiEditMenuParams[] = "PositionForExplainGeminiEditMenu";

BASE_FEATURE_PARAM(int,
                   kExplainGeminiEditMenuFeatureParam,
                   &kExplainGeminiEditMenu,
                   kExplainGeminiEditMenuParams,
                   0);

PositionForExplainGeminiEditMenu ExplainGeminiEditMenuPosition() {
  int param = kExplainGeminiEditMenuFeatureParam.Get();
  if (param == 1) {
    return PositionForExplainGeminiEditMenu::kAfterEdit;
  }
  if (param == 2) {
    return PositionForExplainGeminiEditMenu::kAfterSearch;
  }
  return PositionForExplainGeminiEditMenu::kDisabled;
}

BASE_FEATURE(kExplainGeminiEditMenu, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBWGPreciseLocation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBWGPreciseLocationEnabled() {
  CHECK(IsPageActionMenuEnabled());
  return base::FeatureList::IsEnabled(kBWGPreciseLocation);
}

BASE_FEATURE(kAIHubNewBadge, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsAIHubNewBadgeEnabled() {
  return IsPageActionMenuEnabled();
}

bool ShouldDeleteGeminiConsentPref() {
  return base::FeatureList::IsEnabled(kDeleteGeminiConsentPref);
}

BASE_FEATURE(kDeleteGeminiConsentPref, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSmartTabGrouping, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSmartTabGroupingEnabled() {
  return base::FeatureList::IsEnabled(kSmartTabGrouping);
}

BASE_FEATURE(kPersistTabContext, base::FEATURE_DISABLED_BY_DEFAULT);

const char kPersistTabContextStorageParam[] = "storage_implementation";
const char kPersistTabContextExtractionTimingParam[] = "extraction_timing";
const char kPersistTabContextDataParam[] = "data_extracted";

BASE_FEATURE_PARAM(int,
                   kPersistTabContextStorageFeatureParam,
                   &kPersistTabContext,
                   kPersistTabContextStorageParam,
                   static_cast<int>(PersistTabStorageType::kFileSystem));

BASE_FEATURE_PARAM(int,
                   kPersistTabContextExtractionTimingFeatureParam,
                   &kPersistTabContext,
                   kPersistTabContextExtractionTimingParam,
                   static_cast<int>(PersistTabExtractionTiming::kOnWasHidden));

BASE_FEATURE_PARAM(int,
                   kPersistTabContextDataFeatureParam,
                   &kPersistTabContext,
                   kPersistTabContextDataParam,
                   static_cast<int>(PersistTabDataExtracted::kApcAndInnerText));

bool IsPersistTabContextEnabled() {
  if (IsSmartTabGroupingEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kPersistTabContext);
}

PersistTabStorageType GetPersistTabContextStorageType() {
  int param = kPersistTabContextStorageFeatureParam.Get();
  if (param == static_cast<int>(PersistTabStorageType::kSQLite) &&
      base::FeatureList::IsEnabled(
          page_content_annotations::features::kPageContentCache)) {
    return PersistTabStorageType::kSQLite;
  }
  return PersistTabStorageType::kFileSystem;
}

PersistTabExtractionTiming GetPersistTabContextExtractionTiming() {
  int param = kPersistTabContextExtractionTimingFeatureParam.Get();
  if (param ==
      static_cast<int>(PersistTabExtractionTiming::kOnWasHiddenAndPageLoad)) {
    return PersistTabExtractionTiming::kOnWasHiddenAndPageLoad;
  }
  return PersistTabExtractionTiming::kOnWasHidden;
}

PersistTabDataExtracted GetPersistTabContextDataExtracted() {
  int param = kPersistTabContextDataFeatureParam.Get();
  if (param == static_cast<int>(PersistTabDataExtracted::kInnerTextOnly)) {
    return PersistTabDataExtracted::kInnerTextOnly;
  }
  return PersistTabDataExtracted::kApcAndInnerText;
}

BASE_FEATURE(kCleanupPersistedTabContexts, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsCleanupPersistedTabContextsEnabled() {
  return base::FeatureList::IsEnabled(kCleanupPersistedTabContexts);
}

// The default Time-To-Live in days for persisted contexts.
constexpr int kPersistTabContextDefaultTTL = 21;

BASE_FEATURE_PARAM(int,
                   kPersistTabContextTTLParam,
                   &kPersistTabContext,
                   "ttl_days",
                   kPersistTabContextDefaultTTL);

base::TimeDelta GetPersistedContextEffectiveTTL(PrefService* prefs) {
  int persist_ttl_days = kPersistTabContextTTLParam.Get();
  if (persist_ttl_days < 0) {
    // Fallback to a safe default if the Finch value is invalid.
    persist_ttl_days = kPersistTabContextDefaultTTL;
  }

  base::TimeDelta persist_ttl = base::Days(persist_ttl_days);
  base::TimeDelta inactive_tabs_ttl = InactiveTabsTimeThreshold(prefs);

  return std::min(persist_ttl, inactive_tabs_ttl);
}

BASE_FEATURE(kGeminiNavigationPromo, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiNavigationPromoEnabled() {
  if (!IsPageActionMenuEnabled() ||
      !base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSGeminiFullscreenPromoFeature)) {
    return false;
  }

  return base::FeatureList::IsEnabled(kGeminiNavigationPromo);
}

BASE_FEATURE(kZeroStateSuggestions, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsZeroStateSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kZeroStateSuggestions);
}

const char kZeroStateSuggestionsPlacementAIHub[] =
    "ZeroStateSuggestionsPlacementAIHub";
const char kZeroStateSuggestionsPlacementAskGemini[] =
    "ZeroStateSuggestionsPlacementAskGemini";

bool IsZeroStateSuggestionsAIHubEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kZeroStateSuggestions, kZeroStateSuggestionsPlacementAIHub, false);
}

bool IsZeroStateSuggestionsAskGeminiEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kZeroStateSuggestions, kZeroStateSuggestionsPlacementAskGemini, false);
}

BASE_FEATURE(kGeminiFullChatHistory, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiFullChatHistoryEnabled() {
  return base::FeatureList::IsEnabled(kGeminiFullChatHistory);
}

BASE_FEATURE(kGeminiLoadingStateRedesign, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiLoadingStateRedesignEnabled() {
  return base::FeatureList::IsEnabled(kGeminiLoadingStateRedesign);
}

BASE_FEATURE(kGeminiLatencyImprovement, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiLatencyImprovementEnabled() {
  return base::FeatureList::IsEnabled(kGeminiLatencyImprovement);
}

BASE_FEATURE(kPageContextExtractorRefactored, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPageContextExtractorRefactoredEnabled() {
  return base::FeatureList::IsEnabled(kPageContextExtractorRefactored);
}

BASE_FEATURE(kGeminiRefactoredFRE, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiRefactoredFREEnabled() {
  return base::FeatureList::IsEnabled(kGeminiRefactoredFRE);
}

BASE_FEATURE(kGeminiUpdatedEligibility, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiUpdatedEligibilityEnabled() {
  return base::FeatureList::IsEnabled(kGeminiUpdatedEligibility);
}

BASE_FEATURE(kGeminiImageRemixTool, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiImageRemixToolEnabled() {
  return base::FeatureList::IsEnabled(kGeminiImageRemixTool);
}

const char kGeminiImageRemixToolShowFRERow[] = "ShowFRERow";

bool IsGeminiImageRemixToolShowFRERowEnabled() {
  if (!IsGeminiImageRemixToolEnabled()) {
    return false;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kGeminiImageRemixTool, kGeminiImageRemixToolShowFRERow, false);
}

const char kGeminiImageRemixToolShowAboveSearchImage[] = "ShowAboveSearchImage";

bool IsGeminiImageRemixToolShowAboveSearchImageEnabled() {
  if (!IsGeminiImageRemixToolEnabled()) {
    return false;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kGeminiImageRemixTool, kGeminiImageRemixToolShowAboveSearchImage, true);
}

const char kGeminiImageRemixToolShowBelowSearchImage[] = "ShowBelowSearchImage";

bool IsGeminiImageRemixToolShowBelowSearchImageEnabled() {
  if (!IsGeminiImageRemixToolEnabled()) {
    return false;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kGeminiImageRemixTool, kGeminiImageRemixToolShowBelowSearchImage, false);
}

BASE_FEATURE(kGeminiEligibilityAblation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiEligibilityAblationEnabled() {
  return base::FeatureList::IsEnabled(kGeminiEligibilityAblation);
}

BASE_FEATURE(kGeminiLive, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiLiveEnabled() {
  return base::FeatureList::IsEnabled(kGeminiLive);
}

BASE_FEATURE(kGeminiPersonalization, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiPersonalizationEnabled() {
  return base::FeatureList::IsEnabled(kGeminiPersonalization);
}
BASE_FEATURE(kGeminiCopresence, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiCopresenceEnabled() {
  return base::FeatureList::IsEnabled(kGeminiCopresence);
}

const char kGeminiCopresenceResponseReadyInterval[] =
    "GeminiCopresenceResponseReadyInterval";

// The response ready interval default.
constexpr double kGeminiCopresenceResponseReadyIntervalDefault = 7.0;

double GetGeminiCopresenceResponseReadyInterval() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kGeminiCopresence, kGeminiCopresenceResponseReadyInterval,
      kGeminiCopresenceResponseReadyIntervalDefault);
}

const char kGeminiCopresenceZeroStateWithChatHistory[] =
    "GeminiCopresenceZeroStateWithChatHistory";

bool IsGeminiCopresenceZeroStateWithChatHistoryEnabled() {
  if (!IsGeminiCopresenceEnabled()) {
    return false;
  }

  return base::GetFieldTrialParamByFeatureAsBool(
      kGeminiCopresence, kGeminiCopresenceZeroStateWithChatHistory, false);
}
BASE_FEATURE(kGeminiResponseViewDynamicResizing,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiResponseViewDynamicResizingEnabled() {
  return base::FeatureList::IsEnabled(kGeminiResponseViewDynamicResizing);
}

BASE_FEATURE(kGeminiDynamicSettings, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiDynamicSettingsEnabled() {
  return base::FeatureList::IsEnabled(kGeminiDynamicSettings);
}

BASE_FEATURE(kActuationTools, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsActuationEnabled() {
  return base::FeatureList::IsEnabled(kActuationTools);
}

bool IsToolDisabled(optimization_guide::proto::Action::ActionCase tool) {
  if (!IsActuationEnabled()) {
    return true;
  }

  std::optional<std::string> tool_name = ActuationActionCaseToToolName(tool);
  if (!tool_name) {
    // Don't support tools that aren't in the proto.
    return true;
  }

  std::string disabled_tools =
      base::GetFieldTrialParamValueByFeature(kActuationTools, "DisabledTools");
  if (disabled_tools.empty()) {
    return false;
  }

  std::vector<std::string> disabled_list = base::SplitString(
      disabled_tools, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const auto& disabled_tool : disabled_list) {
    if (disabled_tool == *tool_name) {
      return true;
    }
  }

  return false;
}

BASE_FEATURE(kModelBasedPageClassification, base::FEATURE_DISABLED_BY_DEFAULT);

const char kModelBasedPageClassificationExecutionRateParam[] = "execution_rate";

BASE_FEATURE_PARAM(int,
                   kModelBasedPageClassificationExecutionRateFeatureParam,
                   &kModelBasedPageClassification,
                   kModelBasedPageClassificationExecutionRateParam,
                   0);

bool IsModelBasedPageClassificationEnabled() {
  // Check strict eligibility similar to other AI features.
  // Launched in en-US. Checks for the country (US) and locale (en-US).
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  bool is_launched_country =
      variations_service &&
      base::ToLowerASCII(variations_service->GetStoredPermanentCountry()) ==
          "us";

  ApplicationLocaleStorage* locale_storage =
      GetApplicationContext()->GetApplicationLocaleStorage();
  bool is_launched_locale =
      locale_storage && base::ToLowerASCII(locale_storage->Get()) == "en-us";

  if (!is_launched_country || !is_launched_locale) {
    return false;
  }

  return base::FeatureList::IsEnabled(kModelBasedPageClassification);
}

int GetModelBasedPageClassificationExecutionRate() {
  // Finch parameter for execution rate, we will want to keep it low so it runs
  // on a random small percentage of page loads.
  return kModelBasedPageClassificationExecutionRateFeatureParam.Get();
}

BASE_FEATURE(kPageActionMenuIcon, base::FEATURE_DISABLED_BY_DEFAULT);

const char kPageActionMenuIconParams[] = "PageActionMenuIconParams";

PageActionMenuIconVariations GetPageActionMenuIcon() {
  int param = base::GetFieldTrialParamByFeatureAsInt(
      kPageActionMenuIcon, kPageActionMenuIconParams, 0);
  if (param == 1) {
    return PageActionMenuIconVariations::kSparkles1;
  }
  if (param == 2) {
    return PageActionMenuIconVariations::kSparkles2;
  }
  return PageActionMenuIconVariations::kDefault;
}

BASE_FEATURE(kGeminiBackendMigration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiBackendMigrationEnabled() {
  return base::FeatureList::IsEnabled(kGeminiBackendMigration);
}

BASE_FEATURE(kGeminiActor, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiActorEnabled() {
  return base::FeatureList::IsEnabled(kGeminiActor);
}

BASE_FEATURE(kGeminiRichAPCExtraction, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiRichAPCExtractionEnabled() {
  if (!IsPageContextExtractorRefactoredEnabled()) {
    return false;
  }

  return base::FeatureList::IsEnabled(kGeminiRichAPCExtraction);
}

BASE_FEATURE(kGeminiFloatyAllPages, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiFloatyAllPagesEnabled() {
  return base::FeatureList::IsEnabled(kGeminiFloatyAllPages);
}
