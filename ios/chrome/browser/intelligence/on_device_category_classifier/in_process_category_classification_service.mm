// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace {

class InProcessCategoryClassificationServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static InProcessCategoryClassificationService* GetForProfile(
      ProfileIOS* profile) {
    return GetInstance()
        ->GetServiceForProfileAs<InProcessCategoryClassificationService>(
            profile, /*create=*/true);
  }

  static InProcessCategoryClassificationServiceFactory* GetInstance() {
    static base::NoDestructor<InProcessCategoryClassificationServiceFactory>
        instance;
    return instance.get();
  }

 private:
  friend class base::NoDestructor<
      InProcessCategoryClassificationServiceFactory>;

  InProcessCategoryClassificationServiceFactory()
      : ProfileKeyedServiceFactoryIOS("InProcessCategoryClassificationService",
                                      ProfileSelection::kOwnInstanceInIncognito,
                                      ServiceCreation::kCreateWithProfile,
                                      TestingCreation::kNoServiceForTests) {}

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override {
    if (profile->IsOffTheRecord()) {
      return nullptr;
    }
    return std::make_unique<InProcessCategoryClassificationService>();
  }
};

}  // namespace

// static
InProcessCategoryClassificationService*
InProcessCategoryClassificationService::GetForProfile(ProfileIOS* profile) {
  return InProcessCategoryClassificationServiceFactory::GetForProfile(profile);
}

// static
void InProcessCategoryClassificationService::EnsureFactoryBuilt() {
  InProcessCategoryClassificationServiceFactory::GetInstance();
}

InProcessCategoryClassificationService::
    InProcessCategoryClassificationService() = default;

InProcessCategoryClassificationService::
    ~InProcessCategoryClassificationService() = default;

void InProcessCategoryClassificationService::ClassifyPageContext(
    const GURL& url,
    const std::string& title,
    const std::string& page_content,
    ClassificationCallback callback) {
  // No-op stub for initial interface CL.
  std::move(callback).Run({});
}
