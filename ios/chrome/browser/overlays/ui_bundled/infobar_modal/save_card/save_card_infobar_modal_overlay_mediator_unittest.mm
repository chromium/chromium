// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_save_card_modal_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using ::testing::A;
using ::testing::Return;
using SaveCardPromptOffer = autofill::autofill_metrics::SaveCardPromptOffer;
using SaveCreditCardPromptResultIOS =
    autofill::autofill_metrics::SaveCreditCardPromptResultIOS;
using SaveCreditCardOptions =
    autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions;

// Time duration to wait before auto-closing modal in save card success
// confirmation state.
static constexpr base::TimeDelta kConfirmationStateDuration =
    base::Seconds(1.5);

// Details of the card to be saved.
constexpr NSString* kCardHolderName = @"Name";
NSString* const kValidExpirationMonth =
    base::SysUTF8ToNSString(autofill::test::NextMonth());
NSString* const kValidExpirationYear =
    base::SysUTF8ToNSString(autofill::test::NextYear());

constexpr NSString* kCardCvc = @"123";

constexpr std::string_view kSaveCreditCardPromptOfferBaseHistogram =
    "Autofill.SaveCreditCardPromptOffer.IOS";
constexpr char kSaveCreditCardPromptResultHistogramStringForLocalSave[] =
    "Autofill.SaveCreditCardPromptResult.IOS.Local.Modal.NumStrikes.0."
    "NoFixFlow";
constexpr char kSaveCreditCardPromptResultHistogramStringForServerSave[] =
    "Autofill.SaveCreditCardPromptResult.IOS.Server.Modal.NumStrikes.0."
    "NoFixFlow";

}  // namespace

@interface FakeSaveCardMediatorDelegate
    : NSObject <SaveCardInfobarModalOverlayMediatorDelegate>
@property(nonatomic, assign) GURL pendingURLToLoad;
@end

@implementation FakeSaveCardMediatorDelegate
- (void)pendingURLToLoad:(GURL)URL {
  self.pendingURLToLoad = URL;
}
@end

@interface FakeSaveCardModalConsumer : NSObject <InfobarSaveCardModalConsumer>
// Prefs passed in setupModalViewControllerWithPrefs:.
@property(nonatomic, copy) NSString* cardholderName;
@property(nonatomic, strong) UIImage* cardIssuerIcon;
@property(nonatomic, copy) NSString* cardNetwork;
@property(nonatomic, copy) NSString* cardNumber;
@property(nonatomic, copy) NSString* expirationMonth;
@property(nonatomic, copy) NSString* expirationYear;
@property(nonatomic, copy)
    NSMutableArray<SaveCardMessageWithLinks*>* legalMessages;
@property(nonatomic, assign) BOOL currentCardSaveAccepted;
@property(nonatomic, assign) BOOL supportsEditing;
@property(nonatomic, strong) UIImage* logoIcon;
@property(nonatomic, copy) NSString* logoIconDescription;

// Fake consumer specific properties.
@property(nonatomic, assign) BOOL inLoadingState;
@property(nonatomic, assign) BOOL showingSuccess;
@end

@implementation FakeSaveCardModalConsumer
- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs {
  self.cardholderName = [prefs[kCardholderNamePrefKey] stringValue];
  self.cardIssuerIcon = prefs[kCardIssuerIconNamePrefKey];
  self.cardNetwork = prefs[kCardNetworkPrefKey];
  self.cardNumber = prefs[kCardNumberPrefKey];
  self.expirationMonth = prefs[kExpirationMonthPrefKey];
  self.expirationYear = prefs[kExpirationYearPrefKey];
  self.legalMessages = prefs[kLegalMessagesPrefKey];
  self.currentCardSaveAccepted =
      [prefs[kCurrentCardSaveAcceptedPrefKey] boolValue];
  self.supportsEditing = [prefs[kSupportsEditingPrefKey] boolValue];
  self.logoIcon = prefs[kLogoIconPrefKey];
  self.logoIconDescription = prefs[kLogoIconDescriptionPrefKey];
}

- (void)showProgressWithUploadCompleted:(BOOL)uploadCompleted {
  self.inLoadingState = !uploadCompleted;
  self.showingSuccess = uploadCompleted;
}

@end

