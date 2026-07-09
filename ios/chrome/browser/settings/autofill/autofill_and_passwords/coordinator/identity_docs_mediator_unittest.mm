// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/optimization_guide/core/feature_registry/feature_registration.h"
#import "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#import "components/prefs/pref_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class IdentityDocsMediatorTest : public PlatformTest {
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

    mediator_ = [[IdentityDocsMediator alloc]
        initWithEntityDataManager:entity_data_manager
                      prefService:profile_->GetPrefs()];
    consumer_ = OCMProtocolMock(@protocol(IdentityDocsConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::kAutofillAiWithDataSchema};
  std::unique_ptr<TestProfileIOS> profile_;
  IdentityDocsMediator* mediator_;
  id consumer_;
};

// Tests that setting the consumer does not crash.
TEST_F(IdentityDocsMediatorTest, SetsConsumerValuesSafe) {
  mediator_.consumer = consumer_;
}

@interface FakeIdentityDocsMediatorDelegate
    : NSObject <AutofillAIBaseMediatorDelegate>
@property(nonatomic, assign) std::optional<autofill::EntityInstance::EntityId>
    lastOpenedEntityID;
@end

@implementation FakeIdentityDocsMediatorDelegate
- (void)autofillAIBaseMediator:(AutofillAIBaseMediator*)mediator
    didRequestToOpenEntityWithID:(autofill::EntityInstance::EntityId)entityID {
  _lastOpenedEntityID = entityID;
}

- (void)autofillAIBaseMediator:(AutofillAIBaseMediator*)mediator
    didRequestToCreateEntityWithType:(autofill::EntityType)entityType {
}
@end

// Tests that selecting an item triggers the mutator delegate.
TEST_F(IdentityDocsMediatorTest, SelectsItemForwardsToDelegate) {
  FakeIdentityDocsMediatorDelegate* delegate =
      [[FakeIdentityDocsMediatorDelegate alloc] init];
  mediator_.delegate = delegate;

  AutofillAIEntityItem* item =
      [[AutofillAIEntityItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];
  item.guid = autofill::EntityInstance::EntityId("test-id-123");

  [mediator_ didSelectEntityItem:item];

  ASSERT_TRUE(delegate.lastOpenedEntityID.has_value());
  EXPECT_EQ(delegate.lastOpenedEntityID.value(), item.guid);
}

// Tests that pushing items correctly splits them by entity type and calls
// the corresponding consumer setters.
TEST_F(IdentityDocsMediatorTest, SplitsItemsByType) {
  AutofillAIEntityItem* license =
      [[AutofillAIEntityItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];
  license.entityTypeName = autofill::EntityTypeName::kDriversLicense;

  AutofillAIEntityItem* idCard =
      [[AutofillAIEntityItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];
  idCard.entityTypeName = autofill::EntityTypeName::kNationalIdCard;

  AutofillAIEntityItem* passport =
      [[AutofillAIEntityItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];
  passport.entityTypeName = autofill::EntityTypeName::kPassport;

  OCMExpect([consumer_ setIdentityDocsWithDriversLicenses:@[ license ]
                                          nationalIdCards:@[ idCard ]
                                                passports:@[ passport ]]);

  mediator_.consumer = consumer_;
  [mediator_ pushItemsToConsumer:@[ license, idCard, passport ]];

  [consumer_ verify];
}

// Tests that calling `didSelectDeleteEntityItems` removes the entity from the
// data manager.
TEST_F(IdentityDocsMediatorTest, DidSelectDeleteEntityItemsRemovesEntity) {
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

  AutofillAIEntityItem* item =
      [[AutofillAIEntityItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];
  item.guid = entity_id;
  item.entityTypeName = instance.type().name();

  [mediator_ didSelectDeleteEntityItems:@[ item ]];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return !entity_data_manager->GetEntityInstance(entity_id).has_value();
      }));
}

// Tests that calling `didToggleIdentityDocs` updates the preference.
TEST_F(IdentityDocsMediatorTest, TogglesIdentityDocsPref) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(autofill::prefs::kAutofillAiIdentityEntitiesEnabled, false);

  [mediator_ didToggleIdentityDocs:YES];
  EXPECT_TRUE(
      prefs->GetBoolean(autofill::prefs::kAutofillAiIdentityEntitiesEnabled));

  [mediator_ didToggleIdentityDocs:NO];
  EXPECT_FALSE(
      prefs->GetBoolean(autofill::prefs::kAutofillAiIdentityEntitiesEnabled));
}

// Tests that a preference change updates the consumer toggle state.
TEST_F(IdentityDocsMediatorTest, PrefChangeUpdatesConsumer) {
  profile_->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillAiIdentityEntitiesEnabled, false);
  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setIdentityDocsToggleState:YES enabled:YES managed:NO]);
  profile_->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillAiIdentityEntitiesEnabled, true);
  [consumer_ verify];

  OCMExpect([consumer_ setIdentityDocsToggleState:NO enabled:YES managed:NO]);
  profile_->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillAiIdentityEntitiesEnabled, false);
  [consumer_ verify];
}

