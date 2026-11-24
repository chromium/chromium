// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"

#import <memory>
#import <utility>

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/gtest_util.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/test/values_test_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/personal_context_entity_metadata_manager.h"
#import "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#import "components/autofill/core/browser/integrators/password_form_classification.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/core/common/autofill_test_util.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/autofill/ios/common/features.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/personal_context/core/mock_personal_context_eligibility_service.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"
#import "ios/chrome/browser/autofill/model/autofill_agent_delegate.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_infobar_delegate_ios.h"
#import "ios/chrome/browser/autofill/model/autofill_policy_service_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_eligibility_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/form_input_accessory_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/task_observer_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using personal_context::MockPersonalContextEligibilityService;
using personal_context::PersonalContextEligibilityState;

namespace autofill {

namespace {

using ::testing::HasSubstr;

class TestAutofillManager : public BrowserAutofillManager {
 public:
  explicit TestAutofillManager(AutofillDriverIOS* driver)
      : BrowserAutofillManager(driver) {}

  TestAutofillManagerWaiter& waiter() { return waiter_; }

  const FormStructure* WaitForMatchingForm(
      base::RepeatingCallback<bool(const FormStructure&)> pred) {
    return autofill::WaitForMatchingForm(this, std::move(pred),
                                         base::Seconds(2));
  }

  void OnFormsSeen(std::vector<FormData> updated_forms,
                   std::vector<FormGlobalId> removed_form_ids,
                   RendererEventPassKey pass_key) override {
    ++forms_seen_count_;
    BrowserAutofillManager::OnFormsSeen(std::move(updated_forms),
                                        std::move(removed_form_ids), pass_key);
  }

  int forms_seen_count() const { return forms_seen_count_; }

 private:
  int forms_seen_count_ = 0;
  TestAutofillManagerWaiter waiter_{*this, {AutofillManagerEvent::kFormsSeen}};
};

}  //  namespace

class ChromeAutofillClientIOSTest : public PlatformTest {
 public:
  ChromeAutofillClientIOSTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    scene_state_ = [[SceneState alloc] init];
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindOnce(
            &std::make_unique<affiliations::FakeAffiliationService>));
    builder.AddTestingFactory(
        GeminiServiceFactory::GetInstance(),
        base::BindOnce(&std::make_unique<FakeGeminiService>));
    builder.AddTestingFactory(
        IOSPersonalContextEligibilityServiceFactory::GetInstance(),
        base::BindOnce([](ProfileIOS&) {
          auto service = std::make_unique<
              testing::NiceMock<MockPersonalContextEligibilityService>>();
          ON_CALL(*service, GetEligibilityState())
              .WillByDefault(
                  testing::Return(PersonalContextEligibilityState::kEligible));
          return service;
        }));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    browser_ = std::make_unique<TestBrowser>(profile_, scene_state_);

    web::WebState::CreateParams params(profile_);
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    mock_snackbar_handler_ = OCMStrictProtocolMock(@protocol(SnackbarCommands));
    autofill_agent_delegate_ = [[AutofillAgentDelegate alloc]
        initWithSnackbarHandler:mock_snackbar_handler_];

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_snackbar_handler_
                             forProtocol:@protocol(SnackbarCommands)];

    AutofillAgent* autofill_agent =
        [[AutofillAgent alloc] initWithPrefService:profile_->GetPrefs()
                                          webState:web_state_.get()];

    autofill_agent.delegate = autofill_agent_delegate_;
    ActorTabHelper::CreateForWebState(web_state_.get());
    InfoBarManagerImpl::CreateForWebState(web_state_.get());
    autofill_client_ =
        std::make_unique<WithFakedFromWebState<ChromeAutofillClientIOS>>(
            profile_, web_state_.get(),
            InfoBarManagerImpl::FromWebState(web_state_.get()), autofill_agent);
    autofill_manager_injector_ =
        std::make_unique<TestAutofillManagerInjectorIOS<TestAutofillManager>>(
            web_state_.get());
    autofill_agent_ = autofill_agent;
  }

  void TearDown() override {
    [browser_->GetCommandDispatcher()
        stopDispatchingToTarget:mock_snackbar_handler_];
    web::test::WaitForBackgroundTasks();
    PlatformTest::TearDown();
  }

 protected:
  bool LoadHtmlAndWaitForFormsSeen(NSString* html,
                                   size_t expected_number_of_forms) {
    web::test::LoadHtml(html, web_state_.get());
    return main_frame_manager()->waiter().Wait(1) &&
           test_api(*main_frame_manager()).form_structures().size() ==
               expected_number_of_forms;
  }

  ChromeAutofillClientIOS& client() { return *autofill_client_; }

  TestAutofillManager* main_frame_manager() {
    return autofill_manager_injector_->GetForMainFrame();
  }

  web::WebState* web_state() { return web_state_.get(); }

  // Returns the suggestions the agent last pushed to the keyboard accessory.
  // `-retrieveSuggestionsForForm:webState:completionHandler:` runs its
  // completion synchronously, so the result is available upon return.
  NSArray<FormSuggestion*>* LastSuggestionsSentToKeyboard() {
    __block BOOL completion_called = NO;
    __block NSArray<FormSuggestion*>* suggestions = nil;
    [autofill_agent_
        retrieveSuggestionsForForm:nil
                          webState:web_state()
                 completionHandler:^(NSArray<FormSuggestion*>* form_suggestions,
                                     id<FormSuggestionProvider> provider) {
                   completion_called = YES;
                   suggestions = form_suggestions;
                 }];
    // Guards against the completion becoming asynchronous.
    EXPECT_TRUE(completion_called);
    return suggestions;
  }

  id autofill_agent_delegate_;
  id mock_snackbar_handler_;

  TestProfileIOS* profile() { return profile_; }

  void SignInFakeUserWithUnknownCapabilities() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentityWithUnknownCapabilities(fake_identity);
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile());
    auth_service->SignIn(fake_identity,
                         signin_metrics::AccessPoint::kStartPage);
  }

  void SetMustShowEquallyWeightedButtonsCapability(bool value) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    CoreAccountInfo core_account_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfo(core_account_info);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator
        .set_must_show_equally_weighted_buttons_for_private_passes_in_autofill(
            value);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  // Shows the entity import bubble and returns the extracted
  // `SaveEntityParams` if an infobar was displayed, or `std::nullopt` if the
  // prompt was suppressed.
  std::optional<SaveEntityParams> ShowEntityImportBubbleAndGetParams(
      EntityInstance entity,
      AutofillClient::EntityImportPromptResultCallback callback =
          base::DoNothing()) {
    client().ShowEntityImportBubble(
        std::move(entity), /*old_entity=*/std::nullopt,
        /*save_is_synchronous=*/true, /*public_passes_notice=*/{},
        std::move(callback));
    infobars::InfoBarManager* infobar_manager =
        InfoBarManagerImpl::FromWebState(web_state());
    if (infobar_manager->infobars().empty()) {
      return std::nullopt;
    }
    auto* delegate = static_cast<AutofillAiSaveEntityInfoBarDelegateIOS*>(
        infobar_manager->infobars()[0]->delegate());
    return delegate->ExtractParams();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};

  web::ScopedTestingWebClient web_client_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  // Declared after `web_state_` so that the agent, which holds a raw_ptr to the
  // profile's `PrefService`, is released before the profile goes away.
  AutofillAgent* autofill_agent_;
  std::unique_ptr<ChromeAutofillClientIOS> autofill_client_;
  std::unique_ptr<TestAutofillManagerInjectorIOS<TestAutofillManager>>
      autofill_manager_injector_;
  std::unique_ptr<TestBrowser> browser_;
  SceneState* scene_state_;
};

