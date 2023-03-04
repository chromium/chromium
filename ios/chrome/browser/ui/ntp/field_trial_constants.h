// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FIELD_TRIAL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_FIELD_TRIAL_CONSTANTS_H_

#import "components/variations/variations_associated_data.h"

namespace field_trial_constants {

// Name of the field trial to configure improved default suggestions experiment
// for popular sites.
extern const char kIOSPopularSitesImprovedSuggestionsFieldTrialName[];

// Variation IDs for the improved popular sites default suggestions experiment
// arms.
extern const variations::VariationID
    kIOSPopularSitesImprovedSuggestionsWithAppsEnabledID;
extern const variations::VariationID
    kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledID;
extern const variations::VariationID
    kIOSPopularSitesImprovedSuggestionsControlID;

// Group names for the improved popular sites default suggestions experiment.
extern const char kIOSPopularSitesImprovedSuggestionsWithAppsEnabledGroup[];
extern const char kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledGroup[];
extern const char kIOSPopularSitesImprovedSuggestionsControlGroup[];
extern const char kIOSPopularSitesDefaultSuggestionsGroup[];

}  // namespace field_trial_constants

#endif  // IOS_CHROME_BROWSER_UI_NTP_FIELD_TRIAL_CONSTANTS_H_
