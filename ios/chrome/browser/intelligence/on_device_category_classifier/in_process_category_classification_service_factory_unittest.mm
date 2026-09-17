// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service_factory.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/test/scoped_feature_list.h"
#import "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
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

// Test fixture which registers the factory's real (non-stubbed) default
// factory, so that the feature checks in
// `BuildInProcessCategoryClassificationService()` are exercised.
class InProcessCategoryClassificationServiceFactoryFeatureTest
    : public PlatformTest {
 protected:
  std::unique_ptr<TestProfileIOS> CreateProfile() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        InProcessCategoryClassificationServiceFactory::GetInstance(),
        InProcessCategoryClassificationServiceFactory::GetDefaultFactory());
    return std::move(builder).Build();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
};

// Tests that no service is built when the on-device category classifier is
// disabled, even for a regular profile.
TEST_F(InProcessCategoryClassificationServiceFactoryFeatureTest,
       DoNotCreateServiceWhenOnDeviceClassifierDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kGeminiContextualSuggestionsCues);
  ASSERT_FALSE(IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled());

  auto profile = CreateProfile();
  EXPECT_EQ(InProcessCategoryClassificationServiceFactory::GetForProfile(
                profile.get()),
            nullptr);
}

}  // namespace