TEST_F(ChromeAutofillClientIOSTest, GetAffiliationService) {
  EXPECT_NE(nullptr, client().GetAffiliationService());
}

// Tests that GetAutofillManagerForPrimaryMainFrame() returns the main frame's
// AutofillManager.
TEST_F(ChromeAutofillClientIOSTest, GetAutofillManagerForPrimaryMainFrame) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormsSeen(
      @"<form>"
       "<input name='username' autocomplete='username'>"
       "</form>",
      1));
  EXPECT_EQ(client().GetAutofillManagerForPrimaryMainFrame(),
            main_frame_manager());
}

// Tests that ClassifyAsPasswordForm correctly classifies a login form.
TEST_F(ChromeAutofillClientIOSTest, ClassifyAsPasswordForm) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormsSeen(
      @"<form>"
       "<input name='username' autocomplete='username'>"
       "<input type='password' name='password' autocomplete='current-password'>"
       "</form>",
      1));
  const FormStructure& form =
      *test_api(*main_frame_manager()).form_structures().front();
  FormData form_data = form.ToFormData();
  const auto expected = PasswordFormClassification{
      .type = PasswordFormClassification::Type::kLoginForm,
      .username_field = form_data.fields()[0].global_id(),
      .password_field = form_data.fields()[1].global_id()};
  EXPECT_EQ(client().ClassifyAsPasswordForm(*main_frame_manager(),
                                            form_data.global_id(),
                                            form_data.fields()[0].global_id()),
            expected);
}

// Tests that `ClassifyAsPasswordForm()` correctly classifies a login renderer
// form that is part of a bigger browser form that stretches across multiple
// frames. Also tests that non-login renderer forms aren't classified as such.
TEST_F(ChromeAutofillClientIOSTest, ClassifyAsPasswordForm_AcrossFrames) {
  // Render a xframe form composed of one password form and one address form.
  NSString* html =
      @"<form>"
       "<input name='username' autocomplete='username'>"
       "<input type='password' name='password' autocomplete='current-password'>"
       "<iframe srcdoc=\"<body><form><input type='text' name='address-level1' "
       "autocomplete='address-level1'></form></body>\"></iframe>"
       "</form>";
  web::test::LoadHtml(html, web_state());

  // Wait for any pending seen forms to be processed.
  ASSERT_TRUE(main_frame_manager()->waiter().Wait());

  // Wait on the browser form to be fully constructed.
  const FormStructure* form =
      main_frame_manager()->WaitForMatchingForm(base::BindRepeating(
          [](size_t num_fields, const FormStructure& form) {
            return num_fields == form.field_count();
          },
          3));
  ASSERT_TRUE(form);
  FormData browser_form = form->ToFormData();
  ASSERT_THAT(browser_form.fields(), ::testing::SizeIs(3));

  // Verify that the password renderer form is classified as a password form.
  const auto expected = PasswordFormClassification{
      .type = PasswordFormClassification::Type::kLoginForm,
      .username_field = browser_form.fields()[0].global_id(),
      .password_field = browser_form.fields()[1].global_id()};
  EXPECT_EQ(client().ClassifyAsPasswordForm(
                *main_frame_manager(), browser_form.global_id(),
                browser_form.fields()[0].global_id()),
            expected);
}

// Tests that `ClassifyAsPasswordForm()` doesn't classify non-login forms.
TEST_F(ChromeAutofillClientIOSTest,
       ClassifyAsPasswordForm_AcrossFrames_NonLoginForm) {
  // Render a xframe form composed of one password form and one address form.
  NSString* html =
      @"<form>"
       "<input name='username' autocomplete='username'>"
       "<input type='password' name='password' autocomplete='current-password'>"
       "<iframe srcdoc=\"<body><form><input type='text' name='address-level1' "
       "autocomplete='address-level1'></form></body>\"></iframe>"
       "</form>";
  web::test::LoadHtml(html, web_state());

  // Wait for any pending seen forms to be processed.
  ASSERT_TRUE(main_frame_manager()->waiter().Wait());

  // Wait on the browser form to be fully constructed.
  const FormStructure* form =
      main_frame_manager()->WaitForMatchingForm(base::BindRepeating(
          [](size_t num_fields, const FormStructure& form) {
            return num_fields == form.field_count();
          },
          3));
  ASSERT_TRUE(form);
  FormData browser_form = form->ToFormData();
  ASSERT_THAT(browser_form.fields(), ::testing::SizeIs(3));

  // Verify that the address renderer form isn't classified as a password form.
  EXPECT_EQ(client().ClassifyAsPasswordForm(
                *main_frame_manager(), browser_form.global_id(),
                browser_form.fields()[2].global_id()),
            PasswordFormClassification{});

  // Verify that a field with no corresponding form isn't classified.
  FieldGlobalId random_field_id = test::MakeFieldGlobalId();
  EXPECT_EQ(
      client().ClassifyAsPasswordForm(
          *main_frame_manager(), browser_form.global_id(), random_field_id),
      PasswordFormClassification{});
}

