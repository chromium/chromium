// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#import "components/autofill/core/browser/integrators/autofill_ai/mock_autofill_ai_manager.h"
#import "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "components/autofill/core/common/autofill_test_util.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/autofill/ios/common/features.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/personal_context/first_run/personal_context_first_run_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/model/strike_database_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_first_run_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/OCMock/OCMockMacros.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/origin.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;

constexpr char kTestGuid[] = "00000000-0000-4000-8000-000000000000";

// Fake implementation of PersonalContextFirstRunService to control the notice
// state.
class FakePersonalContextFirstRunService
    : public personal_context::PersonalContextFirstRunService {
 public:
  FakePersonalContextFirstRunService() = default;
  ~FakePersonalContextFirstRunService() override = default;

  void MarkPersonalContextAmbientAutofillNoticeAsAcknowledged() override {
    acknowledged_ = true;
  }
  bool ShouldShowPersonalContextAmbientAutofillNotice() const override {
    return should_show_;
  }
  void RecordAmbientAutofillNoticeImpression(uint32_t session_id) override {}
  void MarkPersonalContextInAtMemoryNoticeAsAcknowledged() override {}
  bool ShouldShowPersonalContextAtMemoryNotice() const override {
    return false;
  }
  void RecordAtMemoryNoticeImpression(uint32_t session_id) override {}

  void set_should_show(bool should_show) { should_show_ = should_show; }
  bool acknowledged() const { return acknowledged_; }

 private:
  bool should_show_ = false;
  bool acknowledged_ = false;
};

class TestChromeAutofillClientIOS : public autofill::ChromeAutofillClientIOS {
 public:
  using ChromeAutofillClientIOS::ChromeAutofillClientIOS;

  autofill::AutofillAiManager* GetAutofillAiManager() override {
    return mock_ai_manager_ ? mock_ai_manager_.get()
                            : ChromeAutofillClientIOS::GetAutofillAiManager();
  }

  void SetMockAiManager(autofill::AutofillAiManager* ai_manager) {
    mock_ai_manager_ = ai_manager;
  }

 private:
  raw_ptr<autofill::AutofillAiManager> mock_ai_manager_ = nullptr;
};

}  // namespace

// Fake implementation of AutofillCommands to capture ambient notice triggers.
@interface FakeAutofillCommandsForBottomSheet : NSObject
@property(nonatomic, assign) BOOL showAmbientAutofillNoticeCalled;
@end

