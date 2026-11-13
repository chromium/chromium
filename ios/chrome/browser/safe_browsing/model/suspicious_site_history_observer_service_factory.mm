// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/suspicious_site_history_observer_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Builds a `SuspiciousSiteHistoryObserverService` instance for `profile`.
std::unique_ptr<KeyedService> BuildSuspiciousSiteHistoryObserverService(
    ProfileIOS& profile) {
  return std::make_unique<SuspiciousSiteHistoryObserverService>(
      ios::HistoryServiceFactory::GetForProfile(profile),
      ios::HostContentSettingsMapFactory::GetForProfile(&profile));
}

}  // namespace

// static
SuspiciousSiteHistoryObserverServiceFactory::TestingFactory
SuspiciousSiteHistoryObserverServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildSuspiciousSiteHistoryObserverService);
}

SuspiciousSiteHistoryObserverServiceFactory::
    SuspiciousSiteHistoryObserverServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "SuspiciousSiteHistoryObserverService",
          ProfileSelection::kNoInstanceInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SuspiciousSiteHistoryObserverServiceFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  return BuildSuspiciousSiteHistoryObserverService(profile);
}