// Tests that `ShowAutofillAiPreFetchFailureNotification()` successfully adds
// the prefetch failure infobar to the InfoBarManager.
TEST_F(ChromeAutofillClientIOSTest, ShowAutofillAiPreFetchFailureNotification) {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  ASSERT_EQ(infobar_manager->infobars().size(), 0u);

  client().ShowAutofillAiPreFetchFailureNotification();

  EXPECT_EQ(infobar_manager->infobars().size(), 1u);
  infobars::InfoBar* infobar = infobar_manager->infobars()[0];
  EXPECT_EQ(infobar->delegate()->GetIdentifier(),
            infobars::InfoBarDelegate::
                AUTOFILL_AI_PRE_FETCH_FAILURE_INFOBAR_DELEGATE_IOS);

  // Calling it again should replace the existing one, so count remains 1.
  client().ShowAutofillAiPreFetchFailureNotification();
  EXPECT_EQ(infobar_manager->infobars().size(), 1u);
}

// Tests that `ShowAutofillAiPrivateInferenceNotice()` dispatches the command to
// show the Autofill AI Private Inference notice bottom sheet.
TEST_F(ChromeAutofillClientIOSTest, ShowAutofillAiPrivateInferenceNotice) {
  id mock_autofill_commands_handler =
      OCMStrictProtocolMock(@protocol(AutofillCommands));
  client().set_commands_handler(mock_autofill_commands_handler);

  OCMExpect(
      [mock_autofill_commands_handler showAutofillAIPrivateInferenceNotice]);

  client().ShowAutofillAiPrivateInferenceNotice();

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_handler);
}

// Tests that IsAutofillTypeBlockedByPolicy returns true when a domain
// is blocked by enterprise policy, and false otherwise.
TEST_F(ChromeAutofillClientIOSTest, IsAutofillTypeBlockedByPolicy) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableAutofillSettingsEnterprisePolicy);

  // Default is not blocked.
  EXPECT_FALSE(client().IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));
  EXPECT_TRUE(client().IsAutofillProfileEnabled());

  // Block the domain.
  base::ListValue blocked_list;
  base::DictValue entry;
  entry.Set("url_pattern", "https://[*.]example.com");
  base::ListValue blocked_types;
  blocked_types.Append("contact_info");
  entry.Set("blocked_types", std::move(blocked_types));
  blocked_list.Append(std::move(entry));
  profile()->GetPrefs()->SetList(prefs::kAutofillTypesBlocked,
                                 std::move(blocked_list));

  EXPECT_TRUE(client().IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));

  // Navigate to blocked domain.
  web::test::LoadHtml(@"<body></body>", GURL("https://www.example.com"),
                      web_state());
  EXPECT_FALSE(client().IsAutofillProfileEnabled());

  // Different category is not blocked.
  EXPECT_FALSE(client().IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kPayments));

  // Different domain is not blocked.
  EXPECT_FALSE(client().IsAutofillTypeBlockedByPolicy(
      GURL("https://www.google.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));

  // Navigate to unblocked domain.
  web::test::LoadHtml(@"<body></body>", GURL("https://www.google.com"),
                      web_state());
  EXPECT_TRUE(client().IsAutofillProfileEnabled());
}

// Tests that IsAutofillTypeBlockedByPolicy correctly applies the original
// profile's enterprise policy settings when queried from an incognito profile.
TEST_F(ChromeAutofillClientIOSTest, IsAutofillTypeBlockedByPolicy_Incognito) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableAutofillSettingsEnterprisePolicy);

  // Block the domain on the regular profile.
  base::ListValue blocked_list;
  base::DictValue entry;
  entry.Set("url_pattern", "https://[*.]example.com");
  base::ListValue blocked_types;
  blocked_types.Append("contact_info");
  entry.Set("blocked_types", std::move(blocked_types));
  blocked_list.Append(std::move(entry));
  profile()->GetPrefs()->SetList(prefs::kAutofillTypesBlocked,
                                 std::move(blocked_list));

  // Create an incognito profile.
  ProfileIOS* otr_profile = profile()->GetOffTheRecordProfile();

  // Ensure the policy service maps to the regular profile's policy service.
  EXPECT_EQ(AutofillPolicyServiceFactory::GetForProfile(profile()),
            AutofillPolicyServiceFactory::GetForProfile(otr_profile));

  // Create a client for the incognito profile.
  web::WebState::CreateParams params(otr_profile);
  std::unique_ptr<web::WebState> otr_web_state = web::WebState::Create(params);
  otr_web_state->GetView();
  otr_web_state->SetKeepRenderProcessAlive(true);

  AutofillAgent* otr_autofill_agent =
      [[AutofillAgent alloc] initWithPrefService:otr_profile->GetPrefs()
                                        webState:otr_web_state.get()];
  InfoBarManagerImpl::CreateForWebState(otr_web_state.get());
  WithFakedFromWebState<ChromeAutofillClientIOS> otr_client(
      otr_profile, otr_web_state.get(),
      InfoBarManagerImpl::FromWebState(otr_web_state.get()),
      otr_autofill_agent);

  // The incognito client should correctly report the blocked policy.
  EXPECT_TRUE(otr_client.IsAutofillTypeBlockedByPolicy(
      GURL("https://www.example.com"),
      AutofillClient::AutofillPolicyDataCategory::kContactInfo));
}

