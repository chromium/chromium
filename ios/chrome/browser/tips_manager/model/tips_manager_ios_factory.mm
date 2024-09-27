// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"

#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/tips_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"

namespace {

// Helper function to create an `TipsManagerIOS` instance.
std::unique_ptr<KeyedService> BuildServiceInstance(web::BrowserState* context) {
  if (!IsSegmentationTipsManagerEnabled()) {
    return nullptr;
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  return std::make_unique<TipsManagerIOS>(
      profile->GetPrefs(), GetApplicationContext()->GetLocalState());
}

}  // namespace

// static
TipsManagerIOS* TipsManagerIOSFactory::GetForProfile(ProfileIOS* profile) {
  if (!IsSegmentationTipsManagerEnabled()) {
    return nullptr;
  }

  return GetInstance()->GetServiceForProfileAs<TipsManagerIOS>(profile,
                                                               /*create=*/true);
}

// static
TipsManagerIOSFactory* TipsManagerIOSFactory::GetInstance() {
  static base::NoDestructor<TipsManagerIOSFactory> instance;
  return instance.get();
}

TipsManagerIOSFactory::TipsManagerIOSFactory()
    : ProfileKeyedServiceFactoryIOS("TipsManagerIOS",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests,
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

TipsManagerIOSFactory::~TipsManagerIOSFactory() = default;

// static
BrowserStateKeyedServiceFactory::TestingFactory
TipsManagerIOSFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildServiceInstance);
}

std::unique_ptr<KeyedService> TipsManagerIOSFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildServiceInstance(context);
}

void TipsManagerIOSFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  segmentation_platform::TipsManager::RegisterProfilePrefs(registry);
}
