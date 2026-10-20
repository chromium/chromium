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
#import "ios/chrome/app/background_task/features.h"
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
// Expansion V4.
constexpr std::array<std::string_view, 186> kDefaultEnabledCountries = {
    // Phase 1
    "us", "ca", "nz", "in",

    // Phase 2
    "as", "au", "bd", "bn", "bt", "cc", "ck", "cx", "fj", "fm", "gu", "hk",
    "hm", "id", "kh", "ki", "kr", "la", "lk", "mh", "mm", "mn", "mo", "mp",
    "mv", "my", "nc", "nf", "np", "nr", "nu", "pf", "pg", "ph", "pk", "pn",
    "pw", "sb", "sg", "th", "tk", "tl", "to", "tv", "tw", "vn", "vu", "wf",
    "ws",

    // Phase 3
    "ag", "ar", "bb", "bo", "br", "bs", "bz", "cl", "co", "cr", "dm", "do",
    "ec", "gd", "gt", "gy", "hn", "ht", "jm", "kn", "lc", "mx", "ni", "pa",
    "pe", "py", "sr", "sv", "tt", "uy", "vc", "ve", "ae", "am", "ao", "aq",
    "az", "ba", "bf", "bh", "bi", "bj", "bw", "cd", "cf", "cg", "ci", "cm",
    "cv", "dj", "dz", "eg", "eh", "er", "et", "ga", "ge", "gh", "gm", "gn",
    "gq", "gw", "il", "iq", "jo", "ke", "kg", "km", "kw", "kz", "lb", "lr",
    "ls", "ly", "ma", "md", "me", "mg", "mk", "ml", "mr", "mu", "mw", "mz",
    "na", "ne", "ng", "om", "pr", "ps", "qa", "rs", "rw", "sa", "sc", "sd",
    "sl", "sn", "so", "ss", "st", "sz", "td", "tg", "tj", "tm", "tn", "tz",
    "ua", "ug", "um", "uz", "vi", "xk", "ye", "za", "zm", "zw",

    // Phase 4
    "ai", "bm", "fk", "gb", "gg", "gi", "gs", "im", "io", "je", "ky", "ms",
    "sh", "tc", "vg"};

// Default enabled locales for PageActionMenu. Locales are
// matching desktop in chrome/browser/glic/public/glic_enabling.cc.
// All locales have been converted to lower case with '-' where it's
// applicable.
constexpr std::array<std::string_view, 51> kDefaultEnabledLocales = {
    "af", "am",     "bg",    "bn",    "ca",    "cs",    "da",    "de", "el",
    "es", "es-419", "et",    "fi",    "fil",   "fr",    "gu",    "hi", "hr",
    "hu", "id",     "it",    "ja",    "kn",    "ko",    "lt",    "lv", "ml",
    "mr", "ms",     "nl",    "no",    "pl",    "pt-br", "pt-pt", "ro", "ru",
    "sk", "sl",     "sr",    "sv",    "sw",    "ta",    "te",    "th", "tr",
    "uk", "vi",     "zh-cn", "zh-tw", "en-gb", "en-us"};

// Default enabled locales that are iOS-specific. These are not shared with the
// desktop list as iOS locales have slight differences.
constexpr std::array<std::string_view, 3> kDefaultEnabledLocalesiOSOnly = {
    "es-mx", "pt", "nb"};

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
      locale_storage ? base::ToLowerASCII(locale_storage->GetTag().tag_string())
                     : "";

  std::string normalized_locale;
  base::ReplaceChars(locale, "_", "-", &normalized_locale);

  bool is_launched_country =
      std::ranges::contains(kDefaultEnabledCountries, country);
  bool is_launched_locale =
      std::ranges::contains(kDefaultEnabledLocales, normalized_locale) ||
      std::ranges::contains(kDefaultEnabledLocalesiOSOnly, normalized_locale);

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

BASE_FEATURE(kZeroStateSuggestions, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsZeroStateSuggestionsEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
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
      locale_storage && base::EqualsCaseInsensitiveASCII(
                            locale_storage->GetTag().tag_string(), "en-us");

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

BASE_FEATURE(kGeminiEligibilityAblation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiEligibilityAblationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiEligibilityAblation);
}

BASE_FEATURE(kGeminiLive, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kGeminiLiveSkipIntro,
                   &kGeminiLive,
                   kGeminiLiveSkipIntroParam,
                   false);

