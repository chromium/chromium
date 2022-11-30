// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_TRACKED_CONFIGURATION_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_TRACKED_CONFIGURATION_H_

#include "services/preferences/public/mojom/preferences.mojom.h"

namespace prefs {

struct TrackedPreferenceMetadata {
  size_t reporting_id;
  const char* name;
  mojom::TrackedPreferenceMetadata::EnforcementLevel enforcement_level;
  mojom::TrackedPreferenceMetadata::PrefTrackingStrategy strategy;
  mojom::TrackedPreferenceMetadata::ValueType value_type;
};

mojom::TrackedPreferenceMetadataPtr ConstructTrackedMetadata(
    const TrackedPreferenceMetadata& metadata);

template <typename ConfigurationContainer>
std::vector<mojom::TrackedPreferenceMetadataPtr> ConstructTrackedConfiguration(
    const ConfigurationContainer& configuration) {
  std::vector<mojom::TrackedPreferenceMetadataPtr> result;
  for (auto metadata : configuration) {
    result.push_back(ConstructTrackedMetadata(metadata));
  }
  return result;
}

std::vector<mojom::TrackedPreferenceMetadataPtr> CloneTrackedConfiguration(
    const std::vector<mojom::TrackedPreferenceMetadataPtr>& configuration);

}  // namespace prefs
#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_TRACKED_CONFIGURATION_H_
