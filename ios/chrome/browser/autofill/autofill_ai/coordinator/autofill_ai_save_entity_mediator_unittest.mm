// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_mediator.h"

#import <tuple>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_consumer.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_prompt_model.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

autofill::EntityInstance CreateTestEntity() {
  return autofill::test::GetPassportEntityInstance();
}

}  // namespace

@interface FakeAutofillAISaveEntityConsumer
    : NSObject <AutofillAISaveEntityConsumer>
@property(nonatomic, copy) NSString* footerText;
@property(nonatomic, copy) NSArray<CrURL*>* footerURLs;
@property(nonatomic, assign) BOOL setNewEntityCalled;
@property(nonatomic, assign) BOOL showsExplicitDeclineButton;
@end

@implementation FakeAutofillAISaveEntityConsumer

- (void)setNewEntity:(autofill::EntityInstance)newEntity
            oldEntity:(std::optional<autofill::EntityInstance>)oldEntity
    saveIsSynchronous:(BOOL)saveIsSynchronous {
  self.setNewEntityCalled = YES;
}

- (void)setFooterText:(NSString*)footerText urls:(NSArray<CrURL*>*)urls {
  self.footerText = footerText;
  self.footerURLs = urls;
}

- (void)showLoadingState {
}

- (void)showConfirmationState {
}

@end

class AutofillAISaveEntityMediatorTest : public PlatformTest {
 protected:
  // Creates a mediator with the given entity, pushes data to a fake consumer,
  // and returns the consumer.
  FakeAutofillAISaveEntityConsumer* PushEntityToMediator(
      autofill::EntityInstance entity,
      std::optional<autofill::EntityInstance> old_entity = std::nullopt,
      autofill::LegalMessageLines legal_message_lines = {},
      bool show_explicit_decline_button = false) {
    autofill::SaveEntityParams params(std::move(entity), std::move(old_entity),
                                      /*user_email=*/u"test@example.com",
                                      /*save_is_synchronous=*/true,
                                      /*callback=*/base::DoNothing(),
                                      std::move(legal_message_lines),
                                      show_explicit_decline_button);

    AutofillAISaveEntityMediator* mediator =
        [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(params)
                                           entityDataManager:nullptr];

    FakeAutofillAISaveEntityConsumer* consumer =
        [[FakeAutofillAISaveEntityConsumer alloc] init];
    mediator.consumer = consumer;

    [mediator disconnect];
    return consumer;
  }

  // Configures the feature flag, fake web state, and tab helper for tab helper
  // integration test cases.
  void SetUpTabHelperIntegration() {
    scoped_feature_list_.InitAndEnableFeature(
        kAutofillReshowSavePromptOnTabReturn);
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web::ContentWorld content_world =
        AutofillBottomSheetJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    fake_web_state_.SetWebFramesManager(content_world,
                                        std::move(frames_manager));
    AutofillBottomSheetTabHelper::CreateForWebState(&fake_web_state_);
    tab_helper_ = AutofillBottomSheetTabHelper::FromWebState(&fake_web_state_);
  }

  // Creates SaveEntityParams with the provided callback and entity.
  autofill::SaveEntityParams CreateDefaultSaveEntityParams(
      base::OnceCallback<void(
          autofill::AutofillClient::AutofillAiBubbleResult,
          std::optional<autofill::EntityInstance>,
          const autofill::AutofillClient::EntityImportUIContext&)> callback,
      autofill::EntityInstance entity = CreateTestEntity()) {
    return autofill::SaveEntityParams(
        std::move(entity), /*old_entity=*/std::nullopt,
        /*user_email=*/u"test@example.com",
        /*save_is_synchronous=*/true, std::move(callback),
        /*public_passes_notice=*/{},
        /*show_explicit_decline_button=*/false);
  }

