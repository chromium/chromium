// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/network/autofill_ai/mock_wallet_pass_access_manager.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/fake_autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class AutofillAIEntityEditMediatorTest : public PlatformTest {
 protected:
  AutofillAIEntityEditMediatorTest() {
    scoped_feature_list_.InitWithFeatures(
        {autofill::features::kAutofillAiWithDataSchema,
         autofill::features::kAutofillAiCreateEntityDataManager},
        {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
    entity_data_manager_ =
        IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());

    consumer_ = [[FakeAutofillAIEntityEditConsumer alloc] init];
    mock_wallet_pass_manager_ = std::make_unique<
        testing::StrictMock<autofill::MockWalletPassAccessManager>>();
  }

  void TearDown() override {
    mediator_ = nil;
    consumer_ = nil;
    PlatformTest::TearDown();
  }

  // Helper method to create a mediator with a given entity instance.
  void CreateMediator(autofill::EntityInstance instance) {
    mediator_ = [[AutofillAIEntityEditMediator alloc]
        initWithEntityInstance:instance
             entityDataManager:entity_data_manager_
             walletPassManager:mock_wallet_pass_manager_.get()];
    mediator_.consumer = consumer_;
  }

  // Helper method to create a mediator with a given entity instance and
  // run verification.
  void VerifyEntity(autofill::EntityInstance instance,
                    NSUInteger expected_count) {
    CreateMediator(instance);
    EXPECT_NSEQ(consumer_.title,
                base::SysUTF16ToNSString(instance.type().GetNameForI18n()));
    EXPECT_GE(consumer_.editItems.count, expected_count);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<autofill::EntityDataManager> entity_data_manager_;
  std::unique_ptr<autofill::MockWalletPassAccessManager>
      mock_wallet_pass_manager_;
  FakeAutofillAIEntityEditConsumer* consumer_;
  AutofillAIEntityEditMediator* mediator_;
};

// Tests that the mediator can format a Passport entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensPassport) {
  VerifyEntity(autofill::test::GetPassportEntityInstance(), 3);
}

// Tests that the mediator can format a Driver's License entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensDriversLicense) {
  VerifyEntity(autofill::test::GetDriversLicenseEntityInstance(), 0);
}

// Tests that the mediator can format a Traveler Number entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensTravelerNumber) {
  VerifyEntity(autofill::test::GetKnownTravelerNumberInstance(), 0);
}

// Tests that the mediator can format a Vehicle entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensVehicle) {
  VerifyEntity(autofill::test::GetVehicleEntityInstance(), 0);
}

// Tests that the mediator can format a Redress Number entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensRedressNumber) {
  VerifyEntity(autofill::test::GetRedressNumberEntityInstance(), 0);
}

// Tests that the mediator can format a National Id Card entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensNationalIdCard) {
  VerifyEntity(autofill::test::GetNationalIdCardEntityInstance(), 0);
}

// Tests that the mediator can format a Flight Reservation entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensFlightReservation) {
  VerifyEntity(autofill::test::GetFlightReservationEntityInstance(), 3);
}

// Tests that the mediator can format an Order entity instance.
TEST_F(AutofillAIEntityEditMediatorTest, OpensOrder) {
  VerifyEntity(autofill::test::GetOrderEntityInstance(), 1);
}

// Tests that saving a wallet-eligible entity successfully calls the Wallet API,
// and saves the resulting entity.
TEST_F(AutofillAIEntityEditMediatorTest, SaveWalletEligibleEntity_Success) {
  // Create a wallet-eligible entity.
  const autofill::EntityInstance instance = autofill::test::MaskEntityInstance(
      autofill::test::GetPassportEntityInstance(
          {.record_type =
               autofill::EntityInstance::RecordType::kServerWallet}));
  CreateMediator(instance);

  EXPECT_CALL(*mock_wallet_pass_manager_,
              SaveWalletEntityInstance(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const autofill::EntityInstance& entity,
              const consent_auditor::ConsentAuditor::SessionId& session_id,
              autofill::WalletPassAccessManager::UpsertEntityInstanceCallback
                  callback) {
            autofill::EntityInstance masked_saved_entity =
                autofill::test::MaskEntityInstance(entity);
            std::move(callback).Run(masked_saved_entity);
          });

  [mediator_ saveEntityInstance];

  EXPECT_TRUE(consumer_.showLoadingStateCalled);
  EXPECT_TRUE(consumer_.hideLoadingStateCalled);
  EXPECT_TRUE(consumer_.didFinishSavingCalled);

  // Verify it was saved to the data manager asynchronously.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(instance.guid())
            .has_value();
      }));
}

// Tests that if the Wallet API fails to save, the mediator falls back to saving
// the entity locally.
TEST_F(AutofillAIEntityEditMediatorTest,
       SaveWalletEligibleEntity_FallbackToLocal) {
  const autofill::EntityInstance instance = autofill::test::MaskEntityInstance(
      autofill::test::GetPassportEntityInstance(
          {.record_type =
               autofill::EntityInstance::RecordType::kServerWallet}));
  CreateMediator(instance);

  // Expect the mock to be called and simulate a failure.
  EXPECT_CALL(*mock_wallet_pass_manager_,
              SaveWalletEntityInstance(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const autofill::EntityInstance& entity,
              const consent_auditor::ConsentAuditor::SessionId& session_id,
              autofill::WalletPassAccessManager::UpsertEntityInstanceCallback
                  callback) { std::move(callback).Run(std::nullopt); });

  [mediator_ saveEntityInstance];

  EXPECT_TRUE(consumer_.showLoadingStateCalled);
  EXPECT_TRUE(consumer_.hideLoadingStateCalled);
  EXPECT_TRUE(consumer_.didFinishSavingCalled);

  // Verify the fallback local entity was saved asynchronously.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(instance.guid())
            .has_value();
      }));

  // Verify the fallback local entity was saved.
  base::optional_ref<const autofill::EntityInstance> saved_instance =
      entity_data_manager_->GetEntityInstance(instance.guid());
  EXPECT_EQ(saved_instance->record_type(),
            autofill::EntityInstance::RecordType::kLocal);
}

// Tests that entities not eligible for wallet storage save directly to the data
// manager.
TEST_F(AutofillAIEntityEditMediatorTest,
       SaveNonWalletEligibleEntity_SavesLocally) {
  autofill::EntityInstance instance =
      autofill::test::GetVehicleEntityInstance();
  CreateMediator(instance);

  // The Wallet API should not be called for this entity type.
  EXPECT_CALL(*mock_wallet_pass_manager_,
              SaveWalletEntityInstance(testing::_, testing::_, testing::_))
      .Times(0);

  [mediator_ saveEntityInstance];

  // Loading state should not be triggered for synchronous local saves.
  EXPECT_FALSE(consumer_.showLoadingStateCalled);
  EXPECT_FALSE(consumer_.hideLoadingStateCalled);
  EXPECT_TRUE(consumer_.didFinishSavingCalled);

  // Verify it was saved.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(instance.guid())
            .has_value();
      }));
}
