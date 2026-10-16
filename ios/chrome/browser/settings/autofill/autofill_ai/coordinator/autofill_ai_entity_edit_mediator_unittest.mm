// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#import "components/autofill/core/browser/network/autofill_ai/mock_wallet_pass_access_manager.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/consent_auditor/fake_consent_auditor.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/sync/test/test_sync_service.h"
#import "components/wallet/core/common/wallet_features.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/fake_autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_entity_instance_builder.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

@interface FakeAutofillAIEntityEditMediatorDelegate
    : NSObject <AutofillAIEntityEditMediatorDelegate>
@property(nonatomic, assign) BOOL canPerformWalletSave;
@end

@implementation FakeAutofillAIEntityEditMediatorDelegate

- (BOOL)mediator:(AutofillAIEntityEditMediator*)mediator
    canPerformWalletSaveForType:(autofill::EntityType)type {
  return self.canPerformWalletSave;
}

@end

class AutofillAIEntityEditMediatorTest : public PlatformTest {
 protected:
  AutofillAIEntityEditMediatorTest() {
    scoped_feature_list_.InitWithFeatures(
        {autofill::features::kAutofillAiWithDataSchema,
         autofill::features::kAutofillEnableWalletDisclosureNoticePublicPass,
         wallet::features::kWalletApiPrivatePassesConsent},
        {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));

    profile_ = std::move(builder).Build();
    entity_data_manager_ =
        IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());

    consumer_ = [[FakeAutofillAIEntityEditConsumer alloc] init];
    mock_wallet_pass_manager_ = std::make_unique<
        testing::StrictMock<autofill::MockWalletPassAccessManager>>();

    fake_delegate_ = [[FakeAutofillAIEntityEditMediatorDelegate alloc] init];

    mockReauthModule_ = OCMProtocolMock(@protocol(ReauthenticationProtocol));

