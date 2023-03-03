// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_RETENTION_FIELD_TRIAL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_RETENTION_FIELD_TRIAL_CONSTANTS_H_

#import "components/variations/variations_associated_data.h"

namespace field_trial_constants {

// Version suffix for group names.
const std::string kNewTabPageRetentionVersionSuffix = "_20230302";

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
const std::string kIOSPopularSitesImprovedSuggestionsWithAppsEnabledGroup =
    "IOSPopularSitesImprovedSuggestions_WithAppsEnabled-V1" +
    kNewTabPageRetentionVersionSuffix;
const std::string kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledGroup =
    "IOSPopularSitesImprovedSuggestions_WithoutAppsEnabled-V1" +
    kNewTabPageRetentionVersionSuffix;
const std::string kIOSPopularSitesImprovedSuggestionsControlGroup =
    "IOSPopularSitesImprovedSuggestions_Control-V1" +
    kNewTabPageRetentionVersionSuffix;

// Group names for the tile ablation experiment.
const std::string kTileAblationHideAllGroup =
    "TileAblation.HideAll" + kNewTabPageRetentionVersionSuffix;
const std::string kTileAblationHideOnlyMVTGroup =
    "TileAblation_HideOnlyMVT" + kNewTabPageRetentionVersionSuffix;
const std::string kTileAblationControlGroup =
    "TileAblation_Control" + kNewTabPageRetentionVersionSuffix;

const std::string kNewTabPageRetentionDefaultGroup = "Default";

// Group weights for the improved popular sites experiments.
const int kIOSPopularSitesImprovedSuggestionsStableWeight = 1;
const int kIOSPopularSitesImprovedSuggestionsPrestableWeight = 16;

// Group weights for the tile ablation experiments.
const int kTileAblationStableWeight = 0;
const int kTileAblationPrestableWeight = 16;

}  // namespace field_trial_constants

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_RETENTION_FIELD_TRIAL_CONSTANTS_H_
