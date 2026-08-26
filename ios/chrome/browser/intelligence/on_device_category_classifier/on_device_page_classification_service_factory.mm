// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service_factory.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildOnDevicePageClassificationService(
    ProfileIOS* profile) {
  CHECK(!profile->IsOffTheRecord());
  InProcessCategoryClassificationService* in_process_service =
      InProcessCategoryClassificationService::GetForProfile(profile);
  if (!in_process_service) {
    return nullptr;
  }

  return std::make_unique<OnDevicePageClassificationService>(
      in_process_service);
}

}  // namespace

// static
OnDevicePageClassificationService*
OnDevicePageClassificationServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<OnDevicePageClassificationService>(
          profile, /*create=*/true);
}

// static
OnDevicePageClassificationServiceFactory*
OnDevicePageClassificationServiceFactory::GetInstance() {
  static base::NoDestructor<OnDevicePageClassificationServiceFactory> instance;
  return instance.get();
}

// static
ProfileKeyedServiceFactoryIOS::TestingFactory
OnDevicePageClassificationServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildOnDevicePageClassificationService);
}

OnDevicePageClassificationServiceFactory::
    OnDevicePageClassificationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("OnDevicePageClassificationService") {
  DependsOn(InProcessCategoryClassificationService::GetFactory());
}

OnDevicePageClassificationServiceFactory::
    ~OnDevicePageClassificationServiceFactory() = default;

std::unique_ptr<KeyedService>
OnDevicePageClassificationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildOnDevicePageClassificationService(profile);
}