    // Sign in a primary account so IdentityManager can provide a valid GaiaId.
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
  }

  void TearDown() override {
    mediator_ = nil;
    consumer_ = nil;
    fake_delegate_ = nil;
    PlatformTest::TearDown();
  }

  // Helper method to create a mediator with a given entity instance and mode.
  void CreateMediator(
      autofill::EntityInstance instance,
      AutofillAIEntityEditMode mode = AutofillAIEntityEditMode::kViewAndEdit) {
    mediator_ = [[AutofillAIEntityEditMediator alloc]
        initWithEntityInstance:instance
                          mode:mode
             entityDataManager:entity_data_manager_
             walletPassManager:mock_wallet_pass_manager_.get()
                consentAuditor:&fake_consent_auditor_
               identityManager:identity_test_env_.identity_manager()
                  reauthModule:mockReauthModule_
                     userEmail:nil];
    mediator_.consumer = consumer_;
    mediator_.delegate = fake_delegate_;
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

  // Helper method to find an item by its attribute type.
  AutofillAIEntityEditItem* FindItem(autofill::AttributeTypeName type) {
    for (TableViewItem* item in consumer_.editItems) {
      if ([item isKindOfClass:[AutofillAIEntityEditItem class]]) {
        AutofillAIEntityEditItem* edit_item =
            base::apple::ObjCCastStrict<AutofillAIEntityEditItem>(item);
        if (edit_item.attributeType == type) {
          return edit_item;
        }
      }
    }
    return nil;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<autofill::EntityDataManager> entity_data_manager_;
  std::unique_ptr<autofill::MockWalletPassAccessManager>
      mock_wallet_pass_manager_;
  testing::NiceMock<consent_auditor::FakeConsentAuditor> fake_consent_auditor_;
  signin::IdentityTestEnvironment identity_test_env_;
  FakeAutofillAIEntityEditConsumer* consumer_;
  AutofillAIEntityEditMediator* mediator_;
  FakeAutofillAIEntityEditMediatorDelegate* fake_delegate_;
  id mockReauthModule_;
};

// Tests that the mediator can format a Passport entity instance and handles
// reauth.
TEST_F(AutofillAIEntityEditMediatorTest, OpensPassport) {
  // Define a passport number that should be obfuscated. This is identical to
  // the one in the default. It is re-declared here so this test is independent
  // of the default value.
  std::u16string passport_number = u"LR1234567";
  NSString* passport_number_nsstring =
      base::SysUTF16ToNSString(passport_number);
  autofill::EntityInstance instance = autofill::test::GetPassportEntityInstance(
      {.number = passport_number.c_str()});

  CreateMediator(instance);

  AutofillAIEntityEditItem* passport_number_item =
      FindItem(autofill::AttributeTypeName::kPassportNumber);

  ASSERT_TRUE(passport_number_item != nil);
  EXPECT_NSNE(passport_number_item.textFieldValue, passport_number_nsstring);

  OCMStub([mockReauthModule_ attemptReauthWithLocalizedReason:[OCMArg any]
                                         canReusePreviousAuth:YES
                                                      handler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^handler)(ReauthenticationResult);
        [invocation getArgument:&handler atIndex:4];
        handler(ReauthenticationResult::kSuccess);
      });

  __block BOOL reauth_completion_called = NO;
  [mediator_ requestEditingWithCompletion:^(ReauthenticationResult result) {
    EXPECT_EQ(result, ReauthenticationResult::kSuccess);
    reauth_completion_called = YES;
  }];

  EXPECT_TRUE(reauth_completion_called);

  passport_number_item = FindItem(autofill::AttributeTypeName::kPassportNumber);

  ASSERT_TRUE(passport_number_item != nil);
  EXPECT_NSEQ(passport_number_item.textFieldValue, passport_number_nsstring);
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
  fake_delegate_.canPerformWalletSave = YES;

  consent_auditor::ConsentAuditor::SessionId captured_session_id;
  EXPECT_CALL(fake_consent_auditor_, RecordWalletPrivatePassConsent)
      .WillOnce(testing::SaveArg<1>(&captured_session_id));

  EXPECT_CALL(*mock_wallet_pass_manager_,
              SaveWalletEntityInstance(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const autofill::EntityInstance& entity,
              const consent_auditor::ConsentAuditor::SessionId& session_id,
              autofill::WalletPassAccessManager::UpsertEntityInstanceCallback
                  callback) {
            EXPECT_EQ(session_id, captured_session_id);
            autofill::EntityInstance masked_saved_entity =
                autofill::test::MaskEntityInstance(entity);
            std::move(callback).Run(masked_saved_entity);
          });

  base::HistogramTester histogram_tester;

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

  histogram_tester.ExpectUniqueSample("Autofill.Ai.EntityUpdatedFromSettings",
                                      autofill::EntityTypeName::kPassport, 1);
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
  fake_delegate_.canPerformWalletSave = YES;

  // Expect the mock to be called and simulate a failure.
  EXPECT_CALL(*mock_wallet_pass_manager_,
              SaveWalletEntityInstance(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](const autofill::EntityInstance& entity,
              const consent_auditor::ConsentAuditor::SessionId& session_id,
              autofill::WalletPassAccessManager::UpsertEntityInstanceCallback
                  callback) { std::move(callback).Run(std::nullopt); });

  base::HistogramTester histogram_tester;

  [mediator_ saveEntityInstance];

  EXPECT_TRUE(consumer_.showLoadingStateCalled);
  EXPECT_TRUE(consumer_.hideLoadingStateCalled);
  EXPECT_TRUE(consumer_.didFinishSavingToLocalAsFallbackCalled);

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

  histogram_tester.ExpectUniqueSample("Autofill.Ai.EntityUpdatedFromSettings",
                                      autofill::EntityTypeName::kPassport, 1);
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

  base::HistogramTester histogram_tester;

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

  histogram_tester.ExpectUniqueSample("Autofill.Ai.EntityUpdatedFromSettings",
                                      autofill::EntityTypeName::kVehicle, 1);
}

// Tests that saving an entity in Create mode logs the Added metric.
TEST_F(AutofillAIEntityEditMediatorTest, SaveEntity_CreateMode) {
  autofill::EntityInstance instance =
      autofill::test::GetVehicleEntityInstance();
  CreateMediator(instance, AutofillAIEntityEditMode::kCreate);

  base::HistogramTester histogram_tester;

  [mediator_ saveEntityInstance];

  EXPECT_TRUE(consumer_.didFinishSavingCalled);

  // Verify it was saved.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(instance.guid())
            .has_value();
      }));

  histogram_tester.ExpectUniqueSample("Autofill.Ai.EntityAddedFromSettings",
                                      autofill::EntityTypeName::kVehicle, 1);
}

// Tests that the mediator sets the correct title in Create mode.
TEST_F(AutofillAIEntityEditMediatorTest, SetsCreateModeTitle) {
  autofill::EntityInstance instance =
      autofill::test::GetVehicleEntityInstance();
  CreateMediator(instance, AutofillAIEntityEditMode::kCreate);
  EXPECT_EQ(consumer_.mode, AutofillAIEntityEditMode::kCreate);
  EXPECT_NSEQ(consumer_.title,
              autofill::GetDialogTitleForAddEntity(instance.type().name()));
}

