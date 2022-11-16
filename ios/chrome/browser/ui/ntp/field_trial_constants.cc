// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ntp/field_trial_constants.h"

namespace field_trial_constants {

const char kIOSPopularSitesImprovedSuggestionsFieldTrialName[] =
    "IOSPopularSitesImprovedSuggestions";

const variations::VariationID
    kIOSPopularSitesImprovedSuggestionsWithAppsEnabledID = 3357603;
const variations::VariationID
    kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledID = 3357604;
const variations::VariationID kIOSPopularSitesImprovedSuggestionsControlID =
    3357605;

const char kIOSPopularSitesImprovedSuggestionsWithAppsEnabledGroup[] =
    "IOSPopularSitesImprovedSuggestionsWithAppsEnabled-V1";
const char kIOSPopularSitesImprovedSuggestionsWithoutAppsEnabledGroup[] =
    "IOSPopularSitesImprovedSuggestionsWithoutAppsEnabled-V1";
const char kIOSPopularSitesImprovedSuggestionsControlGroup[] = "Control-V1";
const char kIOSPopularSitesDefaultSuggestionsGroup[] = "Default";

}  // namespace field_trial_constants
