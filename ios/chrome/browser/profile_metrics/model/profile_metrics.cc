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

namespace {

constexpr base::TimeDelta kActivityThreshold = base::Days(28);

bool ProfileIsActive(const ProfileAttributesIOS& attr) {
  return base::Time::Now() - attr.GetLastActiveTime() <= kActivityThreshold;
}

void UpdateCountsForProfileAttributes(profile_metrics::Counts* counts,
                                      const ProfileAttributesIOS& attr) {
  if (!ProfileIsActive(attr)) {
    counts->unused++;
  } else {
    counts->active++;
    if (attr.IsAuthenticated()) {
      counts->signedin++;
    }
  }
}

void CountProfileInformation(const ProfileAttributesStorageIOS& storage,
                             profile_metrics::Counts* counts) {
  size_t profile_count = storage.GetNumberOfProfiles();
  counts->total = profile_count;

  // Ignore other metrics if we have no profiles.
  if (!profile_count) {
    return;
  }

  storage.IterateOverProfileAttributes(
      base::BindRepeating(&UpdateCountsForProfileAttributes, counts));
}

}  // namespace

void LogNumberOfProfiles(ProfileManagerIOS* manager) {
  const ProfileAttributesStorageIOS& storage =
      *manager->GetProfileAttributesStorage();
  profile_metrics::LogTotalNumberOfProfiles(storage.GetNumberOfProfiles());

  profile_metrics::Counts counts;
  CountProfileInformation(storage, &counts);
  profile_metrics::LogProfileMetricsCounts(
      counts, profile_metrics::ProfileActivityThreshold::kDuration28Days);
}
