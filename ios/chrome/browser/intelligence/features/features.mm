// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import "base/check.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"

BASE_FEATURE(kEnhancedCalendar, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsEnhancedCalendarEnabled() {
  return base::FeatureList::IsEnabled(kEnhancedCalendar);
}

BASE_FEATURE(kPageActionMenu, base::FEATURE_DISABLED_BY_DEFAULT);

const char kPageActionMenuDirectEntryPointParam[] =
    "PageActionMenuDirectEntryPoint";

bool IsPageActionMenuEnabled() {
  if (IsDiamondPrototypeEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kPageActionMenu);
}

BASE_FEATURE(kProactiveSuggestionsFramework, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsProactiveSuggestionsFrameworkEnabled() {
  return IsPageActionMenuEnabled() &&
         base::FeatureList::IsEnabled(kProactiveSuggestionsFramework);
}

BASE_FEATURE(kAskGeminiChip, base::FEATURE_DISABLED_BY_DEFAULT);

const char kAskGeminiChipUseSnackbar[] = "AskGeminiChipUseSnackbar";

const char kAskGeminiChipIgnoreCriteria[] = "AskGeminiChipIgnoreCriteria";

const char kAskGeminiChipPrepopulateFloaty[] = "AskGeminiChipPrepopulateFloaty";

bool IsAskGeminiChipEnabled() {
  return base::FeatureList::IsEnabled(kAskGeminiChip);
}

bool IsAskGeminiChipIgnoreCriteria() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kAskGeminiChip, kAskGeminiChipIgnoreCriteria, false);
}

bool IsAskGeminiSnackbarEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kAskGeminiChip, kAskGeminiChipUseSnackbar, false);
}

bool IsAskGeminiChipPrepopulateFloatyEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kAskGeminiChip, kAskGeminiChipPrepopulateFloaty, false);
}

BASE_FEATURE(kGeminiCrossTab, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiCrossTabEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiCrossTab);
}

bool IsDirectBWGEntryPoint() {
  CHECK(IsPageActionMenuEnabled());
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
  return base::FeatureList::IsEnabled(kPageContextAnchorTags);
}

BASE_FEATURE(kGeminiForManagedAccounts, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiAvailableForManagedAccounts() {
  if (IsGeminiEligibilityAblationEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiForManagedAccounts);
}

BASE_FEATURE(kAIHubNewBadge, base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldDeleteGeminiConsentPref() {
  return base::FeatureList::IsEnabled(kDeleteGeminiConsentPref);
}

BASE_FEATURE(kDeleteGeminiConsentPref, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSmartTabGrouping, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSmartTabGroupingEnabled() {
  return base::FeatureList::IsEnabled(kSmartTabGrouping);
}

BASE_FEATURE(kPersistTabContext, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPersistTabContextEnabled() {
  if (IsSmartTabGroupingEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kPersistTabContext);
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
  if (!IsPageActionMenuEnabled()) {
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
