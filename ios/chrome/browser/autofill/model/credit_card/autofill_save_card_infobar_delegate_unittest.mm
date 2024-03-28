// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"

#import <memory>
#import <string>

#import "base/functional/bind.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

namespace {
constexpr int kNavEntryId = 10;
}  // namespace

class AutofillSaveCardInfoBarDelegateTest : public PlatformTest {
 public:
  void LocalSaveCardPromptCallbackFn(
      AutofillClient::SaveCardOfferUserDecision user_decision) {
    last_user_decision_ = user_decision;
  }

  void UploadSaveCardPromptCallbackFn(
      AutofillClient::SaveCardOfferUserDecision user_decision,
      const AutofillClient::UserProvidedCardDetails&
          user_provided_card_details) {
    last_user_decision_ = user_decision;
    last_user_provided_card_details_ = user_provided_card_details;
  }

 protected:
  ~AutofillSaveCardInfoBarDelegateTest() override = default;

  void SetUp() override {
    delegate_ = CreateDelegate(
        /*callback=*/static_cast<AutofillClient::LocalSaveCardPromptCallback>(
            base::DoNothing()));
  }

  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> CreateDelegate(
      absl::variant<AutofillClient::LocalSaveCardPromptCallback,
                    AutofillClient::UploadSaveCardPromptCallback>
          save_card_callback) {
    auto save_card_delegate = std::make_unique<AutofillSaveCardDelegate>(
        std::move(save_card_callback), AutofillClient::SaveCreditCardOptions());
    return std::make_unique<AutofillSaveCardInfoBarDelegateIOS>(
        AutofillSaveCardUiInfo(), std::move(save_card_delegate));
  }

  infobars::InfoBarDelegate::NavigationDetails nav_details_that_expire_{
      .entry_id = kNavEntryId,
      .is_navigation_to_different_page = true,
      .did_replace_entry = false,
      .is_reload = true,
      .is_redirect = false,
      .is_form_submission = false};
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate_;
  std::optional<AutofillClient::SaveCardOfferUserDecision> last_user_decision_;
  std::optional<AutofillClient::UserProvidedCardDetails>
      last_user_provided_card_details_;
};

// Tests that the user decision is propagated when accepting local save.
TEST_F(AutofillSaveCardInfoBarDelegateTest, UpdateAndAccept_Local) {
  AutofillClient::LocalSaveCardPromptCallback callback = base::BindOnce(
      &AutofillSaveCardInfoBarDelegateTest::LocalSaveCardPromptCallbackFn,
      base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(callback));

  EXPECT_TRUE(delegate->UpdateAndAccept(/*cardholder_name=*/u"Test User",
                                        /*expiration_date_month=*/u"04",
                                        /*expiration_date_year=*/u"24"));

  ASSERT_TRUE(last_user_decision_);
  EXPECT_EQ(AutofillClient::SaveCardOfferUserDecision::kAccepted,
            last_user_decision_);
}

// Tests that the user decision is propagated when accepting upload.
TEST_F(AutofillSaveCardInfoBarDelegateTest, UpdateAndAccept_Upload) {
  AutofillClient::UploadSaveCardPromptCallback callback = base::BindOnce(
      &AutofillSaveCardInfoBarDelegateTest::UploadSaveCardPromptCallbackFn,
      base::Unretained(this));
  std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate =
      CreateDelegate(std::move(callback));

  const std::u16string cardholder_name = u"Test User";
  const std::u16string expiration_date_month = u"04";
  const std::u16string expiration_date_year = u"24";

  EXPECT_TRUE(
      delegate->UpdateAndAccept(/*cardholder_name=*/cardholder_name,
                                /*expiration_date_month=*/expiration_date_month,
                                /*expiration_date_year=*/expiration_date_year));

  ASSERT_TRUE(last_user_decision_ && last_user_provided_card_details_);
  EXPECT_EQ(AutofillClient::SaveCardOfferUserDecision::kAccepted,
            last_user_decision_);
  EXPECT_THAT(
      *last_user_provided_card_details_,
      ::testing::FieldsAre(/*cardholder_name=*/cardholder_name,
                           /*expiration_date_month=*/expiration_date_month,
                           /*expiration_date_year=*/expiration_date_year));
}

// Tests that the infobar expires.
TEST_F(AutofillSaveCardInfoBarDelegateTest, ShouldExpire_True_WhenReload) {
  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when redirect.
TEST_F(AutofillSaveCardInfoBarDelegateTest, ShouldExpire_False_WhenRedirect) {
  nav_details_that_expire_.is_redirect = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when form submission.
TEST_F(AutofillSaveCardInfoBarDelegateTest,
       ShouldExpire_False_WhenFormSubmission) {
  nav_details_that_expire_.is_form_submission = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

}  // namespace autofill
