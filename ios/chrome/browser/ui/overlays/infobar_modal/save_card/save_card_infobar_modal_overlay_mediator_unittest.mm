// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator.h"

#import "base/bind.h"
#import "base/feature_list.h"
#import "base/guid.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/ui/autofill/save_card_message_with_links.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator_delegate.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_card_infobar_overlays::SaveCardModalRequestConfig;
using save_card_infobar_overlays::SaveCardMainAction;

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
@property(nonatomic, copy) NSString* cardNumber;
@property(nonatomic, copy) NSString* expirationMonth;
@property(nonatomic, copy) NSString* expirationYear;
@property(nonatomic, copy)
    NSMutableArray<SaveCardMessageWithLinks*>* legalMessages;
@property(nonatomic, assign) BOOL currentCardSaved;
@property(nonatomic, assign) BOOL supportsEditing;
@end

@implementation FakeSaveCardModalConsumer
- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs {
  self.cardholderName = [prefs[kCardholderNamePrefKey] stringValue];
  self.cardIssuerIcon = prefs[kCardIssuerIconNamePrefKey];
  self.cardNumber = prefs[kCardNumberPrefKey];
  self.expirationMonth = prefs[kExpirationMonthPrefKey];
  self.expirationYear = prefs[kExpirationYearPrefKey];
  self.legalMessages = prefs[kLegalMessagesPrefKey];
  self.currentCardSaved = [prefs[kCurrentCardSavedPrefKey] boolValue];
  self.supportsEditing = [prefs[kSupportsEditingPrefKey] boolValue];
}
@end

// Test fixture for SaveCardInfobarModalOverlayMediator.
class SaveCardInfobarModalOverlayMediatorTest : public PlatformTest {
 public:
  SaveCardInfobarModalOverlayMediatorTest()
      : callback_installer_(&callback_receiver_,
                            {SaveCardMainAction::ResponseSupport()}),
        mediator_delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))) {
    autofill::LegalMessageLines legal_message_lines =
        autofill::LegalMessageLines(
            {autofill::TestLegalMessageLine("Test message")});
    std::unique_ptr<autofill::AutofillSaveCardInfoBarDelegateMobile> delegate =
        std::make_unique<autofill::AutofillSaveCardInfoBarDelegateMobile>(
            /*upload=*/true, autofill::AutofillClient::SaveCreditCardOptions(),
            autofill::CreditCard(base::GenerateGUID(),
                                 "https://www.example.com/"),
            legal_message_lines,
            base::BindOnce(
                ^(autofill::AutofillClient::SaveCardOfferUserDecision
                      user_decision,
                  const autofill::AutofillClient::UserProvidedCardDetails&
                      user_provided_card_details){
                }),
            autofill::AutofillClient::LocalSaveCardPromptCallback(),
            AccountInfo());
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSaveCard,
                                            std::move(delegate));

    request_ = OverlayRequest::CreateWithConfig<SaveCardModalRequestConfig>(
        infobar_.get());
    callback_installer_.InstallCallbacks(request_.get());

    mediator_ = [[SaveCardInfobarModalOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.delegate = mediator_delegate_;
  }

  ~SaveCardInfobarModalOverlayMediatorTest() override {
    EXPECT_CALL(callback_receiver_, CompletionCallback(request_.get()));
    EXPECT_OCMOCK_VERIFY(mediator_delegate_);
  }

 protected:
  autofill::AutofillSaveCardInfoBarDelegateMobile* delegate_;
  std::unique_ptr<InfoBarIOS> infobar_;
  MockOverlayRequestCallbackReceiver callback_receiver_;
  FakeOverlayRequestCallbackInstaller callback_installer_;
  std::unique_ptr<OverlayRequest> request_;
  SaveCardInfobarModalOverlayMediator* mediator_ = nil;
  id<OverlayRequestMediatorDelegate> mediator_delegate_ = nil;
};

// Tests that a SaveCardInfobarModalOverlayMediator correctly sets up its
// consumer.
TEST_F(SaveCardInfobarModalOverlayMediatorTest, SetUpConsumer) {
  FakeSaveCardModalConsumer* consumer =
      [[FakeSaveCardModalConsumer alloc] init];
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
  EXPECT_FALSE(consumer.currentCardSaved);
  EXPECT_TRUE(consumer.supportsEditing);
  ASSERT_EQ(1U, [consumer.legalMessages count]);
  EXPECT_NSEQ(@"Test message", consumer.legalMessages[0].messageText);
}

// Tests that calling saveCardWithCardholderName:expirationMonth:expirationYear:
// triggers a SaveCardMainAction response.
TEST_F(SaveCardInfobarModalOverlayMediatorTest, MainAction) {
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request_.get(), SaveCardMainAction::ResponseSupport()));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ saveCardWithCardholderName:@"name"
                        expirationMonth:@"3"
                         expirationYear:@"23"];
}

// Tests that calling dismissModalAndOpenURL: sends the passed URL to the
// mediator's save_card_delegate.
TEST_F(SaveCardInfobarModalOverlayMediatorTest, LoadURL) {
  FakeSaveCardMediatorDelegate* delegate =
      [[FakeSaveCardMediatorDelegate alloc] init];
  mediator_.save_card_delegate = delegate;
  GURL url("https://testurl.com");
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissModalAndOpenURL:url];
  EXPECT_NSEQ(base::SysUTF8ToNSString(url.spec()),
              base::SysUTF8ToNSString(delegate.pendingURLToLoad.spec()));
}
