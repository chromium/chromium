// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>

#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#import "components/autofill/core/browser/payments/otp_unmask_result.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/single_field_fillers/autocomplete/mock_autocomplete_history_manager.h"
#import "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#import "components/autofill/core/browser/test_utils/autofill_test_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/fake_autofill_agent.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider_query.h"
#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_activity_tab_helper.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_string.h"
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
#import "ios/web/public/test/js_test_util.h"
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
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;
using ActivityType = autofill::FormActivityParams::ActivityType;
using FieldType = autofill::FormActivityParams::FieldType;
using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Records the names of the form activity notifications it receives, so that
// tests can assert on their absence and see exactly which ones fired.
@interface FakeFormActivityDelegate : NSObject <CWVAutofillControllerDelegate>

@property(nonatomic, readonly) NSArray<NSString*>* receivedNotifications;

@end

@implementation FakeFormActivityDelegate {
  NSMutableArray<NSString*>* _receivedNotifications;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _receivedNotifications = [NSMutableArray array];
  }
  return self;
}

- (NSArray<NSString*>*)receivedNotifications {
  return [_receivedNotifications copy];
}

#pragma mark - CWVAutofillControllerDelegate

- (void)autofillController:(CWVAutofillController*)autofillController
    didFocusOnFieldWithIdentifier:(NSString*)fieldIdentifier
                        fieldType:(NSInteger)fieldType
                         formName:(NSString*)formName
                          frameID:(NSString*)frameID
                            value:(NSString*)value
                    userInitiated:(BOOL)userInitiated {
  [_receivedNotifications addObject:@"didFocusOnField"];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    didInputInFieldWithIdentifier:(NSString*)fieldIdentifier
                        fieldType:(NSInteger)fieldType
                         formName:(NSString*)formName
                          frameID:(NSString*)frameID
                            value:(NSString*)value
                    userInitiated:(BOOL)userInitiated {
  [_receivedNotifications addObject:@"didInputInField"];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    didBlurOnFieldWithIdentifier:(NSString*)fieldIdentifier
                       fieldType:(NSInteger)fieldType
                        formName:(NSString*)formName
                         frameID:(NSString*)frameID
                           value:(NSString*)value
                   userInitiated:(BOOL)userInitiated {
  [_receivedNotifications addObject:@"didBlurOnField"];
}

@end

namespace ios_web_view {
namespace {

constexpr std::string_view kTestURL = "https://example.com/";
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

// A `FakeWebState` whose committed URL can be made untrusted. Plain
// `FakeWebState` always reports its URL as trusted, which leaves the untrusted
// branch of `-webState:didRegisterFormActivity:inFrame:` unreachable.
class FakeWebStateWithUntrustableURL : public web::FakeWebState {
 public:
  ~FakeWebStateWithUntrustableURL() override = default;

  void set_url_trusted(bool url_trusted) { url_trusted_ = url_trusted; }

  // web::WebState:
  std::optional<GURL> GetLastCommittedURLIfTrusted() const override {
    if (!url_trusted_) {
      return std::nullopt;
    }
    return web::FakeWebState::GetLastCommittedURLIfTrusted();
  }

 private:
  bool url_trusted_ = true;
};

class CWVAutofillControllerTestBase : public web::WebTest {
 protected:
  CWVAutofillControllerTestBase() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
    pref_service_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillProfileEnabled, true);
    ios_web_view::RegisterCWVAutofillPrefs(pref_service_.registry());
    ios_web_view::SetAutofillScopedFormActivityEnabled(&pref_service_, true);
    RegisterWebViewPasswordManagerPrefs(pref_service_.registry());

    web_state_.SetBrowserState(&browser_state_);
    web_state_.SetCurrentURL(GURL(kTestURL));

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

    password_controller_ = OCMClassMock([SharedPasswordController class]);
    form_activity_tab_helper_ =
        std::make_unique<autofill::TestFormActivityTabHelper>(&web_state_);
  }

  void SetUp() override {
    web::WebTest::SetUp();

    web::test::OverrideJavaScriptFeatures(
        &browser_state_,
        {autofill::AutofillJavaScriptFeature::GetInstance(),
         autofill::FormHandlersJavaScriptFeature::GetInstance(),
         autofill::SuggestionControllerJavaScriptFeature::GetInstance()});

    auto password_manager_client =
        std::make_unique<WebViewPasswordManagerClient>(
            &web_state_, /*sync_service=*/nullptr, &pref_service_,
            /*identity_manager=*/nullptr, /*log_router=*/nullptr,
            /*profile_store=*/nullptr, /*account_store=*/nullptr,
            /*reuse_manager=*/nullptr,
            /*requirements_service=*/nullptr);
    auto password_manager = std::make_unique<password_manager::PasswordManager>(
        password_manager_client.get());
    IOSPasswordManagerDriverFactory::CreateForWebState(
        &web_state_, password_controller_, password_manager.get());
    password_manager_client_ = password_manager_client.get();

    auto autofill_client = std::make_unique<
        autofill::WithFakedFromWebState<autofill::WebViewAutofillClientIOS>>(
        &pref_service_, &personal_data_manager_, &autocomplete_history_manager_,
        &web_state_, /*bridge=*/nil, /*identity_manager=*/nullptr,
        &strike_database_, &sync_service_, /*log_router=*/nullptr);
    // Drain the autoreleased NSInvocation created by OCMock when
    // `-initWithWebState:...` sets `password_controller_.delegate = self`, so
    // `autofill_controller_ = nil` in `TearDown()` actually deallocates the
    // controller before `web_state_` is destroyed.
    @autoreleasepool {
      autofill_controller_ = [[CWVAutofillController alloc]
               initWithWebState:&web_state_
                    prefService:&pref_service_
          autofillClientForTest:std::move(autofill_client)
                  autofillAgent:autofill_agent_
                passwordManager:std::move(password_manager)
          passwordManagerClient:std::move(password_manager_client)
             passwordController:password_controller_];
    }
  }

  void TearDown() override {
    password_manager_client_ = nullptr;
    [password_controller_ stopMocking];
    autofill_controller_ = nil;
    web::WebTest::TearDown();
  }

  void AddWebFrame(std::unique_ptr<web::FakeWebFrame> frame) {
    frame->set_browser_state(&browser_state_);
    web_frames_manager_->AddWebFrame(std::move(frame));
  }

  // Registers a focus event on `kTestFormName` / `kTestFieldIdentifier` in a
  // newly created main frame, and returns that frame so that callers can post
  // follow up activity to it.
  web::WebFrame* RegisterFormActivity(bool has_user_gesture) {
    auto frame =
        web::FakeWebFrame::Create(base::SysNSStringToUTF8(frame_id_),
                                  /*is_main_frame=*/true, GURL(kTestURL));
    web::WebFrame* frame_ptr = frame.get();
    AddWebFrame(std::move(frame));

    autofill::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
    params.type = ActivityType::kFocus;
    params.has_user_gesture = has_user_gesture;
    form_activity_tab_helper_->FormActivityRegistered(frame_ptr, params);
    return frame_ptr;
  }

  web::WebFrame* RegisterFormActivity() {
    return RegisterFormActivity(/*has_user_gesture=*/true);
  }

  // Stubs `password_controller_` to always answer "no suggestions available",
  // recording the query and gesture it was invoked with in
  // `last_suggestions_query_` / `last_suggestions_had_user_gesture_`.
  // `ignoringNonObjectArgs` makes the stub match regardless of the gesture
  // value, so that tests assert on the recorded gesture instead of the mock
  // silently failing to match (which would strand the fetch completion).
  void StubPasswordSuggestionsAvailability() {
    CWVAutofillControllerTestBase* test = this;
    OCMStub([password_controller_
                checkIfSuggestionsAvailableForForm:[OCMArg any]
                                    hasUserGesture:NO
                                          webState:&web_state_
                                 completionHandler:[OCMArg any]])
        .ignoringNonObjectArgs()
        .andDo(^(NSInvocation* invocation) {
          __unsafe_unretained FormSuggestionProviderQuery* query = nil;
          [invocation getArgument:&query atIndex:2];
          BOOL has_user_gesture = NO;
          [invocation getArgument:&has_user_gesture atIndex:3];
          __unsafe_unretained void (^suggestions_available)(BOOL) = nil;
          [invocation getArgument:&suggestions_available atIndex:5];

          test->last_suggestions_query_ = query;
          test->last_suggestions_had_user_gesture_ = has_user_gesture;
          suggestions_available(NO);
        });
  }

  // Fetches suggestions for `fieldIdentifier` on `kTestFormName` in the main
  // frame and waits for the fetch to complete.
  void FetchSuggestionsForField(NSString* fieldIdentifier) {
    base::test::TestFuture<NSArray<CWVAutofillSuggestion*>*> suggestions_future;

    [autofill_controller_
        fetchSuggestionsForFormWithName:kTestFormName
                        fieldIdentifier:fieldIdentifier
                              fieldType:(NSInteger)FieldType::kUnknown
                                frameID:frame_id_
                      completionHandler:base::CallbackToBlock(
                                            suggestions_future.GetCallback())];

    ASSERT_TRUE(suggestions_future.Wait());
  }

  // Returns activity params of `type` describing the default test field in
  // `frame`.
  autofill::FormActivityParams FormActivityParamsForType(ActivityType type,
                                                         web::WebFrame* frame) {
    autofill::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.frame_id = frame ? frame->GetFrameId() : std::string();
    params.has_user_gesture = true;
    params.type = type;
    return params;
  }

  // Returns activity params of `type` describing the default test field, as
  // reported by the main frame.
  autofill::FormActivityParams FormActivityParamsForType(ActivityType type) {
    autofill::FormActivityParams params =
        FormActivityParamsForType(type, /*frame=*/nullptr);
    params.frame_id = web::kMainFakeFrameId;
    return params;
  }

  // Registers activity of `type` on the default test field in `frame`.
  void RegisterFieldActivity(web::WebFrame* frame,
                             ActivityType type,
                             NSString* value,
                             bool has_user_gesture) {
    autofill::FormActivityParams params =
        FormActivityParamsForType(type, frame);
    params.value = base::SysNSStringToUTF8(value);
    params.has_user_gesture = has_user_gesture;
    form_activity_tab_helper_->FormActivityRegistered(frame, params);
  }

  void PrepareFormActivity() {
    RegisterFormActivity();

    OCMExpect([password_controller_
        checkIfSuggestionsAvailableForForm:[OCMArg any]
                            hasUserGesture:YES
                                  webState:&web_state_
                         completionHandler:[OCMArg checkWithBlock:^(void (
                                               ^suggestionsAvailable)(BOOL)) {
                           suggestionsAvailable(NO);
                           return YES;
                         }]]);

    base::test::TestFuture<NSArray<CWVAutofillSuggestion*>*> suggestions_future;

    base::OnceCallback<void(NSArray<CWVAutofillSuggestion*>*)> cpp_callback =
        suggestions_future.GetCallback();

    __block base::OnceCallback<void(NSArray<CWVAutofillSuggestion*>*)>*
        block_safe_callback = &cpp_callback;

    void (^completion_block)(NSArray<CWVAutofillSuggestion*>*) =
        ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
          if (*block_safe_callback) {
            std::move(*block_safe_callback).Run(suggestions);
          }
        };

    [autofill_controller_
        fetchSuggestionsForFormWithName:kTestFormName
                        fieldIdentifier:kTestFieldIdentifier
                              fieldType:(NSInteger)FieldType::kUnknown
                                frameID:frame_id_
                      completionHandler:completion_block];

    EXPECT_TRUE(suggestions_future.Wait());
  }

  TestingPrefServiceSimple pref_service_;
  web::FakeBrowserState browser_state_;
  autofill::TestPersonalDataManager personal_data_manager_;
  autofill::TestStrikeDatabase strike_database_;
  syncer::TestSyncService sync_service_;
  FakeWebStateWithUntrustableURL web_state_;
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
  base::test::ScopedFeatureList scoped_feature_list_;

  // Arguments recorded by `StubPasswordSuggestionsAvailability`.
  FormSuggestionProviderQuery* last_suggestions_query_;
  BOOL last_suggestions_had_user_gesture_ = NO;
};

class CWVAutofillControllerTest : public CWVAutofillControllerTestBase,
                                  public testing::WithParamInterface<bool> {
 protected:
  CWVAutofillControllerTest() {
    ios_web_view::SetAutofillScopedFormActivityEnabled(&pref_service_,
                                                       GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(All, CWVAutofillControllerTest, testing::Bool());

class CWVAutofillControllerScopedFormActivityTest
    : public CWVAutofillControllerTestBase {
 protected:
  CWVAutofillControllerScopedFormActivityTest() {
    ios_web_view::SetAutofillScopedFormActivityEnabled(&pref_service_, true);
  }
};

// Tests CWVAutofillController fetch suggestions for profiles.
TEST_P(CWVAutofillControllerTest, FetchProfileSuggestions) {
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

  RegisterFormActivity();

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
  [autofill_controller_
      fetchSuggestionsForFormWithName:kTestFormName
                      fieldIdentifier:kTestFieldIdentifier
                            fieldType:(NSInteger)FieldType::kUnknown
                              frameID:frame_id_
                    completionHandler:fetch_completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout,
                                          /*run_message_loop=*/true, ^bool {
                                            return fetch_completion_was_called;
                                          }));

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests CWVAutofillController fetch suggestions for passwords.
TEST_P(CWVAutofillControllerTest, FetchPasswordSuggestions) {
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:kTestFieldValue
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
  RegisterFormActivity();

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
  [autofill_controller_
      fetchSuggestionsForFormWithName:kTestFormName
                      fieldIdentifier:kTestFieldIdentifier
                            fieldType:(NSInteger)FieldType::kUnknown
                              frameID:frame_id_
                    completionHandler:fetch_completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout,
                                          /*run_message_loop=*/true, ^bool {
                                            return fetch_completion_was_called;
                                          }));

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests CWVAutofillController accepts suggestion.
TEST_P(CWVAutofillControllerTest, AcceptSuggestion) {
  FormSuggestion* form_suggestion = [FormSuggestion
      suggestionWithValue:kTestFieldValue
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
  CWVAutofillSuggestion* suggestion = [[CWVAutofillSuggestion alloc]
      initWithFormSuggestion:form_suggestion
                    formName:kTestFormName
              formRendererID:autofill::FormRendererId(1)
             fieldIdentifier:kTestFieldIdentifier
             fieldRendererID:autofill::FieldRendererId(2)
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

// Tests that accepting a suggestion generated for Form 1 / Field A
// after focus has rapidly shifted to Form 2 / Field B only fills Form 1 / Field
// A.
TEST_P(CWVAutofillControllerTest, AcceptSuggestionAfterFocusShift) {
  FormSuggestion* form_suggestion = [FormSuggestion
      suggestionWithValue:kTestFieldValue
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];

  NSString* frame_id_1 = frame_id_;
  [autofill_agent_ addSuggestion:form_suggestion
                     forFormName:kTestFormName
                 fieldIdentifier:kTestFieldIdentifier
                         frameID:frame_id_1];

  RegisterFormActivity();

  OCMExpect([password_controller_
      checkIfSuggestionsAvailableForForm:[OCMArg any]
                          hasUserGesture:YES
                                webState:&web_state_
                       completionHandler:[OCMArg checkWithBlock:^(void (
                                             ^suggestionsAvailable)(BOOL)) {
                         suggestionsAvailable(NO);
                         return YES;
                       }]]);

  base::test::TestFuture<NSArray<CWVAutofillSuggestion*>*> suggestions_future;
  base::OnceCallback<void(NSArray<CWVAutofillSuggestion*>*)> fetch_callback =
      suggestions_future.GetCallback();
  __block base::OnceCallback<void(NSArray<CWVAutofillSuggestion*>*)>*
      block_safe_fetch_callback = &fetch_callback;

  [autofill_controller_
      fetchSuggestionsForFormWithName:kTestFormName
                      fieldIdentifier:kTestFieldIdentifier
                            fieldType:(NSInteger)FieldType::kUnknown
                              frameID:frame_id_1
                    completionHandler:^(
                        NSArray<CWVAutofillSuggestion*>* suggestions) {
                      if (*block_safe_fetch_callback) {
                        std::move(*block_safe_fetch_callback).Run(suggestions);
                      }
                    }];

  NSArray<CWVAutofillSuggestion*>* fetched_suggestions =
      suggestions_future.Get();
  ASSERT_EQ(1U, fetched_suggestions.count);
  CWVAutofillSuggestion* suggestion_1 = fetched_suggestions.firstObject;

  NSString* const kTestFormName2 = @"FormName2";
  NSString* const kTestFieldIdentifier2 = @"FieldIdentifier2";
  NSString* const frame_id_2 = @"Frame2";

  auto frame_2 = web::FakeWebFrame::Create(base::SysNSStringToUTF8(frame_id_2),
                                           /*is_main_frame=*/false, GURL());
  web::WebFrame* frame_ptr_2 = frame_2.get();
  AddWebFrame(std::move(frame_2));

  autofill::FormActivityParams params_2;
  params_2.form_name = base::SysNSStringToUTF8(kTestFormName2);
  params_2.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier2);
  params_2.type = ActivityType::kFocus;
  params_2.has_user_gesture = true;
  form_activity_tab_helper_->FormActivityRegistered(frame_ptr_2, params_2);

  base::test::TestFuture<void> accept_future;
  base::OnceClosure accept_callback = accept_future.GetCallback();
  __block base::OnceClosure* block_safe_accept_callback = &accept_callback;

  [autofill_controller_ acceptSuggestion:suggestion_1
                                 atIndex:0
                       completionHandler:^{
                         if (*block_safe_accept_callback) {
                           std::move(*block_safe_accept_callback).Run();
                         }
                       }];

  EXPECT_TRUE(accept_future.Wait());

  EXPECT_NSEQ(
      form_suggestion,
      [autofill_agent_ selectedSuggestionForFormName:kTestFormName
                                     fieldIdentifier:kTestFieldIdentifier
                                             frameID:frame_id_1]);

  EXPECT_EQ(nil,
            [autofill_agent_ selectedSuggestionForFormName:kTestFormName2
                                           fieldIdentifier:kTestFieldIdentifier2
                                                   frameID:frame_id_2]);

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests CWVAutofillController accepts credit card as suggestion.
TEST_P(CWVAutofillControllerTest, AcceptCreditCardAsSuggestion) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  CWVCreditCard* cwv_credit_card =
      [[CWVCreditCard alloc] initWithCreditCard:credit_card];

  PrepareFormActivity();

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

  FormSuggestion* recorded_suggestion =
      [autofill_agent_ selectedSuggestionForFormName:kTestFormName
                                     fieldIdentifier:kTestFieldIdentifier
                                             frameID:frame_id_];
  EXPECT_TRUE(recorded_suggestion);
  EXPECT_EQ(recorded_suggestion.type,
            autofill::SuggestionType::kCreditCardEntry);
  EXPECT_OCMOCK_VERIFY(password_controller_);
}

TEST_P(CWVAutofillControllerTest, AcceptVirtualCreditCardAsSuggestion) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_record_type(autofill::CreditCard::RecordType::kVirtualCard);
  CWVCreditCard* cwv_credit_card =
      [[CWVCreditCard alloc] initWithCreditCard:credit_card];

  PrepareFormActivity();

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

  FormSuggestion* recorded_suggestion =
      [autofill_agent_ selectedSuggestionForFormName:kTestFormName
                                     fieldIdentifier:kTestFieldIdentifier
                                             frameID:frame_id_];
  EXPECT_TRUE(recorded_suggestion);
  EXPECT_EQ(recorded_suggestion.type,
            autofill::SuggestionType::kVirtualCreditCardEntry);
  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests CWVAutofillController delegate focus callback is invoked.
TEST_P(CWVAutofillControllerTest, FocusCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
          didFocusOnFieldWithIdentifier:kTestFieldIdentifier
                              fieldType:(NSInteger)FieldType::kUnknown
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
  params.type = ActivityType::kFocus;
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  form_activity_tab_helper_->FormActivityRegistered(frame.get(), params);
  [delegate verify];
}

// Tests CWVAutofillController delegate input callback is invoked.
TEST_P(CWVAutofillControllerTest, InputCallback) {
  web::WebFrame* frame = RegisterFormActivity();

  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
          didInputInFieldWithIdentifier:kTestFieldIdentifier
                              fieldType:(NSInteger)FieldType::kUnknown
                               formName:kTestFormName
                                frameID:frame_id_
                                  value:kTestFieldValue
                          userInitiated:YES];

  autofill::FormActivityParams params;
  params.form_name = base::SysNSStringToUTF8(kTestFormName);
  params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
  params.value = base::SysNSStringToUTF8(kTestFieldValue);
  params.frame_id = web::kMainFakeFrameId;
  params.type = ActivityType::kInput;
  params.has_user_gesture = true;
  form_activity_tab_helper_->FormActivityRegistered(frame, params);
  [delegate verify];
}

// Tests CWVAutofillController delegate input callback is invoked by keyup
// events.
TEST_P(CWVAutofillControllerTest, InputCallbackFromKeyup) {
  web::WebFrame* frame = RegisterFormActivity();

  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
          didInputInFieldWithIdentifier:kTestFieldIdentifier
                              fieldType:(NSInteger)FieldType::kUnknown
                               formName:kTestFormName
                                frameID:frame_id_
                                  value:kTestFieldValue
                          userInitiated:YES];

  autofill::FormActivityParams params;
  params.form_name = base::SysNSStringToUTF8(kTestFormName);
  params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
  params.value = base::SysNSStringToUTF8(kTestFieldValue);
  params.frame_id = web::kMainFakeFrameId;
  params.type = ActivityType::kKeyUp;
  params.has_user_gesture = true;
  form_activity_tab_helper_->FormActivityRegistered(frame, params);
  [delegate verify];
}

// Tests CWVAutofillController delegate blur callback is invoked.
TEST_P(CWVAutofillControllerTest, BlurCallback) {
  web::WebFrame* frame = RegisterFormActivity();

  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
           didBlurOnFieldWithIdentifier:kTestFieldIdentifier
                              fieldType:(NSInteger)FieldType::kUnknown
                               formName:kTestFormName
                                frameID:frame_id_
                                  value:kTestFieldValue
                          userInitiated:YES];

  autofill::FormActivityParams params;
  params.form_name = base::SysNSStringToUTF8(kTestFormName);
  params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
  params.value = base::SysNSStringToUTF8(kTestFieldValue);
  params.frame_id = web::kMainFakeFrameId;
  params.type = ActivityType::kBlur;
  params.has_user_gesture = true;
  form_activity_tab_helper_->FormActivityRegistered(frame, params);

  [delegate verify];
}
// Tests submission handling.
TEST_P(CWVAutofillControllerTest, SubmitCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  OCMExpect([delegate
         autofillController:autofill_controller_
      didSubmitFormWithName:kTestFormName
                    frameID:base::SysUTF8ToNSString(web::kMainFakeFrameId)
             perfectFilling:YES]);
  OCMExpect([delegate
         autofillController:autofill_controller_
      didSubmitFormWithName:kTestFormName
                    frameID:base::SysUTF8ToNSString(web::kMainFakeFrameId)
              userInitiated:YES
             perfectFilling:YES]);

  autofill::FormData test_form_data;
  test_form_data.set_name(base::SysNSStringToUTF16(kTestFormName));

  // Manually trigger the observer bridge to simulate the event from
  // AutofillManager.
  web::WebFrame* main_frame = web_frames_manager_->GetMainWebFrame();
  if (!main_frame) {
    auto owned_frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
    main_frame = owned_frame.get();
    AddWebFrame(std::move(owned_frame));
  }

  autofill::AutofillDriverIOS* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(&web_state_,
                                                           main_frame);
  ASSERT_TRUE(driver);
  autofill::AutofillManager& manager = driver->GetAutofillManager();

  // Simulate submission.
  manager.NotifyObservers(
      &autofill::AutofillManager::Observer::OnAfterFormSubmitted,
      test_form_data);

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that fetchFullCardDetailsForCard:completionHandler: returns an error
// when the web frame is missing.
TEST_P(CWVAutofillControllerTest, FetchFullCardDetailsNoFrame) {
  CWVCreditCard* card = [[CWVCreditCard alloc]
      initWithCreditCard:autofill::test::GetCreditCard()];
  __block BOOL completion_handler_called = NO;
  [autofill_controller_
      fetchFullCardDetailsForCard:card
                completionHandler:^(CWVCreditCard* fullCard, NSError* error) {
                  completion_handler_called = YES;
                  EXPECT_FALSE(fullCard);
                  EXPECT_NSEQ(CWVAutofillErrorDomain, error.domain);
                  EXPECT_EQ(CWVAutofillErrorNoWebFrame, error.code);
                }];
  EXPECT_TRUE(completion_handler_called);
}

// Tests that fetchFullCardDetailsForCard:completionHandler: returns an error
// when the autofill driver is missing.
TEST_P(CWVAutofillControllerTest, FetchFullCardDetailsNoDriver) {
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  std::string frame_id = frame->GetFrameId();
  web::WebFrame* frame_ptr = frame.get();
  AddWebFrame(std::move(frame));

  // Simulate form activity to populate `_focusedField`.
  autofill::FormActivityParams params;
  params.frame_id = frame_id;
  params.type = ActivityType::kFocus;
  form_activity_tab_helper_->FormActivityRegistered(frame_ptr, params);

  // Simulate missing driver by notifying the factory that the WebState is being
  // destroyed. This will cause the factory to return nullptr for any subsequent
  // DriverForFrame() calls.
  static_cast<web::WebStateObserver&>(
      autofill_controller_.autofillClient->GetAutofillDriverFactory())
      .WebStateDestroyed(&web_state_);

  CWVCreditCard* card = [[CWVCreditCard alloc]
      initWithCreditCard:autofill::test::GetCreditCard()];
  __block BOOL completion_handler_called = NO;
  [autofill_controller_
      fetchFullCardDetailsForCard:card
                completionHandler:^(CWVCreditCard* fullCard, NSError* error) {
                  completion_handler_called = YES;
                  EXPECT_FALSE(fullCard);
                  EXPECT_NSEQ(CWVAutofillErrorDomain, error.domain);
                  EXPECT_EQ(CWVAutofillErrorNoAutofillDriver, error.code);
                }];
  EXPECT_TRUE(completion_handler_called);
}

// Tests that fetchFullCardDetailsForCard:completionHandler: returns a full
// card.
TEST_P(CWVAutofillControllerTest, FetchFullCardDetails) {
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  std::string frame_id = frame->GetFrameId();
  AddWebFrame(std::move(frame));

  // Simulate form activity to populate `_focusedField`.
  autofill::FormActivityParams params;
  params.frame_id = frame_id;
  params.type = ActivityType::kFocus;
  web::WebFrame* main_frame = web_frames_manager_->GetMainWebFrame();
  form_activity_tab_helper_->FormActivityRegistered(main_frame, params);

  CWVCreditCard* card = [[CWVCreditCard alloc]
      initWithCreditCard:autofill::test::GetCreditCard()];
  __block BOOL completion_handler_called = NO;
  [autofill_controller_
      fetchFullCardDetailsForCard:card
                completionHandler:^(CWVCreditCard* fullCard, NSError* error) {
                  completion_handler_called = YES;
                  EXPECT_TRUE(fullCard);
                }];
  EXPECT_TRUE(completion_handler_called);
}

// Tests that CWVAutofillController notifies user of password leaks.
TEST_P(CWVAutofillControllerTest, NotifyUserOfLeak) {
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
  password_form.password_value = password_manager::PasswordString(u"password");
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
TEST_P(CWVAutofillControllerTest, SuggestPasswordCallback) {
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
TEST_P(CWVAutofillControllerTest, AutoSaveNewAutofillProfile) {
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
TEST_P(CWVAutofillControllerTest, SaveNewAutofillProfile) {
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
TEST_P(CWVAutofillControllerTest, ShowUnmaskAuthenticatorSelectorWithOptions) {
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
TEST_P(CWVAutofillControllerTest, LoadRiskDataViaDelegate) {
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
TEST_P(CWVAutofillControllerTest, VirtualCardEnrollmentAccepted) {
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
TEST_P(CWVAutofillControllerTest, VirtualCardEnrollmentDeclined) {
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
TEST_P(CWVAutofillControllerTest,
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
TEST_P(CWVAutofillControllerTest, ShowCardUnmaskOtpInputDialog) {
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
TEST_P(CWVAutofillControllerTest, DidReceiveUnmaskOtpVerificationResult) {
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

TEST_P(CWVAutofillControllerTest, WebStateDestroyedDuringFetch) {
  RegisterFormActivity();

  __block void (^suggestionsAvailable)(BOOL) = nil;
  OCMExpect([password_controller_
      checkIfSuggestionsAvailableForForm:[OCMArg any]
                          hasUserGesture:YES
                                webState:&web_state_
                       completionHandler:[OCMArg checkWithBlock:^(
                                                     void (^callback)(BOOL)) {
                         suggestionsAvailable = [callback copy];
                         return YES;
                       }]]);

  __block BOOL fetch_completion_was_called = NO;
  id fetch_completion = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    fetch_completion_was_called = YES;
  };
  [autofill_controller_
      fetchSuggestionsForFormWithName:kTestFormName
                      fieldIdentifier:kTestFieldIdentifier
                            fieldType:(NSInteger)FieldType::kUnknown
                              frameID:frame_id_
                    completionHandler:fetch_completion];

  // Verify that suggestionsAvailable was captured.
  ASSERT_TRUE(suggestionsAvailable);

  // Destroy the web state.
  [autofill_controller_ webStateDestroyed:&web_state_];

  // Now trigger the callback.
  suggestionsAvailable(YES);

  // Verify that fetch_completion was called (with empty results) and NO CRASH
  // occurred.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout,
                                          /*run_message_loop=*/true, ^bool {
                                            return fetch_completion_was_called;
                                          }));

  [password_controller_ verify];
}

// Tests that accepting a credit card suggestion when no field has been focused
// is a safe no-op.
TEST_P(CWVAutofillControllerTest,
       AcceptCreditCardAsSuggestionWithoutFocusedFieldIsNoOp) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  CWVCreditCard* cwv_credit_card =
      [[CWVCreditCard alloc] initWithCreditCard:credit_card];

  __block BOOL accept_completion_was_called = NO;
  [autofill_controller_ acceptCreditCardAsSuggestion:cwv_credit_card
                                             atIndex:0
                                   completionHandler:^{
                                     accept_completion_was_called = YES;
                                   }];

  EXPECT_FALSE(accept_completion_was_called);
  EXPECT_FALSE([autofill_agent_
      selectedSuggestionForFormName:kTestFormName
                    fieldIdentifier:kTestFieldIdentifier
                            frameID:frame_id_]);
}

// Tests that public methods that rely on `_focusedField` or `_webState`
// safely no-op after `-webStateDestroyed:` under both legacy and scoped modes.
TEST_P(CWVAutofillControllerTest, MethodsNoOpAfterWebStateDestroyed) {
  web::FakeWebFrame* frame_ptr =
      static_cast<web::FakeWebFrame*>(RegisterFormActivity());

  [autofill_controller_ webStateDestroyed:&web_state_];

  frame_ptr->ClearJavaScriptCallHistory();
  [autofill_controller_ focusNextField];
  [autofill_controller_ focusPreviousField];
  EXPECT_TRUE(frame_ptr->GetLastJavaScriptCall().empty());

  __block NSArray<CWVAutofillSuggestion*>* fetched_suggestions = nil;
  [autofill_controller_
      fetchSuggestionsForFormWithName:kTestFormName
                      fieldIdentifier:kTestFieldIdentifier
                            fieldType:(NSInteger)FieldType::kUnknown
                              frameID:frame_id_
                    completionHandler:^(
                        NSArray<CWVAutofillSuggestion*>* suggestions) {
                      fetched_suggestions = suggestions;
                    }];
  EXPECT_NSEQ(@[], fetched_suggestions);

  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  CWVCreditCard* cwv_credit_card =
      [[CWVCreditCard alloc] initWithCreditCard:credit_card];
  __block BOOL accept_completion_was_called = NO;
  [autofill_controller_ acceptCreditCardAsSuggestion:cwv_credit_card
                                             atIndex:0
                                   completionHandler:^{
                                     accept_completion_was_called = YES;
                                   }];
  EXPECT_FALSE(accept_completion_was_called);
}

// Tests that suggestions fetched for a field focused with a user gesture are
// reported as user initiated.
TEST_P(CWVAutofillControllerTest, FetchSuggestionsAfterFocusWithGesture) {
  RegisterFormActivity(/*has_user_gesture=*/true);

  StubPasswordSuggestionsAvailability();

  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_TRUE(last_suggestions_had_user_gesture_);
}

// Tests that suggestions fetched for a field focused without a user gesture
// (e.g. a field the page autofocused) are reported as not user initiated.
TEST_P(CWVAutofillControllerTest, FetchSuggestionsAfterGesturelessFocus) {
  RegisterFormActivity(/*has_user_gesture=*/false);

  StubPasswordSuggestionsAvailability();

  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_FALSE(last_suggestions_had_user_gesture_);
}

// Tests that really typing into a field that was focused by a script dispatched
// event promotes the reported gesture, since the keystroke itself is a trusted
// user interaction with the very field the query is about.
TEST_P(CWVAutofillControllerTest, InputPromotesGestureOnFocusedField) {
  NSString* const kTypedValue = @"TypedValue";

  web::WebFrame* frame = RegisterFormActivity(/*has_user_gesture=*/false);

  RegisterFieldActivity(frame, ActivityType::kInput, kTypedValue,
                        /*has_user_gesture=*/true);

  StubPasswordSuggestionsAvailability();

  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_NSEQ(kTypedValue, last_suggestions_query_.typedValue);
  EXPECT_EQ(ActivityType::kInput, last_suggestions_query_.type);
  EXPECT_TRUE(last_suggestions_had_user_gesture_);
}

// Tests that a keyup promotes the gesture too, since some fields only emit
// keyup and never input.
TEST_P(CWVAutofillControllerTest, KeyUpPromotesGestureOnFocusedField) {
  NSString* const kTypedValue = @"TypedValue";

  web::WebFrame* frame = RegisterFormActivity(/*has_user_gesture=*/false);

  RegisterFieldActivity(frame, ActivityType::kKeyUp, kTypedValue,
                        /*has_user_gesture=*/true);

  StubPasswordSuggestionsAvailability();

  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_TRUE(last_suggestions_had_user_gesture_);
}

// Tests that a script synthesized edit demotes the gesture established by a
// real focus, so that the reported gesture always describes the most recent
// interaction with the focused field.
TEST_P(CWVAutofillControllerTest, UntrustedInputDemotesGestureOnFocusedField) {
  NSString* const kScriptedValue = @"ScriptedValue";

  web::WebFrame* frame = RegisterFormActivity(/*has_user_gesture=*/true);

  RegisterFieldActivity(frame, ActivityType::kInput, kScriptedValue,
                        /*has_user_gesture=*/false);

  StubPasswordSuggestionsAvailability();

  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_FALSE(last_suggestions_had_user_gesture_);
}

// The activity types that each map to a CWVAutofillControllerDelegate
// notification.
constexpr ActivityType kDelegateNotifyingActivityTypes[] = {
    ActivityType::kFocus, ActivityType::kInput, ActivityType::kKeyUp,
    ActivityType::kBlur};

// Tests that the recording delegate does observe notifications when the
// activity is well formed, so that the negative delegate tests below cannot
// pass vacuously.
TEST_P(CWVAutofillControllerTest, DelegateNotificationsForValidActivity) {
  FakeFormActivityDelegate* delegate = [[FakeFormActivityDelegate alloc] init];
  autofill_controller_.delegate = delegate;

  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));

  for (ActivityType type : kDelegateNotifyingActivityTypes) {
    SCOPED_TRACE(::testing::Message()
                 << "activity type " << static_cast<int>(type));
    form_activity_tab_helper_->FormActivityRegistered(
        frame.get(), FormActivityParamsForType(type, frame.get()));
  }

  EXPECT_NSEQ((@[
                @"didFocusOnField", @"didInputInField", @"didInputInField",
                @"didBlurOnField"
              ]),
              delegate.receivedNotifications);
}

// Tests that -focusNextField targets the main frame even after a gestureless
// form_changed event is registered in a child frame.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       FocusNextFieldAfterChildFrameFormChanged) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  AddWebFrame(std::move(main_frame));

  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(GURL(kTestURL));
  web::FakeWebFrame* child_frame_ptr = child_frame.get();
  AddWebFrame(std::move(child_frame));

  // Focus in the main frame.
  form_activity_tab_helper_->FormActivityRegistered(
      main_frame_ptr,
      FormActivityParamsForType(ActivityType::kFocus, main_frame_ptr));

  // Gestureless form_changed from a child frame.
  autofill::FormActivityParams form_changed_params =
      FormActivityParamsForType(ActivityType::kFormChanged, child_frame_ptr);
  form_changed_params.has_user_gesture = false;
  form_activity_tab_helper_->FormActivityRegistered(child_frame_ptr,
                                                    form_changed_params);

  // Clear JavaScript calls from frame setup/mutation tracking.
  main_frame_ptr->ClearJavaScriptCallHistory();
  child_frame_ptr->ClearJavaScriptCallHistory();

  // Focus next field should still target the main frame.
  [autofill_controller_ focusNextField];

  EXPECT_NE(main_frame_ptr->GetLastJavaScriptCall().find(u"selectNextElement"),
            std::u16string::npos);
  EXPECT_NE(main_frame_ptr->GetLastJavaScriptCall().find(u"suggestion"),
            std::u16string::npos);
  EXPECT_TRUE(child_frame_ptr->GetLastJavaScriptCall().empty());
}

// Tests that form activity with missing input neither creates state nor clears
// an already-focused field's state.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       FormActivityIgnoredWhenInputMissing) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  AddWebFrame(std::move(main_frame));

  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(GURL(kTestURL));
  web::FakeWebFrame* child_frame_ptr = child_frame.get();
  AddWebFrame(std::move(child_frame));

  // 1. When no field is focused yet, `input_missing` must not create state.
  autofill::FormActivityParams missing_focus =
      FormActivityParamsForType(ActivityType::kFocus, main_frame_ptr);
  missing_focus.input_missing = true;
  form_activity_tab_helper_->FormActivityRegistered(main_frame_ptr,
                                                    missing_focus);

  main_frame_ptr->ClearJavaScriptCallHistory();
  [autofill_controller_ focusNextField];
  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());

  // 2. Once a legitimate focus target is established, a subsequent
  // `input_missing` event (even from another frame) must be dropped without
  // clearing the cached focus target.
  form_activity_tab_helper_->FormActivityRegistered(
      main_frame_ptr,
      FormActivityParamsForType(ActivityType::kFocus, main_frame_ptr));

  autofill::FormActivityParams child_missing_params =
      FormActivityParamsForType(ActivityType::kFocus, child_frame_ptr);
  child_missing_params.input_missing = true;
  form_activity_tab_helper_->FormActivityRegistered(child_frame_ptr,
                                                    child_missing_params);

  main_frame_ptr->ClearJavaScriptCallHistory();
  child_frame_ptr->ClearJavaScriptCallHistory();
  [autofill_controller_ focusNextField];
  EXPECT_NE(main_frame_ptr->GetLastJavaScriptCall().find(u"selectNextElement"),
            std::u16string::npos);
  EXPECT_TRUE(child_frame_ptr->GetLastJavaScriptCall().empty());
}

// Tests that form activity resets tracked state when the page URL is not
// trusted.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       FormActivityResetsWhenURLNotTrusted) {
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* frame_ptr = frame.get();
  AddWebFrame(std::move(frame));

  // First establish valid focus.
  form_activity_tab_helper_->FormActivityRegistered(
      frame_ptr, FormActivityParamsForType(ActivityType::kFocus, frame_ptr));

  web_state_.set_url_trusted(false);

  form_activity_tab_helper_->FormActivityRegistered(
      frame_ptr, FormActivityParamsForType(ActivityType::kFocus, frame_ptr));

  frame_ptr->ClearJavaScriptCallHistory();

  // State should have been reset, so focusNextField does not invoke JS.
  [autofill_controller_ focusNextField];
  EXPECT_TRUE(frame_ptr->GetLastJavaScriptCall().empty());
}

// Tests that form activity resets tracked state when the page URL is empty or
// invalid.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       FormActivityResetsWhenURLEmpty) {
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* frame_ptr = frame.get();
  AddWebFrame(std::move(frame));

  // First establish valid focus.
  form_activity_tab_helper_->FormActivityRegistered(
      frame_ptr, FormActivityParamsForType(ActivityType::kFocus, frame_ptr));

  web_state_.SetCurrentURL(GURL());

  form_activity_tab_helper_->FormActivityRegistered(
      frame_ptr, FormActivityParamsForType(ActivityType::kFocus, frame_ptr));

  frame_ptr->ClearJavaScriptCallHistory();

  // State should have been reset, so focusNextField does not invoke JS.
  [autofill_controller_ focusNextField];
  EXPECT_TRUE(frame_ptr->GetLastJavaScriptCall().empty());
}

// Tests that form activity resets tracked state when the web frame is null.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       FormActivityResetsWhenFrameNull) {
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* frame_ptr = frame.get();
  AddWebFrame(std::move(frame));

  // First establish valid focus.
  autofill::FormActivityParams focus_params =
      FormActivityParamsForType(ActivityType::kFocus, frame_ptr);
  form_activity_tab_helper_->FormActivityRegistered(frame_ptr, focus_params);

  // Register form activity with null frame.
  [autofill_controller_ webState:&web_state_
         didRegisterFormActivity:FormActivityParamsForType(ActivityType::kFocus,
                                                           /*frame=*/nullptr)
                         inFrame:nullptr];

  frame_ptr->ClearJavaScriptCallHistory();

  // State should have been reset, so focusNextField does not invoke JS.
  [autofill_controller_ focusNextField];
  EXPECT_TRUE(frame_ptr->GetLastJavaScriptCall().empty());
}

// Tests that a suggestion query aimed at a field other than the focused one
// does not inherit the focused field's renderer IDs, typed value, activity type
// or user gesture.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       FetchSuggestionsForUnfocusedFieldDropsState) {
  constexpr FormRendererId kFocusedFormRendererID(11);
  constexpr FieldRendererId kFocusedFieldRendererID(22);
  NSString* const kOtherFieldIdentifier = @"OtherFieldIdentifier";

  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* frame_ptr = frame.get();
  AddWebFrame(std::move(frame));

  autofill::FormActivityParams focus_params =
      FormActivityParamsForType(ActivityType::kFocus, frame_ptr);
  focus_params.form_renderer_id = kFocusedFormRendererID;
  focus_params.field_renderer_id = kFocusedFieldRendererID;
  form_activity_tab_helper_->FormActivityRegistered(frame_ptr, focus_params);

  StubPasswordSuggestionsAvailability();

  // Query a different field than the one holding focus.
  FetchSuggestionsForField(kOtherFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_NSEQ(kOtherFieldIdentifier, last_suggestions_query_.fieldIdentifier);
  EXPECT_EQ(FormRendererId(), last_suggestions_query_.formRendererID);
  EXPECT_EQ(FieldRendererId(), last_suggestions_query_.fieldRendererID);
  EXPECT_EQ(ActivityType::kUnknown, last_suggestions_query_.type);
  EXPECT_NSEQ(nil, last_suggestions_query_.typedValue);
  EXPECT_FALSE(last_suggestions_had_user_gesture_);
}

// Tests that focusing field A, then focusing field B, and then querying field A
// drops state and does not inherit either field A's old state or field B's
// current state.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       FocusSecondFieldDropsFirstFieldStateOnQuery) {
  constexpr FormRendererId kFirstFormRendererID(11);
  constexpr FieldRendererId kFirstFieldRendererID(22);
  constexpr FormRendererId kSecondFormRendererID(33);
  constexpr FieldRendererId kSecondFieldRendererID(44);
  NSString* const kSecondFieldIdentifier = @"SecondFieldIdentifier";
  NSString* const kSecondFieldValue = @"SecondFieldValue";

  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* frame_ptr = frame.get();
  AddWebFrame(std::move(frame));

  autofill::FormActivityParams first_focus =
      FormActivityParamsForType(ActivityType::kFocus, frame_ptr);
  first_focus.form_renderer_id = kFirstFormRendererID;
  first_focus.field_renderer_id = kFirstFieldRendererID;
  form_activity_tab_helper_->FormActivityRegistered(frame_ptr, first_focus);

  autofill::FormActivityParams second_focus =
      FormActivityParamsForType(ActivityType::kFocus, frame_ptr);
  second_focus.field_identifier =
      base::SysNSStringToUTF8(kSecondFieldIdentifier);
  second_focus.value = base::SysNSStringToUTF8(kSecondFieldValue);
  second_focus.form_renderer_id = kSecondFormRendererID;
  second_focus.field_renderer_id = kSecondFieldRendererID;
  form_activity_tab_helper_->FormActivityRegistered(frame_ptr, second_focus);

  StubPasswordSuggestionsAvailability();

  // Query field A (`kTestFieldIdentifier`) after focus has moved to field B.
  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_NSEQ(kTestFieldIdentifier, last_suggestions_query_.fieldIdentifier);
  EXPECT_EQ(FormRendererId(), last_suggestions_query_.formRendererID);
  EXPECT_EQ(FieldRendererId(), last_suggestions_query_.fieldRendererID);
  EXPECT_EQ(ActivityType::kUnknown, last_suggestions_query_.type);
  EXPECT_NSEQ(nil, last_suggestions_query_.typedValue);
  EXPECT_FALSE(last_suggestions_had_user_gesture_);
}

// Tests that value-changing events (`kInput`, `kKeyUp`, `kChange`, `kBlur`)
// arriving when no field is focused (`_focusedField` is `std::nullopt`) do not
// synthesize a focused field record.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       ValueChangeWithoutFocusedFieldIsIgnored) {
  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* frame_ptr = frame.get();
  AddWebFrame(std::move(frame));

  for (ActivityType type : {ActivityType::kInput, ActivityType::kKeyUp,
                            ActivityType::kChange, ActivityType::kBlur}) {
    SCOPED_TRACE(::testing::Message()
                 << "activity type " << static_cast<int>(type));
    form_activity_tab_helper_->FormActivityRegistered(
        frame_ptr, FormActivityParamsForType(type, frame_ptr));
  }

  frame_ptr->ClearJavaScriptCallHistory();
  [autofill_controller_ focusNextField];
  EXPECT_TRUE(frame_ptr->GetLastJavaScriptCall().empty());

  StubPasswordSuggestionsAvailability();
  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_EQ(FormRendererId(), last_suggestions_query_.formRendererID);
  EXPECT_EQ(FieldRendererId(), last_suggestions_query_.fieldRendererID);
  EXPECT_EQ(ActivityType::kUnknown, last_suggestions_query_.type);
  EXPECT_NSEQ(nil, last_suggestions_query_.typedValue);
  EXPECT_FALSE(last_suggestions_had_user_gesture_);
}

// Tests that a field sharing the same `frameID` and `fieldIdentifier` as the
// focused field, but belonging to a different `formName`, is treated as a
// distinct field both when receiving activity and when fetching suggestions.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       SameFrameDifferentFormNameDoesNotMatchFocusedField) {
  constexpr FormRendererId kFocusedFormRendererID(11);
  constexpr FieldRendererId kFocusedFieldRendererID(22);
  NSString* const kOtherFormName = @"OtherFormName";
  NSString* const kOtherFormValue = @"OtherFormValue";

  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* frame_ptr = frame.get();
  AddWebFrame(std::move(frame));

  autofill::FormActivityParams focus_params =
      FormActivityParamsForType(ActivityType::kFocus, frame_ptr);
  focus_params.form_renderer_id = kFocusedFormRendererID;
  focus_params.field_renderer_id = kFocusedFieldRendererID;
  form_activity_tab_helper_->FormActivityRegistered(frame_ptr, focus_params);

  // Activity on a different form in the same frame with the same
  // `kTestFieldIdentifier` must not overwrite the focused field's value or
  // demote its gesture.
  autofill::FormActivityParams other_form_input =
      FormActivityParamsForType(ActivityType::kInput, frame_ptr);
  other_form_input.form_name = base::SysNSStringToUTF8(kOtherFormName);
  other_form_input.value = base::SysNSStringToUTF8(kOtherFormValue);
  other_form_input.has_user_gesture = false;
  form_activity_tab_helper_->FormActivityRegistered(frame_ptr,
                                                    other_form_input);

  StubPasswordSuggestionsAvailability();
  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_EQ(kFocusedFormRendererID, last_suggestions_query_.formRendererID);
  EXPECT_EQ(kFocusedFieldRendererID, last_suggestions_query_.fieldRendererID);
  EXPECT_NSEQ(kTestFieldValue, last_suggestions_query_.typedValue);
  EXPECT_EQ(ActivityType::kFocus, last_suggestions_query_.type);
  EXPECT_TRUE(last_suggestions_had_user_gesture_);

  // Fetching suggestions for `kOtherFormName` with the same `fieldIdentifier`
  // and `frameID` must not inherit `kTestFormName`'s state.
  last_suggestions_query_ = nil;
  base::test::TestFuture<NSArray<CWVAutofillSuggestion*>*> suggestions_future;
  [autofill_controller_
      fetchSuggestionsForFormWithName:kOtherFormName
                      fieldIdentifier:kTestFieldIdentifier
                            fieldType:(NSInteger)FieldType::kUnknown
                              frameID:frame_id_
                    completionHandler:base::CallbackToBlock(
                                          suggestions_future.GetCallback())];
  ASSERT_TRUE(suggestions_future.Wait());

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_NSEQ(kOtherFormName, last_suggestions_query_.formName);
  EXPECT_EQ(FormRendererId(), last_suggestions_query_.formRendererID);
  EXPECT_EQ(FieldRendererId(), last_suggestions_query_.fieldRendererID);
  EXPECT_EQ(ActivityType::kUnknown, last_suggestions_query_.type);
  EXPECT_NSEQ(nil, last_suggestions_query_.typedValue);
  EXPECT_FALSE(last_suggestions_had_user_gesture_);
}