// Tests that IsAutofillEnabled correctly returns false when all active autofill
// types (including AI data types) are globally blocked by policy.
TEST_F(ChromeAutofillClientIOSTest, IsAutofillEnabled_BlockedByPolicy) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableAutofillSettingsEnterprisePolicy);

  // Disable profile and payments so IsAutofillEnabled depends on the AI types.
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled, false);

  // Enable the AI types.
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiIdentityEntitiesEnabled,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiTravelEntitiesEnabled,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiShoppingEntitiesEnabled,
                                    true);

  web::test::LoadHtml(@"<body></body>", GURL("https://www.example.com"),
                      web_state());
  EXPECT_TRUE(client().IsAutofillEnabled());

  // Block only identity docs.
  profile()->GetPrefs()->Set(prefs::kAutofillTypesBlocked,
                             base::test::ParseJson(
                                 R"([
            {
              "url_pattern": "https://[*.]example.com",
              "blocked_types": ["identity_docs"]
            }
          ])"));

  // Still true because travel and shopping are enabled.
  EXPECT_TRUE(client().IsAutofillEnabled());

  // Block identity docs and travel.
  profile()->GetPrefs()->Set(prefs::kAutofillTypesBlocked,
                             base::test::ParseJson(
                                 R"([
            {
              "url_pattern": "https://[*.]example.com",
              "blocked_types": ["identity_docs", "travel"]
            }
          ])"));

  EXPECT_TRUE(client().IsAutofillEnabled());

  // Block all three.
  profile()->GetPrefs()->Set(prefs::kAutofillTypesBlocked,
                             base::test::ParseJson(
                                 R"([
            {
              "url_pattern": "https://[*.]example.com",
              "blocked_types": ["identity_docs", "travel", "shopping"]
            }
          ])"));

  EXPECT_FALSE(client().IsAutofillEnabled());
}

// Tests that IsAutofillEnabled does not consider AI data types when the
// enterprise policy feature flag is disabled, strictly adhering to the original
// behavior.
TEST_F(ChromeAutofillClientIOSTest,
       IsAutofillEnabled_AiTypesGatedByEnterprisePolicyFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnableAutofillSettingsEnterprisePolicy);

  // Disable profile and payments so IsAutofillEnabled depends on the AI types.
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled, false);

  // Enable the AI types.
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiIdentityEntitiesEnabled,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiTravelEntitiesEnabled,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiShoppingEntitiesEnabled,
                                    true);

  EXPECT_FALSE(client().IsAutofillEnabled());
}

// Test that `IsTabInActorMode` returns true when `kAutofillForceActorMode` is
// enabled.
TEST_F(ChromeAutofillClientIOSTest, IsTabInActorMode_ForceActorMode) {
  base::test::ScopedFeatureList feature_list(
      features::debug::kAutofillForceActorMode);
  EXPECT_TRUE(client().IsTabInActorMode());
}

// Tests that `IsTabInActorMode` gets the control state from `ActorTabHelper`.
TEST_F(ChromeAutofillClientIOSTest, IsTabInActorMode_ActorTabHelper) {
  ActorTabHelper* actor_tab_helper = ActorTabHelper::FromWebState(web_state());
  ASSERT_TRUE(actor_tab_helper);

  EXPECT_FALSE(client().IsTabInActorMode());

  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  EXPECT_TRUE(client().IsTabInActorMode());

  actor_tab_helper->SetControlState(actor::ActorControlState::kUserControlled);
  EXPECT_TRUE(client().IsTabInActorMode());

  actor_tab_helper->SetControlState(actor::ActorControlState::kInactive);
  EXPECT_FALSE(client().IsTabInActorMode());
}

// Tests that `IsTabInActorMode` returns false when `ActorTabHelper` is not
// attached.
TEST_F(ChromeAutofillClientIOSTest, IsTabInActorMode_NoActorTabHelper) {
  web_state()->RemoveUserData(ActorTabHelper::UserDataKey());
  EXPECT_FALSE(client().IsTabInActorMode());
}

// Tests that `OnActorTaskStateChange` reparses known forms when entering actor
// mode.
TEST_F(ChromeAutofillClientIOSTest, OnActorTaskStateChange_ReparsesForms) {
  ActorTabHelper* actor_tab_helper = ActorTabHelper::FromWebState(web_state());
  ASSERT_TRUE(actor_tab_helper);

  NSString* html = @"<form><input name='name'><input name='address'></form>";
  ASSERT_TRUE(LoadHtmlAndWaitForFormsSeen(html, 1));

  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  ASSERT_TRUE(main_frame_manager()->waiter().Wait(1));
  EXPECT_TRUE(client().IsTabInActorMode());
}

// Tests that `OnActorTaskStateChange` reparses known forms when entering
// `kUserControlled` from `kInactive`.
TEST_F(ChromeAutofillClientIOSTest,
       OnActorTaskStateChange_ReparsesFormsOnUserControlled) {
  ActorTabHelper* actor_tab_helper = ActorTabHelper::FromWebState(web_state());
  ASSERT_TRUE(actor_tab_helper);

  NSString* html = @"<form><input name='name'><input name='address'></form>";
  ASSERT_TRUE(LoadHtmlAndWaitForFormsSeen(html, 1));

  actor_tab_helper->SetControlState(actor::ActorControlState::kUserControlled);
  ASSERT_TRUE(main_frame_manager()->waiter().Wait(1));
  EXPECT_TRUE(client().IsTabInActorMode());
}

// Tests that `OnActorTaskStateChange` does not reparse forms within actor mode
// or when exiting it, and reparses when re-entering.
TEST_F(ChromeAutofillClientIOSTest,
       OnActorTaskStateChange_DoesNotReparseWithinActorMode) {
  ActorTabHelper* actor_tab_helper = ActorTabHelper::FromWebState(web_state());
  ASSERT_TRUE(actor_tab_helper);

  NSString* html = @"<form><input name='name'><input name='address'></form>";
  ASSERT_TRUE(LoadHtmlAndWaitForFormsSeen(html, 1));

  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  ASSERT_TRUE(main_frame_manager()->waiter().Wait(1));
  const int forms_seen_before = main_frame_manager()->forms_seen_count();

  actor_tab_helper->SetControlState(actor::ActorControlState::kUserControlled);
  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  actor_tab_helper->SetControlState(actor::ActorControlState::kInactive);

  // Flush the extraction pipeline with one sentinel scan; any scan triggered by
  // the transitions above would increment `forms_seen_count()` beyond 1.
  base::test::TestFuture<bool> sentinel_done;
  main_frame_manager()->TriggerFormExtractionInAllFrames(
      /*force=*/true, sentinel_done.GetCallback());
  ASSERT_TRUE(sentinel_done.Wait());
  ASSERT_TRUE(main_frame_manager()->waiter().Wait(1));
  EXPECT_EQ(main_frame_manager()->forms_seen_count(), forms_seen_before + 1);

  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  ASSERT_TRUE(main_frame_manager()->waiter().Wait(1));
  EXPECT_EQ(main_frame_manager()->forms_seen_count(), forms_seen_before + 2);
}