  // Sets up tab helper integration, stages the prompt model, and creates a
  // mediator initialized with the tab helper.
  AutofillAISaveEntityMediator* CreateMediatorWithPromptModel(
      autofill::SaveEntityParams params) {
    SetUpTabHelperIntegration();
    auto model =
        std::make_unique<AutofillAiSaveEntityPromptModel>(std::move(params));
    tab_helper_->ShowAutofillAiSaveEntityBottomSheet(std::move(model));
    EXPECT_NE(nullptr, tab_helper_->GetAutofillAiSaveEntityPromptModel());
    return [[AutofillAISaveEntityMediator alloc] initWithTabHelper:tab_helper_
                                                 entityDataManager:nullptr];
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  web::FakeWebState fake_web_state_;
  raw_ptr<AutofillBottomSheetTabHelper> tab_helper_ = nullptr;
};

// Tests that the footer displays the local storage notice and no legal messages
// when saving locally with empty legal message lines.
TEST_F(AutofillAISaveEntityMediatorTest,
       PushesLocalSaveFooterWithoutLegalMessages) {
  base::HistogramTester histogram_tester;
  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(CreateTestEntity());

  EXPECT_TRUE(consumer.setNewEntityCalled);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_DEVICE),
              consumer.footerText);
  EXPECT_EQ(0u, consumer.footerURLs.count);
  histogram_tester.ExpectTotalCount("Autofill.Ai.WalletNotice.Save.Funnel", 0);
}

// Tests that legal messages are combined with the storage notice and
// `kLegalMessageShown` is logged when legal messages are present in params.
TEST_F(AutofillAISaveEntityMediatorTest,
       PushesFooterWithLegalMessagesWhenPresent) {
  base::HistogramTester histogram_tester;
  autofill::LegalMessageLines legal_message_lines = {
      autofill::TestLegalMessageLine(
          "Test legal disclosure",
          {autofill::LegalMessageLine::Link(0, 4,
                                            "https://www.example.com/legal")})};

  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(CreateTestEntity(), /*old_entity=*/std::nullopt,
                           std::move(legal_message_lines));

  EXPECT_TRUE(consumer.setNewEntityCalled);
  NSString* expectedText = [NSString
      stringWithFormat:@"%@\n\nBEGIN_LINKTestEND_LINK legal disclosure",
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_DEVICE)];
  EXPECT_NSEQ(expectedText, consumer.footerText);
  ASSERT_EQ(1u, consumer.footerURLs.count);
  EXPECT_EQ(GURL("https://www.example.com/legal"), consumer.footerURLs[0].gurl);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.WalletNotice.Save.Funnel",
      autofill::AutofillAiWalletNoticeFunnelEvents::kLegalMessageShown, 1);
}

// Tests that an invalid URL in a legal message is emitted as plain text and
// does not add to `footerURLs`.
TEST_F(AutofillAISaveEntityMediatorTest,
       InvalidLegalMessageURLDoesNotAddFooterLink) {
  autofill::LegalMessageLines legal_message_lines = {
      autofill::TestLegalMessageLine(
          "Test invalid legal disclosure",
          {autofill::LegalMessageLine::Link(0, 4, "invalid-url")})};

  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(CreateTestEntity(), /*old_entity=*/std::nullopt,
                           std::move(legal_message_lines));

  NSString* expectedText = [NSString
      stringWithFormat:@"%@\n\nTest invalid legal disclosure",
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_DEVICE)];
  EXPECT_NSEQ(expectedText, consumer.footerText);
  EXPECT_EQ(0u, consumer.footerURLs.count);
}

// Tests that saving a private pass to Wallet with
// `kAutofillRegionalizedPrivatePassesUxRefine` enabled pushes the refined
// private pass storage notice and both URLs (manage info and Privacy Policy).
TEST_F(AutofillAISaveEntityMediatorTest,
       PushesPrivatePassWalletFooterWithRefinedUx) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillRegionalizedPrivatePassesUxRefine);

  autofill::EntityInstance passport = autofill::test::GetPassportEntityInstance(
      {.record_type = autofill::EntityInstance::RecordType::kServerWallet});
  autofill::AutofillAiEntityFooterContent expected_footer =
      autofill::GetEntityFooterContent(passport, /*is_update=*/false,
                                       u"test@example.com",
                                       /*legal_messages=*/nil);
  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(std::move(passport));
  EXPECT_NSEQ(expected_footer.text, consumer.footerText);
  ASSERT_EQ(2u, consumer.footerURLs.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), consumer.footerURLs[0].gurl);
  EXPECT_EQ(autofill::GetGooglePrivacyPolicyURL(), consumer.footerURLs[1].gurl);
}

