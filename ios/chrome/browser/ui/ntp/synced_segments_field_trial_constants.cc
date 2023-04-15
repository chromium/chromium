// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ntp/synced_segments_field_trial_constants.h"

namespace synced_segments_field_trial_constants {

const char kIOSSyncedSegmentsFieldTrialName[] = "IOSSyncedSegments";

const variations::VariationID kIOSSyncedSegmentsEnabledID = 4976469;
const variations::VariationID kIOSSyncedSegmentsControlID = 4976470;

extern const char kIOSSyncedSegmentsEnabledGroup[] =
    "IOSSyncedSegmentsEnabled-V1";
extern const char kIOSSyncedSegmentsControlGroup[] =
    "IOSSyncedSegmentsControl-V1";
extern const char kIOSSyncedSegmentsDefaultGroup[] = "Default";

}  // namespace synced_segments_field_trial_constants