// Test that `GetPersonalContextEntityMetadataManager` returns the manager for
// the profile.
TEST_F(ChromeAutofillClientIOSTest, GetPersonalContextEntityMetadataManager) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillAmbientAutofillSuppression);
  PersonalContextEntityMetadataManager* manager =
      client().GetPersonalContextEntityMetadataManager();
  ASSERT_NE(manager, nullptr);
  EXPECT_NE(manager->GetSyncControllerDelegate(), nullptr);
}

// Test that suggestion updates are attributed to the form and field the popup
// was opened for. `UpdateAutofillSuggestions()` carries no `PopupOpenArgs`, so
// the client has to reuse the IDs captured by `ShowAutofillSuggestions()`.
TEST_F(ChromeAutofillClientIOSTest, UpdateSuggestionsKeepsFormAndFieldId) {
  const FormGlobalId form_id = test::MakeFormGlobalId();
  const FieldGlobalId field_id = test::MakeFieldGlobalId();

  AutofillClient::PopupOpenArgs open_args;
  open_args.form_id = form_id;
  open_args.field_id = field_id;
  open_args.suggestions = {
      Suggestion(u"", SuggestionType::kFetchingAmbientData)};

  testing::NiceMock<MockAutofillSuggestionDelegate> delegate;
  client().ShowAutofillSuggestions(open_args, delegate.GetWeakPtr());

  client().UpdateAutofillSuggestions(
      {Suggestion(u"John Doe", SuggestionType::kAddressEntry)},
      FillingProduct::kAddress,
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      AutofillSuggestionsIgnoreFocusLoss(false));

  NSArray<FormSuggestion*>* suggestions = LastSuggestionsSentToKeyboard();
  ASSERT_EQ(1U, suggestions.count);
  EXPECT_EQ(SuggestionType::kAddressEntry, suggestions[0].type);
  EXPECT_EQ(form_id, suggestions[0].metadata.form_id);
  EXPECT_EQ(field_id, suggestions[0].metadata.field_id);
}

// Test that `IsGlicEnabled` reflects Gemini eligibility for the profile.
TEST_F(ChromeAutofillClientIOSTest, IsGlicEnabled) {
  base::test::ScopedFeatureList feature_list(kPageActionMenu);
  FakeGeminiService* fake_gemini_service = static_cast<FakeGeminiService*>(
      GeminiServiceFactory::GetForProfile(profile()));
  ASSERT_TRUE(fake_gemini_service);

  fake_gemini_service->SetIsEligible(false);
  EXPECT_FALSE(client().IsGlicEnabled());

  fake_gemini_service->SetIsEligible(true);
  EXPECT_TRUE(client().IsGlicEnabled());
}

// Tests that `GetAiPageContent` extracts page content in default mode when
// `kAutofillActionableAIPageContent` is disabled.
TEST_F(ChromeAutofillClientIOSTest, GetAiPageContent_Default) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillActionableAIPageContent);

  // Note: attributeType 1 = CONTENT_ATTRIBUTE_ROOT,
  //       attributeType 8 = CONTENT_ATTRIBUTE_FORM,
  //       attributeType 24 = CONTENT_ATTRIBUTE_FORM_CONTROL.
  base::Value js_result = base::test::ParseJson(R"json({
    "currentNodeInnerText": "Example text",
    "title": "Example",
    "rootNode": {
      "contentAttributes": {
        "attributeType": 1
      },
      "childrenNodes": [{
        "contentAttributes": {
          "attributeType": 8,
          "formData": {}
        },
        "childrenNodes": [{
          "contentAttributes": {
            "attributeType": 24,
            "formControlData": {
              "fieldName": "username"
            }
          }
        }]
      }]
    }
  })json");

  web::test::OverrideJavaScriptFeatures(
      profile(), {PageContextExtractorJavaScriptFeature::GetInstance(),
                  AutofillJavaScriptFeature::GetInstance()});

  web::FakeWebState fake_web_state;
  fake_web_state.SetBrowserState(profile());
  fake_web_state.SetVisibleURL(GURL("https://example.com/"));
  fake_web_state.SetContentsMimeType("text/html");

  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web::FakeWebFramesManager* frames_manager_ptr = frames_manager.get();
  fake_web_state.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                     std::move(frames_manager));
  fake_web_state.SetWebFramesManager(
      web::ContentWorld::kPageContentWorld,
      std::make_unique<web::FakeWebFramesManager>());

  std::unique_ptr<web::FakeWebFrame> main_frame =
      web::FakeWebFrame::CreateMainWebFrame(GURL("https://example.com/"));
  main_frame->set_browser_state(profile());
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  frames_manager_ptr->AddWebFrame(std::move(main_frame));
  main_frame_ptr->AddJsResultForFunctionCall(
      &js_result, "pageContextExtractor.extractPageContext");

  ActorTabHelper::CreateForWebState(&fake_web_state);
  InfoBarManagerImpl::CreateForWebState(&fake_web_state);
  WithFakedFromWebState<ChromeAutofillClientIOS> fake_client(
      profile(), &fake_web_state,
      InfoBarManagerImpl::FromWebState(&fake_web_state), nil);

  base::test::TestFuture<
      std::optional<optimization_guide::proto::AnnotatedPageContent>>
      future;
  fake_client.GetAiPageContent(future.GetCallback());

  std::optional<optimization_guide::proto::AnnotatedPageContent> result =
      future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->mode(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_MODE_DEFAULT);
  // Expected arguments after `nonce` in
  // `pageContextExtractor.extractPageContext`:
  // - `include_cross_origin_frame_content`: true
  // - `use_rich_extraction`: true
  // - `use_rich_extraction_with_actionable`: false
  // - `extract_paid_content`: true
  // - `attempt_paid_content_json_fixing`: false
  // - `include_sensitive_payments_for_redaction`: false
  // - `extract_autofill_otp_redactions`: false
  // - `extract_password_screenshot_redactions`: true
  EXPECT_THAT(
      base::UTF16ToUTF8(main_frame_ptr->GetLastJavaScriptCall()),
      HasSubstr(", true, true, false, true, false, false, false, true]);"));
  ASSERT_EQ(result->root_node().children_nodes_size(), 1);
  const optimization_guide::proto::ContentNode& form_node =
      result->root_node().children_nodes(0);
  EXPECT_EQ(form_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM);
  ASSERT_EQ(form_node.children_nodes_size(), 1);
  const optimization_guide::proto::ContentNode& input_node =
      form_node.children_nodes(0);
  EXPECT_EQ(input_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  EXPECT_EQ(input_node.content_attributes().form_control_data().field_name(),
            "username");
}

