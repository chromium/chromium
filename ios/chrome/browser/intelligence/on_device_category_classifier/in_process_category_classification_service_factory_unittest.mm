// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service_factory.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

std::unique_ptr<KeyedService> BuildTestInProcessClassificationService(
    ProfileIOS* profile) {
  static base::NoDestructor<
      optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider;
  return std::make_unique<InProcessCategoryClassificationService>(
      test_model_provider.get());
}

class InProcessCategoryClassificationServiceFactoryTest : public PlatformTest {
 protected:
  std::unique_ptr<TestProfileIOS> CreateProfile() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        InProcessCategoryClassificationServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestInProcessClassificationService));
    return std::move(builder).Build();
  }

  web::WebTaskEnvironment task_environment_;
};

// Tests that the factory creates a service instance for a regular profile.
TEST_F(InProcessCategoryClassificationServiceFactoryTest,
       CreateServiceForProfile) {
  auto profile = CreateProfile();
  EXPECT_NE(InProcessCategoryClassificationServiceFactory::GetForProfile(
                profile.get()),
            nullptr);
}

// Tests that the factory does not create a service for an off-the-record
// profile.
TEST_F(InProcessCategoryClassificationServiceFactoryTest,
       DoNotCreateServiceForOffTheRecordProfile) {
  auto profile = CreateProfile();
  ProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories();
  EXPECT_EQ(
      InProcessCategoryClassificationServiceFactory::GetForProfile(otr_profile),
      nullptr);
}

}  // namespace
