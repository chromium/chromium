// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_client.h"
#include "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildTabRestoreService(ProfileIOS* profile) {
  DCHECK(!profile->IsOffTheRecord());
  return std::make_unique<sessions::TabRestoreServiceImpl>(
      std::make_unique<IOSChromeTabRestoreServiceClient>(
          profile->GetStatePath(), BrowserListFactory::GetForProfile(profile)),
      profile->GetPrefs(), /*time_factory=*/nullptr);
}

}  // namespace

// static
sessions::TabRestoreService* IOSChromeTabRestoreServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<sessions::TabRestoreService>(
      profile, /*create=*/true);
}

// static
IOSChromeTabRestoreServiceFactory*
IOSChromeTabRestoreServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeTabRestoreServiceFactory> instance;
  return instance.get();
}

// static
IOSChromeTabRestoreServiceFactory::TestingFactory
IOSChromeTabRestoreServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildTabRestoreService);
}

IOSChromeTabRestoreServiceFactory::IOSChromeTabRestoreServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TabRestoreService",
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(BrowserListFactory::GetInstance());
}

IOSChromeTabRestoreServiceFactory::~IOSChromeTabRestoreServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeTabRestoreServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildTabRestoreService(profile);
}