// Tests that a public pass eligible for a wallet disclosure notice without
// preloaded disclosure details is presented and saved as a local entity.
TEST_F(AutofillAIEntityEditMediatorTest,
       PublicPass_NoPreloadedDetails_FallsBackToLocal) {
  autofill::EntityInstance wallet_instance =
      autofill::test::GetVehicleEntityInstance(
          {.record_type = autofill::EntityInstance::WalletRecordTypePayload{
               .management_url = GURL()}});
  autofill::EntityInstance::EntityId guid = wallet_instance.guid();

  EXPECT_CALL(*mock_wallet_pass_manager_,
              ExtractPreloadedDetailsForUpsertPass(wallet_instance.type()))
      .WillOnce(testing::Return(std::nullopt));

  CreateMediator(std::move(wallet_instance), AutofillAIEntityEditMode::kCreate);

  EXPECT_FALSE(consumer_.isServerWalletItem);
  EXPECT_EQ(consumer_.legalMessages.count, 0u);

  [mediator_ saveEntityInstance];

  // The entity was presented as local, so no fallback alert is needed.
  EXPECT_TRUE(consumer_.didFinishSavingCalled);
  EXPECT_FALSE(consumer_.didFinishSavingToLocalAsFallbackCalled);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(guid).has_value();
      }));

  base::optional_ref<const autofill::EntityInstance> saved_instance =
      entity_data_manager_->GetEntityInstance(guid);
  EXPECT_EQ(saved_instance->record_type(),
            autofill::EntityInstance::RecordType::kLocal);
}

// Tests that when the wallet disclosure notice feature is disabled, a public
// pass is saved to Google Wallet without showing any legal messages and without
// consuming the preloaded single-use disclosure details.
TEST_F(AutofillAIEntityEditMediatorTest,
       FeatureDisabled_PublicPass_SavesToWalletWithoutDisclosure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillEnableWalletDisclosureNoticePublicPass);

  autofill::EntityInstance wallet_instance =
      autofill::test::GetVehicleEntityInstance(
          {.record_type = autofill::EntityInstance::WalletRecordTypePayload{
               .management_url = GURL()}});
  autofill::EntityInstance::EntityId guid = wallet_instance.guid();

  EXPECT_CALL(*mock_wallet_pass_manager_, ExtractPreloadedDetailsForUpsertPass)
      .Times(0);

  CreateMediator(std::move(wallet_instance), AutofillAIEntityEditMode::kCreate);

  // No disclosure notice is required, so no legal messages are shown and the
  // entity is still presented and saved as a Wallet item.
  EXPECT_EQ(consumer_.legalMessages.count, 0u);
  EXPECT_TRUE(consumer_.isServerWalletItem);

  [mediator_ saveEntityInstance];

  EXPECT_TRUE(consumer_.didFinishSavingCalled);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(guid).has_value();
      }));

  base::optional_ref<const autofill::EntityInstance> saved_instance =
      entity_data_manager_->GetEntityInstance(guid);
  EXPECT_EQ(saved_instance->record_type(),
            autofill::EntityInstance::RecordType::kServerWallet);
}

// Tests that when an entity is ineligible for a wallet disclosure notice, legal
// messages are not pushed to the consumer and the preloaded single-use
// disclosure details are not consumed.
TEST_F(AutofillAIEntityEditMediatorTest,
       IneligibleEntity_DoesNotConsumePreloadedDetails) {
  // Local vehicle entities are ineligible for wallet disclosure notices.
  autofill::EntityInstance instance =
      autofill::test::GetVehicleEntityInstance();

  EXPECT_CALL(*mock_wallet_pass_manager_, ExtractPreloadedDetailsForUpsertPass)
      .Times(0);

  CreateMediator(instance, AutofillAIEntityEditMode::kCreate);

  // Legal messages should be suppressed because the entity is ineligible.
  EXPECT_EQ(consumer_.legalMessages.count, 0u);

  [mediator_ saveEntityInstance];

  EXPECT_TRUE(consumer_.didFinishSavingCalled);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(instance.guid())
            .has_value();
      }));
}