// Tests that a value typed in a child frame is not attached to the suggestion
// query issued for the field focused in the main frame.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       ChildFrameValueNotUsedForFocusedField) {
  NSString* const kChildFrameFieldValue = @"ChildFrameFieldValue";

  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  AddWebFrame(std::move(main_frame));

  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(GURL(kTestURL));
  web::FakeWebFrame* child_frame_ptr = child_frame.get();
  AddWebFrame(std::move(child_frame));

  form_activity_tab_helper_->FormActivityRegistered(
      main_frame_ptr,
      FormActivityParamsForType(ActivityType::kFocus, main_frame_ptr));

  // The child frame reports untrusted input on a field that does not hold
  // focus.
  RegisterFieldActivity(child_frame_ptr, ActivityType::kInput,
                        kChildFrameFieldValue, /*has_user_gesture=*/false);

  StubPasswordSuggestionsAvailability();

  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_NSEQ(kTestFieldValue, last_suggestions_query_.typedValue);
  EXPECT_EQ(ActivityType::kFocus, last_suggestions_query_.type);
  EXPECT_TRUE(last_suggestions_had_user_gesture_);
}

// Tests that leaving a field does not promote the gesture: a blur is not an
// interaction with the field the suggestion query is about.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       BlurDoesNotPromoteGestureOnFocusedField) {
  NSString* const kTypedValue = @"TypedValue";

  web::WebFrame* frame = RegisterFormActivity(/*has_user_gesture=*/false);

  RegisterFieldActivity(frame, ActivityType::kBlur, kTypedValue,
                        /*has_user_gesture=*/true);

  StubPasswordSuggestionsAvailability();

  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  // The blur still refreshes the value it reports...
  EXPECT_NSEQ(kTypedValue, last_suggestions_query_.typedValue);
  EXPECT_EQ(ActivityType::kBlur, last_suggestions_query_.type);
  // ...but it must not make the interaction look user initiated.
  EXPECT_FALSE(last_suggestions_had_user_gesture_);
}

