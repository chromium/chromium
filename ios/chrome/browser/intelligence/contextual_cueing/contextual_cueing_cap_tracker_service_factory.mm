// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace contextual_cueing {

namespace {

std::unique_ptr<KeyedService> BuildContextualCueingCapTrackerService(
    ProfileIOS* profile) {
  return std::make_unique<ContextualCueingCapTrackerService>(
      profile->GetPrefs());
}

}  // namespace

// static
ContextualCueingCapTrackerServiceFactory::TestingFactory
ContextualCueingCapTrackerServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildContextualCueingCapTrackerService);
}

ContextualCueingCapTrackerServiceFactory::
    ContextualCueingCapTrackerServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "ContextualCueingCapTrackerService",
          ProfileSelection::kNoInstanceInIncognito) {}

std::unique_ptr<KeyedService>
ContextualCueingCapTrackerServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildContextualCueingCapTrackerService(profile);
}

}  // namespace contextual_cueing
