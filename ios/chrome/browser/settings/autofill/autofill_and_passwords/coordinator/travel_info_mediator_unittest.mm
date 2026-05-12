// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/travel_info_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class TravelInfoMediatorTest : public PlatformTest {
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
    autofill::EntityDataManager* entity_data_manager =
        IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());

    mediator_ = [[TravelInfoMediator alloc]
        initWithEntityDataManager:entity_data_manager];
    consumer_ = OCMProtocolMock(@protocol(TravelInfoConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  TravelInfoMediator* mediator_;
  id consumer_;
};

// Tests that setting the consumer does not crash.
TEST_F(TravelInfoMediatorTest, SetsConsumerValuesSafe) {
  mediator_.consumer = consumer_;
}

@interface FakeTravelInfoMediatorDelegate
    : NSObject <AutofillAIBaseMediatorDelegate>
@property(nonatomic, assign) std::optional<autofill::EntityInstance::EntityId>
    lastOpenedEntityID;
@end

@implementation FakeTravelInfoMediatorDelegate
- (void)autofillAIBaseMediator:(AutofillAIBaseMediator*)mediator
    didRequestToOpenEntityWithID:(autofill::EntityInstance::EntityId)entityID {
  _lastOpenedEntityID = entityID;
}
@end

// Tests that selecting an item triggers the mutator delegate.
TEST_F(TravelInfoMediatorTest, SelectsItemForwardsToDelegate) {
  FakeTravelInfoMediatorDelegate* delegate =
      [[FakeTravelInfoMediatorDelegate alloc] init];
  mediator_.delegate = delegate;

  AutofillAIEntityItem* item = [[AutofillAIEntityItem alloc] initWithType:0];
  item.guid = autofill::EntityInstance::EntityId("test-id-123");

  [mediator_ didSelectEntityItem:item];

  ASSERT_TRUE(delegate.lastOpenedEntityID.has_value());
  EXPECT_EQ(delegate.lastOpenedEntityID.value(), item.guid);
}
