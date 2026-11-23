// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/live_activity/model/ios_chrome_live_activity_manager_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "ios/chrome/browser/live_activity/model/ios_chrome_live_activity_manager.h"
#import "ios/chrome/browser/live_activity/public/features.h"

namespace {

// Creates an `IOSChromeLiveActivityManager` instance for `profile`, or returns
// `nullptr` when `kIOSLiveActivityManager` is disabled.
std::unique_ptr<KeyedService> BuildServiceInstance(ProfileIOS& profile) {
  if (!IsIOSLiveActivityManagerEnabled()) {
    return nullptr;
  }
  return std::make_unique<IOSChromeLiveActivityManager>();
}

}  // namespace

IOSChromeLiveActivityManagerFactory::IOSChromeLiveActivityManagerFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "IOSChromeLiveActivityManager",
                                         ServiceCreation::kCreateWithProfile,
                                         TestingCreation::kNoServiceForTests) {}

// static
IOSChromeLiveActivityManagerFactory::TestingFactory
IOSChromeLiveActivityManagerFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildServiceInstance);
}

std::unique_ptr<KeyedService>
IOSChromeLiveActivityManagerFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  return BuildServiceInstance(profile);
}
