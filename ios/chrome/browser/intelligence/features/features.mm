// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import "base/check.h"
#import "base/strings/string_util.h"
#import "base/time/time.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/service/variations_service_utils.h"
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

const char kPageActionMenuDirectEntryPointParam[] =
    "PageActionMenuDirectEntryPoint";

bool IsPageActionMenuEnabled() {
  if (IsDiamondPrototypeEnabled()) {
    return true;
  }

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

BASE_FEATURE(kGeminiCrossTab, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiCrossTabEnabled() {
  return IsPageActionMenuEnabled();
}

bool IsDirectBWGEntryPoint() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kPageActionMenu, kPageActionMenuDirectEntryPointParam, false);
}

const char kBWGSessionValidityDurationParam[] = "BWGSessionValidityDuration";

const base::TimeDelta BWGSessionValidityDuration() {
  return base::Minutes(base::GetFieldTrialParamByFeatureAsInt(
      kPageActionMenu, kBWGSessionValidityDurationParam, 30));
}

const char kBWGPromoConsentParams[] = "BWGPromoConsentVariations";

BWGPromoConsentVariations BWGPromoConsentVariationsParam() {
  int param = base::GetFieldTrialParamByFeatureAsInt(kBWGPromoConsent,
                                                     kBWGPromoConsentParams, 0);
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

PositionForExplainGeminiEditMenu ExplainGeminiEditMenuPosition() {
  int param = base::GetFieldTrialParamByFeatureAsInt(
      kExplainGeminiEditMenu, kExplainGeminiEditMenuParams, 0);
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

BASE_FEATURE(kPageContextAnchorTags, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPageContextAnchorTagsEnabled() {
  return IsPageActionMenuEnabled();
}

BASE_FEATURE(kGeminiForManagedAccounts, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiAvailableForManagedAccounts() {
  return IsPageActionMenuEnabled();
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

bool IsPersistTabContextEnabled() {
  if (IsSmartTabGroupingEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kPersistTabContext);
}

PersistTabStorageType GetPersistTabContextStorageType() {
  int param = base::GetFieldTrialParamByFeatureAsInt(
      kPersistTabContext, kPersistTabContextStorageParam,
      static_cast<int>(PersistTabStorageType::kFileSystem));
  if (param == static_cast<int>(PersistTabStorageType::kSQLite) &&
      base::FeatureList::IsEnabled(
          page_content_annotations::features::kPageContentCache)) {
    return PersistTabStorageType::kSQLite;
  }
  return PersistTabStorageType::kFileSystem;
}

PersistTabExtractionTiming GetPersistTabContextExtractionTiming() {
  int param = base::GetFieldTrialParamByFeatureAsInt(
      kPersistTabContext, kPersistTabContextExtractionTimingParam,
      static_cast<int>(PersistTabExtractionTiming::kOnWasHidden));
  if (param ==
      static_cast<int>(PersistTabExtractionTiming::kOnWasHiddenAndPageLoad)) {
    return PersistTabExtractionTiming::kOnWasHiddenAndPageLoad;
  }
  return PersistTabExtractionTiming::kOnWasHidden;
}

PersistTabDataExtracted GetPersistTabContextDataExtracted() {
  int param = base::GetFieldTrialParamByFeatureAsInt(
      kPersistTabContext, kPersistTabContextDataParam,
      static_cast<int>(PersistTabDataExtracted::kApcAndInnerText));
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

base::TimeDelta GetPersistedContextEffectiveTTL(PrefService* prefs) {
  int persist_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      kPersistTabContext, "ttl_days", kPersistTabContextDefaultTTL);
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

BASE_FEATURE(kGeminiFullChatHistory, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiFullChatHistoryEnabled() {
  return base::FeatureList::IsEnabled(kGeminiFullChatHistory);
}

BASE_FEATURE(kGeminiLoadingStateRedesign, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiLoadingStateRedesignEnabled() {
  return base::FeatureList::IsEnabled(kGeminiLoadingStateRedesign);
}

BASE_FEATURE(kGeminiLatencyImprovement, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiLatencyImprovementEnabled() {
  return base::FeatureList::IsEnabled(kGeminiLatencyImprovement);
}

BASE_FEATURE(kGeminiImmediateOverlay, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiImmediateOverlayEnabled() {
  return base::FeatureList::IsEnabled(kGeminiImmediateOverlay);
}

BASE_FEATURE(kGeminiOnboardingCards, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiOnboardingCardsEnabled() {
  return base::FeatureList::IsEnabled(kGeminiOnboardingCards);
}

BASE_FEATURE(kPageContextExtractorRefactored, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGeminiRefactoredFRE, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiRefactoredFREEnabled() {
  return base::FeatureList::IsEnabled(kGeminiRefactoredFRE);
}

BASE_FEATURE(kWebPageReportedImagesSheet, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsWebPageReportedImagesSheetEnabled() {
  return base::FeatureList::IsEnabled(kWebPageReportedImagesSheet);
}

BASE_FEATURE(kImageContextMenuGeminiEntryPoint,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsImageContextMenuGeminiEntryPointEnabled() {
  return base::FeatureList::IsEnabled(kImageContextMenuGeminiEntryPoint);
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
