// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/ios_chrome_payments_autofill_client.h"

#import "base/uuid.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace autofill {
namespace {

using ::testing::_;

class TestChromeAutofillClient : public ChromeAutofillClientIOS {
 public:
  explicit TestChromeAutofillClient(ChromeBrowserState* browser_state,
                                    web::WebState* web_state,
                                    infobars::InfoBarManager* infobar_manager,
                                    AutofillAgent* autofill_agent)
      : ChromeAutofillClientIOS(browser_state,
                                web_state,
                                infobar_manager,
                                autofill_agent) {
    autofill::CreditCard credit_card(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        "https://www.example.test/");
    save_card_delegate_ = MockAutofillSaveCardInfoBarDelegateMobileFactory::
        CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(/*upload=*/true,
                                                               credit_card);
  }
  MockAutofillSaveCardInfoBarDelegateMobile*
  GetAutofillSaveCardInfoBarDelegateIOS() override {
    return save_card_delegate_.get();
  }

 private:
  std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile>
      save_card_delegate_;
};

class IOSChromePaymentsAutofillClientTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    browser_state_ = TestChromeBrowserState::Builder().Build();
    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
    InfoBarManagerImpl::CreateForWebState(web_state_.get());
    infobars::InfoBarManager* infobar_manager =
        InfoBarManagerImpl::FromWebState(web_state_.get());
    autofill_agent_ =
        [[AutofillAgent alloc] initWithPrefService:browser_state_->GetPrefs()
                                          webState:web_state_.get()];
    autofill_client_ = std::make_unique<TestChromeAutofillClient>(
        browser_state_.get(), web_state_.get(), infobar_manager,
        autofill_agent_);
  }

  TestChromeAutofillClient* client() { return autofill_client_.get(); }

  payments::IOSChromePaymentsAutofillClient* payments_client() {
    return client()->GetPaymentsAutofillClient();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  AutofillAgent* autofill_agent_;
  std::unique_ptr<TestChromeAutofillClient> autofill_client_;
};

TEST_F(IOSChromePaymentsAutofillClientTest, CreditCardUploadCompleted) {
  EXPECT_CALL(*(client()->GetAutofillSaveCardInfoBarDelegateIOS()),
              CreditCardUploadCompleted(_, _));
  payments_client()->CreditCardUploadCompleted(
      /*card_saved=*/true, /*on_confirmation_closed_callback=*/std::nullopt);
}

}  // namespace

}  // namespace autofill
