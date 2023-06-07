// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FIELD_TRIAL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FIELD_TRIAL_CONSTANTS_H_

#import "components/variations/variations_associated_data.h"

namespace field_trial_constants {

// Version suffix for group names.
const char kNewTabPageFieldTrialVersionSuffixTileAblation[] = "_M114";

// Variation IDs for the tile ablation experiment.
const variations::VariationID kTileAblationHideAllID = 3365413;
const variations::VariationID kTileAblationHideOnlyMVTID = 3365414;
const variations::VariationID kTileAblationControlID = 3365415;

// Group names for the tile ablation experiment.
const char kTileAblationHideAllGroup[] = "TileAblation_HideAll";
const char kTileAblationHideOnlyMVTGroup[] = "TileAblation_HideOnlyMVT";
const char kTileAblationControlGroup[] = "TileAblation_Control";

const char kNewTabPageFieldTrialDefaultGroup[] = "Default";

// Group weights for the tile ablation experiments.
const int kTileAblationStableWeight = 5;
const int kTileAblationPrestableWeight = 16;

}  // namespace field_trial_constants

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FIELD_TRIAL_CONSTANTS_H_