// Test fixture for SaveCardInfobarModalOverlayMediator.
class SaveCardInfobarModalOverlayMediatorTest : public PlatformTest {
 public:
  SaveCardInfobarModalOverlayMediatorTest(
      bool for_upload = true,
      autofill::payments::PaymentsAutofillClient::CardSaveType card_save_type =
          autofill::payments::PaymentsAutofillClient::CardSaveType::
              kCardSaveOnly)
      : mediator_delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))) {
    feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnableCvcStorageAndFilling);
    task_environment_ = std::make_unique<web::WebTaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
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
            infobar_.get(), InfobarOverlayType::kModal);

    EXPECT_CALL(*delegate_,
                SetCreditCardUploadCompletionCallback(
                    A<base::OnceCallback<void(BOOL card_saved)>>()));
    mediator_ = [[SaveCardInfobarModalOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.delegate = mediator_delegate_;
  }

  ~SaveCardInfobarModalOverlayMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(mediator_delegate_);
  }

  web::WebTaskEnvironment* task_environment() {
    return task_environment_.get();
  }

  void SaveCard() {
    [mediator_ saveCardWithCardholderName:kCardHolderName
                          expirationMonth:kValidExpirationMonth
                           expirationYear:kValidExpirationYear
                                  cardCvc:kCardCvc];
  }

 protected:
  std::unique_ptr<web::WebTaskEnvironment> task_environment_;
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  raw_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate_ = nil;
  SaveCardInfobarModalOverlayMediator* mediator_ = nil;
  id<OverlayRequestMediatorDelegate> mediator_delegate_ = nil;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a SaveCardInfobarModalOverlayMediator correctly sets up its
// consumer.
TEST_F(SaveCardInfobarModalOverlayMediatorTest, SetUpConsumer) {
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
  EXPECT_CALL(*delegate_, SetInfobarIsPresenting(YES));
  mediator_.consumer = consumer;

  NSString* cardNumber = [NSString
      stringWithFormat:@"•••• %@", base::SysUTF16ToNSString(
                                       delegate_->card_last_four_digits())];

  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->cardholder_name()),
              consumer.cardholderName);
  EXPECT_NSEQ(cardNumber, consumer.cardNumber);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->expiration_date_month()),
              consumer.expirationMonth);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->expiration_date_year()),
              consumer.expirationYear);
  EXPECT_TRUE(consumer.logoIcon);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate_->logo_icon_description()),
              consumer.logoIconDescription);
  EXPECT_FALSE(consumer.currentCardSaveAccepted);
  EXPECT_TRUE(consumer.supportsEditing);
  EXPECT_FALSE(consumer.inLoadingState);
  ASSERT_EQ(1U, [consumer.legalMessages count]);
  EXPECT_NSEQ(@"Test message", consumer.legalMessages[0].messageText);
}

// Tests that calling dismissModalAndOpenURL: sends the passed URL to the
// mediator's save_card_delegate.
TEST_F(SaveCardInfobarModalOverlayMediatorTest, LoadURL) {
  FakeSaveCardMediatorDelegate* delegate =
      [[FakeSaveCardMediatorDelegate alloc] init];
  mediator_.save_card_delegate = delegate;
  GURL url("https://testurl.com");
  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  EXPECT_CALL(*delegate_, SetInfobarIsPresenting(NO));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissModalAndOpenURL:url];
  EXPECT_NSEQ(base::SysUTF8ToNSString(url.spec()),
              base::SysUTF8ToNSString(delegate.pendingURLToLoad.spec()));
}

// Tests that when modal is dismissed, mediator resets the callback passed to
// the delegate and informs that infobar is not presenting.
TEST_F(SaveCardInfobarModalOverlayMediatorTest, OnInfoBarDismissed) {
  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  EXPECT_CALL(*delegate_, SetInfobarIsPresenting(NO));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarModal:nil];
}

// Tests histogram entries for server save modal shown and accepted.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       LogsModalShownAndAcceptedForServerSave) {
  base::HistogramTester histogramTester;
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];

  mediator_.consumer = consumer;
  histogramTester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Modal"}),
      SaveCardPromptOffer::kShown, 1);
  histogramTester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Server.Modal.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  SaveCard();
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kAccepted, 1);

  histogramTester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 2);
}