@implementation FakeAutofillCommandsForBottomSheet
- (void)showAmbientAutofillNotice:(const autofill::FormActivityParams&)params {
  _showAmbientAutofillNoticeCalled = YES;
}
@end

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
                              /*request_url=*/std::nullopt, url::Origin());
  }

  // Helper to set up a page, form, and fake service, returning the main web
  // frame and autofill manager.
  void SetupAmbientNoticeForm(autofill::FieldRendererId renderer_id,
                              web::WebFrame*& out_frame,
                              autofill::AutofillManager*& out_manager,
                              autofill::FormGlobalId& out_form_id,
                              bool should_show_ambient_notice = true) {
    web::test::LoadHtml(@"<html><body></body></html>", web_state_.get());
    web::WebFrame* frame = AutofillBottomSheetJavaScriptFeature::GetInstance()
                               ->GetWebFramesManager(web_state_.get())
                               ->GetMainWebFrame();
    ASSERT_NE(frame, nullptr);
    out_frame = frame;

    autofill::AutofillDriverIOS* driver =
        autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state_.get(),
                                                             frame);
    autofill::LocalFrameToken frame_token = driver->GetFrameToken();
    out_manager = &driver->GetAutofillManager();

    autofill::FormData form;
    form.set_url(GURL("https://myform.com"));
    form.set_action(GURL("https://myform.com/submit"));

    autofill::FormFieldData field;
    field.set_form_control_type(autofill::FormControlType::kInputText);
    field.set_id_attribute(u"id1");
    field.set_name(u"name1");
    field.set_name_attribute(field.name());
    field.set_renderer_id(renderer_id);
    field.set_host_frame(frame_token);

    form.set_fields({field});
    out_form_id = form.global_id();

    autofill::TestAutofillManagerWaiter waiter(
        *out_manager, {autofill::AutofillManagerEvent::kFormsSeen});
    out_manager->OnFormsSeen({form}, {},
                             autofill::AutofillManagerTestApi::pass_key());
    ASSERT_TRUE(waiter.Wait(1));

    autofill::FormStructure* form_structure =
        const_cast<autofill::FormStructure*>(
            out_manager->FindCachedFormById(out_form_id));
    ASSERT_NE(nullptr, form_structure);
    form_structure->field(0)->SetTypeTo(
        autofill::AutofillType(autofill::NAME_FULL),
        autofill::AutofillPredictionSource::kHeuristics);

    std::vector<autofill::Suggestion> suggestions;
    if (should_show_ambient_notice) {
      autofill::Suggestion suggestion(
          autofill::SuggestionType::kAutofillAiPrivateInferenceNotice);
      suggestion.payload = autofill::Suggestion::AutofillAiPayload(
          autofill::EntityInstance::EntityId(kTestGuid));
      suggestions.push_back(suggestion);

      autofill::EntityDataManager* entity_manager =
          IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());
      ASSERT_NE(entity_manager, nullptr);
      autofill::test::PassportEntityOptions options;
      options.guid = kTestGuid;
      options.record_type =
          autofill::EntityInstance::RecordType::kPersonalContext;
      entity_manager->SetPersonalContextEntitiesForTesting(
          {autofill::test::GetPassportEntityInstance(options)});
    }
    ON_CALL(*mock_ai_manager_, GetSuggestions)
        .WillByDefault(Return(suggestions));

    FakePersonalContextFirstRunService* first_run_service =
        static_cast<FakePersonalContextFirstRunService*>(
            IOSPersonalContextFirstRunServiceFactory::GetForProfile(
                profile_.get()));
    ASSERT_NE(first_run_service, nullptr);
    first_run_service->set_should_show(should_show_ambient_notice);
  }

 protected:
  AutofillBottomSheetTabHelperTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactories(
        {TestProfileIOS::TestingFactory{
             static_cast<ProfileKeyedServiceFactoryIOS*>(
                 autofill::PersonalDataManagerFactory::GetInstance()),
             base::BindOnce(
                 [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
                   std::unique_ptr<autofill::TestPersonalDataManager> service =
                       std::make_unique<autofill::TestPersonalDataManager>();
                   service->SetPrefService(profile->GetPrefs());
                   return std::unique_ptr<KeyedService>(service.release());
                 })},
         TestProfileIOS::TestingFactory{
             static_cast<ProfileKeyedServiceFactoryIOS*>(
                 IOSPersonalContextFirstRunServiceFactory::GetInstance()),
             base::BindOnce([](ProfileIOS* profile)
                                -> std::unique_ptr<KeyedService> {
               return std::make_unique<FakePersonalContextFirstRunService>();
             })},
         TestProfileIOS::TestingFactory{
             static_cast<ProfileKeyedServiceFactoryIOS*>(
                 ios::HistoryServiceFactory::GetInstance()),
             ios::HistoryServiceFactory::GetDefaultFactory()},
         TestProfileIOS::TestingFactory{
             static_cast<ProfileKeyedServiceFactoryIOS*>(
                 ios::WebDataServiceFactory::GetInstance()),
             ios::WebDataServiceFactory::GetDefaultFactory()}});
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
        autofill::WithFakedFromWebState<TestChromeAutofillClientIOS>>(
        profile_.get(), web_state_.get(), infobar_manager, autofill_agent_);
    // Use NiceMock to ignore uninteresting lifecycle calls (e.g., OnFormSeen)
    // that are triggered by form activities but not relevant to testing the
    // bottom sheet trigger.
    mock_ai_manager_ =
        std::make_unique<NiceMock<autofill::MockAutofillAiManager>>(
            autofill_client_.get(),
            autofill::StrikeDatabaseFactory::GetForProfile(profile_.get()));
    static_cast<TestChromeAutofillClientIOS*>(autofill_client_.get())
        ->SetMockAiManager(mock_ai_manager_.get());
  }

  ~AutofillBottomSheetTabHelperTest() override {
    static_cast<TestChromeAutofillClientIOS*>(autofill_client_.get())
        ->SetMockAiManager(nullptr);
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<autofill::AutofillClient> autofill_client_;
  std::unique_ptr<autofill::MockAutofillAiManager> mock_ai_manager_;
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
  manager.OnFormsSeen({form}, {}, autofill::AutofillManagerTestApi::pass_key());
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
  manager.OnFormsSeen({form}, {}, autofill::AutofillManagerTestApi::pass_key());
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

// Tests that we attach the listeners when the form has fields with Autofill AI
// types.
TEST_F(AutofillBottomSheetTabHelperTest,
       UpdateListenersForAmbientAutofillFormAttachesListeners) {
  web::WebFrame* frame = nullptr;
  autofill::AutofillManager* manager = nullptr;
  autofill::FormGlobalId form_id;
  SetupAmbientNoticeForm(autofill::FieldRendererId(1), frame, manager, form_id);

  // Inject a spy script to verify api calls to attach the listeners.
  NSString* apiCallListenerScript =
      @"window.attachListenersCallCount = 0;"
      @"const originalAttach = "
      @"__gCrWeb.registeredApis.bottomSheet.functions.attachListeners;"
      @"__gCrWeb.registeredApis.bottomSheet.functions.attachListeners = "
      @"function(...args) { "
      @"    ++window.attachListenersCallCount;"
      @"    return originalAttach.apply(this, args);};";
  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(), apiCallListenerScript,
      AutofillBottomSheetJavaScriptFeature::GetInstance());

  // Register the listeners.
  helper_->UpdateListenersForAmbientAutofillForm(*manager, form_id,
                                                 /*only_new=*/false);

  // Wait and verify that there was an api call to attach the listeners.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Seconds(1), true, ^bool(void) {
        id result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
            web_state_.get(), @"window.attachListenersCallCount",
            AutofillBottomSheetJavaScriptFeature::GetInstance());
        if ([result isKindOfClass:[NSNumber class]] && [result intValue] == 1) {
          return true;
        }
        return false;
      }));
}

