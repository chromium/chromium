// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>

#import "base/ios/block_types.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/mock_callback.h"
#import "base/test/test_future.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#import "components/autofill/core/browser/payments/otp_unmask_result.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/single_field_fillers/autocomplete/mock_autocomplete_history_manager.h"
#import "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/fake_autofill_agent.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
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
#import "ios/web_view/internal/autofill/cwv_autofill_controller+testing.h"
#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_prefs.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "ios/web_view/internal/autofill/cwv_card_unmask_challenge_option_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_otp_verifier_internal.h"
#import "ios/web_view/internal/autofill/cwv_vcn_enrollment_manager_internal.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;
using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {
namespace {

NSString* const kTestFormName = @"FormName";
FormRendererId kTestFormRendererID = FormRendererId(0);
NSString* const kTestFieldIdentifier = @"FieldIdentifier";
FieldRendererId kTestFieldRendererID = FieldRendererId(1);
NSString* const kTestFieldValue = @"FieldValue";
NSString* const kTestDisplayDescription = @"DisplayDescription";

class MockOtpUnmaskDelegate : public autofill::OtpUnmaskDelegate {
 public:
  MOCK_METHOD(void,
              OnUnmaskPromptAccepted,
              (const std::u16string& otp),
              (override));
  MOCK_METHOD(void,
              OnUnmaskPromptClosed,
              (bool user_closed_dialog),
              (override));
  MOCK_METHOD(void, OnNewOtpRequested, (), (override));

  base::WeakPtr<autofill::OtpUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockOtpUnmaskDelegate> weak_factory_{this};
};

class CWVAutofillControllerTest : public web::WebTest {
 protected:
  CWVAutofillControllerTest() {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
    pref_service_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillProfileEnabled, true);

    web_state_.SetBrowserState(&browser_state_);

    frame_id_ = base::SysUTF8ToNSString(web::kMainFakeFrameId);

    for (auto content_world : {web::ContentWorld::kIsolatedWorld,
                               web::ContentWorld::kPageContentWorld}) {
      auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
      web_state_.SetWebFramesManager(content_world, std::move(frames_manager));
    }

    web_frames_manager_ =
        static_cast<web::FakeWebFramesManager*>(web_state_.GetWebFramesManager(
            autofill::AutofillJavaScriptFeature::GetInstance()
                ->GetSupportedContentWorld()));

    autofill_agent_ =
        [[FakeAutofillAgent alloc] initWithPrefService:&pref_service_
                                              webState:&web_state_];

    auto password_manager_client =
        std::make_unique<WebViewPasswordManagerClient>(
            &web_state_, /*sync_service=*/nullptr, &pref_service_,
            /*identity_manager=*/nullptr, /*log_router=*/nullptr,
            /*profile_store=*/nullptr, /*account_store=*/nullptr,
            /*reuse_manager=*/nullptr,
            /*requirements_service=*/nullptr);
    auto password_manager = std::make_unique<password_manager::PasswordManager>(
        password_manager_client.get());
    password_controller_ = OCMClassMock([SharedPasswordController class]);
    IOSPasswordManagerDriverFactory::CreateForWebState(
        &web_state_, password_controller_, password_manager.get());
    password_manager_client_ = password_manager_client.get();

    auto autofill_client = std::make_unique<
        autofill::WithFakedFromWebState<autofill::WebViewAutofillClientIOS>>(
        &pref_service_, &personal_data_manager_, &autocomplete_history_manager_,
        &web_state_, /*bridge=*/nil, /*identity_manager=*/nullptr,
        &strike_database_, &sync_service_, /*log_router=*/nullptr);
    autofill_controller_ = [[CWVAutofillController alloc]
             initWithWebState:&web_state_
        autofillClientForTest:std::move(autofill_client)
                autofillAgent:autofill_agent_
              passwordManager:std::move(password_manager)
        passwordManagerClient:std::move(password_manager_client)
           passwordController:password_controller_];
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
  autofill::MockAutocompleteHistoryManager autocomplete_history_manager_;
  CWVAutofillController* autofill_controller_;
  FakeAutofillAgent* autofill_agent_;
  id password_controller_;
  std::unique_ptr<autofill::TestFormActivityTabHelper>
      form_activity_tab_helper_;
  WebViewPasswordManagerClient* password_manager_client_;
  CWVVCNEnrollmentManager* _retainedEnrollmentManager;
};

// Tests CWVAutofillController fetch suggestions for profiles.
TEST_F(CWVAutofillControllerTest, FetchProfileSuggestions) {
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:kTestFieldValue
       displayDescription:kTestDisplayDescription
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
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

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout,
                                          /*run_message_loop=*/true, ^bool {
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
                  payload:autofill::Suggestion::Payload()
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

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout,
                                          /*run_message_loop=*/true, ^bool {
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
                  payload:autofill::Suggestion::Payload()
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

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout,
                                          /*run_message_loop=*/true, ^bool {
                                            return accept_completion_was_called;
                                          }));
  EXPECT_NSEQ(
      form_suggestion,
      [autofill_agent_ selectedSuggestionForFormName:kTestFormName
                                     fieldIdentifier:kTestFieldIdentifier
                                             frameID:frame_id_]);
}

// Tests CWVAutofillController accepts credit card as suggestion.
TEST_F(CWVAutofillControllerTest, AcceptCreditCardAsSuggestion) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  CWVCreditCard* cwv_credit_card =
      [[CWVCreditCard alloc] initWithCreditCard:credit_card];

  __block BOOL accept_completion_was_called = NO;
  [autofill_controller_ acceptCreditCardAsSuggestion:cwv_credit_card
                                             atIndex:0
                                   completionHandler:^{
                                     accept_completion_was_called = YES;
                                   }];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout,
                                          /*run_message_loop=*/true, ^bool {
                                            return accept_completion_was_called;
                                          }));
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
                          userInitiated:YES
                         perfectFilling:YES];
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  autofill::FormData test_form_data;
  test_form_data.set_name(base::SysNSStringToUTF16(kTestFormName));

  form_activity_tab_helper_->DocumentSubmitted(
      /*sender_frame*/ frame.get(), /*form_data=*/test_form_data,
      /*user_initiated=*/true,
      /*perfect_filling=*/true);

  [[delegate expect] autofillController:autofill_controller_
                  didSubmitFormWithName:kTestFormName
                                frameID:frame_id_
                          userInitiated:NO
                         perfectFilling:NO];

  form_activity_tab_helper_->DocumentSubmitted(
      /*sender_frame*/ frame.get(),
      /*form_data=*/test_form_data,
      /*user_initiated=*/false,
      /*perfect_filling=*/false);

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

  password_manager::PasswordForm password_form;
  password_form.password_value = u"password";
  password_form.username_value = u"fake-username";
  password_form.url = leak_url;
  password_form.signon_realm = leak_url.GetWithEmptyPath().spec();
  password_manager_client_->NotifyUserCredentialsWereLeaked(
      password_manager::LeakedPasswordDetails(leak_type,
                                              std::move(password_form),
                                              /* in_account_store = */ false));

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
                                           frame:nullptr
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

