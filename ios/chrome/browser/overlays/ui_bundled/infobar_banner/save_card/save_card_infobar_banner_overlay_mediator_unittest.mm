// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/save_card/save_card_infobar_banner_overlay_mediator.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/save_card/save_card_infobar_banner_overlay_mediator+Testing.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using testing::_;
using SaveCardPromptOffer = autofill::autofill_metrics::SaveCardPromptOffer;
using SaveCreditCardPromptResultIOS =
    autofill::autofill_metrics::SaveCreditCardPromptResultIOS;
using SaveCreditCardOptions =
    autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions;

constexpr std::string_view kSaveCreditCardPromptOfferBaseHistogram =
    "Autofill.SaveCreditCardPromptOffer.IOS";
constexpr char kSaveCreditCardPromptResultHistogramStringForLocalSave[] =
    "Autofill.SaveCreditCardPromptResult.IOS.Local.Banner.NumStrikes.0."
    "NoFixFlow";
constexpr char kSaveCreditCardPromptResultHistogramStringForServerSave[] =
    "Autofill.SaveCreditCardPromptResult.IOS.Server.Banner.NumStrikes.0."
    "NoFixFlow";

}  // namespace

// Test fixture for SaveCardInfobarBannerOverlayMediator.
class SaveCardInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  ~SaveCardInfobarBannerOverlayMediatorTest() override {
    EXPECT_OCMOCK_VERIFY((id)mediator_);
    EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_handler_);
  }

  void InitInfobar(
      const bool for_upload,
      autofill::payments::PaymentsAutofillClient::CardSaveType card_save_type =
          autofill::payments::PaymentsAutofillClient::CardSaveType::
              kCardSaveOnly) {
    feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnableCvcStorageAndFilling);

    autofill::CreditCard credit_card(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        "https://www.example.com/");
    std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate =
        MockAutofillSaveCardInfoBarDelegateMobileFactory::
            CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
                for_upload, credit_card,
                SaveCreditCardOptions().with_num_strikes(0).with_card_save_type(
                    card_save_type));
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSaveCard,
                                            std::move(delegate));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    consumer_ = [[FakeInfobarBannerConsumer alloc] init];
    mediator_ = OCMPartialMock([[SaveCardInfobarBannerOverlayMediator alloc]
        initWithRequest:request_.get()]);

    mediator_.consumer = consumer_;
    mock_snackbar_commands_handler_ =
        OCMProtocolMock(@protocol(SnackbarCommands));
    mediator_.snackbarCommandsHandler = mock_snackbar_commands_handler_;
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  }

  void TearDown() override {
    [mediator_ clearOverrideVoiceOverForTesting];
    PlatformTest::TearDown();
  }
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  raw_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate_ = nil;
  FakeInfobarBannerConsumer* consumer_ = nil;
  SaveCardInfobarBannerOverlayMediator* mediator_ = nil;
  id mock_snackbar_commands_handler_ = nil;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

TEST_F(SaveCardInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  InitInfobar(/*for_upload=*/false);

  // Verify that the infobar was set up properly.
  NSString* title = base::SysUTF16ToNSString(delegate_->GetMessageText());
  EXPECT_NSEQ(title, consumer_.titleText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK)),
              consumer_.buttonText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->card_label()),
              consumer_.subtitleText);
}

// Tests that when upload is turned on, tapping on the banner action button
// presents the modal.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest, PresentModalWhenUploadOn) {
  InitInfobar(/*for_upload=*/true);

  OCMExpect([mediator_ presentInfobarModalFromBanner]);
  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Tests that when upload is turned off, tapping on the banner action button
// does not present the modal.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest, PresentModalWhenUploadOff) {
  InitInfobar(/*for_upload=*/false);

  EXPECT_CALL(*delegate_,
              UpdateAndAccept(delegate_->cardholder_name(),
                              delegate_->expiration_date_month(),
                              delegate_->expiration_date_year(), _));
  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Verifies histogram entries for server save infobar banner shown and accepted.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndAcceptedForUploadSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/true);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Banner"}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Server.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);

  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ bannerInfobarButtonWasPressed:nil];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kAccepted, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 2);
}

// Verifies histogram entries for local save infobar banner shown and accepted.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndAcceptedForLocalSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/false);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Local.Banner"}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Local.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);

  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ bannerInfobarButtonWasPressed:nil];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kAccepted, 1);

  // Verify that local save banner's button when pressed is not recorded as user
  // initiated dismissal.
  [mediator_ dismissInfobarBannerForUserInteraction:YES];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kSwiped, 0);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave, 2);
}

