// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/live_activity/model/first_run_live_activity_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "ios/chrome/browser/first_run/live_activity/public/features.h"
#import "ios/chrome/browser/live_activity/model/ios_chrome_live_activity_manager_factory.h"

namespace {

// Creates a `FirstRunLiveActivityService` instance for `profile`, or returns
// `nullptr` when `kFreLiveActivity` / `kIOSLiveActivityManager` is disabled or
// `IOSChromeLiveActivityManager` is unavailable.
std::unique_ptr<KeyedService> BuildServiceInstance(ProfileIOS& profile) {
  if (!IsFreLiveActivityEnabled()) {
    return nullptr;
  }
  IOSChromeLiveActivityManager* manager =
      IOSChromeLiveActivityManagerFactory::GetForProfile(profile);
  if (!manager) {
    return nullptr;
  }
  return std::make_unique<FirstRunLiveActivityService>(manager);
}

}  // namespace

FirstRunLiveActivityServiceFactory::FirstRunLiveActivityServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "FirstRunLiveActivityService",
                                         ServiceCreation::kCreateWithProfile,
                                         TestingCreation::kNoServiceForTests) {
  DependsOn(IOSChromeLiveActivityManagerFactory::GetInstance());
}

// static
FirstRunLiveActivityServiceFactory::TestingFactory
FirstRunLiveActivityServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildServiceInstance);
}

std::unique_ptr<KeyedService>
FirstRunLiveActivityServiceFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  return BuildServiceInstance(profile);
}