// Tests that saving a public pass to Wallet with
// `kAutofillRegionalizedPrivatePassesUxRefine` enabled pushes the standard
// public pass storage notice and single URL.
TEST_F(AutofillAISaveEntityMediatorTest,
       PushesPublicPassWalletFooterWithRefinedUx) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillRegionalizedPrivatePassesUxRefine);

  autofill::EntityInstance vehicle =
      autofill::test::GetVehicleEntityInstance().CopyWithNewRecordType(
          autofill::EntityInstance::RecordType::kServerWallet);
  autofill::AutofillAiEntityFooterContent expected_footer =
      autofill::GetEntityFooterContent(vehicle, /*is_update=*/false,
                                       u"test@example.com",
                                       /*legal_messages=*/nil);
  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(std::move(vehicle));

  EXPECT_NSEQ(expected_footer.text, consumer.footerText);
  ASSERT_EQ(1u, consumer.footerURLs.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), consumer.footerURLs[0].gurl);
}

// Tests that updating an entity in Wallet pushes the update footer text and
// Google Wallet passes URL.
TEST_F(AutofillAISaveEntityMediatorTest, PushesWalletUpdateFooter) {
  autofill::EntityInstance new_entity =
      autofill::test::GetVehicleEntityInstance().CopyWithNewRecordType(
          autofill::EntityInstance::RecordType::kServerWallet);
  autofill::EntityInstance old_entity =
      autofill::test::GetVehicleEntityInstance().CopyWithNewRecordType(
          autofill::EntityInstance::RecordType::kServerWallet);
  autofill::AutofillAiEntityFooterContent expected_footer =
      autofill::GetEntityFooterContent(new_entity, /*is_update=*/true,
                                       u"test@example.com",
                                       /*legal_messages=*/nil);

  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(std::move(new_entity), std::move(old_entity));

  EXPECT_NSEQ(expected_footer.text, consumer.footerText);
  ASSERT_EQ(1u, consumer.footerURLs.count);
  EXPECT_EQ(autofill::GetGoogleWalletPassesURL(), consumer.footerURLs[0].gurl);
}

// Tests that `didTapFooterLinkURL:` logs `kLinkClicked` only when the tapped
// URL belongs to the legal message.
TEST_F(AutofillAISaveEntityMediatorTest,
       DidTapFooterLinkURL_LogsLinkClickedOnlyForLegalMessageURL) {
  base::HistogramTester histogram_tester;
  autofill::EntityInstance new_entity =
      autofill::test::GetVehicleEntityInstance().CopyWithNewRecordType(
          autofill::EntityInstance::RecordType::kServerWallet);
  autofill::LegalMessageLines legal_message_lines = {
      autofill::TestLegalMessageLine(
          "Test legal disclosure",
          {autofill::LegalMessageLine::Link(0, 4,
                                            "https://www.example.com/legal")})};

  autofill::SaveEntityParams params(
      std::move(new_entity), /*old_entity=*/std::nullopt,
      /*user_email=*/u"test@example.com",
      /*save_is_synchronous=*/false,
      /*callback=*/base::DoNothing(),
      /*public_passes_notice=*/std::move(legal_message_lines),
      /*show_explicit_decline_button=*/false);

  AutofillAISaveEntityMediator* mediator =
      [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(params)
                                         entityDataManager:nullptr];
  FakeAutofillAISaveEntityConsumer* consumer =
      [[FakeAutofillAISaveEntityConsumer alloc] init];
  mediator.consumer = consumer;

  ASSERT_EQ(2u, consumer.footerURLs.count);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.WalletNotice.Save.Funnel",
      autofill::AutofillAiWalletNoticeFunnelEvents::kLegalMessageShown, 1);

  // Tapping a non-legal URL does not log `kLinkClicked`.
  [mediator didTapFooterLinkURL:autofill::GetManageYourInfoURL()];
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.WalletNotice.Save.Funnel",
      autofill::AutofillAiWalletNoticeFunnelEvents::kLinkClicked, 0);

  // Tapping a legal message URL logs `kLinkClicked`.
  [mediator didTapFooterLinkURL:GURL("https://www.example.com/legal")];
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.WalletNotice.Save.Funnel",
      autofill::AutofillAiWalletNoticeFunnelEvents::kLinkClicked, 1);

  [mediator disconnect];
}

// Tests that `showsExplicitDeclineButton` is pushed to consumer when true in
// params.
TEST_F(AutofillAISaveEntityMediatorTest, PushesShowsExplicitDeclineButtonTrue) {
  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(CreateTestEntity(), /*old_entity=*/std::nullopt,
                           /*legal_message_lines=*/{},
                           /*show_explicit_decline_button=*/true);
  EXPECT_TRUE(consumer.showsExplicitDeclineButton);
}

