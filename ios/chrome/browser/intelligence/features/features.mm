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
#import "base/notreached.h"
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

// Launched for kDefaultEnabledCountries and kDefaultEnabledLocales, but
// remains disabled by default for other locales and countries.
BASE_FEATURE(kPageActionMenu, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGeminiKillSwitch, base::FEATURE_DISABLED_BY_DEFAULT);

// Default enabled countries for PageActionMenu in Gemini for Chrome
// Expansion V3.
constexpr std::array<std::string_view, 171> kDefaultEnabledCountries = {
    "ae", "ag", "am", "ao", "aq", "ar", "as", "au", "az", "ba", "bb", "bd",
    "bf", "bh", "bi", "bj", "bn", "bo", "br", "bs", "bt", "bw", "bz", "ca",
    "cc", "cd", "cf", "cg", "ci", "ck", "cl", "cm", "co", "cr", "cv", "cx",
    "dj", "dm", "do", "dz", "ec", "eg", "eh", "er", "et", "fj", "fm", "ga",
    "gd", "ge", "gh", "gm", "gn", "gq", "gt", "gu", "gw", "gy", "hk", "hm",
    "hn", "ht", "id", "il", "in", "iq", "jm", "jo", "ke", "kg", "kh", "ki",
    "km", "kn", "kr", "kw", "kz", "la", "lb", "lc", "lk", "lr", "ls", "ly",
    "ma", "md", "me", "mg", "mh", "mk", "ml", "mm", "mn", "mo", "mp", "mr",
    "mu", "mv", "mw", "mx", "my", "mz", "na", "nc", "ne", "nf", "ng", "ni",
    "np", "nr", "nu", "nz", "om", "pa", "pe", "pf", "pg", "ph", "pk", "pn",
    "pr", "ps", "pw", "py", "qa", "rs", "rw", "sa", "sb", "sc", "sd", "sg",
    "sl", "sn", "so", "sr", "ss", "st", "sv", "sz", "td", "tg", "th", "tj",
    "tk", "tl", "tm", "tn", "to", "tt", "tv", "tw", "tz", "ua", "ug", "um",
    "us", "uy", "uz", "vc", "ve", "vi", "vn", "vu", "wf", "ws", "xk", "ye",
    "za", "zm", "zw"};

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
      base::EqualsCaseInsensitiveASCII(
          variations_service->GetStoredPermanentCountry(), "us");

  ApplicationLocaleStorage* locale_storage =
      GetApplicationContext()->GetApplicationLocaleStorage();
  bool is_launched_locale =
      locale_storage &&
      base::EqualsCaseInsensitiveASCII(locale_storage->Get(), "en-us");

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
      base::EqualsCaseInsensitiveASCII(
          variations_service->GetStoredPermanentCountry(), "us");

  ApplicationLocaleStorage* locale_storage =
      GetApplicationContext()->GetApplicationLocaleStorage();
  bool is_launched_locale =
      locale_storage &&
      base::EqualsCaseInsensitiveASCII(locale_storage->Get(), "en-us");

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

BASE_FEATURE(kGeminiUpdatedEligibility, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiUpdatedEligibilityEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiUpdatedEligibility);
}

BASE_FEATURE(kGeminiUpdatedConsent, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiUpdatedConsentEnabled() {
  return base::FeatureList::IsEnabled(kGeminiUpdatedConsent);
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

BASE_FEATURE_PARAM(int, kActorPageStabilityMutationCap, &kActorTools, 10);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kActorPageStabilityWindowDuration,
                   &kActorTools,
                   base::Milliseconds(1000));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kActorPageStabilityLcpDelay,
                   &kActorTools,
                   base::Seconds(1));
// LINT.IfChange(kActorPageStabilityAutofillPredictionsTimeout)
BASE_FEATURE_PARAM(base::TimeDelta,
                   kActorPageStabilityAutofillPredictionsTimeout,
                   &kActorTools,
                   base::Seconds(1));
// LINT.ThenChange(//chrome/common/chrome_features.cc:kActorObservationDelayAutofillPredictionsTimeout)

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

base::TimeDelta GetActorPageStabilityLcpDelay() {
  CHECK(IsPageStabilityEnabled());
  return kActorPageStabilityLcpDelay.Get();
}

base::TimeDelta GetActorPageStabilityAutofillPredictionsTimeout() {
  CHECK(IsPageStabilityEnabled());
  return kActorPageStabilityAutofillPredictionsTimeout.Get();
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
      base::EqualsCaseInsensitiveASCII(
          variations_service->GetStoredPermanentCountry(), "us");

  ApplicationLocaleStorage* locale_storage =
      GetApplicationContext()->GetApplicationLocaleStorage();
  bool is_launched_locale =
      locale_storage &&
      base::EqualsCaseInsensitiveASCII(locale_storage->Get(), "en-us");

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

BASE_FEATURE(kGeminiAureus, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiAureusEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiAureus);
}