// Tests histogram entry is not recorded for server save offer shown when modal
// is reshown in loading state.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       DoNotLogOfferShownWhenModalReshownInLoadingState) {
  base::HistogramTester histogramTester;
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
  infobar_->set_accepted(true);
  mediator_.consumer = consumer;

  EXPECT_TRUE(consumer.inLoadingState);
  histogramTester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Modal"}),
      SaveCardPromptOffer::kShown, 0);
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kShown, 0);
}

// Tests histogram entries for server save modal shown and denied when dismissed
// through the cancel button before being accepted.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       LogsModalShownAndDeniedBeforeAcceptingServerSave) {
  base::HistogramTester histogramTester;
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];

  mediator_.consumer = consumer;
  histogramTester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Server.Modal"}),
      SaveCardPromptOffer::kShown, 1);
  histogramTester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Server.Modal.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarModal:nil];
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kDenied, 1);

  histogramTester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 2);
}

// Tests histogram entry is not recorded for modal as denied when dismissed
// through the cancel button after being accepted.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       DoNotLogModalDeniedAfterAcceptingServerSave) {
  base::HistogramTester histogramTester;

  SaveCard();
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kAccepted, 1);

  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarModal:nil];
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kDenied, 0);

  histogramTester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 1);
}

// Tests histogram entry for server save modal denied on link clicked before
// being accepted.
TEST_F(SaveCardInfobarModalOverlayMediatorTest, LogsModalDeniedOnLinkClick) {
  base::HistogramTester histogramTester;
  GURL url("https://testurl.com");

  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissModalAndOpenURL:url];
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kLinkClicked, 1);
}

// Tests histogram entry is not recorded for modal denied on link clicked after
// being accepted.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       DoNotLogModalDeniedOnLinkClickAfterAccepting) {
  base::HistogramTester histogramTester;
  GURL url("https://testurl.com");

  ON_CALL(*delegate_,
          UpdateAndAccept(base::SysNSStringToUTF16(kCardHolderName),
                          base::SysNSStringToUTF16(kValidExpirationMonth),
                          base::SysNSStringToUTF16(kValidExpirationYear),
                          base::SysNSStringToUTF16(kCardCvc)))
      .WillByDefault(Return(true));
  SaveCard();
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kAccepted, 1);

  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissModalAndOpenURL:url];
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave,
      SaveCreditCardPromptResultIOS::kLinkClicked, 0);

  histogramTester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForServerSave, 1);
}

class SaveCardInfobarModalOverlayMediatorWithLocalSave
    : public SaveCardInfobarModalOverlayMediatorTest {
 public:
  SaveCardInfobarModalOverlayMediatorWithLocalSave()
      : SaveCardInfobarModalOverlayMediatorTest(/*for_upload=*/false) {}
};

// Tests histogram entries for local save modal shown and accepted.
TEST_F(SaveCardInfobarModalOverlayMediatorWithLocalSave,
       LogsModalShownAndAcceptedForLocalSave) {
  base::HistogramTester histogramTester;
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];

  mediator_.consumer = consumer;
  histogramTester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram, ".Local.Modal"}),
      SaveCardPromptOffer::kShown, 1);
  histogramTester.ExpectBucketCount(
      base::StrCat({kSaveCreditCardPromptOfferBaseHistogram,
                    ".Local.Modal.NumStrikes.0.NoFixFlow"}),
      SaveCardPromptOffer::kShown, 1);
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kShown, 1);

  SaveCard();
  histogramTester.ExpectBucketCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave,
      SaveCreditCardPromptResultIOS::kAccepted, 1);

  histogramTester.ExpectTotalCount(
      kSaveCreditCardPromptResultHistogramStringForLocalSave, 2);
}

// Tests that a SaveCardInfobarModalOverlayMediator does not show Modal in
// loading state when accepted Modal is for local save.
TEST_F(SaveCardInfobarModalOverlayMediatorWithLocalSave,
       DoNotShowLoadingStateForAcceptedInfobar) {
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
  infobar_->set_accepted(true);
  mediator_.consumer = consumer;

  EXPECT_TRUE(consumer.currentCardSaveAccepted);
  EXPECT_FALSE(consumer.supportsEditing);
  EXPECT_FALSE(consumer.inLoadingState);
}

