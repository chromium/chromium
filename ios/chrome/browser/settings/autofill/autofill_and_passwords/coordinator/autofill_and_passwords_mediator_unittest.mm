// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AutofillAndPasswordsMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));

    profile_ = std::move(builder).Build();
    pref_service_ = profile_->GetPrefs();

    entity_data_manager_ =
        IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());

    mediator_ = [[AutofillAndPasswordsMediator alloc]
        initWithUserPrefService:pref_service_
              entityDataManager:entity_data_manager_];
    consumer_ = OCMProtocolMock(@protocol(AutofillAndPasswordsConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  // Adds a test passport entity to the EntityDataManager and waits for sync.
  autofill::EntityInstance::EntityId AddTestEntity() {
    autofill::EntityInstance instance =
        autofill::test::GetPassportEntityInstance();
    autofill::EntityInstance::EntityId entity_id = instance.guid();
    entity_data_manager_->AddOrUpdateEntityInstance(instance);

    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout, true, ^{
          return entity_data_manager_->GetEntityInstance(entity_id).has_value();
        }));
    return entity_id;
  }

  // Removes a test entity from the EntityDataManager and waits for sync.
  void RemoveTestEntity(autofill::EntityInstance::EntityId entity_id) {
    entity_data_manager_->RemoveEntityInstance(entity_id);

    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout, true, ^{
          return !entity_data_manager_->GetEntityInstance(entity_id)
                      .has_value();
        }));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<autofill::EntityDataManager> entity_data_manager_;
  AutofillAndPasswordsMediator* mediator_;
  id consumer_;
};

// Tests that the consumer receives the initial values when it is set and the
// schema is enabled.
TEST_F(AutofillAndPasswordsMediatorTest,
       SetsInitialConsumerValuesWithSchemaOn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  pref_service_->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                            YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillCreditCardEnabled, NO);
  pref_service_->SetBoolean(autofill::prefs::kAutofillProfileEnabled, YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillAiIdentityEntitiesEnabled,
                            YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillAiTravelEntitiesEnabled,
                            NO);

  OCMExpect([consumer_ setPasswordsEnabled:YES]);
  OCMExpect([consumer_ setAutofillCreditCardEnabled:NO]);
  OCMExpect([consumer_ setAutofillProfileEnabled:YES]);
  OCMExpect([consumer_ setIdentityDocsEnabled:YES]);
  OCMExpect([consumer_ setTravelInfoEnabled:NO]);
  OCMExpect([consumer_ setShouldShowAutofillAIFeatures:YES]);

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that Autofill AI features are hidden when the schema flag is off and no
// local data exists.
TEST_F(AutofillAndPasswordsMediatorTest,
       SetsInitialConsumerValuesWithSchemaOffAndNoData) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  pref_service_->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                            YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillCreditCardEnabled, NO);
  pref_service_->SetBoolean(autofill::prefs::kAutofillProfileEnabled, YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillAiIdentityEntitiesEnabled,
                            YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillAiTravelEntitiesEnabled,
                            YES);

  OCMExpect([consumer_ setPasswordsEnabled:YES]);
  OCMExpect([consumer_ setAutofillCreditCardEnabled:NO]);
  OCMExpect([consumer_ setAutofillProfileEnabled:YES]);
  OCMExpect([consumer_ setIdentityDocsEnabled:YES]);
  OCMExpect([consumer_ setTravelInfoEnabled:YES]);
  OCMExpect([consumer_ setShouldShowAutofillAIFeatures:NO]);

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that Autofill AI features are shown when the schema flag is off but
// local data exists.
TEST_F(AutofillAndPasswordsMediatorTest,
       SetsInitialConsumerValuesWithSchemaOffAndDataExists) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  AddTestEntity();

  pref_service_->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                            YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillCreditCardEnabled, NO);
  pref_service_->SetBoolean(autofill::prefs::kAutofillProfileEnabled, YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillAiIdentityEntitiesEnabled,
                            YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillAiTravelEntitiesEnabled,
                            YES);

  OCMExpect([consumer_ setPasswordsEnabled:YES]);
  OCMExpect([consumer_ setAutofillCreditCardEnabled:NO]);
  OCMExpect([consumer_ setAutofillProfileEnabled:YES]);
  OCMExpect([consumer_ setIdentityDocsEnabled:YES]);
  OCMExpect([consumer_ setTravelInfoEnabled:YES]);
  OCMExpect([consumer_ setShouldShowAutofillAIFeatures:YES]);

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer receives updates when the preferences change.
TEST_F(AutofillAndPasswordsMediatorTest, UpdatesConsumerOnPreferenceChange) {
  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setPasswordsEnabled:YES]);
  pref_service_->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                            YES);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setAutofillCreditCardEnabled:YES]);
  pref_service_->SetBoolean(autofill::prefs::kAutofillCreditCardEnabled, YES);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setAutofillProfileEnabled:YES]);
  pref_service_->SetBoolean(autofill::prefs::kAutofillProfileEnabled, YES);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setIdentityDocsEnabled:YES]);
  pref_service_->SetBoolean(autofill::prefs::kAutofillAiIdentityEntitiesEnabled,
                            YES);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setTravelInfoEnabled:YES]);
  pref_service_->SetBoolean(autofill::prefs::kAutofillAiTravelEntitiesEnabled,
                            YES);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that adding/removing local data dynamically updates whether Autofill AI
// features are shown when the schema is off.
TEST_F(AutofillAndPasswordsMediatorTest,
       UpdatesShouldShowAutofillAIFeaturesOnDataChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillAiWithDataSchema);

  pref_service_->SetBoolean(autofill::prefs::kAutofillAiIdentityEntitiesEnabled,
                            YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillAiTravelEntitiesEnabled,
                            YES);

  OCMExpect([consumer_ setPasswordsEnabled:YES]);
  OCMExpect([consumer_ setAutofillCreditCardEnabled:YES]);
  OCMExpect([consumer_ setAutofillProfileEnabled:YES]);
  OCMExpect([consumer_ setIdentityDocsEnabled:YES]);
  OCMExpect([consumer_ setTravelInfoEnabled:YES]);
  OCMExpect([consumer_ setShouldShowAutofillAIFeatures:NO]);

  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Adding data should trigger an update to show the features.
  OCMExpect([consumer_ setShouldShowAutofillAIFeatures:YES]);
  autofill::EntityInstance::EntityId entity_id = AddTestEntity();
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Removing the last data item should trigger an update to hide the features
  // again.
  OCMExpect([consumer_ setShouldShowAutofillAIFeatures:NO]);
  RemoveTestEntity(entity_id);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that setting the consumer to nil after disconnect does not crash.
// (e.g. during teardown where `setConsumer:nil` might evaluate arguments on a
// nullptr `_userPrefService`).
TEST_F(AutofillAndPasswordsMediatorTest,
       DoesNotCrashOnSetConsumerNilAfterDisconnect) {
  mediator_.consumer = consumer_;
  [mediator_ disconnect];

  mediator_.consumer = nil;
}

// Tests that setting a non-nil consumer after disconnect does not crash.
TEST_F(AutofillAndPasswordsMediatorTest,
       DoesNotCrashOnSetConsumerAfterDisconnect) {
  [mediator_ disconnect];

  mediator_.consumer = consumer_;
}
