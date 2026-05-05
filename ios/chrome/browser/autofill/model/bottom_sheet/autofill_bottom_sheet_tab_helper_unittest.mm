// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#import "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/autofill/ios/common/features.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/OCMock/OCMockMacros.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture to test AutofillBottomSheetTabHelper class.
class AutofillBottomSheetTabHelperTest : public PlatformTest {
 public:
  // Returns a valid form message body to trigger the proactive password
  // generation bottom sheet.
  std::unique_ptr<base::Value> ValidFormMessageBody(std::string frame_id) {
    return std::make_unique<base::Value>(
        base::DictValue()
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
  AutofillBottomSheetTabHelperTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactories({TestProfileIOS::TestingFactory{
        (ProfileKeyedServiceFactoryIOS*)
            autofill::PersonalDataManagerFactory::GetInstance(),
        base::BindOnce(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              std::unique_ptr<autofill::TestPersonalDataManager> service =
                  std::make_unique<autofill::TestPersonalDataManager>();
              service->SetPrefService(profile->GetPrefs());
              return std::unique_ptr<KeyedService>(service.release());
            })}});
    profile_ = std::move(builder).Build();

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

    // The AutofillClient has strange dependencies:
    // - It must be initialized *after* `web_state_` because it depends on
    //   `web_state_`.
    // - It must be destroyed *after* `web_state_` because AutofillDriverIOS
    //   holds a reference to it and is destroyed together with `web_state_`.
    //
    // That's why we initialize it in the constructor but put it in the
    // declaration order above `web_state_`.
    autofill_client_ = std::make_unique<
        autofill::WithFakedFromWebState<autofill::ChromeAutofillClientIOS>>(
        profile_.get(), web_state_.get(), infobar_manager, autofill_agent_);
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<autofill::AutofillClient> autofill_client_;
  raw_ptr<AutofillBottomSheetTabHelper> helper_;
  AutofillAgent* autofill_agent_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that receiving a valid form for password generation triggers the
// proactive password generation bottom sheet.
TEST_F(AutofillBottomSheetTabHelperTest,
       ShowProactivePasswordGenerationBottomSheet) {
  scoped_feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSProactivePasswordGenerationBottomSheet);

  // Using LoadHtml to fake a real page load. This instantiates the main
  // web::WebFrame and sets up the JS features needed by AttachListeners.
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

  // Attempt to trigger a second time but this should be no op this time
  // because the listeners were detached on the first trigger.
  helper_->OnFormMessageReceived(form_message);

  // Verify that the bottom sheet is triggered upon receiving the signal from
  // the proactive password generation listeners.
  EXPECT_OCMOCK_VERIFY(generation_provider_mock);
}

// Tests that we detach the listeners when the invalidation of listeners is
// enabled and the form is not a CC form.
TEST_F(
    AutofillBottomSheetTabHelperTest,
    UpdateListenersForPaymentsForm_ListenersInvalidation_DetachWhenNotCreditCard) {
  scoped_feature_list_.InitAndEnableFeature(
      kAutofillPaymentsSheetDetachInvalidatedListenersIos);

  // Using LoadHtml to fake a real page load. This instantiates the main
  // web::WebFrame and sets up the JS features needed by AttachListeners.
  web::test::LoadHtml(@"<html><body></body></html>", web_state_.get());
  web::WebFrame* frame = AutofillBottomSheetJavaScriptFeature::GetInstance()
                             ->GetWebFramesManager(web_state_.get())
                             ->GetMainWebFrame();
  ASSERT_NE(frame, nullptr);
  autofill::LocalFrameToken frame_token =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state_.get(),
                                                           frame)
          ->GetFrameToken();
  std::string frame_id = frame->GetFrameId();

