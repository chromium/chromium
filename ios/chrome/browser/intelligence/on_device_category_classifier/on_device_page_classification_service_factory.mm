// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service_factory.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildOnDevicePageClassificationService(
    ProfileIOS* profile) {
  CHECK(!profile->IsOffTheRecord());
  InProcessCategoryClassificationService* in_process_service =
      InProcessCategoryClassificationServiceFactory::GetForProfile(profile);
  if (!in_process_service) {
    return nullptr;
  }

  return std::make_unique<OnDevicePageClassificationService>(
      in_process_service);
}

}  // namespace

// static
OnDevicePageClassificationServiceFactory::TestingFactory
OnDevicePageClassificationServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildOnDevicePageClassificationService);
}

OnDevicePageClassificationServiceFactory::
    OnDevicePageClassificationServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "OnDevicePageClassificationService") {
  DependsOn(InProcessCategoryClassificationServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
OnDevicePageClassificationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildOnDevicePageClassificationService(profile);
}