// Tests that `GetAiPageContent` extracts page content in actionable mode when
// `kAutofillActionableAIPageContent` is enabled.
TEST_F(ChromeAutofillClientIOSTest, GetAiPageContent_Actionable) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillActionableAIPageContent);

  // Note: attributeType 1 = CONTENT_ATTRIBUTE_ROOT,
  //       attributeType 8 = CONTENT_ATTRIBUTE_FORM,
  //       attributeType 24 = CONTENT_ATTRIBUTE_FORM_CONTROL.
  base::Value js_result = base::test::ParseJson(R"json({
    "currentNodeInnerText": "Example text",
    "title": "Example",
    "rootNode": {
      "contentAttributes": {
        "attributeType": 1
      },
      "childrenNodes": [{
        "contentAttributes": {
          "attributeType": 8,
          "formData": {}
        },
        "childrenNodes": [{
          "contentAttributes": {
            "attributeType": 24,
            "formControlData": {
              "fieldName": "username"
            }
          }
        }]
      }]
    }
  })json");

  web::test::OverrideJavaScriptFeatures(
      profile(), {PageContextExtractorJavaScriptFeature::GetInstance(),
                  AutofillJavaScriptFeature::GetInstance()});

  web::FakeWebState fake_web_state;
  fake_web_state.SetBrowserState(profile());
  fake_web_state.SetVisibleURL(GURL("https://example.com/"));
  fake_web_state.SetContentsMimeType("text/html");

  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web::FakeWebFramesManager* frames_manager_ptr = frames_manager.get();
  fake_web_state.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                     std::move(frames_manager));
  fake_web_state.SetWebFramesManager(
      web::ContentWorld::kPageContentWorld,
      std::make_unique<web::FakeWebFramesManager>());

  std::unique_ptr<web::FakeWebFrame> main_frame =
      web::FakeWebFrame::CreateMainWebFrame(GURL("https://example.com/"));
  main_frame->set_browser_state(profile());
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  frames_manager_ptr->AddWebFrame(std::move(main_frame));
  main_frame_ptr->AddJsResultForFunctionCall(
      &js_result, "pageContextExtractor.extractPageContext");

  ActorTabHelper::CreateForWebState(&fake_web_state);
  InfoBarManagerImpl::CreateForWebState(&fake_web_state);
  WithFakedFromWebState<ChromeAutofillClientIOS> fake_client(
      profile(), &fake_web_state,
      InfoBarManagerImpl::FromWebState(&fake_web_state), nil);

  base::test::TestFuture<
      std::optional<optimization_guide::proto::AnnotatedPageContent>>
      future;
  fake_client.GetAiPageContent(future.GetCallback());

  std::optional<optimization_guide::proto::AnnotatedPageContent> result =
      future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->mode(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_MODE_ACTIONABLE_ELEMENTS);
  // Expected arguments after `nonce` in
  // `pageContextExtractor.extractPageContext`:
  // - `include_cross_origin_frame_content`: true
  // - `use_rich_extraction`: true
  // - `use_rich_extraction_with_actionable`: true
  // - `extract_paid_content`: true
  // - `attempt_paid_content_json_fixing`: false
  // - `include_sensitive_payments_for_redaction`: false
  // - `extract_autofill_otp_redactions`: false
  // - `extract_password_screenshot_redactions`: true
  EXPECT_THAT(
      base::UTF16ToUTF8(main_frame_ptr->GetLastJavaScriptCall()),
      HasSubstr(", true, true, true, true, false, false, false, true]);"));
  ASSERT_EQ(result->root_node().children_nodes_size(), 1);
  const optimization_guide::proto::ContentNode& form_node =
      result->root_node().children_nodes(0);
  EXPECT_EQ(form_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM);
  ASSERT_EQ(form_node.children_nodes_size(), 1);
  const optimization_guide::proto::ContentNode& input_node =
      form_node.children_nodes(0);
  EXPECT_EQ(input_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  EXPECT_EQ(input_node.content_attributes().form_control_data().field_name(),
            "username");
}

// Test that `HideSuggestions` with `kAcceptSuggestion` calls
// `resetAutofillSuggestionsLoadingStates` on the FormInputAccessoryCommands
// handler.
TEST_F(ChromeAutofillClientIOSTest,
       HideSuggestions_ResetsLoadingStatesOnAccept) {
  id mock_form_input_accessory_handler =
      OCMStrictProtocolMock(@protocol(FormInputAccessoryCommands));
  client().set_form_input_accessory_commands_handler(
      mock_form_input_accessory_handler);

  OCMExpect([mock_form_input_accessory_handler
      resetAutofillSuggestionsLoadingStates]);

  client().HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                           std::nullopt);

  EXPECT_OCMOCK_VERIFY(mock_form_input_accessory_handler);

  // For other reasons, it should not be called.
  OCMReject([mock_form_input_accessory_handler
      resetAutofillSuggestionsLoadingStates]);
  client().HideSuggestions(SuggestionHidingReason::kUserAborted, std::nullopt);
  EXPECT_OCMOCK_VERIFY(mock_form_input_accessory_handler);
}

// Test that `GetPersonalContextEligibilityState` requires Gemini consent.
TEST_F(ChromeAutofillClientIOSTest,
       GetPersonalContextEligibilityStateRequiresConsent) {
  EXPECT_EQ(client().GetPersonalContextEligibilityState(),
            PersonalContextEligibilityState::kDisabledNotEligible);

  gemini::UpdateUserConsentPrefs(true, profile()->GetPrefs());
  EXPECT_EQ(client().GetPersonalContextEligibilityState(),
            PersonalContextEligibilityState::kEligible);

  gemini::UpdateUserConsentPrefs(false, profile()->GetPrefs());
  EXPECT_EQ(client().GetPersonalContextEligibilityState(),
            PersonalContextEligibilityState::kDisabledNotEligible);
}