BASE_FEATURE(kGeminiActor, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiActorEnabled() {
  if (!IsPageActionMenuEnabled() || !IsActorEnabled() ||
      !IsGeminiClientMigrationEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiActor);
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

BASE_FEATURE(kGeminiClientMigration, base::FEATURE_ENABLED_BY_DEFAULT);

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

const char kGeminiContextualSuggestionsCuesOnDeviceClassifierParam[] =
    "enable_on_device_classifier";

BASE_FEATURE_PARAM(bool,
                   kGeminiContextualSuggestionsCuesOnDeviceClassifier,
                   &kGeminiContextualSuggestionsCues,
                   kGeminiContextualSuggestionsCuesOnDeviceClassifierParam,
                   false);

const char kGeminiContextualSuggestionsCuesAllowGpuExecutionParam[] =
    "allow_gpu_execution";

BASE_FEATURE_PARAM(bool,
                   kGeminiContextualSuggestionsCuesAllowGpuExecution,
                   &kGeminiContextualSuggestionsCues,
                   kGeminiContextualSuggestionsCuesAllowGpuExecutionParam,
                   false);

const char kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam[] =
    "use_title_and_url_only";

BASE_FEATURE_PARAM(bool,
                   kGeminiContextualSuggestionsCuesTitleAndUrlOnly,
                   &kGeminiContextualSuggestionsCues,
                   kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam,
                   true);

bool IsGeminiContextualSuggestionsCuesEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiContextualSuggestionsCues);
}

bool IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled() {
  return IsGeminiContextualSuggestionsCuesEnabled() &&
         kGeminiContextualSuggestionsCuesOnDeviceClassifier.Get();
}

bool IsGeminiContextualSuggestionsCuesAllowGpuExecutionEnabled() {
  return IsGeminiContextualSuggestionsCuesEnabled() &&
         kGeminiContextualSuggestionsCuesAllowGpuExecution.Get();
}

bool IsGeminiContextualSuggestionsCuesTitleAndUrlOnlyEnabled() {
  return kGeminiContextualSuggestionsCuesTitleAndUrlOnly.Get();
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

const char kGeminiFREExperimentParam[] = "variant";
const char kGeminiFREExperimentParamVisualRich[] = "visual-rich";
const char kGeminiFREExperimentParamLightweightConvenience[] =
    "lightweight-convenience";
const char kGeminiFREExperimentParamLightweightPageSharing[] =
    "lightweight-page-sharing";
const char kGeminiFREExperimentParamLightweightDiverse[] =
    "lightweight-diverse";

BASE_FEATURE(kGeminiFREExperiment, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiFREExperimentEnabled() {
  return base::FeatureList::IsEnabled(kGeminiFREExperiment);
}

bool IsGeminiVisualRichFREEnabled() {
  if (!base::FeatureList::IsEnabled(kGeminiFREExperiment)) {
    return false;
  }
  std::string variant = base::GetFieldTrialParamValueByFeature(
      kGeminiFREExperiment, kGeminiFREExperimentParam);
  return variant.empty() || variant == kGeminiFREExperimentParamVisualRich;
}

bool IsGeminiLightweightFREEnabled() {
  if (!base::FeatureList::IsEnabled(kGeminiFREExperiment)) {
    return false;
  }
  std::string variant = base::GetFieldTrialParamValueByFeature(
      kGeminiFREExperiment, kGeminiFREExperimentParam);
  return variant == kGeminiFREExperimentParamLightweightConvenience ||
         variant == kGeminiFREExperimentParamLightweightPageSharing ||
         variant == kGeminiFREExperimentParamLightweightDiverse;
}

GeminiLightweightFREVariant GetGeminiLightweightFREVariant() {
  std::string variant = base::GetFieldTrialParamValueByFeature(
      kGeminiFREExperiment, kGeminiFREExperimentParam);
  if (variant == kGeminiFREExperimentParamLightweightPageSharing) {
    return GeminiLightweightFREVariant::kPageSharing;
  }
  if (variant == kGeminiFREExperimentParamLightweightDiverse) {
    return GeminiLightweightFREVariant::kDiverse;
  }
  if (variant == kGeminiFREExperimentParamLightweightConvenience) {
    return GeminiLightweightFREVariant::kConvenience;
  }
  NOTREACHED();
}

// Meant for experiments only.
BASE_FEATURE(kGeminiExperimentalGuidedOnboarding,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kGeminiExperimentalGuidedOnboardingForceParam[] =
    "force_guided_onboarding";

BASE_FEATURE_PARAM(bool,
                   kGeminiExperimentalGuidedOnboardingForce,
                   &kGeminiExperimentalGuidedOnboarding,
                   kGeminiExperimentalGuidedOnboardingForceParam,
                   false);

bool IsGeminiExperimentalGuidedOnboardingEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiExperimentalGuidedOnboarding);
}

bool ShouldForceGeminiExperimentalGuidedOnboarding() {
  if (!IsGeminiExperimentalGuidedOnboardingEnabled()) {
    return false;
  }
  return kGeminiExperimentalGuidedOnboardingForce.Get();
}

BASE_FEATURE(kPageContextScreenshotSensitivePaymentRedaction,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPageContextScreenshotSensitivePaymentRedactionEnabled() {
  return base::FeatureList::IsEnabled(
      kPageContextScreenshotSensitivePaymentRedaction);
}

BASE_FEATURE(kPageContextAutofillCreditCardRedactions,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPageContextAutofillCreditCardRedactionsEnabled() {
  return base::FeatureList::IsEnabled(kPageContextAutofillCreditCardRedactions);
}

BASE_FEATURE(kPageContextAutofillOtpRedactions,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPageContextAutofillOtpRedactionsEnabled() {
  return base::FeatureList::IsEnabled(kPageContextAutofillOtpRedactions);
}

BASE_FEATURE(kPageContextScreenshotPasswordRedaction,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPageContextScreenshotPasswordRedactionEnabled() {
  return base::FeatureList::IsEnabled(kPageContextScreenshotPasswordRedaction);
}

BASE_FEATURE(kGeminiInsightsChipAblation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiInsightsChipAblationEnabled() {
  return base::FeatureList::IsEnabled(kGeminiInsightsChipAblation);
}
