// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/photos/model/photos_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/photos/model/photos_service.h"
#import "ios/chrome/browser/photos/model/photos_service_configuration.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/photos/photos_api.h"

// static
PhotosService* PhotosServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<PhotosService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
PhotosServiceFactory* PhotosServiceFactory::GetInstance() {
  static base::NoDestructor<PhotosServiceFactory> instance;
  return instance.get();
}

PhotosServiceFactory::PhotosServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PhotosService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
}

PhotosServiceFactory::~PhotosServiceFactory() = default;

std::unique_ptr<KeyedService> PhotosServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  PhotosServiceConfiguration* configuration =
      [[PhotosServiceConfiguration alloc] init];
  ApplicationContext* application_context = GetApplicationContext();
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  configuration.singleSignOnService =
      application_context->GetSingleSignOnService();
  configuration.prefService = profile->GetPrefs();
  configuration.identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  configuration.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  return ios::provider::CreatePhotosService(configuration);
}

web::BrowserState* PhotosServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool PhotosServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}
