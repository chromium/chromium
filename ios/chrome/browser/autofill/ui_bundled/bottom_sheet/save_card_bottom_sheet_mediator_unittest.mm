// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mediator.h"

#import <Foundation/Foundation.h>

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/grit/components_scaled_resources.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeSaveCardBottomSheetConsumer
    : NSObject <SaveCardBottomSheetConsumer>

@property(nonatomic, strong) UIImage* aboveTitleImage;
@property(nonatomic, copy) NSString* aboveTitleImageDescription;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* subtitle;

@end

@implementation FakeSaveCardBottomSheetConsumer

@end

class SaveCardBottomSheetMediatorTest : public PlatformTest {
 public:
  SaveCardBottomSheetMediatorTest() {
    autofill::AutofillSaveCardUiInfo ui_info =
        autofill::AutofillSaveCardUiInfo();
    ui_info.title_text = std::u16string(u"Title");
    ui_info.description_text = std::u16string(u"Description Text");
    ui_info.logo_icon_id = IDR_AUTOFILL_GOOGLE_PAY;
    ui_info.logo_icon_description = std::u16string(u"Logo description");
    std::unique_ptr<autofill::SaveCardBottomSheetModel> model =
        std::make_unique<autofill::SaveCardBottomSheetModel>(
            std::move(ui_info),
            std::make_unique<autofill::AutofillSaveCardDelegate>(
                static_cast<autofill::payments::PaymentsAutofillClient::
                                UploadSaveCardPromptCallback>(
                    base::DoNothing()),
                autofill::payments::PaymentsAutofillClient::
                    SaveCreditCardOptions{}));
    model_ = model->GetWeakPtr();
    mediator_ =
        [[SaveCardBottomSheetMediator alloc] initWithUIModel:std::move(model)];
  }

 protected:
  base::WeakPtr<autofill::SaveCardBottomSheetModel> model_;
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
}
