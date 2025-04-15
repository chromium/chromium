// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"

#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/grit/components_scaled_resources.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill {

using SaveCardBottomSheetModelFieldsTest = PlatformTest;

// Verfies each field of the model correctly maps to the AutofillSaveCardUiInfo
// properties.
TEST_F(SaveCardBottomSheetModelFieldsTest,
       VerifyModelToUiInfoPropertiesMapping) {
  int logo_icon_id = IDR_AUTOFILL_GOOGLE_PAY;
  std::u16string logo_icon_description = std::u16string(u"Logo description");
  std::u16string title_text = std::u16string(u"Title");
  std::u16string description_text = std::u16string(u"Description Text");
  std::u16string confirm_text = std::u16string(u"Save");
  std::u16string cancel_text = std::u16string(u"No thanks");
  std::u16string card_label = std::u16string(u"CardName ****2345");
  std::u16string card_sub_label = std::u16string(u"01/29");
  std::u16string card_description = std::u16string(u"Card description");
  int issuer_icon_id = IDR_AUTOFILL_METADATA_CC_VISA;

  autofill::AutofillSaveCardUiInfo ui_info = autofill::AutofillSaveCardUiInfo();
  ui_info.logo_icon_id = logo_icon_id;
  ui_info.logo_icon_description = logo_icon_description;
  ui_info.title_text = title_text;
  ui_info.description_text = description_text;
  ui_info.confirm_text = confirm_text;
  ui_info.cancel_text = cancel_text;
  ui_info.card_label = card_label;
  ui_info.card_sub_label = card_sub_label;
  ui_info.card_description = card_description;
  ui_info.issuer_icon_id = issuer_icon_id;

  std::unique_ptr<SaveCardBottomSheetModel> model = std::make_unique<
      SaveCardBottomSheetModel>(
      std::move(ui_info),
      std::make_unique<autofill::AutofillSaveCardDelegate>(
          static_cast<autofill::payments::PaymentsAutofillClient::
                          UploadSaveCardPromptCallback>(base::DoNothing()),
          autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions{}));
  EXPECT_EQ(model->logo_icon_id(), logo_icon_id);
  EXPECT_EQ(model->logo_icon_description(), logo_icon_description);
  EXPECT_EQ(model->title(), title_text);
  EXPECT_EQ(model->subtitle(), description_text);
  EXPECT_EQ(model->accept_button_text(), confirm_text);
  EXPECT_EQ(model->cancel_button_text(), cancel_text);
  EXPECT_EQ(model->card_name_last_four_digits(), card_label);
  EXPECT_EQ(model->card_expiry_date(), card_sub_label);
  EXPECT_EQ(model->card_accessibility_description(), card_description);
  EXPECT_EQ(model->issuer_icon_id(), issuer_icon_id);
}

class MockAutofillSaveCardDelegate : public AutofillSaveCardDelegate {
 public:
  MockAutofillSaveCardDelegate()
      : AutofillSaveCardDelegate(
            static_cast<autofill::payments::PaymentsAutofillClient::
                            UploadSaveCardPromptCallback>(base::DoNothing()),
            payments::PaymentsAutofillClient::SaveCreditCardOptions()) {}

  ~MockAutofillSaveCardDelegate() override = default;

  MOCK_METHOD(void, OnUiAccepted, (base::OnceClosure), (override));
  MOCK_METHOD(void, OnUiCanceled, (), (override));
};

class SaveCardBottomSheetModelTest : public PlatformTest {
 public:
  SaveCardBottomSheetModelTest() {
    std::unique_ptr<MockAutofillSaveCardDelegate> delegate =
        std::make_unique<MockAutofillSaveCardDelegate>();
    save_card_delegate_ = delegate.get();
    save_card_bottom_sheet_model_ = std::make_unique<SaveCardBottomSheetModel>(
        AutofillSaveCardUiInfo(), std::move(delegate));
  }

 protected:
  raw_ptr<MockAutofillSaveCardDelegate> save_card_delegate_ = nil;
  std::unique_ptr<SaveCardBottomSheetModel> save_card_bottom_sheet_model_;
};

TEST_F(SaveCardBottomSheetModelTest, OnAccepted) {
  EXPECT_CALL(*save_card_delegate_, OnUiAccepted);
  save_card_bottom_sheet_model_->OnAccepted();
}

TEST_F(SaveCardBottomSheetModelTest, OnCanceled) {
  EXPECT_CALL(*save_card_delegate_, OnUiCanceled);
  save_card_bottom_sheet_model_->OnCanceled();
}

}  // namespace autofill
