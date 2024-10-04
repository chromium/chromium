// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/logging/stub_log_manager.h"
#import "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#import "components/autofill/core/browser/test_personal_data_manager.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/fake_autofill_agent.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_activity_tab_helper.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill::FormRendererId;
using autofill::FieldRendererId;
using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

namespace {

const char kApplicationLocale[] = "en-US";
NSString* const kTestFormName = @"FormName";
FormRendererId kTestFormRendererID = FormRendererId(0);
NSString* const kTestFieldIdentifier = @"FieldIdentifier";
FieldRendererId kTestFieldRendererID = FieldRendererId(1);
NSString* const kTestFieldValue = @"FieldValue";
NSString* const kTestDisplayDescription = @"DisplayDescription";

}  // namespace

class CWVAutofillControllerTest : public web::WebTest {
 protected:
  CWVAutofillControllerTest() {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
    pref_service_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillProfileEnabled, true);

    web_state_.SetBrowserState(&browser_state_);

    frame_id_ = base::SysUTF8ToNSString(web::kMainFakeFrameId);

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = frames_manager.get();
    web::ContentWorld content_world =
        autofill::AutofillJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    web_state_.SetWebFramesManager(content_world, std::move(frames_manager));

    autofill_agent_ =
        [[FakeAutofillAgent alloc] initWithPrefService:&pref_service_
                                              webState:&web_state_];

    auto password_manager_client =
        std::make_unique<WebViewPasswordManagerClient>(
            &web_state_, /*sync_service=*/nullptr, &pref_service_,
            /*identity_manager=*/nullptr, /*log_manager=*/nullptr,
            /*profile_store=*/nullptr, /*account_store=*/nullptr,
            /*reuse_manager=*/nullptr,
            /*requirements_service=*/nullptr);
    auto password_manager = std::make_unique<password_manager::PasswordManager>(
        password_manager_client.get());
    password_controller_ = OCMClassMock([SharedPasswordController class]);
    IOSPasswordManagerDriverFactory::CreateForWebState(
        &web_state_, password_controller_, password_manager.get());
    password_manager_client_ = password_manager_client.get();

    auto autofill_client = std::make_unique<autofill::WebViewAutofillClientIOS>(
        &pref_service_, &personal_data_manager_,
        /*autocomplete_history_manager=*/nullptr, &web_state_,
        /*identity_manager=*/nullptr, &strike_database_, &sync_service_,
        std::make_unique<autofill::StubLogManager>());
    autofill_controller_ = [[CWVAutofillController alloc]
             initWithWebState:&web_state_
               autofillClient:std::move(autofill_client)
                autofillAgent:autofill_agent_
              passwordManager:std::move(password_manager)
        passwordManagerClient:std::move(password_manager_client)
           passwordController:password_controller_
            applicationLocale:kApplicationLocale];
    form_activity_tab_helper_ =
        std::make_unique<autofill::TestFormActivityTabHelper>(&web_state_);
  }

  void SetUp() override {
    web::WebTest::SetUp();

    OverrideJavaScriptFeatures(
        {autofill::AutofillJavaScriptFeature::GetInstance()});
  }

  void AddWebFrame(std::unique_ptr<web::WebFrame> frame) {
    web_frames_manager_->AddWebFrame(std::move(frame));
  }

  TestingPrefServiceSimple pref_service_;
  web::FakeBrowserState browser_state_;
  autofill::TestPersonalDataManager personal_data_manager_;
  autofill::TestStrikeDatabase strike_database_;
  syncer::TestSyncService sync_service_;
  web::FakeWebState web_state_;
  NSString* frame_id_;
  web::FakeWebFramesManager* web_frames_manager_;
  CWVAutofillController* autofill_controller_;
  FakeAutofillAgent* autofill_agent_;
  id password_controller_;
  std::unique_ptr<autofill::TestFormActivityTabHelper>
      form_activity_tab_helper_;
  WebViewPasswordManagerClient* password_manager_client_;
};