// Verifies histogram entries for server save banner shown and gets swiped up.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndSwipedForUploadSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/true);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Banner"}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Server.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);

  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ dismissInfobarBannerForUserInteraction:YES];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kSwiped, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 2);
}

// Verifies histogram entries for local save banner shown and gets swiped up.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndSwipedForLocalSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/false);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Local.Banner"}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Local.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);

  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ dismissInfobarBannerForUserInteraction:YES];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kSwiped, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave, 2);
}

// Verifies histogram entries for server save banner shown and then times out.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndTimedOutForUploadSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/true);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Banner"}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Server.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);

  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ dismissInfobarBannerForUserInteraction:NO];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::KTimedOut, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 2);
}

// Verifies histogram entries for local save banner shown and then times out.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       LogInfoBarBannerShownAndTimedOutForLocalSave) {
  base::HistogramTester histogram_tester;
  InitInfobar(/*for_upload=*/false);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Local.Banner"}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Local.Banner.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);

  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  [mediator_ dismissInfobarBannerForUserInteraction:NO];
  histogram_tester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::KTimedOut, 1);

  histogram_tester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave, 2);
}

// Tests that a snackbar is shown when a card is saved locally (non-upload).
TEST_F(SaveCardInfobarBannerOverlayMediatorTest, ShowSnackbarForLocalSave) {
  InitInfobar(/*for_upload=*/false);

  EXPECT_CALL(*delegate_, UpdateAndAccept).WillOnce(testing::Return(true));

  // Expected snackbar message content.
  NSString* expectedTitleText = base::SysUTF16ToNSString(
      l10n_util::GetStringUTF16(IDS_IOS_AUTOFILL_CARD_SAVED));
  NSString* expectedSubtitleText =
      base::SysUTF16ToNSString(delegate_->card_label());
  NSString* expectedButtonText = base::SysUTF16ToNSString(
      l10n_util::GetStringUTF16(IDS_IOS_AUTOFILL_SAVE_CARD_GOT_IT));

  // Set up expectation for the snackbar message.
  OCMExpect(([mock_snackbar_commands_handler_
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                      SnackbarMessage* message) {
        NSString* expectedAccessibilityLabel = expectedTitleText;
        EXPECT_NSEQ(expectedAccessibilityLabel, message.accessibilityLabel);
        EXPECT_NSEQ(expectedTitleText, message.title);
        EXPECT_NSEQ(expectedSubtitleText, message.subtitle);
        // Check that action is not nil, "Got it" button is present.
        EXPECT_NE(message.action, nil);
        if (message.action) {
          EXPECT_NSEQ(expectedButtonText, message.action.title);
          // Check that handler is nil, verifies the button can be tapped.
          EXPECT_EQ(message.action.handler, nil);
        }
        return YES;
      }]]));

  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Tests that an accessibility notification is posted when a card is saved
// locally.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       PostAccessibilityNotificationForLocalSave) {
  InitInfobar(/*for_upload=*/false);
  EXPECT_CALL(*delegate_, UpdateAndAccept).WillOnce(testing::Return(true));

  __block BOOL notificationWasPosted = NO;
  __block UIAccessibilityNotifications postedNotification =
      UIAccessibilityAnnouncementNotification;

  mediator_.accessibilityNotificationPoster =
      ^(UIAccessibilityNotifications notification, id argument) {
        postedNotification = notification;
        notificationWasPosted = YES;
      };
  [mediator_ setOverrideVoiceOverForTesting:false];

  [mediator_ bannerInfobarButtonWasPressed:nil];

  EXPECT_TRUE(notificationWasPosted);
  EXPECT_EQ(postedNotification, UIAccessibilityScreenChangedNotification);
}

// Tests that the snackbar presentation is delayed when VoiceOver is running.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest,
       SnackbarIsDelayedWithVoiceOver) {
  InitInfobar(/*for_upload=*/false);
  EXPECT_CALL(*delegate_, UpdateAndAccept).WillOnce(testing::Return(true));

  __block BOOL notificationWasPosted = NO;
  mediator_.accessibilityNotificationPoster =
      ^(UIAccessibilityNotifications notification, id argument) {
        notificationWasPosted = YES;
      };
  [mediator_ setOverrideVoiceOverForTesting:true];

  OCMExpect([mock_snackbar_commands_handler_ showSnackbarMessage:OCMOCK_ANY]);

  [mediator_ bannerInfobarButtonWasPressed:nil];

  EXPECT_FALSE(notificationWasPosted);
  // Define the delay locally for the test. This should match the value in
  // the implementation file.
  constexpr base::TimeDelta kShowSnackbarDelay = base::Milliseconds(300);
  task_environment_->FastForwardBy(kShowSnackbarDelay);

  EXPECT_TRUE(notificationWasPosted);
}