// Tests that a `kChange` event on the focused field refreshes the cached value
// and activity type without promoting or demoting `has_user_gesture` (since
// `change` on text inputs can fire when clicking elsewhere on the page to leave
// the field).
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       ChangeRefreshesValueWithoutPromotingGestureOnFocusedField) {
  NSString* const kChangedValue1 = @"ChangedValue1";
  NSString* const kChangedValue2 = @"ChangedValue2";

  web::WebFrame* frame = RegisterFormActivity(/*has_user_gesture=*/false);

  RegisterFieldActivity(frame, ActivityType::kChange, kChangedValue1,
                        /*has_user_gesture=*/true);

  StubPasswordSuggestionsAvailability();

  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_NSEQ(kChangedValue1, last_suggestions_query_.typedValue);
  EXPECT_EQ(ActivityType::kChange, last_suggestions_query_.type);
  EXPECT_FALSE(last_suggestions_had_user_gesture_);

  // When the field is focused with a user gesture, a subsequent `kChange`
  // refreshes the value while preserving `has_user_gesture = true`.
  form_activity_tab_helper_->FormActivityRegistered(
      frame, FormActivityParamsForType(ActivityType::kFocus, frame));
  RegisterFieldActivity(frame, ActivityType::kChange, kChangedValue2,
                        /*has_user_gesture=*/false);

  last_suggestions_query_ = nil;
  FetchSuggestionsForField(kTestFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_NSEQ(kChangedValue2, last_suggestions_query_.typedValue);
  EXPECT_EQ(ActivityType::kChange, last_suggestions_query_.type);
  EXPECT_TRUE(last_suggestions_had_user_gesture_);
}

// Tests that no delegate notification is sent for any activity type when the
// reported activity is missing its input data.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       NoDelegateNotificationsWhenInputMissing) {
  FakeFormActivityDelegate* delegate = [[FakeFormActivityDelegate alloc] init];
  autofill_controller_.delegate = delegate;

  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));

  for (ActivityType type : kDelegateNotifyingActivityTypes) {
    SCOPED_TRACE(::testing::Message()
                 << "activity type " << static_cast<int>(type));
    autofill::FormActivityParams params =
        FormActivityParamsForType(type, frame.get());
    params.input_missing = true;
    form_activity_tab_helper_->FormActivityRegistered(frame.get(), params);
  }

  EXPECT_NSEQ(@[], delegate.receivedNotifications);
}