// Tests CWVAutofillController fetch suggestions for profiles.
TEST_F(CWVAutofillControllerTest, FetchProfileSuggestions) {
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:kTestFieldValue
       displayDescription:kTestDisplayDescription
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
        backendIdentifier:nil
           requiresReauth:NO];
  [autofill_agent_ addSuggestion:suggestion
                     forFormName:kTestFormName
                 fieldIdentifier:kTestFieldIdentifier
                         frameID:frame_id_];

  OCMExpect([password_controller_
      checkIfSuggestionsAvailableForForm:[OCMArg any]
                          hasUserGesture:YES
                                webState:&web_state_
                       completionHandler:[OCMArg checkWithBlock:^(void (
                                             ^suggestionsAvailable)(BOOL)) {
                         suggestionsAvailable(NO);
                         return YES;
                       }]]);

  __block BOOL fetch_completion_was_called = NO;
  id fetch_completion = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    ASSERT_EQ(1U, suggestions.count);
    CWVAutofillSuggestion* autofillSuggestion = suggestions.firstObject;
    EXPECT_NSEQ(kTestFieldValue, autofillSuggestion.value);
    EXPECT_NSEQ(kTestDisplayDescription, autofillSuggestion.displayDescription);
    EXPECT_NSEQ(kTestFormName, autofillSuggestion.formName);
    fetch_completion_was_called = YES;
  };
  [autofill_controller_ fetchSuggestionsForFormWithName:kTestFormName
                                        fieldIdentifier:kTestFieldIdentifier
                                              fieldType:@""
                                                frameID:frame_id_
                                      completionHandler:fetch_completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fetch_completion_was_called;
  }));

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests CWVAutofillController fetch suggestions for passwords.
TEST_F(CWVAutofillControllerTest, FetchPasswordSuggestions) {
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:kTestFieldValue
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
        backendIdentifier:nil
           requiresReauth:NO];
  OCMExpect([password_controller_
      checkIfSuggestionsAvailableForForm:[OCMArg any]
                          hasUserGesture:YES
                                webState:&web_state_
                       completionHandler:[OCMArg checkWithBlock:^(void (
                                             ^suggestionsAvailable)(BOOL)) {
                         suggestionsAvailable(YES);
                         return YES;
                       }]]);
  OCMExpect([password_controller_
      retrieveSuggestionsForForm:[OCMArg any]
                        webState:&web_state_
               completionHandler:[OCMArg checkWithBlock:^(void (
                                     ^completionHandler)(NSArray*, id)) {
                 completionHandler(@[ suggestion ], nil);
                 return YES;
               }]]);

  __block BOOL fetch_completion_was_called = NO;
  id fetch_completion = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    ASSERT_EQ(1U, suggestions.count);
    CWVAutofillSuggestion* autofillSuggestion = suggestions.firstObject;
    EXPECT_TRUE([autofillSuggestion isPasswordSuggestion]);
    EXPECT_NSEQ(kTestFieldValue, autofillSuggestion.value);
    EXPECT_NSEQ(kTestFormName, autofillSuggestion.formName);
    fetch_completion_was_called = YES;
  };
  [autofill_controller_ fetchSuggestionsForFormWithName:kTestFormName
                                        fieldIdentifier:kTestFieldIdentifier
                                              fieldType:@""
                                                frameID:frame_id_
                                      completionHandler:fetch_completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fetch_completion_was_called;
  }));

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests CWVAutofillController accepts suggestion.
TEST_F(CWVAutofillControllerTest, AcceptSuggestion) {
  FormSuggestion* form_suggestion = [FormSuggestion
      suggestionWithValue:kTestFieldValue
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
        backendIdentifier:nil
           requiresReauth:NO];
  CWVAutofillSuggestion* suggestion =
      [[CWVAutofillSuggestion alloc] initWithFormSuggestion:form_suggestion
                                                   formName:kTestFormName
                                            fieldIdentifier:kTestFieldIdentifier
                                                    frameID:frame_id_
                                       isPasswordSuggestion:NO];
  __block BOOL accept_completion_was_called = NO;
  [autofill_controller_ acceptSuggestion:suggestion
                                 atIndex:0
                       completionHandler:^{
                         accept_completion_was_called = YES;
                       }];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return accept_completion_was_called;
  }));
  EXPECT_NSEQ(
      form_suggestion,
      [autofill_agent_ selectedSuggestionForFormName:kTestFormName
                                     fieldIdentifier:kTestFieldIdentifier
                                             frameID:frame_id_]);
}

