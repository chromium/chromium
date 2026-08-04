// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import <algorithm>
#import <array>
#import <optional>
#import <string_view>

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
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"

BASE_FEATURE(kEnhancedCalendar, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsEnhancedCalendarEnabled() {
  return base::FeatureList::IsEnabled(kEnhancedCalendar);
}

// Launched in en-US, but remains disabled by default for other locales.
BASE_FEATURE(kPageActionMenu, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGeminiKillSwitch, base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled countries for PageActionMenu in Gemini for Chrome
// Expansion V2.
constexpr std::array<std::string_view, 53> kDefaultEnabledCountries = {
    "as", "au", "bd", "bn", "bt", "ca", "cc", "ck", "cx", "fj", "fm",
    "gu", "hk", "hm", "id", "in", "kh", "ki", "kr", "la", "lk", "mh",
    "mm", "mn", "mo", "mp", "mv", "my", "nc", "nf", "np", "nr", "nu",
    "nz", "pf", "pg", "ph", "pk", "pn", "pw", "sb", "sg", "th", "tk",
    "tl", "to", "tv", "tw", "us", "vn", "vu", "wf", "ws"};

// Default enabled locales for PageActionMenu. Locales are
// matching Bluebird in chrome/browser/glic/public/glic_enabling.cc.
// All locales have been converted to lower case with '-' where it's
// applicable.
constexpr std::array<std::string_view, 51> kDefaultEnabledLocales = {
    "af", "am",     "bg",    "bn",    "ca",    "cs",    "da",    "de", "el",
    "es", "es-419", "et",    "fi",    "fil",   "fr",    "gu",    "hi", "hr",
    "hu", "id",     "it",    "ja",    "kn",    "ko",    "lt",    "lv", "ml",
    "mr", "ms",     "nl",    "no",    "pl",    "pt-br", "pt-pt", "ro", "ru",
    "sk", "sl",     "sr",    "sv",    "sw",    "ta",    "te",    "th", "tr",
    "uk", "vi",     "zh-cn", "zh-tw", "en-gb", "en-us"};

const char kPageActionMenuDirectEntryPointParam[] =
    "PageActionMenuDirectEntryPoint";

bool IsPageActionMenuEnabled() {
  // Checks the killswtich, allowing to disable the feature for any user
  // including those in launched locales.
  if (base::FeatureList::IsEnabled(kGeminiKillSwitch)) {
    return false;
  }

  // Checks if enabled for country and locale.
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  std::string country =
      variations_service
          ? base::ToLowerASCII(variations_service->GetStoredPermanentCountry())
          : "";

  ApplicationLocaleStorage* locale_storage =
      GetApplicationContext()->GetApplicationLocaleStorage();
  std::string locale =
      locale_storage ? base::ToLowerASCII(locale_storage->Get()) : "";

  std::string normalized_locale;
  base::ReplaceChars(locale, "_", "-", &normalized_locale);

  bool is_launched_country =
      std::ranges::contains(kDefaultEnabledCountries, country);
  bool is_launched_locale =
      std::ranges::contains(kDefaultEnabledLocales, normalized_locale);

  if (is_launched_country && is_launched_locale) {
    return true;
  }

  // Allows for the feature to be enabled through Finch or chrome://flags.
  return base::FeatureList::IsEnabled(kPageActionMenu);
}

BASE_FEATURE(kPageActionMenuAuthFlow, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPageActionMenuAuthFlowEnabled() {
  return IsPageActionMenuEnabled() &&

         base::FeatureList::IsEnabled(kPageActionMenuAuthFlow);
}

BASE_FEATURE(kProactiveSuggestionsFramework, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsProactiveSuggestionsFrameworkEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kProactiveSuggestionsFramework);
}

const char kProactiveSuggestionsFrameworkPopupBlocker[] = "PopupBlocker";

bool IsProactiveSuggestionsFrameworkPopupBlockerEnabled() {
  if (!IsProactiveSuggestionsFrameworkEnabled()) {
    return false;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kProactiveSuggestionsFramework,
      kProactiveSuggestionsFrameworkPopupBlocker, false);
}

BASE_FEATURE(kAskGeminiChipIgnoreCriteria, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAskGeminiChipIgnoreCriteriaEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kAskGeminiChipIgnoreCriteria);
}

bool IsDirectBWGEntryPoint() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kPageActionMenu, kPageActionMenuDirectEntryPointParam, false);
}