BASE_FEATURE_PARAM(bool,
                   kGeminiLiveDisableIPH,
                   &kGeminiLive,
                   kGeminiLiveDisableIPHParam,
                   false);

bool IsGeminiLiveEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiLive);
}

bool ShouldSkipGeminiLiveIntro() {
  if (!IsGeminiLiveEnabled()) {
    return false;
  }
  return kGeminiLiveSkipIntro.Get();
}

bool IsGeminiLiveIPHDisabled() {
  if (!IsGeminiLiveEnabled()) {
    return false;
  }
  return kGeminiLiveDisableIPH.Get();
}

BASE_FEATURE(kGeminiLiveDormantReasons, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiLiveDormantReasonsEnabled() {
  if (!IsGeminiLiveEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiLiveDormantReasons);
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
                   true);

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
// LINT.ThenChange(//components/actor/core/actor_features.cc:kActorObservationDelayAutofillPredictionsTimeout)

const char kActorToolsDisableInputDuringActuationParam[] =
    "DisableInputDuringActuation";

// Enabled by default since suppressing input views during actuation is a bug
// fix; kept on a feature parameter so it can be used as a killswitch if needed.
BASE_FEATURE_PARAM(bool,
                   kActorDisableInputDuringActuation,
                   &kActorTools,
                   kActorToolsDisableInputDuringActuationParam,
                   true);

bool IsActorEnabled() {
  return base::FeatureList::IsEnabled(kActorTools);
}

bool IsPageStabilityEnabled() {
  return IsActorEnabled() && kPageStabilityEnabled.Get();
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

bool ActorDisablesInputDuringActuation() {
  return IsActorEnabled() && kActorDisableInputDuringActuation.Get();
}

BASE_FEATURE(kActorOriginGating, base::FEATURE_DISABLED_BY_DEFAULT);

const char kActorOriginGatingExplicitParam[] = "gate_explicit_navigation";
const char kActorOriginGatingImplicitParam[] = "gate_implicit_navigation";

BASE_FEATURE_PARAM(bool,
                   kActorOriginGatingExplicitFeatureParam,
                   &kActorOriginGating,
                   kActorOriginGatingExplicitParam,
                   true);  // Default to true when kActorOriginGating is enabled

BASE_FEATURE_PARAM(bool,
                   kActorOriginGatingImplicitFeatureParam,
                   &kActorOriginGating,
                   kActorOriginGatingImplicitParam,
                   true);  // Default to true when kActorOriginGating is enabled

bool IsActorOriginGatingForExplicitNavigationEnabled() {
  return base::FeatureList::IsEnabled(kActorOriginGating) &&
         kActorOriginGatingExplicitFeatureParam.Get();
}

bool IsActorOriginGatingForImplicitNavigationEnabled() {
  return base::FeatureList::IsEnabled(kActorOriginGating) &&
         kActorOriginGatingImplicitFeatureParam.Get();
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
      locale_storage && base::EqualsCaseInsensitiveASCII(
                            locale_storage->GetTag().tag_string(), "en-us");

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

BASE_FEATURE_PARAM(bool,
                   kGeminiAureusForegroundQuotaRefresh,
                   &kGeminiAureus,
                   kGeminiAureusForegroundQuotaRefreshParam,
                   false);

bool IsGeminiAureusEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiAureus);
}

bool IsGeminiAureusForegroundQuotaRefreshEnabled() {
  if (!IsGeminiAureusEnabled()) {
    return false;
  }
  return kGeminiAureusForegroundQuotaRefresh.Get();
}

BASE_FEATURE(kGeminiActor, base::FEATURE_DISABLED_BY_DEFAULT);

const char kGeminiActorBackgroundingParam[] = "backgrounding_enabled";

BASE_FEATURE_PARAM(bool,
                   kGeminiActorBackgrounding,
                   &kGeminiActor,
                   kGeminiActorBackgroundingParam,
                   true);

const char kGeminiActorBackgroundWebStateKeepAliveHeartbeatIntervalParam[] =
    "background_web_state_keep_alive_heartbeat_interval";

BASE_FEATURE_PARAM(
    base::TimeDelta,
    kGeminiActorBackgroundWebStateKeepAliveHeartbeatInterval,
    &kGeminiActor,
    kGeminiActorBackgroundWebStateKeepAliveHeartbeatIntervalParam,
    base::Milliseconds(400));

const char kGeminiActorBackgroundProgressHeartbeatIntervalParam[] =
    "background_progress_heartbeat_interval";

BASE_FEATURE_PARAM(base::TimeDelta,
                   kGeminiActorBackgroundProgressHeartbeatInterval,
                   &kGeminiActor,
                   kGeminiActorBackgroundProgressHeartbeatIntervalParam,
                   base::Seconds(3));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kActorTabObservationPageContextTimeout,
                   &kGeminiActor,
                   base::Seconds(4));