// Tests CWVAutofillController delegate focus callback is invoked.
TEST_F(CWVAutofillControllerTest, FocusCallback) {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    [[delegate expect] autofillController:autofill_controller_
            didFocusOnFieldWithIdentifier:kTestFieldIdentifier
                                fieldType:@""
                                 formName:kTestFormName
                                  frameID:frame_id_
                                    value:kTestFieldValue
                            userInitiated:YES];

    autofill::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.form_renderer_id = kTestFormRendererID;
    params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
    params.field_renderer_id = kTestFieldRendererID;
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.frame_id = web::kMainFakeFrameId;
    params.has_user_gesture = true;
    params.type = "focus";
    auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
    form_activity_tab_helper_->FormActivityRegistered(frame.get(), params);
    [delegate verify];
}

// Tests CWVAutofillController delegate input callback is invoked.
TEST_F(CWVAutofillControllerTest, InputCallback) {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    [[delegate expect] autofillController:autofill_controller_
            didInputInFieldWithIdentifier:kTestFieldIdentifier
                                fieldType:@""
                                 formName:kTestFormName
                                  frameID:frame_id_
                                    value:kTestFieldValue
                            userInitiated:YES];

    autofill::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.frame_id = web::kMainFakeFrameId;
    params.type = "input";
    params.has_user_gesture = true;
    auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
    form_activity_tab_helper_->FormActivityRegistered(frame.get(), params);
    [delegate verify];
}

// Tests CWVAutofillController delegate input callback is invoked by keyup
// events.
TEST_F(CWVAutofillControllerTest, InputCallbackFromKeyup) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
          didInputInFieldWithIdentifier:kTestFieldIdentifier
                              fieldType:@""
                               formName:kTestFormName
                                frameID:frame_id_
                                  value:kTestFieldValue
                          userInitiated:YES];

  autofill::FormActivityParams params;
  params.form_name = base::SysNSStringToUTF8(kTestFormName);
  params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
  params.value = base::SysNSStringToUTF8(kTestFieldValue);
  params.frame_id = web::kMainFakeFrameId;
  params.type = "keyup";
  params.has_user_gesture = true;
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  form_activity_tab_helper_->FormActivityRegistered(frame.get(), params);
  [delegate verify];
}

// Tests CWVAutofillController delegate blur callback is invoked.
TEST_F(CWVAutofillControllerTest, BlurCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
           didBlurOnFieldWithIdentifier:kTestFieldIdentifier
                              fieldType:@""
                               formName:kTestFormName
                                frameID:frame_id_
                                  value:kTestFieldValue
                          userInitiated:YES];

  autofill::FormActivityParams params;
  params.form_name = base::SysNSStringToUTF8(kTestFormName);
  params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
  params.value = base::SysNSStringToUTF8(kTestFieldValue);
  params.frame_id = web::kMainFakeFrameId;
  params.type = "blur";
  params.has_user_gesture = true;
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  form_activity_tab_helper_->FormActivityRegistered(frame.get(), params);

  [delegate verify];
}

// Tests CWVAutofillController delegate submit callback is invoked.
TEST_F(CWVAutofillControllerTest, SubmitCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
                  didSubmitFormWithName:kTestFormName
                                frameID:frame_id_
                          userInitiated:YES];
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  autofill::FormData test_form_data;
  test_form_data.set_name(base::SysNSStringToUTF16(kTestFormName));

  form_activity_tab_helper_->DocumentSubmitted(
      /*sender_frame*/ frame.get(), /*form_data=*/test_form_data,
      /*user_initiated=*/true);

  [[delegate expect] autofillController:autofill_controller_
                  didSubmitFormWithName:kTestFormName
                                frameID:frame_id_
                          userInitiated:NO];

  form_activity_tab_helper_->DocumentSubmitted(
      /*sender_frame*/ frame.get(),
      /*form_data=*/test_form_data,
      /*user_initiated=*/false);

  [delegate verify];
}

