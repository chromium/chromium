// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "components/sessions/core/tab_restore_service_impl.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_client.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildTabRestoreService(ProfileIOS* profile) {
  DCHECK(!profile->IsOffTheRecord());
  return std::make_unique<sessions::TabRestoreServiceImpl>(
      std::make_unique<IOSChromeTabRestoreServiceClient>(
          profile->GetStatePath(), BrowserListFactory::GetForProfile(profile)),
      profile->GetPrefs(), /*time_factory=*/nullptr,
      GetApplicationContext()->GetOSCryptAsync());
}

}  // namespace

// static
IOSChromeTabRestoreServiceFactory::TestingFactory
IOSChromeTabRestoreServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildTabRestoreService);
}

IOSChromeTabRestoreServiceFactory::IOSChromeTabRestoreServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "TabRestoreService",
                                         TestingCreation::kNoServiceForTests) {
  DependsOn(BrowserListFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSChromeTabRestoreServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildTabRestoreService(profile);
}
