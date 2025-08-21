// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/ios_rules_service_factory.h"

#import "ios/chrome/browser/enterprise/data_controls/ios_rules_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace data_controls {

// static
IOSRulesServiceFactory* IOSRulesServiceFactory::GetInstance() {
  static base::NoDestructor<IOSRulesServiceFactory> instance;
  return instance.get();
}

// static
IOSRulesService* IOSRulesServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<IOSRulesService>(
      profile, /*create=*/true);
}

IOSRulesServiceFactory::IOSRulesServiceFactory()
    : ProfileKeyedServiceFactoryIOS("IOSRulesService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

IOSRulesServiceFactory::~IOSRulesServiceFactory() = default;

std::unique_ptr<KeyedService> IOSRulesServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  auto* profile = ProfileIOS::FromBrowserState(browser_state);
  return std::make_unique<IOSRulesService>(profile);
}

}  // namespace data_controls