// Tests that calling -saveCardWithCardholderName goes straight to confirmation
// without showing the loading spinner when the card is saved locally.
TEST_F(SaveCardInfobarModalOverlayMediatorWithLocalSave,
       OnSaveShowConfirmation) {
  base::HistogramTester histogramTester;

  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
  mediator_.consumer = consumer;

  EXPECT_CALL(*delegate_,
              UpdateAndAccept(base::SysNSStringToUTF16(kCardHolderName),
                              base::SysNSStringToUTF16(kValidExpirationMonth),
                              base::SysNSStringToUTF16(kValidExpirationYear),
                              base::SysNSStringToUTF16(kCardCvc)));
  SaveCard();

  EXPECT_FALSE(consumer.inLoadingState);

  histogramTester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationShown.CardNotUploaded",
      /*sample=*/true, 1);
}

// Tests that calling saveCardWithCardholderName shows loading state when the
// card is uploaded.
TEST_F(SaveCardInfobarModalOverlayMediatorTest, OnSaveShowLoading) {
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
  mediator_.consumer = consumer;

  EXPECT_CALL(*delegate_,
              UpdateAndAccept(base::SysNSStringToUTF16(kCardHolderName),
                              base::SysNSStringToUTF16(kValidExpirationMonth),
                              base::SysNSStringToUTF16(kValidExpirationYear),
                              base::SysNSStringToUTF16(kCardCvc)));
  SaveCard();

  EXPECT_TRUE(consumer.inLoadingState);
}

// Tests that when already accepted modal for upload is reshown
// SaveCardInfobarModalOverlayMediator shows modal in loading state when
// loading and confirmation flag is enabled.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       ReShowLoadingStateForAcceptedInfobar) {
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
  infobar_->set_accepted(true);
  mediator_.consumer = consumer;

  EXPECT_TRUE(consumer.currentCardSaveAccepted);
  EXPECT_FALSE(consumer.supportsEditing);
  EXPECT_TRUE(consumer.inLoadingState);
}

// Tests that when credit card upload is completed and modal is reshown
// SaveCardInfobarModalOverlayMediator shows modal in confirmation state when
// loading and confirmation flag is enabled.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       ReShowConfirmationStateForUploadCompletedInfobar) {
  ON_CALL(*delegate_, IsCreditCardUploadComplete)
      .WillByDefault(testing::Return(true));
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
  infobar_->set_accepted(true);
  mediator_.consumer = consumer;

  EXPECT_TRUE(consumer.currentCardSaveAccepted);
  EXPECT_FALSE(consumer.supportsEditing);
  EXPECT_FALSE(consumer.inLoadingState);
  EXPECT_TRUE(consumer.showingSuccess);
}

// Tests that calling creditCardUploadCompleted with `card_saved` as true shows
// success when loading and confirmation flag is enabled.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       OnCreditCardUploadCompletedSuccess) {
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
  mediator_.consumer = consumer;

  [mediator_ creditCardUploadCompleted:/*card_saved=*/true];

  EXPECT_TRUE(consumer.showingSuccess);
}

// Tests that calling creditCardUploadCompleted with `card_saved` as false
// dismisses the modal when loading and confirmation flag is enabled.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       OnCreditCardUploadCompletedNonSuccess) {
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
  mediator_.consumer = consumer;

  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  EXPECT_CALL(*delegate_, SetInfobarIsPresenting(NO));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ creditCardUploadCompleted:/*card_saved=*/false];

  EXPECT_FALSE(consumer.showingSuccess);
}

// Tests that modal is auto-closed and
// `AutofillSaveCardInfoBarDelegateIOS::OnConfirmationClosed()` is called
// when timer for confirmation state times out.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       ConfirmationAutoClosed_OnTimeOut) {
  // Shows modal in confirmation state and starts the timer to auto-close the
  // modal.
  [mediator_ creditCardUploadCompleted:/*card_saved=*/true];

  // Verify the modal is dismissed and call is made to
  // `OnConfirmationClosed` on timeout.
  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  EXPECT_CALL(*delegate_, SetInfobarIsPresenting(NO));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  EXPECT_CALL(*delegate_, OnConfirmationClosed);
  task_environment()->FastForwardBy(kConfirmationStateDuration);
}