// Tests that no delegate notification is sent for any activity type when the
// page URL cannot be trusted.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       NoDelegateNotificationsWhenURLNotTrusted) {
  FakeFormActivityDelegate* delegate = [[FakeFormActivityDelegate alloc] init];
  autofill_controller_.delegate = delegate;

  web_state_.set_url_trusted(false);

  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));

  for (ActivityType type : kDelegateNotifyingActivityTypes) {
    SCOPED_TRACE(::testing::Message()
                 << "activity type " << static_cast<int>(type));
    form_activity_tab_helper_->FormActivityRegistered(
        frame.get(), FormActivityParamsForType(type, frame.get()));
  }

  EXPECT_NSEQ(@[], delegate.receivedNotifications);
}

// Tests that no delegate notification is sent for any activity type when the
// activity cannot be attributed to a frame. This is an intentional change of
// the public delegate contract: such activity used to be reported with an empty
// frame ID.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       NoDelegateNotificationsWhenFrameNull) {
  FakeFormActivityDelegate* delegate = [[FakeFormActivityDelegate alloc] init];
  autofill_controller_.delegate = delegate;

  for (ActivityType type : kDelegateNotifyingActivityTypes) {
    SCOPED_TRACE(::testing::Message()
                 << "activity type " << static_cast<int>(type));
    [autofill_controller_ webState:&web_state_
           didRegisterFormActivity:FormActivityParamsForType(type,
                                                             /*frame=*/nullptr)
                           inFrame:nullptr];
  }

  EXPECT_NSEQ(@[], delegate.receivedNotifications);
}