// Tests that we do not attach listeners when the first run service returns
// false for showing the ambient notice.
TEST_F(AutofillBottomSheetTabHelperTest,
       UpdateListenersForAmbientAutofillFormDoesNotAttachIfShouldShowIsFalse) {
  web::WebFrame* frame = nullptr;
  autofill::AutofillManager* manager = nullptr;
  autofill::FormGlobalId form_id;
  SetupAmbientNoticeForm(autofill::FieldRendererId(1), frame, manager, form_id,
                         /*should_show_ambient_notice=*/false);

  // Inject a spy script to verify api calls.
  NSString* apiCallListenerScript =
      @"window.attachListenersCallCount = 0;"
      @"const originalAttach = "
      @"__gCrWeb.registeredApis.bottomSheet.functions.attachListeners;"
      @"__gCrWeb.registeredApis.bottomSheet.functions.attachListeners = "
      @"function(...args) { "
      @"    ++window.attachListenersCallCount;"
      @"    return originalAttach.apply(this, args);};";
  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(), apiCallListenerScript,
      AutofillBottomSheetJavaScriptFeature::GetInstance());

  // Try to register the listeners.
  helper_->UpdateListenersForAmbientAutofillForm(*manager, form_id,
                                                 /*only_new=*/false);

  // Wait a bit and check that count is still 0.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(100));
  id result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state_.get(), @"window.attachListenersCallCount",
      AutofillBottomSheetJavaScriptFeature::GetInstance());
  EXPECT_NSEQ(result, @0);
}

// Tests that receiving a focused message for a registered ambient renderer ID
// triggers the ambient notice sheet.
TEST_F(AutofillBottomSheetTabHelperTest,
       OnFormMessageReceivedTriggersAmbientNotice) {
  web::WebFrame* frame = nullptr;
  autofill::AutofillManager* manager = nullptr;
  autofill::FormGlobalId form_id;
  SetupAmbientNoticeForm(autofill::FieldRendererId(0), frame, manager, form_id);

  // Register the listeners.
  helper_->UpdateListenersForAmbientAutofillForm(*manager, form_id,
                                                 /*only_new=*/false);

  // Set up the fake command handler.
  FakeAutofillCommandsForBottomSheet* commands_handler =
      [[FakeAutofillCommandsForBottomSheet alloc] init];
  helper_->SetAutofillBottomSheetHandler(
      (id<AutofillCommands>)commands_handler);

  // Simulate focus trigger event message.
  web::ScriptMessage form_message =
      ScriptMessageForForm(ValidFormMessageBody(frame->GetFrameId()));
  helper_->OnFormMessageReceived(form_message);

  // Verify that showAmbientAutofillNotice: is triggered.
  EXPECT_TRUE(commands_handler.showAmbientAutofillNoticeCalled);
}