// Tests that modal is not auto-closed and
// `AutofillSaveCardInfoBarDelegateIOS::OnConfirmationClosed()` is not called
// before the timer for confirmation state times out.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       ConfirmationNotAutoclosed_BeforeTimeout) {
  // Shows modal in confirmation state and starts the timer to auto-close the
  // modal.
  [mediator_ creditCardUploadCompleted:/*card_saved=*/true];

  // Verify the modal is not yet dismissed and call is not made to
  // `OnConfirmationClosed` before timer times out.
  EXPECT_CALL(*delegate_, SetInfobarIsPresenting(NO)).Times(0);
  OCMReject([mediator_delegate_ stopOverlayForMediator:mediator_]);
  EXPECT_CALL(*delegate_, OnConfirmationClosed).Times(0);

  // Advance timer slightly less than the actual timeout duration i.e
  // `kConfirmationStateDuration`.
  base::TimeDelta delta = base::Seconds(0.5);
  task_environment()->FastForwardBy(kConfirmationStateDuration - delta);
  EXPECT_EQ(delta, task_environment()->NextMainThreadPendingTaskDelay());
}

// Tests metrics for loading view shown and dismissed by user.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       LoadingViewShownAndDismissedByUser_Metrics) {
  base::HistogramTester histogramTester;

  SaveCard();

  histogramTester.ExpectUniqueSample("Autofill.CreditCardUpload.LoadingShown",
                                     true, 1);

  // When modal is dismissed by user, on tapping the `Close` button before
  // receiving server response, `dismissInfobarModal` is called. Verify
  // `Autofill.CreditCardUpload.LoadingResult` metrics is logged with reason for
  // dismissal as `kClosed`.
  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarModal:nil];

  histogramTester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.LoadingResult",
      autofill::autofill_metrics::LegacySaveCardPromptResult::kClosed, 1);
}

// Tests metrics for loading view shown and dismissed on receiving result from
// server.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       LoadingViewShownAndNotDismissedByUser_Metrics) {
  base::HistogramTester histogramTester;

  SaveCard();
  histogramTester.ExpectUniqueSample("Autofill.CreditCardUpload.LoadingShown",
                                     true, 1);

  // On receving server response if loading result is showing, it will be
  // dismissed by the mediator. Verify `Autofill.CreditCardUpload.LoadingResult`
  // metrics is logged with reason for dismissal as `kNotInteracted`.
  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ creditCardUploadCompleted:/*card_saved=*/false];

  histogramTester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.LoadingResult",
      autofill::autofill_metrics::LegacySaveCardPromptResult::kNotInteracted,
      1);
  histogramTester.ExpectBucketCount(
      "Autofill.CreditCardUpload.LoadingResult",
      autofill::autofill_metrics::LegacySaveCardPromptResult::kClosed, 0);
}

// Tests metrics for confirmation view shown and dismissed by user.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       ConfirmationViewShownAndDismissedByUser_Metrics) {
  base::HistogramTester histogramTester;

  [mediator_ creditCardUploadCompleted:/*card_saved=*/true];

  histogramTester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationShown.CardUploaded",
      /*is_shown=*/true, 1);

  // When confirmation view is dismissed by user on tapping the close button
  // before the modal is auto-closed, `dismissInfobarModal` is called. Verify
  // `Autofill.CreditCardUpload.ConfirmationResult` metrics is logged with
  // reason for dismissal as `kClosed`.
  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarModal:nil];
  task_environment()->FastForwardBy(kConfirmationStateDuration);

  histogramTester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationResult.CardUploaded",
      autofill::autofill_metrics::LegacySaveCardPromptResult::kClosed, 1);
  histogramTester.ExpectBucketCount(
      "Autofill.CreditCardUpload.ConfirmationResult.CardUploaded",
      autofill::autofill_metrics::LegacySaveCardPromptResult::kNotInteracted,
      0);
}

