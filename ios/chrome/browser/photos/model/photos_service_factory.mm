// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/photos/model/photos_service_factory.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/photos/model/photos_service.h"
#import "ios/chrome/browser/photos/model/photos_service_configuration.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/photos/photos_api.h"

namespace {

// Build a PhotosService instance.
std::unique_ptr<KeyedService> BuildPhotosService(ProfileIOS* profile) {
  PhotosServiceConfiguration* configuration =
      [[PhotosServiceConfiguration alloc] init];
  ApplicationContext* application_context = GetApplicationContext();
  configuration.singleSignOnService =
      application_context->GetSingleSignOnService();
  configuration.prefService = profile->GetPrefs();
  configuration.identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  configuration.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  return ios::provider::CreatePhotosService(configuration);
}

}  // namespace

// static
PhotosService* PhotosServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<PhotosService>(profile,
                                                              /*create=*/true);
}

// static
PhotosServiceFactory* PhotosServiceFactory::GetInstance() {
  static base::NoDestructor<PhotosServiceFactory> instance;
  return instance.get();
}

// static
PhotosServiceFactory::TestingFactory PhotosServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildPhotosService);
}

PhotosServiceFactory::PhotosServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PhotosService",
                                    TestingCreation::kNoServiceForTests,
                                    ProfileSelection::kRedirectedInIncognito,
                                    ServiceCreation::kCreateWithProfile) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
}

PhotosServiceFactory::~PhotosServiceFactory() = default;

std::unique_ptr<KeyedService> PhotosServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildPhotosService(profile);
}
