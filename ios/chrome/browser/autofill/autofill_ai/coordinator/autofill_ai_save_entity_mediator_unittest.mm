// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_consumer.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/grit/ios_strings.h"
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

  base::test::TaskEnvironment task_environment_;
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

  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(autofill::test::GetPassportEntityInstance(
          {.record_type =
               autofill::EntityInstance::RecordType::kServerWallet}));

  EXPECT_NSEQ(autofill::GetSaveEntityToWalletFooterTextAppendingURLsTo(
                  @"test@example.com", /*is_private_pass=*/true, /*urls=*/nil),
              consumer.footerText);
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
  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(std::move(vehicle));

  EXPECT_NSEQ(autofill::GetSaveEntityToWalletFooterTextAppendingURLsTo(
                  @"test@example.com", /*is_private_pass=*/false, /*urls=*/nil),
              consumer.footerText);
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

  FakeAutofillAISaveEntityConsumer* consumer =
      PushEntityToMediator(std::move(new_entity), std::move(old_entity));

  EXPECT_NSEQ(autofill::GetUpdateEntitySavedInWalletFooterTextAppendingURLsTo(
                  @"test@example.com", /*urls=*/nil),
              consumer.footerText);
  ASSERT_EQ(1u, consumer.footerURLs.count);
  EXPECT_EQ(autofill::GetGoogleWalletPassesURL(), consumer.footerURLs[0].gurl);
}

// Tests that `didTapFooterLinkURL:` logs `kLinkClicked` only when the tapped
// URL belongs to the legal message (comparing `GURL` values across distinct
// `CrURL` instances).
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

  // Allocate distinct `CrURL` instances to test `GURL` value equality rather
  // than pointer identity.
  CrURL* manage_info_url =
      [[CrURL alloc] initWithGURL:autofill::GetManageYourInfoURL()];
  [mediator didTapFooterLinkURL:manage_info_url];
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.WalletNotice.Save.Funnel",
      autofill::AutofillAiWalletNoticeFunnelEvents::kLinkClicked, 0);

  CrURL* legal_url =
      [[CrURL alloc] initWithGURL:GURL("https://www.example.com/legal")];
  [mediator didTapFooterLinkURL:legal_url];
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
