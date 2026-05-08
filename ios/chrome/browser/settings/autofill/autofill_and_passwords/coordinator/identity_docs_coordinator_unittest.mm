// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_mediator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class IdentityDocsCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {autofill::features::kAutofillAiWithDataSchema,
         autofill::features::kAutofillAiCreateEntityDataManager},
        {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    navigation_controller_ = [[UINavigationController alloc] init];

    coordinator_ = [[IdentityDocsCoordinator alloc]
        initWithBaseNavigationController:navigation_controller_
                                 browser:browser_.get()];
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UINavigationController* navigation_controller_;
  IdentityDocsCoordinator* coordinator_;
};

// Tests that start and stop don't crash.
TEST_F(IdentityDocsCoordinatorTest, StartAndStop) {
  [coordinator_ start];
  [coordinator_ stop];
}

// Tests that requesting to open an entity launches the entity edit coordinator
// without crashing.
TEST_F(IdentityDocsCoordinatorTest, StartsEntityEditCoordinator) {
  [coordinator_ start];

  autofill::EntityInstance instance =
      autofill::test::GetPassportEntityInstance();
  autofill::EntityInstance::EntityId entity_id = instance.guid();
  autofill::EntityDataManager* entity_data_manager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());
  entity_data_manager->AddOrUpdateEntityInstance(instance);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager->GetEntityInstance(entity_id).has_value();
      }));

  IdentityDocsMediator* mediator = [[IdentityDocsMediator alloc]
      initWithEntityDataManager:entity_data_manager];

  NSUInteger initialCount = navigation_controller_.viewControllers.count;

  id<IdentityDocsMediatorDelegate> delegate =
      static_cast<id<IdentityDocsMediatorDelegate>>(coordinator_);
  [delegate identityDocsMediator:mediator
      didRequestToOpenEntityWithID:entity_id];
  EXPECT_GT(navigation_controller_.viewControllers.count, initialCount);

  [coordinator_ stop];
}