// Tests that CWVAutofillController notifies user of password leaks.
TEST_F(CWVAutofillControllerTest, NotifyUserOfLeak) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  GURL leak_url("https://www.chromium.org");
  password_manager::CredentialLeakType leak_type =
      password_manager::CreateLeakType(password_manager::IsSaved(true),
                                       password_manager::IsReused(true),
                                       password_manager::IsSyncing(true));
  CWVPasswordLeakType expected_leak_type = CWVPasswordLeakTypeSaved |
                                           CWVPasswordLeakTypeUsedOnOtherSites |
                                           CWVPasswordLeakTypeSynced;
  OCMExpect([delegate autofillController:autofill_controller_
           notifyUserOfPasswordLeakOnURL:net::NSURLWithGURL(leak_url)
                                leakType:expected_leak_type]);
  OCMExpect([delegate autofillController:autofill_controller_
           notifyUserOfPasswordLeakOnURL:net::NSURLWithGURL(leak_url)
                                leakType:expected_leak_type
                                username:@"fake-username"]);

  password_manager_client_->NotifyUserCredentialsWereLeaked(
      leak_type, leak_url, base::SysNSStringToUTF16(@"fake-username"),
      /* in_account_store = */ false);

  [delegate verify];
}

// Tests that CWVAutofillController suggests passwords to its delegate.
TEST_F(CWVAutofillControllerTest, SuggestPasswordCallback) {
  NSString* fake_generated_password = @"12345";
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;
  OCMExpect([delegate autofillController:autofill_controller_
                suggestGeneratedPassword:fake_generated_password
                         decisionHandler:[OCMArg checkWithBlock:^(void (
                                             ^decisionHandler)(BOOL)) {
                           decisionHandler(/*accept=*/YES);
                           return YES;
                         }]]);
  __block BOOL decision_handler_called = NO;
  [autofill_controller_ sharedPasswordController:password_controller_
                  showGeneratedPotentialPassword:fake_generated_password
                                       proactive:NO
                                 decisionHandler:^(BOOL accept) {
                                   decision_handler_called = YES;
                                   EXPECT_TRUE(accept);
                                 }];
  EXPECT_TRUE(decision_handler_called);

  [delegate verify];
}

// Tests that CWVAutofillController automatically saves new profiles if the
// delegate method is not implemented.
TEST_F(CWVAutofillControllerTest, AutoSaveNewAutofillProfile) {
  auto new_profile = autofill::test::GetFullProfile();
  __block BOOL decision_handler_called = NO;
  auto callback = base::BindOnce(
      ^(autofill::AutofillClient::AddressPromptUserDecision decision,
        base::optional_ref<const autofill::AutofillProfile> profile) {
        EXPECT_EQ(
            autofill::AutofillClient::AddressPromptUserDecision::kUserNotAsked,
            decision);
        EXPECT_EQ(new_profile, profile.value());
        decision_handler_called = YES;
      });
  [autofill_controller_ confirmSaveAddressProfile:new_profile
                                  originalProfile:nil
                                         callback:std::move(callback)];
  EXPECT_TRUE(decision_handler_called);
}

// Tests that CWVAutofillController's delegate can save a new profile.
TEST_F(CWVAutofillControllerTest, SaveNewAutofillProfile) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  auto new_profile = autofill::test::GetFullProfile();
  OCMExpect([delegate autofillController:autofill_controller_
        confirmSaveForNewAutofillProfile:[OCMArg
                                             checkWithBlock:^(
                                                 CWVAutofillProfile* profile) {
                                               return new_profile ==
                                                      *profile.internalProfile;
                                             }]
                              oldProfile:nil
                         decisionHandler:[OCMArg checkWithBlock:^(void (
                                             ^decisionHandler)(
                                             CWVAutofillProfileUserDecision)) {
                           decisionHandler(
                               CWVAutofillProfileUserDecisionAccepted);
                           return YES;
                         }]]);
  __block BOOL decision_handler_called = NO;
  auto callback = base::BindOnce(^(
      autofill::AutofillClient::AddressPromptUserDecision decision,
      base::optional_ref<const autofill::AutofillProfile> profile) {
    EXPECT_EQ(autofill::AutofillClient::AddressPromptUserDecision::kAccepted,
              decision);
    EXPECT_EQ(new_profile, profile.value());
    decision_handler_called = YES;
  });
  [autofill_controller_ confirmSaveAddressProfile:new_profile
                                  originalProfile:nil
                                         callback:std::move(callback)];
  EXPECT_TRUE(decision_handler_called);

  [delegate verify];
}

}  // namespace ios_web_view