  autofill::AutofillManager& manager =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state_.get(),
                                                           frame)
          ->GetAutofillManager();

  // Add a credit card to the personal data manager.
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.SetExpirationYear(2099);
  // Add via autofill_client_ directly to ensure we use the test client's PDM.
  autofill_client_->GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card);

  // Inject a spy script to verify api calls to detach the listeners.
  // The script wraps the `detachListeners` function to increment a
  // counter. This must be done in the isolated world used by the feature.
  web::WebFrame* main_frame =
      AutofillBottomSheetJavaScriptFeature::GetInstance()
          ->GetWebFramesManager(web_state_.get())
          ->GetMainWebFrame();
  ASSERT_TRUE(main_frame);

  NSString* apiCallListenerScript =
      @"window.detachListenersCallCount = 0;"
      @"const originalDetach = "
      @"__gCrWeb.registeredApis.bottomSheet.functions.detachListeners;"
      @"__gCrWeb.registeredApis.bottomSheet.functions.detachListeners = "
      @"function(...args) { "
      @"    ++window.detachListenersCallCount;"
      @"    return originalDetach.apply(this, args);};";
  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(), apiCallListenerScript,
      AutofillBottomSheetJavaScriptFeature::GetInstance());

  // Set up the form and its fields.
  autofill::FormData form;
  form.set_url(GURL("https://myform.com"));
  form.set_action(GURL("https://myform.com/submit"));

  autofill::FormFieldData field;
  field.set_form_control_type(autofill::FormControlType::kInputText);
  field.set_id_attribute(u"id1");
  field.set_name(u"name1");
  field.set_name_attribute(field.name());
  field.set_renderer_id(autofill::FieldRendererId(1));
  field.set_host_frame(frame_token);

  // Add a credit card to make it a credit card form initially.
  // We need a number and an expiration date for IsCompleteCreditCardForm.
  autofill::FormFieldData cc_field;
  cc_field.set_form_control_type(autofill::FormControlType::kInputText);
  cc_field.set_id_attribute(u"cc_number");
  cc_field.set_name(u"cc_number");
  cc_field.set_name_attribute(cc_field.name());
  cc_field.set_renderer_id(autofill::FieldRendererId(2));
  cc_field.set_host_frame(frame_token);

  autofill::FormFieldData exp_field;
  exp_field.set_form_control_type(autofill::FormControlType::kInputText);
  exp_field.set_id_attribute(u"cc_exp");
  exp_field.set_name(u"cc_exp");
  exp_field.set_name_attribute(exp_field.name());
  exp_field.set_label(u"Expiration Date");
  exp_field.set_renderer_id(autofill::FieldRendererId(3));
  exp_field.set_host_frame(frame_token);

  form.set_fields({field, cc_field, exp_field});

  autofill::TestAutofillManagerWaiter waiter(
      manager, {autofill::AutofillManagerEvent::kFormsSeen});
  manager.OnFormsSeen({form}, {});
  ASSERT_TRUE(waiter.Wait(1));

  // Manually set field types to ensure the form is recognized as a credit card
  // form.
  autofill::FormStructure* form_structure =
      const_cast<autofill::FormStructure*>(
          manager.FindCachedFormById(form.global_id()));
  ASSERT_NE(nullptr, form_structure);
  ASSERT_EQ(form_structure->field_count(), 3u);
  // Indices: 0->text, 1->cc_number, 2->cc_exp
  form_structure->field(1)->SetTypeTo(
      autofill::AutofillType(autofill::CREDIT_CARD_NUMBER),
      autofill::AutofillPredictionSource::kHeuristics);
  form_structure->field(2)->SetTypeTo(
      autofill::AutofillType(autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR),
      autofill::AutofillPredictionSource::kHeuristics);

  ASSERT_TRUE(form_structure->IsCompleteCreditCardForm(
      autofill::FormStructure::CreditCardFormCompleteness::
          kCompleteCreditCardForm));
  ASSERT_FALSE(
      autofill::GetCreditCardsToSuggest(
          manager.client().GetPersonalDataManager().payments_data_manager())
          .empty());

  // Register the listeners (should attach because it is a CC form).
  helper_->UpdateListenersForPaymentsForm(manager, form.global_id(),
                                          /*only_new=*/false);

  // Now change the form to modify the CC fields, making it non-CC.
  // We keep the fields but change their attributes so they are not recognized
  // as CC fields.
  cc_field.set_name(u"other_field");
  cc_field.set_name_attribute(cc_field.name());
  cc_field.set_label(u"Other Field");

  exp_field.set_name(u"other_field_2");
  exp_field.set_name_attribute(exp_field.name());
  exp_field.set_label(u"Other Field 2");

  form.set_fields({field, cc_field, exp_field});
  manager.OnFormsSeen({form}, {});
  ASSERT_TRUE(waiter.Wait(1));

  // Update the listeners again which should detach the listeners this time
  // because the CC fields are no more recognized as such.
  helper_->UpdateListenersForPaymentsForm(manager, form.global_id(),
                                          /*only_new=*/false);

  // Wait on the api call to detach the listeners. Verifies that there is an
  // api call to detach the listeners.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Seconds(1), true, ^bool(void) {
        id result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
            web_state_.get(), @"window.detachListenersCallCount",
            AutofillBottomSheetJavaScriptFeature::GetInstance());
        if ([result isKindOfClass:[NSNumber class]] && [result intValue] == 1) {
          return true;
        }
        return false;
      }));
}
