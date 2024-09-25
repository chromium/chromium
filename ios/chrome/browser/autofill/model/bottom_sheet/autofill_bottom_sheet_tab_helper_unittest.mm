// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/OCMock/OCMockMacros.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Test fixture to test AutofillBottomSheetTabHelper class.
class AutofillBottomSheetTabHelperTest : public PlatformTest {
 public:
  // Returns a valid form message body to trigger the proactive password
  // generation bottom sheet.
  std::unique_ptr<base::Value> ValidFormMessageBody(std::string frame_id) {
    return std::make_unique<base::Value>(
        base::Value::Dict()
            .Set("formName", "test_form")
            .Set("formRendererID", "1234")
            .Set("fieldIdentifier", "new_password")
            .Set("fieldRendererID", "0")
            .Set("fieldType", "new_password")
            .Set("type", "new_password")
            .Set("value", "new_password")
            .Set("hasUserGesture", "YES")
            .Set("frameID", frame_id));
  }

  // Returns a script message that represents a form.
  web::ScriptMessage ScriptMessageForForm(std::unique_ptr<base::Value> body) {
    return web::ScriptMessage(std::move(body),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt);
  }

 protected:
  class TestAutofillClient : public autofill::ChromeAutofillClientIOS {
   public:
    using ChromeAutofillClientIOS::ChromeAutofillClientIOS;
    autofill::AutofillCrowdsourcingManager* GetCrowdsourcingManager() override {
      return nullptr;
    }
  };

  AutofillBottomSheetTabHelperTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);

    AutofillBottomSheetTabHelper::CreateForWebState(web_state_.get());
    helper_ = AutofillBottomSheetTabHelper::FromWebState(web_state_.get());

    autofill_agent_ =
        [[AutofillAgent alloc] initWithPrefService:profile_->GetPrefs()
                                          webState:web_state_.get()];

    InfoBarManagerImpl::CreateForWebState(web_state_.get());
    infobars::InfoBarManager* infobar_manager =
        InfoBarManagerImpl::FromWebState(web_state_.get());

    autofill_client_ = std::make_unique<TestAutofillClient>(
        profile_.get(), web_state_.get(), infobar_manager, autofill_agent_);

    autofill::AutofillDriverIOSFactory::CreateForWebState(
        web_state_.get(), autofill_client_.get(), autofill_agent_,
        /*app_locale=*/"en");
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  raw_ptr<AutofillBottomSheetTabHelper> helper_;
  std::unique_ptr<autofill::AutofillClient> autofill_client_;
  AutofillAgent* autofill_agent_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that receiving a valid form for password generation triggers the
// proactive password generation bottom sheet.
TEST_F(AutofillBottomSheetTabHelperTest,
       ShowProactivePasswordGenerationBottomSheet) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSProactivePasswordGenerationBottomSheet);

  // Using LoadHtml to set up the JavaScript features needed by the
  // AttachListeners function.
  web::test::LoadHtml(@"<html><body></body></html>", web_state_.get());

  autofill::FieldRendererId new_password_rendererID(0);
  const std::vector<autofill::FieldRendererId> renderer_ids = {
      new_password_rendererID};
  web::WebFrame* frame = AutofillBottomSheetJavaScriptFeature::GetInstance()
                             ->GetWebFramesManager(web_state_.get())
                             ->GetMainWebFrame();
  helper_->AttachPasswordGenerationListeners(renderer_ids, frame->GetFrameId());

  id<PasswordGenerationProvider> generation_provider_mock =
      OCMProtocolMock(@protocol(PasswordGenerationProvider));
  autofill::FormRendererId form_renderer_ID(1234);
  OCMExpect([generation_provider_mock
      triggerPasswordGenerationForFormId:form_renderer_ID
                         fieldIdentifier:new_password_rendererID
                                 inFrame:frame
                               proactive:YES]);
  helper_->SetPasswordGenerationProvider(generation_provider_mock);

  id<AutofillCommands> commands_handler =
      OCMProtocolMock(@protocol(AutofillCommands));
  helper_->SetAutofillBottomSheetHandler(commands_handler);

  web::ScriptMessage form_message =
      ScriptMessageForForm(ValidFormMessageBody(frame->GetFrameId()));
  // Using OnFormMessageReceived to emulate receiving a signal from the
  // proactive password generation listeners. This emulates the trigger (the
  // focus on the listened field), so the bottom sheet should show.
  helper_->OnFormMessageReceived(form_message);

  // Verify that the bottom sheet is triggered upon receiving the signal from
  // the proactive password generation listeners.
  EXPECT_OCMOCK_VERIFY(generation_provider_mock);
}

}  // namespace