// Tests that a preference change for address autofill updates the consumer
// toggle enabled state.
TEST_F(IdentityDocsMediatorTest, AutofillProfilePrefChangeUpdatesConsumer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      autofill::features::kAutofillEnableAutofillSettingsEnterprisePolicy);

  profile_->GetPrefs()->SetBoolean(autofill::prefs::kAutofillProfileEnabled,
                                   true);
  profile_->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillAiIdentityEntitiesEnabled, true);
  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setIdentityDocsToggleState:NO enabled:NO managed:NO]);
  profile_->GetPrefs()->SetBoolean(autofill::prefs::kAutofillProfileEnabled,
                                   false);
  [consumer_ verify];

  OCMExpect([consumer_ setIdentityDocsToggleState:YES enabled:YES managed:NO]);
  profile_->GetPrefs()->SetBoolean(autofill::prefs::kAutofillProfileEnabled,
                                   true);
  [consumer_ verify];
}

// Tests that a preference change for address autofill does not affect the
// consumer toggle enabled state when the enterprise policy feature is enabled.
TEST_F(IdentityDocsMediatorTest,
       AutofillProfilePrefChangeUpdatesConsumer_EnterprisePolicy) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableAutofillSettingsEnterprisePolicy);

  profile_->GetPrefs()->SetBoolean(autofill::prefs::kAutofillProfileEnabled,
                                   true);
  profile_->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillAiIdentityEntitiesEnabled, true);
  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setIdentityDocsToggleState:YES enabled:YES managed:NO]);
  profile_->GetPrefs()->SetBoolean(autofill::prefs::kAutofillProfileEnabled,
                                   false);
  [consumer_ verify];
}

// Tests that a policy preference change updates the consumer toggle managed
// state.
TEST_F(IdentityDocsMediatorTest, PolicyPrefChangeUpdatesConsumer) {
  using optimization_guide::model_execution::prefs::
      ModelExecutionEnterprisePolicyValue;
  const std::string kPolicyPref = optimization_guide::prefs::
      kAutofillPredictionImprovementsEnterprisePolicyAllowed;

  profile_->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillAiIdentityEntitiesEnabled, true);
  profile_->GetPrefs()->SetInteger(
      kPolicyPref,
      static_cast<int>(ModelExecutionEnterprisePolicyValue::kAllow));
  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setIdentityDocsToggleState:YES enabled:YES managed:YES]);
  profile_->GetPrefs()->SetInteger(
      kPolicyPref,
      static_cast<int>(ModelExecutionEnterprisePolicyValue::kDisable));
  [consumer_ verify];

  OCMExpect([consumer_ setIdentityDocsToggleState:YES enabled:YES managed:NO]);
  profile_->GetPrefs()->SetInteger(
      kPolicyPref,
      static_cast<int>(ModelExecutionEnterprisePolicyValue::kAllow));
  [consumer_ verify];
}