// Tests that non-focus activity (`kInput`, `kKeyUp`, `kBlur`) reported for a
// field that does not currently hold focus does not trigger delegate
// notifications when scoped form activity is enabled.
TEST_F(CWVAutofillControllerScopedFormActivityTest,
       NoDelegateNotificationsForUnfocusedFieldActivity) {
  FakeFormActivityDelegate* delegate = [[FakeFormActivityDelegate alloc] init];
  autofill_controller_.delegate = delegate;

  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));

  for (ActivityType type :
       {ActivityType::kInput, ActivityType::kKeyUp, ActivityType::kBlur}) {
    SCOPED_TRACE(::testing::Message()
                 << "activity type " << static_cast<int>(type));
    form_activity_tab_helper_->FormActivityRegistered(
        frame.get(), FormActivityParamsForType(type, frame.get()));
  }

  EXPECT_NSEQ(@[], delegate.receivedNotifications);
}

class CWVAutofillControllerLegacyFormActivityTest
    : public CWVAutofillControllerTestBase {
 protected:
  CWVAutofillControllerLegacyFormActivityTest() {
    ios_web_view::SetAutofillScopedFormActivityEnabled(&pref_service_, false);
  }
};

// Tests that `RegisterCWVAutofillPrefs` defaults
// `kCWVAutofillScopedFormActivityEnabled` to `false` and that disabling the
// pref preserves the legacy unscoped form activity behavior.
TEST_F(CWVAutofillControllerLegacyFormActivityTest,
       FormActivityLegacyBehaviorWhenScopedFormActivityDisabled) {
  constexpr FormRendererId kInputFormRendererID(10);
  constexpr FieldRendererId kInputFieldRendererID(20);
  NSString* const kOtherFieldIdentifier = @"OtherFieldIdentifier";

  TestingPrefServiceSimple default_prefs;
  ios_web_view::RegisterCWVAutofillPrefs(default_prefs.registry());
  EXPECT_FALSE(
      ios_web_view::IsAutofillScopedFormActivityEnabled(&default_prefs));

  auto frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
  web::FakeWebFrame* frame_ptr = frame.get();
  AddWebFrame(std::move(frame));

  // Without `kFocus`, a `kInput` event still populates the cached renderer IDs
  // and `typed_value` when the kill switch is disabled.
  autofill::FormActivityParams input_params =
      FormActivityParamsForType(ActivityType::kInput, frame_ptr);
  input_params.form_renderer_id = kInputFormRendererID;
  input_params.field_renderer_id = kInputFieldRendererID;
  input_params.value = base::SysNSStringToUTF8(kTestFieldValue);
  input_params.has_user_gesture = true;
  form_activity_tab_helper_->FormActivityRegistered(frame_ptr, input_params);

  StubPasswordSuggestionsAvailability();
  FetchSuggestionsForField(kOtherFieldIdentifier);

  ASSERT_TRUE(last_suggestions_query_);
  EXPECT_EQ(kInputFormRendererID, last_suggestions_query_.formRendererID);
  EXPECT_EQ(kInputFieldRendererID, last_suggestions_query_.fieldRendererID);
  EXPECT_NSEQ(kTestFieldValue, last_suggestions_query_.typedValue);
  EXPECT_FALSE(last_suggestions_had_user_gesture_);
}

using CWVAutofillPrefsTest = PlatformTest;

// Tests that `MigrateObsoleteCWVAutofillPrefs` clears a persisted value of the
// deprecated `cwv.autofill.safe_lifecycle_enabled` pref.
TEST_F(CWVAutofillPrefsTest, MigrateObsoleteClearsSafeLifecyclePref) {
  constexpr char kObsoleteSafeLifecyclePref[] =
      "cwv.autofill.safe_lifecycle_enabled";
  TestingPrefServiceSimple prefs;
  ios_web_view::RegisterCWVAutofillPrefs(prefs.registry());
  prefs.SetBoolean(kObsoleteSafeLifecyclePref, true);
  ASSERT_TRUE(prefs.HasPrefPath(kObsoleteSafeLifecyclePref));

  ios_web_view::MigrateObsoleteCWVAutofillPrefs(&prefs);

  EXPECT_FALSE(prefs.HasPrefPath(kObsoleteSafeLifecyclePref));
}

}  // namespace
}  // namespace ios_web_view
