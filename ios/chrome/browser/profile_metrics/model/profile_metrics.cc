// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/profile_metrics/model/profile_metrics.h"

#include <stddef.h>

#include "components/profile_metrics/browser_profile_type.h"
#include "components/profile_metrics/counts.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#include "ios/web/public/browser_state.h"

namespace {

constexpr base::TimeDelta kActivityThreshold = base::Days(28);

bool ProfileIsActive(const ProfileAttributesIOS& attr) {
  return base::Time::Now() - attr.GetLastActiveTime() <= kActivityThreshold;
}

void CountProfileInformation(const ProfileAttributesStorageIOS& storage,
                             profile_metrics::Counts* counts) {
  size_t profile_count = storage.GetNumberOfProfiles();
  counts->total = profile_count;

  // Ignore other metrics if we have no profiles.
  if (!profile_count) {
    return;
  }

  for (size_t i = 0; i < profile_count; ++i) {
    ProfileAttributesIOS attr = storage.GetAttributesForProfileAtIndex(i);
    if (!ProfileIsActive(attr)) {
      counts->unused++;
    } else {
      counts->active++;
      if (attr.IsAuthenticated()) {
        counts->signedin++;
      }
    }
  }
}

}  // namespace

void LogNumberOfProfiles(ProfileManagerIOS* manager) {
  profile_metrics::Counts counts;
  CountProfileInformation(*manager->GetProfileAttributesStorage(), &counts);
  profile_metrics::LogProfileMetricsCounts(counts);
}