// Tests that no snackbar is shown when the save is for upload.
TEST_F(SaveCardInfobarBannerOverlayMediatorTest, NoSnackbarForUploadSave) {
  InitInfobar(/*for_upload=*/true);

  OCMExpect([mediator_ presentInfobarModalFromBanner]);
  // No expectation on mock_snackbar_commands_handler_ as it shouldn't be
  // called.
  OCMReject([mock_snackbar_commands_handler_ showSnackbarMessage:OCMOCK_ANY]);

  [mediator_ bannerInfobarButtonWasPressed:nil];
}

struct SaveCardBannerMetricsTestCase {
  const std::string name;
  const bool is_for_upload;
  const autofill::payments::PaymentsAutofillClient::CardSaveType card_save_type;
};

class SaveCardInfobarBannerOverlayMediatorMetricsTest
    : public SaveCardInfobarBannerOverlayMediatorTest,
      public ::testing::WithParamInterface<SaveCardBannerMetricsTestCase> {
 protected:
  std::string GetExpectedHistogramName() {
    const auto& test_case = GetParam();
    std::string_view destination =
        test_case.is_for_upload ? ".Server" : ".Local";
    std::string_view suffix;
    switch (test_case.card_save_type) {
      case autofill::payments::PaymentsAutofillClient::CardSaveType::
          kCardSaveWithCvc:
        suffix = ".SavingWithCvc";
        break;
      case autofill::payments::PaymentsAutofillClient::CardSaveType::
          kCardSaveOnly:
        suffix = "";
        break;
      case autofill::payments::PaymentsAutofillClient::CardSaveType::
          kCvcSaveOnly:
        ADD_FAILURE() << "This test case shouldn't exist for the banner UI.";
        break;
    }
    return base::StrCat({"Autofill.SaveCreditCardPromptResult.IOS", destination,
                         ".Banner.NumStrikes.0.NoFixFlow", suffix});
  }
};

// Tests that the "Shown" metrics for the save card prompt offer are correctly
// recorded when the infobar banner is shown.
TEST_P(SaveCardInfobarBannerOverlayMediatorMetricsTest, LogsOfferBannerShown) {
  base::HistogramTester histogram_tester;
  const auto& test_case = GetParam();
  InitInfobar(test_case.is_for_upload, test_case.card_save_type);

  std::string_view destination = test_case.is_for_upload ? ".Server" : ".Local";
  std::string_view suffix;
  switch (test_case.card_save_type) {
    case autofill::payments::PaymentsAutofillClient::CardSaveType::
        kCardSaveWithCvc:
      suffix = ".SavingWithCvc";
      break;
    case autofill::payments::PaymentsAutofillClient::CardSaveType::
        kCardSaveOnly:
      suffix = "";
      break;
    case autofill::payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly:
      FAIL() << "This test case shouldn't exist for the banner UI.";
  }

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.SaveCreditCardPromptOffer.IOS", destination,
                    ".Banner", suffix}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.SaveCreditCardPromptOffer.IOS", destination,
                    ".Banner.NumStrikes.0.NoFixFlow", suffix}),
      SaveCardPromptOffer::kShown, 1);
}

TEST_P(SaveCardInfobarBannerOverlayMediatorMetricsTest, LogsBannerShown) {
  base::HistogramTester histogram_tester;
  const auto& test_case = GetParam();
  InitInfobar(test_case.is_for_upload, test_case.card_save_type);

  histogram_tester.ExpectUniqueSample(GetExpectedHistogramName(),
                                      SaveCreditCardPromptResultIOS::kShown, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SaveCardInfobarBannerOverlayMediatorMetricsTest,
    testing::ValuesIn<SaveCardBannerMetricsTestCase>({
        {"ServerCardSaveOnly", true,
         autofill::payments::PaymentsAutofillClient::CardSaveType::
             kCardSaveOnly},
        {"ServerCardSaveWithCvc", true,
         autofill::payments::PaymentsAutofillClient::CardSaveType::
             kCardSaveWithCvc},
        {"LocalCardSaveOnly", false,
         autofill::payments::PaymentsAutofillClient::CardSaveType::
             kCardSaveOnly},
        {"LocalCardSaveWithCvc", false,
         autofill::payments::PaymentsAutofillClient::CardSaveType::
             kCardSaveWithCvc},
    }),
    [](const ::testing::TestParamInfo<SaveCardBannerMetricsTestCase>& info) {
      return info.param.name;
    });
