// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mediator.h"

#import <Foundation/Foundation.h>

#import <string>

#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/grit/components_scaled_resources.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

autofill::AutofillSaveCardUiInfo CreateAutofillSaveCardUiInfo() {
  autofill::AutofillSaveCardUiInfo ui_info = autofill::AutofillSaveCardUiInfo();
  ui_info.title_text = std::u16string(u"Title");
  ui_info.description_text = std::u16string(u"Description Text");
  ui_info.logo_icon_id = IDR_AUTOFILL_GOOGLE_PAY;
  ui_info.logo_icon_description = std::u16string(u"Logo description");
  ui_info.confirm_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT);
  ui_info.cancel_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_NO_THANKS_MOBILE_UPLOAD_SAVE);
  ui_info.card_label = std::u16string(u"CardName ****2345");
  ui_info.card_sub_label = std::u16string(u"MM/YY");
  ui_info.card_description = std::u16string(u"Card description");
  ui_info.issuer_icon_id = IDR_AUTOFILL_METADATA_CC_VISA;
  ui_info.legal_message_lines =
      autofill::LegalMessageLines({autofill::TestLegalMessageLine(
          /*ascii_text=*/"Save Card Legal Message",
          /*links=*/{
              autofill::LegalMessageLine::Link(
                  /*start=*/3, /*end=*/4, /*url_spec=*/"https://savecard.test"),
          })});
  ui_info.loading_description = std::u16string(u"Loading description");
  return ui_info;
}

}  // namespace

@interface FakeSaveCardBottomSheetConsumer
    : NSObject <SaveCardBottomSheetConsumer>

@property(nonatomic, strong) UIImage* aboveTitleImage;
@property(nonatomic, copy) NSString* aboveTitleImageDescription;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* subtitle;
@property(nonatomic, copy) NSString* acceptActionText;
@property(nonatomic, copy) NSString* cancelActionText;
@property(nonatomic, copy) NSString* cardNameAndLastFourDigits;
@property(nonatomic, copy) NSString* expiryDate;
@property(nonatomic, copy) NSString* cardAccessibilityLabel;
@property(nonatomic, strong) UIImage* issuerIcon;
@property(nonatomic, strong) NSArray<SaveCardMessageWithLinks*>* legalMessages;

@end

@implementation FakeSaveCardBottomSheetConsumer

- (void)setCardNameAndLastFourDigits:(NSString*)label
                  withCardExpiryDate:(NSString*)subLabel
                         andCardIcon:(UIImage*)issuerIcon
           andCardAccessibilityLabel:(NSString*)accessibilityLabel {
  self.cardNameAndLastFourDigits = label;
  self.expiryDate = subLabel;
  self.issuerIcon = issuerIcon;
  self.cardAccessibilityLabel = accessibilityLabel;
}

- (void)showLoadingStateWithAccessibilityLabel:(NSString*)accessibilityLabel {
}

- (void)showConfirmationState {
}

@end

class MockSaveCardBottomSheetModel : public autofill::SaveCardBottomSheetModel {
 public:
  MockSaveCardBottomSheetModel(autofill::AutofillSaveCardUiInfo ui_info)
      : SaveCardBottomSheetModel(
            std::move(ui_info),
            std::make_unique<autofill::AutofillSaveCardDelegate>(
                static_cast<autofill::payments::PaymentsAutofillClient::
                                UploadSaveCardPromptCallback>(
                    base::DoNothing()),
                autofill::payments::PaymentsAutofillClient::
                    SaveCreditCardOptions{})) {}

  MOCK_METHOD(void, OnAccepted, (), (override));
  MOCK_METHOD(void, OnCanceled, (), (override));
};

class SaveCardBottomSheetMediatorTest : public PlatformTest {
 public:
  SaveCardBottomSheetMediatorTest() {
    task_environment_ = std::make_unique<web::WebTaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    mock_autofill_commands_handler_ =
        OCMProtocolMock(@protocol(AutofillCommands));
    std::unique_ptr<MockSaveCardBottomSheetModel> model =
        std::make_unique<MockSaveCardBottomSheetModel>(
            CreateAutofillSaveCardUiInfo());
    model_ = model.get();
    mediator_ = [[SaveCardBottomSheetMediator alloc]
                initWithUIModel:std::move(model)
        autofillCommandsHandler:mock_autofill_commands_handler_];
  }

  ~SaveCardBottomSheetMediatorTest() override { [mediator_ disconnect]; }

  web::WebTaskEnvironment* task_environment() {
    return task_environment_.get();
  }

 protected:
  std::unique_ptr<web::WebTaskEnvironment> task_environment_;
  id<AutofillCommands> mock_autofill_commands_handler_;
  raw_ptr<MockSaveCardBottomSheetModel> model_ = nil;
  SaveCardBottomSheetMediator* mediator_ = nil;
};