const char kExplainGeminiEditMenuParams[] = "PositionForExplainGeminiEditMenu";

BASE_FEATURE_PARAM(int,
                   kExplainGeminiEditMenuFeatureParam,
                   &kExplainGeminiEditMenu,
                   kExplainGeminiEditMenuParams,
                   2);

PositionForExplainGeminiEditMenu ExplainGeminiEditMenuPosition() {
  if (!IsPageActionMenuEnabled()) {
    return PositionForExplainGeminiEditMenu::kDisabled;
  }

  int param = kExplainGeminiEditMenuFeatureParam.Get();
  if (param == 1) {
    return PositionForExplainGeminiEditMenu::kAfterEdit;
  }
  if (param == 2) {
    return PositionForExplainGeminiEditMenu::kAfterSearch;
  }
  if (param == 3) {
    return PositionForExplainGeminiEditMenu::kAdjacent;
  }
  return PositionForExplainGeminiEditMenu::kDisabled;
}

BASE_FEATURE(kExplainGeminiEditMenu, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGeminiPreciseLocation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiPreciseLocationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiPreciseLocation);
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

BASE_FEATURE(kPersistTabContext, base::FEATURE_ENABLED_BY_DEFAULT);

const char kPersistTabContextStorageParam[] = "storage_implementation";
const char kPersistTabContextExtractionTimingParam[] = "extraction_timing";
const char kPersistTabContextDataParam[] = "data_extracted";

BASE_FEATURE_PARAM(int,
                   kPersistTabContextStorageFeatureParam,
                   &kPersistTabContext,
                   kPersistTabContextStorageParam,
                   static_cast<int>(PersistTabStorageType::kSQLite));

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
constexpr int kPersistTabContextDefaultTTL = 7;

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
  if (!IsPageActionMenuEnabled()) {
    return false;
  }

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

  return base::FeatureList::IsEnabled(kZeroStateSuggestions);
}

BASE_FEATURE(kZeroStateSuggestionsWCGD, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsZeroStateSuggestionsWCGDEnabled() {
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

  return base::FeatureList::IsEnabled(kZeroStateSuggestionsWCGD);
}

BASE_FEATURE(kZeroStateSuggestionsCentralization,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsZeroStateSuggestionsCentralizationEnabled() {
  return base::FeatureList::IsEnabled(kZeroStateSuggestionsCentralization);
}

BASE_FEATURE(kPageContextExtractorRefactored, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPageContextExtractorRefactoredEnabled() {
  return base::FeatureList::IsEnabled(kPageContextExtractorRefactored);
}

BASE_FEATURE(kGeminiUpdatedEligibility, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiUpdatedEligibilityEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiUpdatedEligibility);
}

BASE_FEATURE(kGeminiUpdatedConsent, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiUpdatedConsentEnabled() {
  return base::FeatureList::IsEnabled(kGeminiUpdatedConsent);
}

BASE_FEATURE(kGeminiImageRemixTool, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiImageRemixToolEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiImageRemixTool);
}

const char kGeminiImageRemixToolShowFRERow[] = "ShowFRERow";

bool IsGeminiImageRemixToolShowFRERowEnabled() {
  if (!IsGeminiImageRemixToolEnabled()) {
    return false;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kGeminiImageRemixTool, kGeminiImageRemixToolShowFRERow, true);
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

const char kGeminiImageRemixToolRemovePageContext[] = "RemovePageContext";

bool IsGeminiImageRemixToolRemovePageContextEnabled() {
  if (!IsGeminiImageRemixToolEnabled()) {
    return false;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kGeminiImageRemixTool, kGeminiImageRemixToolRemovePageContext, true);
}

BASE_FEATURE(kGeminiEligibilityAblation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiEligibilityAblationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiEligibilityAblation);
}

BASE_FEATURE(kGeminiLive, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiLiveEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiLive);
}

BASE_FEATURE(kGeminiLiveDormantReasons, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiLiveDormantReasonsEnabled() {
  if (!IsGeminiLiveEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiLiveDormantReasons);
}

BASE_FEATURE(kGeminiChatPersistence, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiChatPersistenceEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiChatPersistence);
}

BASE_FEATURE(kGeminiConfigParams, base::FEATURE_ENABLED_BY_DEFAULT);

const char kGeminiResponseReadyInterval[] = "GeminiResponseReadyInterval";
constexpr double kGeminiResponseReadyIntervalDefault = 7.0;

double GetGeminiResponseReadyInterval() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kGeminiConfigParams, kGeminiResponseReadyInterval,
      kGeminiResponseReadyIntervalDefault);
}

