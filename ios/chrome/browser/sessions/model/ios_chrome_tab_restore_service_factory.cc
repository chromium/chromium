// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_client.h"
#include "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildTabRestoreService(
    web::BrowserState* context) {
  DCHECK(!context->IsOffTheRecord());

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<sessions::TabRestoreServiceImpl>(
      std::make_unique<IOSChromeTabRestoreServiceClient>(
          profile->GetStatePath(), BrowserListFactory::GetForProfile(profile)),
      profile->GetPrefs(), /*time_factory=*/nullptr);
}

}  // namespace

// static
sessions::TabRestoreService*
IOSChromeTabRestoreServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
sessions::TabRestoreService* IOSChromeTabRestoreServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<sessions::TabRestoreService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
IOSChromeTabRestoreServiceFactory*
IOSChromeTabRestoreServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeTabRestoreServiceFactory> instance;
  return instance.get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
IOSChromeTabRestoreServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildTabRestoreService);
}

IOSChromeTabRestoreServiceFactory::IOSChromeTabRestoreServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TabRestoreService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(BrowserListFactory::GetInstance());
}

IOSChromeTabRestoreServiceFactory::~IOSChromeTabRestoreServiceFactory() {}

bool IOSChromeTabRestoreServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
IOSChromeTabRestoreServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildTabRestoreService(context);
}