TEST_F(SaveCardBottomSheetMediatorTest, SetConsumer) {
  FakeSaveCardBottomSheetConsumer* consumer =
      [[FakeSaveCardBottomSheetConsumer alloc] init];
  mediator_.consumer = consumer;
  EXPECT_TRUE(consumer.aboveTitleImage);
  EXPECT_NSEQ(base::SysUTF16ToNSString(model_->logo_icon_description()),
              consumer.aboveTitleImageDescription);
  EXPECT_NSEQ(base::SysUTF16ToNSString(model_->title()), consumer.title);
  EXPECT_NSEQ(base::SysUTF16ToNSString(model_->subtitle()), consumer.subtitle);
  EXPECT_NSEQ(base::SysUTF16ToNSString(model_->accept_button_text()),
              consumer.acceptActionText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(model_->cancel_button_text()),
              consumer.cancelActionText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(model_->card_name_last_four_digits()),
              consumer.cardNameAndLastFourDigits);
  EXPECT_NSEQ(base::SysUTF16ToNSString(model_->card_expiry_date()),
              consumer.expiryDate);
  EXPECT_NSEQ(
      base::SysUTF16ToNSString(model_->card_accessibility_description()),
      consumer.cardAccessibilityLabel);
  EXPECT_TRUE(consumer.issuerIcon);
  NSMutableArray<SaveCardMessageWithLinks*>* messages =
      [SaveCardMessageWithLinks convertFrom:model_->legal_messages()];
  for (NSUInteger index = 0; index < [messages count]; index++) {
    EXPECT_NSEQ(messages[index].messageText,
                (consumer.legalMessages[index]).messageText);
    EXPECT_NSEQ(messages[index].linkRanges,
                (consumer.legalMessages[index]).linkRanges);
    EXPECT_EQ(messages[index].linkURLs,
              (consumer.legalMessages[index]).linkURLs);
  }
}

// Test that `OnAccepted` is called on the model when bottomsheet is accepted.
TEST_F(SaveCardBottomSheetMediatorTest, OnAccept) {
  EXPECT_CALL(*model_, OnAccepted());
  EXPECT_CALL(*model_, OnCanceled()).Times(0);
  [mediator_ didAccept];
}

// Test that pushing accept button calls the consumer to show the loading state.
TEST_F(SaveCardBottomSheetMediatorTest, OnAcceptShowLoadingState) {
  id<SaveCardBottomSheetConsumer> mock_consumer =
      OCMProtocolMock(@protocol(SaveCardBottomSheetConsumer));
  mediator_.consumer = mock_consumer;

  OCMExpect([mock_consumer
      showLoadingStateWithAccessibilityLabel:[OCMArg checkWithBlock:^BOOL(
                                                         NSString* label) {
        EXPECT_NSEQ(label, @"Loading description");
        return YES;
      }]]);

  [mediator_ didAccept];

  EXPECT_OCMOCK_VERIFY((id)mock_consumer);
}

// Test that successful credit card upload completion calls the consumer to show
// the confirmation state.
TEST_F(SaveCardBottomSheetMediatorTest, OnSuccessShowConfirmationState) {
  id<SaveCardBottomSheetConsumer> mock_consumer =
      OCMProtocolMock(@protocol(SaveCardBottomSheetConsumer));
  mediator_.consumer = mock_consumer;

  OCMExpect([mock_consumer showConfirmationState]);
  [mediator_ onCreditCardUploadCompleted:YES];

  EXPECT_OCMOCK_VERIFY((id)mock_consumer);
}

// Test that unsuccessful credit card upload completion dismisses the
// bottomsheet.
TEST_F(SaveCardBottomSheetMediatorTest, OnFailureDismissBottomSheet) {
  id<SaveCardBottomSheetConsumer> mock_consumer =
      OCMProtocolMock(@protocol(SaveCardBottomSheetConsumer));
  mediator_.consumer = mock_consumer;

  OCMReject([mock_consumer showConfirmationState]);
  OCMExpect([mock_autofill_commands_handler_ dismissSaveCardBottomSheet]);
  [mediator_ onCreditCardUploadCompleted:NO];

  EXPECT_OCMOCK_VERIFY((id)mock_consumer);
}

// Tests that bottomsheet is auto-dismissed when the timer for confirmation
// state times out.
TEST_F(SaveCardBottomSheetMediatorTest, ConfirmationAutoDismissed_OnTimeOut) {
  id<SaveCardBottomSheetConsumer> mock_consumer =
      OCMProtocolMock(@protocol(SaveCardBottomSheetConsumer));
  mediator_.consumer = mock_consumer;

  OCMExpect([mock_consumer showConfirmationState]);
  [mediator_ onCreditCardUploadCompleted:YES];

  OCMExpect([mock_autofill_commands_handler_ dismissSaveCardBottomSheet]);
  task_environment()->FastForwardBy(kConfirmationDismissDelay);
}

// Tests that bottomsheet is not auto-dismissed before the timer for
// confirmation state times out.
TEST_F(SaveCardBottomSheetMediatorTest,
       ConfirmationNotAutoDismissed_BeforeTimeout) {
  id<SaveCardBottomSheetConsumer> mock_consumer =
      OCMProtocolMock(@protocol(SaveCardBottomSheetConsumer));
  mediator_.consumer = mock_consumer;

  OCMExpect([mock_consumer showConfirmationState]);
  [mediator_ onCreditCardUploadCompleted:YES];

  OCMReject([mock_autofill_commands_handler_ dismissSaveCardBottomSheet]);

  // Advance timer slightly less than the actual timeout duration i.e
  // `kConfirmationDismissDelay`.
  task_environment()->FastForwardBy(kConfirmationDismissDelay * 0.99);
}

// Test that `OnCanceled` is called on the model and bottomsheet is dismissed
// when cancel button is pressed.
TEST_F(SaveCardBottomSheetMediatorTest, OnCancel) {
  EXPECT_CALL(*model_, OnCanceled());
  EXPECT_CALL(*model_, OnAccepted()).Times(0);
  OCMExpect([mock_autofill_commands_handler_ dismissSaveCardBottomSheet]);
  [mediator_ didCancel];
}
