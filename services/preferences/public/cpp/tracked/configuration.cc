// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/tracked/configuration.h"

namespace prefs {

mojom::TrackedPreferenceMetadataPtr ConstructTrackedMetadata(
    const TrackedPreferenceMetadata& metadata) {
  return mojom::TrackedPreferenceMetadata::New(
      metadata.reporting_id, metadata.name, metadata.enforcement_level,
      metadata.strategy, metadata.value_type);
}

std::vector<mojom::TrackedPreferenceMetadataPtr> CloneTrackedConfiguration(
    const std::vector<mojom::TrackedPreferenceMetadataPtr>& configuration) {
  std::vector<mojom::TrackedPreferenceMetadataPtr> result;
  for (const auto& metadata : configuration) {
    result.push_back(metadata.Clone());
  }
  return result;
}

}  // namespace prefs