// Tests metrics for confirmation view shown and auto-closed on
// timeout.
TEST_F(SaveCardInfobarModalOverlayMediatorTest,
       ConfirmationViewShownAndAutoClosedOnTimeout_Metrics) {
  base::HistogramTester histogramTester;

  [mediator_ creditCardUploadCompleted:/*card_saved=*/true];

  histogramTester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationShown.CardUploaded",
      /*is_shown=*/true, 1);

  // When confirmation view is auto-closed on timeout, verify
  // `Autofill.CreditCardUpload.ConfirmationResult` metrics is logged with
  // reason for dismissal as `kNotInteracted`.
  EXPECT_CALL(*delegate_, SetCreditCardUploadCompletionCallback);
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  task_environment()->FastForwardBy(kConfirmationStateDuration);

  histogramTester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationResult.CardUploaded",
      autofill::autofill_metrics::LegacySaveCardPromptResult::kNotInteracted,
      1);
  histogramTester.ExpectBucketCount(
      "Autofill.CreditCardUpload.ConfirmationResult.CardUploaded",
      autofill::autofill_metrics::LegacySaveCardPromptResult::kClosed, 0);
}

// Tests that the mediator correctly handles an empty CVC when the user saves.
TEST_F(SaveCardInfobarModalOverlayMediatorTest, OnSaveWithEmptyCVC) {
  // Expect that the delegate's UpdateAndAccept method is called with an empty
  // string for the CVC.
  EXPECT_CALL(*delegate_,
              UpdateAndAccept(base::SysNSStringToUTF16(kCardHolderName),
                              base::SysNSStringToUTF16(kValidExpirationMonth),
                              base::SysNSStringToUTF16(kValidExpirationYear),
                              testing::IsEmpty()));

  // Call the mediator's save method directly, passing an empty NSString for the
  // CVC.
  [mediator_ saveCardWithCardholderName:kCardHolderName
                        expirationMonth:kValidExpirationMonth
                         expirationYear:kValidExpirationYear
                                cardCvc:@""];
}

struct SaveCardModalMetricsTestCase {
  const std::string name;
  const bool is_for_upload;
  const autofill::payments::PaymentsAutofillClient::CardSaveType card_save_type;
};

std::string TestCaseName(
    const ::testing::TestParamInfo<SaveCardModalMetricsTestCase>& info) {
  return info.param.name;
}

class SaveCardInfobarModalOverlayMediatorMetricsTest
    : public SaveCardInfobarModalOverlayMediatorTest,
      public ::testing::WithParamInterface<SaveCardModalMetricsTestCase> {
 public:
  SaveCardInfobarModalOverlayMediatorMetricsTest()
      // This call now correctly matches the updated base constructor.
      : SaveCardInfobarModalOverlayMediatorTest(GetParam().is_for_upload,
                                                GetParam().card_save_type) {}

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
                         ".Modal.NumStrikes.0.NoFixFlow", suffix});
  }
};

TEST_P(SaveCardInfobarModalOverlayMediatorMetricsTest, LogsOfferModalShown) {
  base::HistogramTester histogram_tester;
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];

  mediator_.consumer = consumer;

  const auto& test_case = GetParam();
  std::string destination = test_case.is_for_upload ? ".Server" : ".Local";
  std::string suffix;
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
      FAIL() << "This test case shouldn't exist for the modal UI.";
  }

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.SaveCreditCardPromptOffer.IOS", destination,
                    ".Modal", suffix}),
      SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.SaveCreditCardPromptOffer.IOS", destination,
                    ".Modal.NumStrikes.0.NoFixFlow", suffix}),
      SaveCardPromptOffer::kShown, 1);
}

TEST_P(SaveCardInfobarModalOverlayMediatorMetricsTest, LogsModalShown) {
  base::HistogramTester histogram_tester;
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];

  mediator_.consumer = consumer;

  histogram_tester.ExpectUniqueSample(GetExpectedHistogramName(),
                                      SaveCreditCardPromptResultIOS::kShown, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SaveCardInfobarModalOverlayMediatorMetricsTest,
                         testing::ValuesIn<SaveCardModalMetricsTestCase>({
                             {"ServerCardSaveOnly", true,
                              autofill::payments::PaymentsAutofillClient::
                                  CardSaveType::kCardSaveOnly},
                             {"ServerCardSaveWithCvc", true,
                              autofill::payments::PaymentsAutofillClient::
                                  CardSaveType::kCardSaveWithCvc},
                             {"LocalCardSaveOnly", false,
                              autofill::payments::PaymentsAutofillClient::
                                  CardSaveType::kCardSaveOnly},
                             {"LocalCardSaveWithCvc", false,
                              autofill::payments::PaymentsAutofillClient::
                                  CardSaveType::kCardSaveWithCvc},
                         }),
                         TestCaseName);