// Tests that the delegate is correctly called for showing the unmask
// authenticator selector.
TEST_F(CWVAutofillControllerTest, ShowUnmaskAuthenticatorSelectorWithOptions) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  std::vector<autofill::CardUnmaskChallengeOption> challenge_options;

  autofill::CardUnmaskChallengeOption cvc_option;
  cvc_option.id =
      autofill::CardUnmaskChallengeOption::ChallengeOptionId("cvc_id");
  cvc_option.type = autofill::CardUnmaskChallengeOptionType::kCvc;
  cvc_option.challenge_info =
      u"Enter the 3-digit code on the back of your card";
  cvc_option.challenge_input_length = 3;
  cvc_option.cvc_position = autofill::CvcPosition::kBackOfCard;
  challenge_options.push_back(cvc_option);

  autofill::CardUnmaskChallengeOption sms_option;
  sms_option.id =
      autofill::CardUnmaskChallengeOption::ChallengeOptionId("sms_otp_id");
  sms_option.type = autofill::CardUnmaskChallengeOptionType::kSmsOtp;
  sms_option.challenge_info = u"Send OTP to ••••1234";
  sms_option.challenge_input_length = 6;
  challenge_options.push_back(sms_option);

  // Use MockOnceCallback for the C++ callbacks
  base::MockOnceCallback<void(const std::string&)> acceptCallback;
  base::MockOnceCallback<void()> cancelCallback;

  __block void (^capturedAcceptBlock)(NSString*);
  __block void (^capturedCancelBlock)(void);

  OCMExpect([delegate autofillController:autofill_controller_
      showUnmaskCreditCardAuthenticatorWithChallengeOptions:
          [OCMArg checkWithBlock:^BOOL(id obj) {
            NSArray<CWVCardUnmaskChallengeOption*>* objcOptions = obj;
            // Use EXPECT so the test continues and returns YES,
            // but records a failure if counts don't match.
            EXPECT_EQ(objcOptions.count, challenge_options.size());
            if (objcOptions.count != challenge_options.size()) {
              return YES;  // Return early if counts mismatch to avoid crash
            }

            for (size_t i = 0; i < challenge_options.size(); ++i) {
              const auto& cppOption = challenge_options[i];
              CWVCardUnmaskChallengeOption* objcOption = objcOptions[i];

              EXPECT_TRUE([base::SysUTF8ToNSString(cppOption.id.value())
                  isEqualToString:objcOption.identifier]);
              EXPECT_EQ(static_cast<int>(cppOption.type),
                        static_cast<int>(objcOption.type));
              EXPECT_TRUE([base::SysUTF16ToNSString(cppOption.challenge_info)
                  isEqualToString:objcOption.challengeLabel]);
              EXPECT_EQ(cppOption.challenge_input_length,
                        static_cast<size_t>(objcOption.challengeInputLength));
            }
            return YES;  // OCMock argument check block must return YES on
                         // success.
          }]
                                                acceptBlock:
                                                    [OCMArg
                                                        checkWithBlock:^BOOL(
                                                            id obj) {
                                                          capturedAcceptBlock =
                                                              [obj copy];
                                                          return YES;
                                                        }]
                                                cancelBlock:
                                                    [OCMArg
                                                        checkWithBlock:^BOOL(
                                                            id obj) {
                                                          capturedCancelBlock =
                                                              [obj copy];
                                                          return YES;
                                                        }]]);

  [autofill_controller_
      showUnmaskAuthenticatorSelectorWithOptions:challenge_options
                                  acceptCallback:acceptCallback.Get()
                                  cancelCallback:cancelCallback.Get()];

  [delegate verify];

  // Use GoogleTest assertions for checking block capture
  ASSERT_NE(capturedAcceptBlock, nullptr) << "Accept block was not captured";
  ASSERT_NE(capturedCancelBlock, nullptr) << "Cancel block was not captured";

  // Test invoking the captured accept block
  NSString* testOptionId = @"selected_option_id";
  // Expect the C++ acceptCallback to be Run with the correct string
  EXPECT_CALL(acceptCallback, Run(base::SysNSStringToUTF8(testOptionId)));
  if (capturedAcceptBlock) {
    capturedAcceptBlock(testOptionId);
  }

  // Test invoking the captured cancel block
  // Expect the C++ cancelCallback to be Run.
  EXPECT_CALL(cancelCallback, Run());
  if (capturedCancelBlock) {
    capturedCancelBlock();
  }
}

