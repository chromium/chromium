// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/model/inactive_tabs/inactive_tabs_service_factory.h"

#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tab_switcher/model/inactive_tabs/inactive_tabs_service.h"

// static
InactiveTabsServiceFactory* InactiveTabsServiceFactory::GetInstance() {
  static base::NoDestructor<InactiveTabsServiceFactory> instance;
  return instance.get();
}

// static
InactiveTabsService* InactiveTabsServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<InactiveTabsService>(
      profile, /*create=*/true);
}

InactiveTabsServiceFactory::InactiveTabsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("InactiveTabsService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(BrowserListFactory::GetInstance());
}

InactiveTabsServiceFactory::~InactiveTabsServiceFactory() = default;

#pragma mark ProfileKeyedServiceFactoryIOS

std::unique_ptr<KeyedService>
InactiveTabsServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<InactiveTabsService>(
      profile->GetPrefs(), BrowserListFactory::GetForProfile(profile));
}
