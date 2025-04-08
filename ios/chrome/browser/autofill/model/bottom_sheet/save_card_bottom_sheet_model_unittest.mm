// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"

#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill {

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