// Tests that when an eligible user has preloaded disclosure details, legal
// messages are pushed to the consumer and the entity is saved to Wallet.
TEST_F(AutofillAIEntityEditMediatorTest,
       PublicPass_WithPreloadedDetails_PushesLegalMessagesAndSaves) {
  autofill::EntityInstance wallet_instance =
      autofill::test::GetVehicleEntityInstance(
          {.record_type = autofill::EntityInstance::WalletRecordTypePayload{
               .management_url = GURL()}});
  autofill::EntityInstance::EntityId guid = wallet_instance.guid();

  autofill::WalletPassAccessManager::GetDetailsForUpsertPassResponse response;
  response.context_token = "test_context_token";
  response.legal_message_lines.push_back(
      autofill::TestLegalMessageLine("Test Legal Message"));
  response.user_eligibility =
      autofill::WalletPassAccessManager::UserEligibility::kEligible;

  EXPECT_CALL(*mock_wallet_pass_manager_,
              ExtractPreloadedDetailsForUpsertPass(wallet_instance.type()))
      .WillOnce(testing::Return(response));

  CreateMediator(std::move(wallet_instance), AutofillAIEntityEditMode::kCreate);

  // Legal messages should be pushed to the consumer.
  EXPECT_EQ(consumer_.legalMessages.count, 1u);
  EXPECT_TRUE(consumer_.isServerWalletItem);

  base::HistogramTester histogram_tester;
  [mediator_ saveEntityInstance];
  [mediator_ disconnect];

  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.WalletNotice.Settings.Funnel",
      autofill::AutofillAiWalletNoticeFunnelEvents::kEntitySaved, 1);
  EXPECT_TRUE(consumer_.didFinishSavingCalled);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(guid).has_value();
      }));
}

// Tests that when an eligible user has preloaded disclosure details and the
// mediator disconnects without saving, `kEntityNotSaved` is logged.
TEST_F(
    AutofillAIEntityEditMediatorTest,
    PublicPass_WithPreloadedDetails_DisconnectWithoutSaveLogsEntityNotSaved) {
  autofill::EntityInstance wallet_instance =
      autofill::test::GetVehicleEntityInstance(
          {.record_type = autofill::EntityInstance::WalletRecordTypePayload{
               .management_url = GURL()}});

  autofill::WalletPassAccessManager::GetDetailsForUpsertPassResponse response;
  response.context_token = "test_context_token";
  response.legal_message_lines.push_back(
      autofill::TestLegalMessageLine("Test Legal Message"));
  response.user_eligibility =
      autofill::WalletPassAccessManager::UserEligibility::kEligible;

  EXPECT_CALL(*mock_wallet_pass_manager_,
              ExtractPreloadedDetailsForUpsertPass(wallet_instance.type()))
      .WillOnce(testing::Return(response));

  CreateMediator(std::move(wallet_instance), AutofillAIEntityEditMode::kCreate);

  base::HistogramTester histogram_tester;
  [mediator_ disconnect];

  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.WalletNotice.Settings.Funnel",
      autofill::AutofillAiWalletNoticeFunnelEvents::kEntityNotSaved, 1);
}

// Tests that a user who is not eligible to see the Google Wallet disclosure
// notice saves a public pass to Google Wallet without legal messages, matching
// the desktop save prompt.
TEST_F(AutofillAIEntityEditMediatorTest,
       PublicPass_IneligibleUser_SavesToWalletWithoutDisclosure) {
  autofill::EntityInstance wallet_instance =
      autofill::test::GetVehicleEntityInstance(
          {.record_type = autofill::EntityInstance::WalletRecordTypePayload{
               .management_url = GURL()}});
  autofill::EntityInstance::EntityId guid = wallet_instance.guid();

  autofill::WalletPassAccessManager::GetDetailsForUpsertPassResponse response;
  response.context_token = "test_context_token";
  response.legal_message_lines.push_back(
      autofill::TestLegalMessageLine("Test Legal Message"));
  response.user_eligibility =
      autofill::WalletPassAccessManager::UserEligibility::kIneligible;

  EXPECT_CALL(*mock_wallet_pass_manager_,
              ExtractPreloadedDetailsForUpsertPass(wallet_instance.type()))
      .WillOnce(testing::Return(response));

  CreateMediator(std::move(wallet_instance), AutofillAIEntityEditMode::kCreate);

  EXPECT_EQ(consumer_.legalMessages.count, 0u);
  EXPECT_TRUE(consumer_.isServerWalletItem);

  [mediator_ saveEntityInstance];

  EXPECT_TRUE(consumer_.didFinishSavingCalled);
  EXPECT_FALSE(consumer_.didFinishSavingToLocalAsFallbackCalled);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(guid).has_value();
      }));

  base::optional_ref<const autofill::EntityInstance> saved_instance =
      entity_data_manager_->GetEntityInstance(guid);
  EXPECT_EQ(saved_instance->record_type(),
            autofill::EntityInstance::RecordType::kServerWallet);
}

