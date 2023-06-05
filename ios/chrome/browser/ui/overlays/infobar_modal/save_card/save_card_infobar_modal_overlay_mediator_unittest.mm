// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/autofill/message/save_card_message_with_links.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/overlays/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator_delegate.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
      : mediator_delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))) {
    autofill::CreditCard credit_card(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        "https://www.example.com/");
    std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate =
        MockAutofillSaveCardInfoBarDelegateMobileFactory::
            CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(true,
                                                                   credit_card);
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSaveCard,
                                            std::move(delegate));

    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kModal);
    mediator_ = [[SaveCardInfobarModalOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.delegate = mediator_delegate_;
  }

  ~SaveCardInfobarModalOverlayMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(mediator_delegate_);
  }

 protected:
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  MockAutofillSaveCardInfoBarDelegateMobile* delegate_ = nil;
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
// calls UpdateAndAccept().
TEST_F(SaveCardInfobarModalOverlayMediatorTest, MainAction) {
  NSString* cardholderName = @"name";
  NSString* month = @"3";
  NSString* year = @"23";

  EXPECT_CALL(*delegate_,
              UpdateAndAccept(base::SysNSStringToUTF16(cardholderName),
                              base::SysNSStringToUTF16(month),
                              base::SysNSStringToUTF16(year)));
  OCMExpect([mediator_delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ saveCardWithCardholderName:cardholderName
                        expirationMonth:month
                         expirationYear:year];
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