bool IsGeminiActorEnabled() {
  if (!IsPageActionMenuEnabled() || !IsActorEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiActor);
}

bool IsGeminiActorBackgroundingEnabled() {
  if (!IsGeminiActorEnabled() || !IsBackgroundContinuedProcessingEnabled()) {
    return false;
  }
  return kGeminiActorBackgrounding.Get();
}

base::TimeDelta GetGeminiActorBackgroundWebStateKeepAliveHeartbeatInterval() {
  return kGeminiActorBackgroundWebStateKeepAliveHeartbeatInterval.Get();
}

base::TimeDelta GetGeminiActorBackgroundProgressHeartbeatInterval() {
  return kGeminiActorBackgroundProgressHeartbeatInterval.Get();
}

base::TimeDelta GetActorTabObservationPageContextTimeout() {
  return kActorTabObservationPageContextTimeout.Get();
}

BASE_FEATURE(kGeminiUnaryMigration, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiUnaryMigrationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiUnaryMigration);
}

BASE_FEATURE(kGeminiBinaryMigration, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiBinaryMigrationEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiBinaryMigration);
}

BASE_FEATURE(kPageContextActionableOptimization,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPageContextActionableOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kPageContextActionableOptimization);
}

BASE_FEATURE(kPageContextPdf, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPageContextPDFEnabled() {
  return base::FeatureList::IsEnabled(kPageContextPdf);
}

BASE_FEATURE(kGeminiMultiTabContext, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiMultiTabContextEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiMultiTabContext);
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
  if (param == 6) {
    return BWGPromoConsentVariations::kForceLiveFRE;
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

bool ShouldForceGeminiLiveFRE() {
  return BWGPromoConsentVariationsParam() ==
         BWGPromoConsentVariations::kForceLiveFRE;
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

BASE_FEATURE(kGeminiQuizzes, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiQuizzesEnabled() {
  return base::FeatureList::IsEnabled(kGeminiQuizzes);
}

bool ShouldOpenGeminiQuizzesInNewTab() {
  return IsGeminiQuizzesEnabled() &&
         base::GetFieldTrialParamByFeatureAsBool(
             kGeminiQuizzes, kGeminiQuizzesOpenInNewTabParam, false);
}

BASE_FEATURE(kGeminiFRERefactor, base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kGeminiFREExperiment, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGeminiFREExperimentEnabled() {
  return base::FeatureList::IsEnabled(kGeminiFREExperiment);
}

bool IsGeminiVisualRichFREEnabled() {
  if (!base::FeatureList::IsEnabled(kGeminiFREExperiment)) {
    return false;
  }
  std::string variant = base::GetFieldTrialParamValueByFeature(
      kGeminiFREExperiment, kGeminiFREExperimentParam);
  return variant == kGeminiFREExperimentParamVisualRich;
}

bool IsGeminiLightweightFREEnabled() {
  if (!base::FeatureList::IsEnabled(kGeminiFREExperiment)) {
    return false;
  }
  std::string variant = base::GetFieldTrialParamValueByFeature(
      kGeminiFREExperiment, kGeminiFREExperimentParam);
  return variant.empty() ||
         variant == kGeminiFREExperimentParamLightweightConvenience ||
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
  return GeminiLightweightFREVariant::kConvenience;
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
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPageContextScreenshotPasswordRedactionEnabled() {
  return base::FeatureList::IsEnabled(kPageContextScreenshotPasswordRedaction);
}

BASE_FEATURE(kGeminiInsightsChipAblation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiInsightsChipAblationEnabled() {
  return base::FeatureList::IsEnabled(kGeminiInsightsChipAblation);
}

BASE_FEATURE(kGeminiHostExperimentIDs, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiHostExperimentIDsEnabled() {
  return base::FeatureList::IsEnabled(kGeminiHostExperimentIDs);
}
