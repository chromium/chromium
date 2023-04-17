// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_RETENTION_FIELD_TRIAL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_RETENTION_FIELD_TRIAL_CONSTANTS_H_

#import "components/variations/variations_associated_data.h"

namespace field_trial_constants {

// Version suffix for group names.
const char kNewTabPageRetentionVersionSuffixImprovedTiles[] = "_20230308";
const char kNewTabPageRetentionVersionSuffixTileAblation[] = "_20230504";

// Variation IDs for the improved popular sites default suggestions experiment
// arms.
const variations::VariationID
    kPopularSitesImprovedSuggestionsWithAppsEnabledID = 3361862;
const variations::VariationID
    kPopularSitesImprovedSuggestionsWithoutAppsEnabledID = 3361863;
const variations::VariationID kPopularSitesImprovedSuggestionsControlID =
    3361864;

// Variation IDs for the tile ablation experiment.
const variations::VariationID kTileAblationHideAllID = 3361865;
const variations::VariationID kTileAblationHideOnlyMVTID = 3361866;
const variations::VariationID kTileAblationControlID = 3361867;

// Group names for the improved popular sites default suggestions experiment.
const char kIOSPopularSitesImprovedSuggestionsWithAppsEnabledGroup[] =
    "IOSPopularSitesImprovedSuggestions_WithAppsEnabled-V1";
const char kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledGroup[] =
    "IOSPopularSitesImprovedSuggestions_WithoutAppsEnabled-V1";
const char kIOSPopularSitesImprovedSuggestionsControlGroup[] =
    "IOSPopularSitesImprovedSuggestions_Control-V1";

// Group names for the tile ablation experiment.
const char kTileAblationHideAllGroup[] = "TileAblation.HideAll";
const char kTileAblationHideOnlyMVTGroup[] = "TileAblation_HideOnlyMVT";
const char kTileAblationControlGroup[] = "TileAblation_Control";

const char kNewTabPageRetentionDefaultGroup[] = "Default";

// Group weights for the improved popular sites experiments.
const int kIOSPopularSitesImprovedSuggestionsStableWeight = 5;
const int kIOSPopularSitesImprovedSuggestionsPrestableWeight = 16;

// Group weights for the tile ablation experiments.
const int kTileAblationStableWeight = 5;
const int kTileAblationPrestableWeight = 16;

}  // namespace field_trial_constants

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_RETENTION_FIELD_TRIAL_CONSTANTS_H_