// Tests that `showsExplicitDeclineButton` is false by default.
TEST_F(AutofillAISaveEntityMediatorTest,
       PushesShowsExplicitDeclineButtonFalse) {
  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(CreateTestEntity());

  EXPECT_FALSE(consumer.showsExplicitDeclineButton);
}

// Tests that `acceptSaving` invokes the completion callback with `kAccepted`
// when the feature flag is disabled.
TEST_F(AutofillAISaveEntityMediatorTest, AcceptSavingReportsAccepted) {
  scoped_feature_list_.InitAndDisableFeature(
      kAutofillReshowSavePromptOnTabReturn);
  std::optional<autofill::AutofillClient::AutofillAiBubbleResult>
      callback_result;
  autofill::SaveEntityParams params =
      CreateDefaultSaveEntityParams(base::BindLambdaForTesting(
          [&](autofill::AutofillClient::AutofillAiBubbleResult result,
              std::optional<autofill::EntityInstance> entity,
              const autofill::AutofillClient::EntityImportUIContext& context) {
            callback_result = result;
          }));

  AutofillAISaveEntityMediator* mediator =
      [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(params)
                                         entityDataManager:nullptr];
  [mediator acceptSaving];

  EXPECT_EQ(callback_result,
            autofill::AutofillClient::AutofillAiBubbleResult::kAccepted);
  [mediator disconnect];
}

// Tests that `cancelSaving` invokes the completion callback with `kCancelled`
// when the feature flag is disabled.
TEST_F(AutofillAISaveEntityMediatorTest, CancelSavingReportsCancelled) {
  scoped_feature_list_.InitAndDisableFeature(
      kAutofillReshowSavePromptOnTabReturn);
  std::optional<autofill::AutofillClient::AutofillAiBubbleResult>
      callback_result;
  autofill::SaveEntityParams params =
      CreateDefaultSaveEntityParams(base::BindLambdaForTesting(
          [&](autofill::AutofillClient::AutofillAiBubbleResult result,
              std::optional<autofill::EntityInstance> entity,
              const autofill::AutofillClient::EntityImportUIContext& context) {
            callback_result = result;
          }));

  AutofillAISaveEntityMediator* mediator =
      [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(params)
                                         entityDataManager:nullptr];
  [mediator cancelSaving];

  EXPECT_EQ(callback_result,
            autofill::AutofillClient::AutofillAiBubbleResult::kCancelled);
  [mediator disconnect];
}

// Tests that under the legacy fallback path, `dismissSaving` falls back to
// `cancelSaving` and reports `kCancelled`.
TEST_F(AutofillAISaveEntityMediatorTest,
       DismissSavingFallsBackToCancelSavingWhenFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      kAutofillReshowSavePromptOnTabReturn);

  std::optional<autofill::AutofillClient::AutofillAiBubbleResult>
      callback_result;
  autofill::SaveEntityParams params =
      CreateDefaultSaveEntityParams(base::BindLambdaForTesting(
          [&](autofill::AutofillClient::AutofillAiBubbleResult result,
              std::optional<autofill::EntityInstance> entity,
              const autofill::AutofillClient::EntityImportUIContext& context) {
            callback_result = result;
          }));

  AutofillAISaveEntityMediator* mediator =
      [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(params)
                                         entityDataManager:nullptr];
  [mediator dismissSaving];

  EXPECT_EQ(callback_result,
            autofill::AutofillClient::AutofillAiBubbleResult::kCancelled);
  [mediator disconnect];
}

// Tests that under the legacy fallback path, disconnecting without user action
// invokes the completion callback with `kUnknown`.
TEST_F(AutofillAISaveEntityMediatorTest,
       DisconnectInvokesCallbackWithUnknownWhenFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      kAutofillReshowSavePromptOnTabReturn);

  std::optional<autofill::AutofillClient::AutofillAiBubbleResult>
      callback_result;
  autofill::SaveEntityParams params =
      CreateDefaultSaveEntityParams(base::BindLambdaForTesting(
          [&](autofill::AutofillClient::AutofillAiBubbleResult result,
              std::optional<autofill::EntityInstance> entity,
              const autofill::AutofillClient::EntityImportUIContext& context) {
            callback_result = result;
          }));

  AutofillAISaveEntityMediator* mediator =
      [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(params)
                                         entityDataManager:nullptr];
  [mediator disconnect];

  EXPECT_EQ(callback_result,
            autofill::AutofillClient::AutofillAiBubbleResult::kUnknown);
}

