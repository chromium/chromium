// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mediator.h"

#import <Foundation/Foundation.h>

#import <string>

#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/grit/components_scaled_resources.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

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

 protected:
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
}

// Test that `OnAccepted` is called on the model when bottomsheet is accepted.
TEST_F(SaveCardBottomSheetMediatorTest, OnAccept) {
  EXPECT_CALL(*model_, OnAccepted());
  EXPECT_CALL(*model_, OnCanceled()).Times(0);
  [mediator_ didAccept];
}

// Test that `OnCanceled` is called on the model and bottomsheet is dismissed
// when cancel button is pressed.
TEST_F(SaveCardBottomSheetMediatorTest, OnCancel) {
  EXPECT_CALL(*model_, OnCanceled());
  EXPECT_CALL(*model_, OnAccepted()).Times(0);
  OCMExpect([mock_autofill_commands_handler_ dismissSaveCardBottomSheet]);
  [mediator_ didCancel];
}