const char kGeminiSessionValidityDuration[] = "GeminiSessionValidityDuration";
constexpr int kGeminiSessionValidityDurationDefault = 30;

base::TimeDelta GetGeminiSessionValidityDuration() {
  return base::Minutes(base::GetFieldTrialParamByFeatureAsInt(
      kGeminiConfigParams, kGeminiSessionValidityDuration,
      kGeminiSessionValidityDurationDefault));
}


BASE_FEATURE(kActorTools, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kDisabledTools,
                   &kActorTools,
                   "DisabledTools",
                   "");

const char kActorToolsPageStabilityParam[] = "PageStabilityEnabled";

BASE_FEATURE_PARAM(bool,
                   kPageStabilityEnabled,
                   &kActorTools,
                   kActorToolsPageStabilityParam,
                   false);

// This mirrors the desktop equivalent at:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/common/chrome_features.cc;l=317;drc=b8690fd8da7ae2367f4060dbb4bb35a43adcebed
BASE_FEATURE_PARAM(base::TimeDelta,
                   kObservationDelayTimeout,
                   &kActorTools,
                   base::Seconds(10));

// These mirrors the Desktop equivalents at:
// https://source.chromium.org/chromium/chromium/src/+/main:components/page_content_annotations/core/page_content_annotations_features.cc;l=152-157;drc=17a4f936106fad40f48b69820687df64ff45b77c
BASE_FEATURE_PARAM(base::TimeDelta,
                   kActorPageStabilityTimeout,
                   &kActorTools,
                   base::Seconds(4));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kActorPageStabilityMinWait,
                   &kActorTools,
                   base::Seconds(1));

BASE_FEATURE_PARAM(int, kActorPageStabilityMutationCap, &kActorTools, 250);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kActorPageStabilityWindowDuration,
                   &kActorTools,
                   base::Milliseconds(4000));

bool IsActorEnabled() {
  return base::FeatureList::IsEnabled(kActorTools);
}

bool IsPageStabilityEnabled() {
  return kPageStabilityEnabled.Get();
}

base::TimeDelta GetActorObservationDelayTimeout() {
  // This CHECK is safe since this param is only accessed by the page stability
  // logic for the ActorTools feature.
  // TODO(crbug.com/498991756): remove when the feature is launched.
  CHECK(IsPageStabilityEnabled());
  return kObservationDelayTimeout.Get();
}

base::TimeDelta GetActorPageStabilityMinWait() {
  CHECK(IsPageStabilityEnabled());
  return kActorPageStabilityMinWait.Get();
}

base::TimeDelta GetActorPageStabilityTimeout() {
  CHECK(IsPageStabilityEnabled());
  return kActorPageStabilityTimeout.Get();
}

int GetActorPageStabilityMutationCap() {
  CHECK(IsPageStabilityEnabled());
  return kActorPageStabilityMutationCap.Get();
}

base::TimeDelta GetActorPageStabilityWindowDuration() {
  CHECK(IsPageStabilityEnabled());
  return kActorPageStabilityWindowDuration.Get();
}

bool IsToolDisabled(optimization_guide::proto::Action::ActionCase tool) {
  if (!IsActorEnabled()) {
    return true;
  }

  std::optional<std::string> tool_name = actor::ActorActionCaseToToolName(tool);
  if (!tool_name) {
    // Don't support tools that aren't in the proto.
    return true;
  }

  std::string disabled_tools = kDisabledTools.Get();
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

BASE_FEATURE(kPageActionMenuIcon, base::FEATURE_ENABLED_BY_DEFAULT);

const char kPageActionMenuIconParams[] = "PageActionMenuIconParams";

PageActionMenuIconVariations GetPageActionMenuIcon() {
  if (@available(iOS 26, *)) {
    return PageActionMenuIconVariations::kSparkles2;
  } else {
    return PageActionMenuIconVariations::kDefault;
  }
}

BASE_FEATURE(kGeminiBackendMigration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiBackendMigrationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiBackendMigration);
}

