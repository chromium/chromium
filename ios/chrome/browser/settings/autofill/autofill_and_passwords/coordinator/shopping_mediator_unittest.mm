// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/shopping_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/prefs/pref_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class ShoppingMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));

    profile_ = std::move(builder).Build();
    autofill::EntityDataManager* entity_data_manager =
        IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());

    mediator_ = [[ShoppingMediator alloc]
        initWithEntityDataManager:entity_data_manager
                      prefService:profile_->GetPrefs()];
    consumer_ = OCMProtocolMock(@protocol(ShoppingConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::kAutofillAiWithDataSchema};
  std::unique_ptr<TestProfileIOS> profile_;
  ShoppingMediator* mediator_;
  id consumer_;
};

// Tests that setting the consumer does not crash.
TEST_F(ShoppingMediatorTest, SetsConsumerValuesSafe) {
  mediator_.consumer = consumer_;
}

// Tests that pushing items correctly splits them by entity type and calls
// the corresponding consumer setters.
TEST_F(ShoppingMediatorTest, SplitsItemsByType) {
  AutofillAIEntityItem* order =
      [[AutofillAIEntityItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];
  order.entityTypeName = autofill::EntityTypeName::kOrder;

  AutofillAIEntityItem* shipment =
      [[AutofillAIEntityItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];
  shipment.entityTypeName = autofill::EntityTypeName::kShipment;

  OCMExpect([consumer_ setShoppingWithOrders:@[ order ]
                                   shipments:@[ shipment ]]);

  mediator_.consumer = consumer_;
  [mediator_ pushItemsToConsumer:@[ order, shipment ]];

  [consumer_ verify];
}

// Tests that the mediator returns the correct supported entity types.
TEST_F(ShoppingMediatorTest, SupportedEntityTypes) {
  autofill::DenseSet<autofill::EntityTypeName> expected_types = {
      autofill::EntityTypeName::kOrder, autofill::EntityTypeName::kShipment};
  EXPECT_EQ([mediator_ supportedEntityTypes], expected_types);
}

// Tests that setting the consumer to nil after disconnect does not crash.
TEST_F(ShoppingMediatorTest, DoesNotCrashOnSetConsumerNilAfterDisconnect) {
  mediator_.consumer = consumer_;
  [mediator_ disconnect];

  mediator_.consumer = nil;
}

// Tests that setting a non-nil consumer after disconnect does not crash.
TEST_F(ShoppingMediatorTest, DoesNotCrashOnSetConsumerAfterDisconnect) {
  [mediator_ disconnect];

  mediator_.consumer = consumer_;
}