// Tests that the delegate is called to load risk data when no
// `CWVCreditCardVerifier` or `CWVCreditCardSaver` is present and the delegate
// responds to autofillControllerLoadRiskData:riskDataHandler.
TEST_F(CWVAutofillControllerTest, LoadRiskDataViaDelegate) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  const std::string kRiskData = "TestRiskDataString";

  base::test::TestFuture<std::string> risk_data_future;

  OCMExpect([delegate
      autofillControllerLoadRiskData:autofill_controller_
                     riskDataHandler:[OCMArg checkWithBlock:^BOOL(void (
                                         ^riskDataHandler)(NSString*)) {
                       riskDataHandler(base::SysUTF8ToNSString(kRiskData));
                       return YES;
                     }]]);

  [autofill_controller_
      loadRiskData:risk_data_future.GetCallback<const std::string&>()];

  const std::string actualRiskData = risk_data_future.Get();

  EXPECT_EQ(kRiskData, actualRiskData);

  [delegate verify];
}

// Tests that the delegate is called for VCN enrollment and handles acceptance.
TEST_F(CWVAutofillControllerTest, VirtualCardEnrollmentAccepted) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  autofill::VirtualCardEnrollmentFields enrollmentFields;
  enrollmentFields.credit_card = autofill::test::GetCreditCard();
  autofill::TestLegalMessageLine google_line("Google message");
  enrollmentFields.google_legal_message.push_back(google_line);
  autofill::TestLegalMessageLine issuer_line("Issuer message");
  enrollmentFields.issuer_legal_message.push_back(issuer_line);

  base::MockOnceCallback<void()> acceptCallback;
  base::MockOnceCallback<void()> declineCallback;

  OCMExpect([delegate autofillController:autofill_controller_
                enrollCreditCardWithVCNEnrollmentManager:
                    [OCMArg isKindOfClass:[CWVVCNEnrollmentManager class]]])
      .andDo(^(NSInvocation* invocation) {
        __unsafe_unretained CWVVCNEnrollmentManager* manager;
        [invocation getArgument:&manager atIndex:3];
        _retainedEnrollmentManager = manager;
      });

  [autofill_controller_
      showVirtualCardEnrollmentWithEnrollmentFields:enrollmentFields
                                     acceptCallback:acceptCallback.Get()
                                    declineCallback:declineCallback.Get()];

  [delegate verify];
  ASSERT_NE(_retainedEnrollmentManager, nil);

  EXPECT_CALL(acceptCallback, Run());
  EXPECT_CALL(declineCallback, Run()).Times(0);

  __block BOOL enrollment_completion_handler_called = NO;
  [_retainedEnrollmentManager enrollWithCompletionHandler:^(BOOL success) {
    EXPECT_TRUE(success);
    enrollment_completion_handler_called = YES;
  }];

  [autofill_controller_ handleVirtualCardEnrollmentResult:YES];

  EXPECT_TRUE(enrollment_completion_handler_called);
}