// Tests that when initialized with model and tab helper and the feature flag is
// enabled, `acceptSaving` consumes the model from the tab helper and reports
// `kAccepted`.
TEST_F(AutofillAISaveEntityMediatorTest,
       TabHelperIntegrationAcceptConsumesModel) {
  std::optional<autofill::AutofillClient::AutofillAiBubbleResult>
      callback_result;
  AutofillAISaveEntityMediator* mediator = CreateMediatorWithPromptModel(
      CreateDefaultSaveEntityParams(base::BindLambdaForTesting(
          [&](autofill::AutofillClient::AutofillAiBubbleResult result,
              std::optional<autofill::EntityInstance> entity,
              const autofill::AutofillClient::EntityImportUIContext& context) {
            callback_result = result;
          })));

  [mediator acceptSaving];

  EXPECT_EQ(callback_result,
            autofill::AutofillClient::AutofillAiBubbleResult::kAccepted);
  EXPECT_EQ(nullptr, tab_helper_->GetAutofillAiSaveEntityPromptModel());
  [mediator disconnect];
}

// Tests that when initialized with a `ServerWallet` entity and tab helper,
// `acceptSaving` populates `EntityImportUIContext` on the prompt model.
TEST_F(AutofillAISaveEntityMediatorTest,
       TabHelperIntegrationAcceptServerWalletPassesUIContext) {
  std::optional<autofill::AutofillClient::AutofillAiBubbleResult>
      callback_result;
  std::optional<autofill::AutofillClient::EntityImportUIContext> ui_context;
  autofill::EntityInstance vehicle =
      autofill::test::GetVehicleEntityInstance().CopyWithNewRecordType(
          autofill::EntityInstance::RecordType::kServerWallet);
  AutofillAISaveEntityMediator* mediator =
      CreateMediatorWithPromptModel(CreateDefaultSaveEntityParams(
          base::BindLambdaForTesting(
              [&](autofill::AutofillClient::AutofillAiBubbleResult result,
                  std::optional<autofill::EntityInstance> entity,
                  const autofill::AutofillClient::EntityImportUIContext&
                      context) {
                callback_result = result;
                ui_context = context;
              }),
          std::move(vehicle)));
  // Setting a consumer displays the footer, whose strings are reported as the
  // accepted consent.
  FakeAutofillAISaveEntityConsumer* consumer =
      [[FakeAutofillAISaveEntityConsumer alloc] init];
  mediator.consumer = consumer;

  [mediator acceptSaving];

  EXPECT_EQ(callback_result,
            autofill::AutofillClient::AutofillAiBubbleResult::kAccepted);
  ASSERT_TRUE(ui_context.has_value());
  EXPECT_THAT(ui_context->accepted_consent_string_ids,
              testing::ElementsAre(
                  IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_NEW,
                  IDS_AUTOFILL_GOOGLE_WALLET_TITLE,
                  IDS_AUTOFILL_MANAGE_YOUR_INFO_LINK));
  EXPECT_EQ(ui_context->accept_button_string_id,
            autofill::GetSaveEntityAcceptButtonStringId());
  EXPECT_EQ(nullptr, tab_helper_->GetAutofillAiSaveEntityPromptModel());
  [mediator disconnect];
}