// Tests that `ShowAutofillSuggestions`, `UpdateAutofillSuggestions`, and
// `HideSuggestions` delegate to `AutofillSuggestionControllerIOS` when the BAM
// suggestion flow is enabled.
TEST_F(ChromeAutofillClientIOSTest,
       ShowAndHideSuggestions_BAMSuggestionFlowEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kAutofillSuggestionPipelineIOS,
      {{kAutofillSuggestionPipelineStageParamName, "1"}});

  const FormGlobalId form_id = test::MakeFormGlobalId();
  const FieldGlobalId field_id = test::MakeFieldGlobalId();

  AutofillClient::PopupOpenArgs open_args;
  open_args.form_id = form_id;
  open_args.field_id = field_id;
  open_args.suggestions = {Suggestion(u"Alice", SuggestionType::kAddressEntry)};

  testing::NiceMock<MockAutofillSuggestionDelegate> delegate;
  EXPECT_CALL(delegate, OnSuggestionsShown).Times(1);
  AutofillClient::SuggestionUiSessionId session_id =
      client().ShowAutofillSuggestions(open_args, delegate.GetWeakPtr());

  EXPECT_TRUE(client().suggestion_controller().is_showing());
  EXPECT_EQ(session_id, client().GetSessionIdForCurrentAutofillSuggestions());
  ASSERT_EQ(1U, client().GetAutofillSuggestions().size());
  EXPECT_EQ(u"Alice", client().GetAutofillSuggestions()[0].main_text.value);

  NSArray<FormSuggestion*>* suggestions = LastSuggestionsSentToKeyboard();
  ASSERT_EQ(1U, suggestions.count);
  EXPECT_EQ(SuggestionType::kAddressEntry, suggestions[0].type);
  EXPECT_TRUE([@"Alice" isEqualToString:suggestions[0].value]);

  client().HideSuggestions(SuggestionHidingReason::kUserAborted,
                           /*product=*/std::nullopt);
  EXPECT_FALSE(client().suggestion_controller().is_showing());
  EXPECT_FALSE(
      client().GetSessionIdForCurrentAutofillSuggestions().has_value());
}

// Tests that `UpdateAutofillSuggestions` hides the suggestions as stale when
// the updated `main_filling_product` doesn't match the showing one.
TEST_F(ChromeAutofillClientIOSTest,
       UpdateSuggestions_ProductMismatchHidesAsStale_BAMSuggestionFlowEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kAutofillSuggestionPipelineIOS,
      {{kAutofillSuggestionPipelineStageParamName, "1"}});

  AutofillClient::PopupOpenArgs open_args;
  open_args.form_id = test::MakeFormGlobalId();
  open_args.field_id = test::MakeFieldGlobalId();
  open_args.suggestions = {
      Suggestion(u"John Doe", SuggestionType::kAddressEntry)};

  testing::NiceMock<MockAutofillSuggestionDelegate> delegate;
  ON_CALL(delegate, GetMainFillingProduct)
      .WillByDefault(testing::Return(FillingProduct::kAddress));
  EXPECT_CALL(delegate,
              OnSuggestionsHidden(SuggestionHidingReason::kStaleData));
  client().ShowAutofillSuggestions(open_args, delegate.GetWeakPtr());
  ASSERT_TRUE(client().suggestion_controller().is_showing());

  client().UpdateAutofillSuggestions(
      {Suggestion(u"4111", SuggestionType::kCreditCardEntry)},
      FillingProduct::kCreditCard,
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      AutofillSuggestionsIgnoreFocusLoss(false));

  EXPECT_FALSE(client().suggestion_controller().is_showing());
  EXPECT_EQ(0U, LastSuggestionsSentToKeyboard().count);
}

// Tests that `UpdateAutofillSuggestions` updates the controller when the
// `main_filling_product` matches the showing one.
TEST_F(ChromeAutofillClientIOSTest,
       UpdateSuggestions_ProductMatchUpdates_BAMSuggestionFlowEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kAutofillSuggestionPipelineIOS,
      {{kAutofillSuggestionPipelineStageParamName, "1"}});

  AutofillClient::PopupOpenArgs open_args;
  open_args.form_id = test::MakeFormGlobalId();
  open_args.field_id = test::MakeFieldGlobalId();
  open_args.suggestions = {
      Suggestion(u"John Doe", SuggestionType::kAddressEntry)};

  testing::NiceMock<MockAutofillSuggestionDelegate> delegate;
  ON_CALL(delegate, GetMainFillingProduct)
      .WillByDefault(testing::Return(FillingProduct::kAddress));
  client().ShowAutofillSuggestions(open_args, delegate.GetWeakPtr());

  client().UpdateAutofillSuggestions(
      {Suggestion(u"Jane Doe", SuggestionType::kAddressEntry)},
      FillingProduct::kAddress,
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      AutofillSuggestionsIgnoreFocusLoss(false));

  ASSERT_TRUE(client().suggestion_controller().is_showing());
  ASSERT_EQ(1U, client().GetAutofillSuggestions().size());
  EXPECT_EQ(u"Jane Doe", client().GetAutofillSuggestions()[0].main_text.value);
}

// Tests that `ShowAutofillSuggestions` with empty suggestions hides the
// suggestions and returns a valid session ID without crashing.
TEST_F(ChromeAutofillClientIOSTest,
       ShowAutofillSuggestions_EmptySuggestionsHides_BAMSuggestionFlowEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kAutofillSuggestionPipelineIOS,
      {{kAutofillSuggestionPipelineStageParamName, "1"}});

  AutofillClient::PopupOpenArgs open_args;
  open_args.form_id = test::MakeFormGlobalId();
  open_args.field_id = test::MakeFieldGlobalId();
  open_args.suggestions = {
      Suggestion(u"John Doe", SuggestionType::kAddressEntry)};

  testing::NiceMock<MockAutofillSuggestionDelegate> delegate;
  client().ShowAutofillSuggestions(open_args, delegate.GetWeakPtr());
  ASSERT_TRUE(client().suggestion_controller().is_showing());
  ASSERT_EQ(1U, LastSuggestionsSentToKeyboard().count);

  open_args.suggestions = {};
  EXPECT_CALL(delegate,
              OnSuggestionsHidden(SuggestionHidingReason::kNoSuggestions));
  client().ShowAutofillSuggestions(open_args, delegate.GetWeakPtr());

  EXPECT_FALSE(client().suggestion_controller().is_showing());
  EXPECT_EQ(std::nullopt, client().suggestion_controller().GetUiSessionId());
  EXPECT_EQ(0U, LastSuggestionsSentToKeyboard().count);
}