BASE_FEATURE(kGeminiActor, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiActorEnabled() {
  if (!IsPageActionMenuEnabled() || !IsActorEnabled() ||
      !IsGeminiClientMigrationEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiActor);
}

BASE_FEATURE(kGeminiRichAPCExtraction, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiRichAPCExtractionEnabled() {
  if (!IsPageActionMenuEnabled() ||
      !IsPageContextExtractorRefactoredEnabled()) {
    return false;
  }

  return base::FeatureList::IsEnabled(kGeminiRichAPCExtraction);
}

BASE_FEATURE(kGeminiUnaryMigration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiUnaryMigrationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiUnaryMigration);
}

BASE_FEATURE(kGeminiBinaryMigration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiBinaryMigrationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiBinaryMigration);
}

BASE_FEATURE(kPersistTabContextRichExtraction,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPersistTabContextRichExtractionEnabled() {
  return base::FeatureList::IsEnabled(kPersistTabContextRichExtraction);
}

BASE_FEATURE(kPageContextIPCOptimization, base::FEATURE_ENABLED_BY_DEFAULT);

const char kPageContextIPCOptimizationActionableParam[] = "enable_actionable";

BASE_FEATURE_PARAM(bool,
                   kPageContextIPCOptimizationActionable,
                   &kPageContextIPCOptimization,
                   kPageContextIPCOptimizationActionableParam,
                   false);

bool IsPageContextIPCOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kPageContextIPCOptimization);
}

bool IsPageContextIPCOptimizationActionableEnabled() {
  return IsPageContextIPCOptimizationEnabled() &&
         kPageContextIPCOptimizationActionable.Get();
}

BASE_FEATURE(kPageContextPdf, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPageContextPDFEnabled() {
  return base::FeatureList::IsEnabled(kPageContextPdf);
}

BASE_FEATURE(kGeminiClientMigration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiClientMigrationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiClientMigration);
}

BASE_FEATURE(kGeminiMultiTabContext, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiMultiTabContextEnabled() {
  if (!IsPageActionMenuEnabled() || !IsGeminiScreenContextMigrationEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiMultiTabContext);
}

BASE_FEATURE(kGeminiScreenContextMigration, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiScreenContextMigrationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiScreenContextMigration);
}

BASE_FEATURE(kAppStoreInAppEvents, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAppStoreInAppEventsEnabled() {
  return IsPageActionMenuEnabled() &&
         base::FeatureList::IsEnabled(kAppStoreInAppEvents);
}

BASE_FEATURE(kGeneralizedGeminiEntryFlow, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeneralizedGeminiEntryFlowEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeneralizedGeminiEntryFlow);
}

BASE_FEATURE(kGeminiLuminous, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiLuminousEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiLuminous);
}

BASE_FEATURE(kAppSwitcherAISummarization, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppSwitcherAISummarizationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kAppSwitcherAISummarization);
}

BASE_FEATURE(kGeminiContextualSuggestionsCues,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiContextualSuggestionsCuesEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiContextualSuggestionsCues);
}

#pragma mark - Debugging Features

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
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return BWGPromoConsentVariationsParam() ==
         BWGPromoConsentVariations::kForceFRE;
}

bool ShouldSkipBWGPromoNewUserDelay() {
  return BWGPromoConsentVariationsParam() ==
         BWGPromoConsentVariations::kSkipNewUserDelay;
}

BASE_FEATURE(kBWGPromoConsent, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kActorServiceLogging, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsActorServiceLoggingEnabled() {
  return base::FeatureList::IsEnabled(kActorServiceLogging);
}

BASE_FEATURE(kIOSGeminiBottomSheetMigration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSGeminiBottomSheetMigrationEnabled() {
  return IsAssistantContainerEnabled() &&
         base::FeatureList::IsEnabled(kIOSGeminiBottomSheetMigration);
}

BASE_FEATURE(kGeminiQuizzes, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiQuizzesEnabled() {
  return base::FeatureList::IsEnabled(kGeminiQuizzes);
}

BASE_FEATURE(kGeminiFRERefactor, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiFRERefactorEnabled() {
  return base::FeatureList::IsEnabled(kGeminiFRERefactor);
}

BASE_FEATURE(kGeminiCoordinatorTeardownFix, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiCoordinatorTeardownFixEnabled() {
  return base::FeatureList::IsEnabled(kGeminiCoordinatorTeardownFix);
}

BASE_FEATURE(kGeminiVisualRichFRE, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiVisualRichFREEnabled() {
  return base::FeatureList::IsEnabled(kGeminiVisualRichFRE);
}