// Tests that an eligible user's response without legal messages cannot back
// the disclosure notice, so the public pass falls back to local storage.
TEST_F(AutofillAIEntityEditMediatorTest,
       PublicPass_EligibleUserWithoutLegalMessages_FallsBackToLocal) {
  autofill::EntityInstance wallet_instance =
      autofill::test::GetVehicleEntityInstance(
          {.record_type = autofill::EntityInstance::WalletRecordTypePayload{
               .management_url = GURL()}});
  autofill::EntityInstance::EntityId guid = wallet_instance.guid();

  autofill::WalletPassAccessManager::GetDetailsForUpsertPassResponse response;
  response.context_token = "test_context_token";
  response.user_eligibility =
      autofill::WalletPassAccessManager::UserEligibility::kEligible;

  EXPECT_CALL(*mock_wallet_pass_manager_,
              ExtractPreloadedDetailsForUpsertPass(wallet_instance.type()))
      .WillOnce(testing::Return(response));

  CreateMediator(std::move(wallet_instance), AutofillAIEntityEditMode::kCreate);

  EXPECT_EQ(consumer_.legalMessages.count, 0u);
  EXPECT_FALSE(consumer_.isServerWalletItem);

  [mediator_ saveEntityInstance];

  // The entity was presented as local, so no fallback alert is needed.
  EXPECT_TRUE(consumer_.didFinishSavingCalled);
  EXPECT_FALSE(consumer_.didFinishSavingToLocalAsFallbackCalled);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(guid).has_value();
      }));

  base::optional_ref<const autofill::EntityInstance> saved_instance =
      entity_data_manager_->GetEntityInstance(guid);
  EXPECT_EQ(saved_instance->record_type(),
            autofill::EntityInstance::RecordType::kLocal);
}

// Tests that an eligible user's response without a context token cannot back
// the disclosure notice, so the public pass falls back to local storage.
TEST_F(AutofillAIEntityEditMediatorTest,
       PublicPass_EligibleUserWithoutContextToken_FallsBackToLocal) {
  autofill::EntityInstance wallet_instance =
      autofill::test::GetVehicleEntityInstance(
          {.record_type = autofill::EntityInstance::WalletRecordTypePayload{
               .management_url = GURL()}});
  autofill::EntityInstance::EntityId guid = wallet_instance.guid();

  autofill::WalletPassAccessManager::GetDetailsForUpsertPassResponse response;
  response.legal_message_lines.push_back(
      autofill::TestLegalMessageLine("Test Legal Message"));
  response.user_eligibility =
      autofill::WalletPassAccessManager::UserEligibility::kEligible;

  EXPECT_CALL(*mock_wallet_pass_manager_,
              ExtractPreloadedDetailsForUpsertPass(wallet_instance.type()))
      .WillOnce(testing::Return(response));

  CreateMediator(std::move(wallet_instance), AutofillAIEntityEditMode::kCreate);

  EXPECT_EQ(consumer_.legalMessages.count, 0u);
  EXPECT_FALSE(consumer_.isServerWalletItem);

  [mediator_ saveEntityInstance];

  // The entity was presented as local, so no fallback alert is needed.
  EXPECT_TRUE(consumer_.didFinishSavingCalled);
  EXPECT_FALSE(consumer_.didFinishSavingToLocalAsFallbackCalled);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager_->GetEntityInstance(guid).has_value();
      }));

  base::optional_ref<const autofill::EntityInstance> saved_instance =
      entity_data_manager_->GetEntityInstance(guid);
  EXPECT_EQ(saved_instance->record_type(),
            autofill::EntityInstance::RecordType::kLocal);
}

// Tests that viewing or editing an existing public pass does not consume the
// preloaded single-use disclosure details, which are reserved for the create
// flow.
TEST_F(AutofillAIEntityEditMediatorTest,
       PublicPass_ViewAndEditMode_DoesNotConsumePreloadedDetails) {
  autofill::EntityInstance wallet_instance =
      autofill::test::GetVehicleEntityInstance(
          {.record_type = autofill::EntityInstance::WalletRecordTypePayload{
               .management_url = GURL()}});

  EXPECT_CALL(*mock_wallet_pass_manager_, ExtractPreloadedDetailsForUpsertPass)
      .Times(0);

  CreateMediator(std::move(wallet_instance),
                 AutofillAIEntityEditMode::kViewAndEdit);
}