// Tests that `UpdateAutofillSuggestions` with empty suggestions hides the
// active suggestions and notifies the bridge.
TEST_F(ChromeAutofillClientIOSTest,
       UpdateSuggestions_EmptySuggestionsHides_BAMSuggestionFlowEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kAutofillSuggestionPipelineIOS,
      {{kAutofillSuggestionPipelineStageParamName, "1"}});

  AutofillClient::PopupOpenArgs open_args;
  open_args.form_id = test::MakeFormGlobalId();
  open_args.field_id = test::MakeFieldGlobalId();
  open_args.suggestions = {
      Suggestion(u"John Doe", SuggestionType::kAddressEntry)};

  testing::NiceMock<MockAutofillSuggestionDelegate> delegate;
  ON_CALL(delegate, GetMainFillingProduct)
      .WillByDefault(testing::Return(FillingProduct::kAddress));
  client().ShowAutofillSuggestions(open_args, delegate.GetWeakPtr());
  ASSERT_TRUE(client().suggestion_controller().is_showing());
  ASSERT_EQ(1U, LastSuggestionsSentToKeyboard().count);

  EXPECT_CALL(delegate,
              OnSuggestionsHidden(SuggestionHidingReason::kNoSuggestions));
  client().UpdateAutofillSuggestions(
      {}, FillingProduct::kAddress,
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      AutofillSuggestionsIgnoreFocusLoss(false));

  EXPECT_FALSE(client().suggestion_controller().is_showing());
  EXPECT_EQ(std::nullopt, client().suggestion_controller().GetUiSessionId());
  EXPECT_EQ(0U, LastSuggestionsSentToKeyboard().count);
}

// Tests that `HideSuggestions` with a `product` filter only hides the showing
// suggestions when the product matches theirs, even if the delegate is gone.
TEST_F(ChromeAutofillClientIOSTest,
       HideSuggestions_ProductFilter_BAMSuggestionFlowEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kAutofillSuggestionPipelineIOS,
      {{kAutofillSuggestionPipelineStageParamName, "1"}});

  AutofillClient::PopupOpenArgs open_args;
  open_args.form_id = test::MakeFormGlobalId();
  open_args.field_id = test::MakeFieldGlobalId();
  open_args.suggestions = {
      Suggestion(u"John Doe", SuggestionType::kAddressEntry)};

  auto delegate =
      std::make_unique<testing::NiceMock<MockAutofillSuggestionDelegate>>();
  ON_CALL(*delegate, GetMainFillingProduct)
      .WillByDefault(testing::Return(FillingProduct::kAddress));
  client().ShowAutofillSuggestions(open_args, delegate->GetWeakPtr());
  ASSERT_TRUE(client().suggestion_controller().is_showing());
  delegate.reset();

  client().HideSuggestions(SuggestionHidingReason::kUserAborted,
                           FillingProduct::kCreditCard);
  EXPECT_TRUE(client().suggestion_controller().is_showing());

  client().HideSuggestions(SuggestionHidingReason::kUserAborted,
                           FillingProduct::kAddress);
  EXPECT_FALSE(client().suggestion_controller().is_showing());
}

class ChromeAutofillClientIOSPrivatePassRecordTypeTest
    : public ChromeAutofillClientIOSTest,
      public testing::WithParamInterface<EntityInstance::RecordType> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeAutofillClientIOSPrivatePassRecordTypeTest,
    testing::Values(EntityInstance::RecordType::kLocal,
                    EntityInstance::RecordType::kServerWallet));

// Tests that `ShowEntityImportBubble()` enables the explicit decline button
// when the account capability is true for private passes.
TEST_P(ChromeAutofillClientIOSPrivatePassRecordTypeTest,
       ShowEntityImportBubble_RefinedUxWhenCapabilityTrueForPrivatePass) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillRegionalizedPrivatePassesUxRefine};
  SignInFakeUserWithUnknownCapabilities();
  SetMustShowEquallyWeightedButtonsCapability(true);

  std::optional<SaveEntityParams> params = ShowEntityImportBubbleAndGetParams(
      test::GetPassportEntityInstance({.record_type = GetParam()}));
  ASSERT_TRUE(params.has_value());
  EXPECT_TRUE(params->show_explicit_decline_button);
}

// Tests that `ShowEntityImportBubble()` keeps the explicit decline button
// disabled when the account capability is false for private passes.
TEST_P(ChromeAutofillClientIOSPrivatePassRecordTypeTest,
       ShowEntityImportBubble_StandardUxWhenCapabilityFalseForPrivatePass) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillRegionalizedPrivatePassesUxRefine};
  SignInFakeUserWithUnknownCapabilities();
  SetMustShowEquallyWeightedButtonsCapability(false);

  std::optional<SaveEntityParams> params = ShowEntityImportBubbleAndGetParams(
      test::GetPassportEntityInstance({.record_type = GetParam()}));
  ASSERT_TRUE(params.has_value());
  EXPECT_FALSE(params->show_explicit_decline_button);
}

// Tests that `ShowEntityImportBubble()` keeps the explicit decline button
// disabled when `kAutofillRegionalizedPrivatePassesUxRefine` is disabled.
TEST_P(ChromeAutofillClientIOSPrivatePassRecordTypeTest,
       ShowEntityImportBubble_StandardUxWhenFlagDisabledForPrivatePass) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillRegionalizedPrivatePassesUxRefine);
  SignInFakeUserWithUnknownCapabilities();

  std::optional<SaveEntityParams> params = ShowEntityImportBubbleAndGetParams(
      test::GetPassportEntityInstance({.record_type = GetParam()}));
  ASSERT_TRUE(params.has_value());
  EXPECT_FALSE(params->show_explicit_decline_button);
}

}  // namespace autofill
