// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_SYNCED_SEGMENTS_FIELD_TRIAL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_SYNCED_SEGMENTS_FIELD_TRIAL_CONSTANTS_H_

#include "components/variations/variations_associated_data.h"

namespace synced_segments_field_trial_constants {

// Name of the field trial to configure the synced segments experiment
// for most visited tiles.
extern const char kIOSSyncedSegmentsFieldTrialName[];

// Variation IDs for the synced segments experiment arms.
extern const variations::VariationID kIOSSyncedSegmentsEnabledID;
extern const variations::VariationID kIOSSyncedSegmentsControlID;

// Group names for the synced segments experiment.
extern const char kIOSSyncedSegmentsEnabledGroup[];
extern const char kIOSSyncedSegmentsControlGroup[];
extern const char kIOSSyncedSegmentsDefaultGroup[];

}  // namespace synced_segments_field_trial_constants

#endif  // IOS_CHROME_BROWSER_UI_NTP_SYNCED_SEGMENTS_FIELD_TRIAL_CONSTANTS_H_