// Tests that the delegate is called for VCN enrollment and handles declination.
TEST_F(CWVAutofillControllerTest, VirtualCardEnrollmentDeclined) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  autofill::VirtualCardEnrollmentFields enrollmentFields;
  enrollmentFields.credit_card = autofill::test::GetCreditCard();

  base::MockOnceCallback<void()> acceptCallback;
  base::MockOnceCallback<void()> declineCallback;

  OCMExpect([delegate autofillController:autofill_controller_
                enrollCreditCardWithVCNEnrollmentManager:
                    [OCMArg isKindOfClass:[CWVVCNEnrollmentManager class]]])
      .andDo(^(NSInvocation* invocation) {
        __unsafe_unretained CWVVCNEnrollmentManager* manager;
        [invocation getArgument:&manager atIndex:3];
        _retainedEnrollmentManager = manager;
      });

  [autofill_controller_
      showVirtualCardEnrollmentWithEnrollmentFields:enrollmentFields
                                     acceptCallback:acceptCallback.Get()
                                    declineCallback:declineCallback.Get()];

  [delegate verify];
  ASSERT_NE(_retainedEnrollmentManager, nil);

  EXPECT_CALL(acceptCallback, Run()).Times(0);
  EXPECT_CALL(declineCallback, Run());

  [_retainedEnrollmentManager decline];
}

// Tests that the decline callback is invoked if the enrollment manager is
// deallocated before a decision is made.
TEST_F(CWVAutofillControllerTest,
       VirtualCardEnrollmentImplicitlyDeclinedOnDealloc) {
  autofill::VirtualCardEnrollmentFields enrollmentFields;
  enrollmentFields.credit_card = autofill::test::GetCreditCard();

  base::MockOnceCallback<void()> acceptCallback;
  base::MockOnceCallback<void()> declineCallback;

  EXPECT_CALL(acceptCallback, Run()).Times(0);
  EXPECT_CALL(declineCallback, Run());

  @autoreleasepool {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    __block CWVVCNEnrollmentManager* strongManager = nil;

    OCMExpect([delegate autofillController:autofill_controller_
                  enrollCreditCardWithVCNEnrollmentManager:
                      [OCMArg isKindOfClass:[CWVVCNEnrollmentManager class]]])
        .andDo(^(NSInvocation* invocation) {
          __unsafe_unretained CWVVCNEnrollmentManager* manager;
          [invocation getArgument:&manager atIndex:3];
          strongManager = manager;
        });

    [autofill_controller_
        showVirtualCardEnrollmentWithEnrollmentFields:enrollmentFields
                                       acceptCallback:acceptCallback.Get()
                                      declineCallback:declineCallback.Get()];

    [delegate verify];

    strongManager = nil;
  }
}

