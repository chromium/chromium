// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator.h"

#import "base/containers/flat_set.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator+testing.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/fake_autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class AutofillAIEntityEditCoordinatorTest : public PlatformTest {
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
    base_navigation_controller_ = OCMClassMock([UINavigationController class]);
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    base_navigation_controller_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  id base_navigation_controller_;
  AutofillAIEntityEditCoordinator* coordinator_;
};

// Tests that the coordinator initializes and manages the view controller.
TEST_F(AutofillAIEntityEditCoordinatorTest, StartsAndStops) {
  autofill::EntityInstance instance =
      autofill::test::GetPassportEntityInstance();

  // Add the instance to the EntityDataManager so the coordinator can fetch it.
  autofill::EntityDataManager* entity_data_manager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());
  entity_data_manager->AddOrUpdateEntityInstance(instance);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager->GetEntityInstance(instance.guid())
            .has_value();
      }));

  coordinator_ = [[AutofillAIEntityEditCoordinator alloc]
      initWithBaseNavigationController:base_navigation_controller_
                               browser:browser_.get()
                              entityID:instance.guid()];

  [[base_navigation_controller_ expect] pushViewController:[OCMArg any]
                                                  animated:YES];

  [coordinator_ start];

  [base_navigation_controller_ verify];

  [coordinator_ stop];
}

// Tests that the mediator correctly sets the entity instance on the consumer.
TEST_F(AutofillAIEntityEditCoordinatorTest, MediatorSetsConsumer) {
  autofill::EntityInstance instance =
      autofill::test::GetVehicleEntityInstance();

  // Add the instance to the EntityDataManager so the coordinator can fetch it.
  autofill::EntityDataManager* entity_data_manager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());
  entity_data_manager->AddOrUpdateEntityInstance(instance);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager->GetEntityInstance(instance.guid())
            .has_value();
      }));

  coordinator_ = [[AutofillAIEntityEditCoordinator alloc]
      initWithBaseNavigationController:base_navigation_controller_
                               browser:browser_.get()
                              entityID:instance.guid()];

  [coordinator_ start];

  AutofillAIEntityEditMediator* mediator = coordinator_.mediator;
  ASSERT_TRUE(mediator);

  FakeAutofillAIEntityEditConsumer* consumer =
      [[FakeAutofillAIEntityEditConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_GT(consumer.title.length, 0u);
  EXPECT_GT(consumer.editItems.count, 0u);
}
