// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/tab_usage_recorder_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"

// static
TabUsageRecorderService* TabUsageRecorderServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<TabUsageRecorderService>(
      profile, /*create=*/true);
}

// static
TabUsageRecorderServiceFactory* TabUsageRecorderServiceFactory::GetInstance() {
  static base::NoDestructor<TabUsageRecorderServiceFactory> instance(PassKey{});
  return instance.get();
}

TabUsageRecorderServiceFactory::TabUsageRecorderServiceFactory(PassKey)
    : ProfileKeyedServiceFactoryIOS("TabUsageRecorderService",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(BrowserListFactory::GetInstance());
  DependsOn(SessionRestorationServiceFactory::GetInstance());
}

TabUsageRecorderServiceFactory::~TabUsageRecorderServiceFactory() = default;

std::unique_ptr<KeyedService>
TabUsageRecorderServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<TabUsageRecorderService>(
      BrowserListFactory::GetForProfile(profile),
      SessionRestorationServiceFactory::GetForProfile(profile));
}
