// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/tab_usage_recorder_service_factory.h"

#import <utility>

#import "ios/chrome/browser/metrics/model/tab_usage_recorder_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"

TabUsageRecorderServiceFactory::TabUsageRecorderServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "TabUsageRecorderService",
          ProfileSelection::kNoInstanceInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {
  DependsOn(BrowserListFactory::GetInstance());
  DependsOn(SessionRestorationServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TabUsageRecorderServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<TabUsageRecorderService>(
      BrowserListFactory::GetForProfile(profile),
      SessionRestorationServiceFactory::GetForProfile(profile));
}