// Tests that the delegate is called to show the OTP input dialog.
TEST_F(CWVAutofillControllerTest, ShowCardUnmaskOtpInputDialog) {
  id mock_delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = mock_delegate;

  autofill::CardUnmaskChallengeOption option;
  option.id =
      autofill::CardUnmaskChallengeOption::ChallengeOptionId("test_otp_id");
  option.type = autofill::CardUnmaskChallengeOptionType::kSmsOtp;

  MockOtpUnmaskDelegate mock_otp_delegate;
  base::WeakPtr<autofill::OtpUnmaskDelegate> otp_delegate_ptr =
      mock_otp_delegate.GetWeakPtr();

  __block CWVCreditCardOTPVerifier* capturedVerifier = nil;
  OCMExpect([mock_delegate
                   autofillController:autofill_controller_
      verifyCreditCardWithOTPVerifier:[OCMArg checkWithBlock:^BOOL(id obj) {
        capturedVerifier = obj;
        return [obj isKindOfClass:[CWVCreditCardOTPVerifier class]];
      }]]);

  [autofill_controller_
      showCardUnmaskOtpInputDialogForCardType:autofill::CreditCard::RecordType::
                                                  kVirtualCard
                              challengeOption:option
                                     delegate:otp_delegate_ptr];

  [mock_delegate verify];
  ASSERT_NE(capturedVerifier, nullptr) << "CWVCreditCardOTPVerifier should be "
                                          "created and passed to the delegate";
}

// Tests that the verification result is passed to the CWVCreditCardOTPVerifier.
TEST_F(CWVAutofillControllerTest, DidReceiveUnmaskOtpVerificationResult) {
  id mock_delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = mock_delegate;

  autofill::CardUnmaskChallengeOption option;
  option.type = autofill::CardUnmaskChallengeOptionType::kSmsOtp;
  MockOtpUnmaskDelegate mock_otp_delegate;
  base::WeakPtr<autofill::OtpUnmaskDelegate> otp_delegate_ptr =
      mock_otp_delegate.GetWeakPtr();

  __block CWVCreditCardOTPVerifier* capturedVerifier = nil;
  OCMExpect([mock_delegate
                   autofillController:autofill_controller_
      verifyCreditCardWithOTPVerifier:[OCMArg checkWithBlock:^BOOL(id obj) {
        capturedVerifier = obj;
        return [obj isKindOfClass:[CWVCreditCardOTPVerifier class]];
      }]]);

  [autofill_controller_
      showCardUnmaskOtpInputDialogForCardType:autofill::CreditCard::RecordType::
                                                  kVirtualCard
                              challengeOption:option
                                     delegate:otp_delegate_ptr];
  [mock_delegate verify];
  ASSERT_NE(capturedVerifier, nullptr);

  id mockVerifierInstance = OCMPartialMock(capturedVerifier);

  @try {
    OCMExpect([mockVerifierInstance didReceiveUnmaskOtpVerificationResult:
                                        autofill::OtpUnmaskResult::kSuccess]);

    [autofill_controller_ didReceiveUnmaskOtpVerificationResult:
                              autofill::OtpUnmaskResult::kSuccess];

    [mockVerifierInstance verify];

    OCMExpect(
        [mockVerifierInstance didReceiveUnmaskOtpVerificationResult:
                                  autofill::OtpUnmaskResult::kOtpExpired]);

    [autofill_controller_ didReceiveUnmaskOtpVerificationResult:
                              autofill::OtpUnmaskResult::kOtpExpired];

    [(OCMockObject*)mockVerifierInstance verify];
  } @finally {
    [mockVerifierInstance stopMocking];
  }
}

}  // namespace
}  // namespace ios_web_view