// Tests that accepting the save of a private pass to Wallet with the refined UX
// enabled reports the IDs of all strings shown in the private pass notice.
TEST_F(AutofillAISaveEntityMediatorTest,
       TabHelperIntegrationAcceptPrivatePassWithRefinedUxPassesUIContext) {
  std::optional<autofill::AutofillClient::EntityImportUIContext> ui_context;
  AutofillAISaveEntityMediator* mediator =
      CreateMediatorWithPromptModel(CreateDefaultSaveEntityParams(
          base::BindLambdaForTesting(
              [&](autofill::AutofillClient::AutofillAiBubbleResult result,
                  std::optional<autofill::EntityInstance> entity,
                  const autofill::AutofillClient::EntityImportUIContext&
                      context) { ui_context = context; }),
          autofill::test::GetPassportEntityInstance(
              {.record_type =
                   autofill::EntityInstance::RecordType::kServerWallet})));
  // Initialized after `CreateMediatorWithPromptModel()`, which initializes the
  // fixture's `scoped_feature_list_`, so that scoped feature lists are
  // destroyed in reverse order of initialization.
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillRegionalizedPrivatePassesUxRefine);
  // Setting a consumer displays the footer, whose strings are reported as the
  // accepted consent.
  FakeAutofillAISaveEntityConsumer* consumer =
      [[FakeAutofillAISaveEntityConsumer alloc] init];
  mediator.consumer = consumer;

  [mediator acceptSaving];

  ASSERT_TRUE(ui_context.has_value());
  EXPECT_THAT(
      ui_context->accepted_consent_string_ids,
      testing::ElementsAre(
          IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_PRIVATE_PASS,
          IDS_AUTOFILL_GOOGLE_WALLET_TITLE,
          IDS_AUTOFILL_HOW_THIS_INFO_IS_USED_ACROSS_GOOGLE_LINK,
          IDS_AUTOFILL_PRIVACY_POLICY_LINK));
  [mediator disconnect];
}

// Tests that when initialized with model and tab helper and the feature flag is
// enabled, `cancelSaving` consumes the model from the tab helper and reports
// `kCancelled`.
TEST_F(AutofillAISaveEntityMediatorTest,
       TabHelperIntegrationCancelConsumesModel) {
  std::optional<autofill::AutofillClient::AutofillAiBubbleResult>
      callback_result;
  AutofillAISaveEntityMediator* mediator = CreateMediatorWithPromptModel(
      CreateDefaultSaveEntityParams(base::BindLambdaForTesting(
          [&](autofill::AutofillClient::AutofillAiBubbleResult result,
              std::optional<autofill::EntityInstance> entity,
              const autofill::AutofillClient::EntityImportUIContext& context) {
            callback_result = result;
          })));

  [mediator cancelSaving];

  EXPECT_EQ(callback_result,
            autofill::AutofillClient::AutofillAiBubbleResult::kCancelled);
  EXPECT_EQ(nullptr, tab_helper_->GetAutofillAiSaveEntityPromptModel());
  [mediator disconnect];
}

// Tests that when initialized with model and tab helper and the feature flag is
// enabled, `dismissSaving` consumes the model from the tab helper and reports
// `kClosed`.
TEST_F(AutofillAISaveEntityMediatorTest,
       TabHelperIntegrationDismissConsumesModel) {
  std::optional<autofill::AutofillClient::AutofillAiBubbleResult>
      callback_result;
  AutofillAISaveEntityMediator* mediator = CreateMediatorWithPromptModel(
      CreateDefaultSaveEntityParams(base::BindLambdaForTesting(
          [&](autofill::AutofillClient::AutofillAiBubbleResult result,
              std::optional<autofill::EntityInstance> entity,
              const autofill::AutofillClient::EntityImportUIContext& context) {
            callback_result = result;
          })));

  [mediator dismissSaving];

  EXPECT_EQ(callback_result,
            autofill::AutofillClient::AutofillAiBubbleResult::kClosed);
  EXPECT_EQ(nullptr, tab_helper_->GetAutofillAiSaveEntityPromptModel());
  [mediator disconnect];
}

// Tests that disconnecting without user action leaves the model pending in
// the tab helper so it can be reshown upon return.
TEST_F(AutofillAISaveEntityMediatorTest,
       TabHelperIntegrationDisconnectLeavesModelPending) {
  std::optional<autofill::AutofillClient::AutofillAiBubbleResult>
      callback_result;
  AutofillAISaveEntityMediator* mediator = CreateMediatorWithPromptModel(
      CreateDefaultSaveEntityParams(base::BindLambdaForTesting(
          [&](autofill::AutofillClient::AutofillAiBubbleResult result,
              std::optional<autofill::EntityInstance> entity,
              const autofill::AutofillClient::EntityImportUIContext& context) {
            callback_result = result;
          })));

  // Disconnect without accept/cancel/dismiss.
  [mediator disconnect];

  // Callback was not invoked, and model remains pending in tab helper.
  EXPECT_FALSE(callback_result.has_value());
  EXPECT_NE(nullptr, tab_helper_->GetAutofillAiSaveEntityPromptModel());

  // Explicitly destroy the pending model while callback_result is still in
  // scope to avoid stack-use-after-scope during test teardown.
  std::ignore = tab_helper_->TakeAutofillAiSaveEntityPromptModel();
}
