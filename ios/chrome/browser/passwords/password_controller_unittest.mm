// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <utility>

#import "base/ios/ios_util.h"
#import "base/json/json_reader.h"
#import "base/memory/ref_counted.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/values.h"
#import "components/autofill/core/browser/ui/popup_item_ids.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#import "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#import "components/password_manager/core/browser/mock_password_store_interface.h"
#import "components/password_manager/core/browser/password_form_manager.h"
#import "components/password_manager/core/browser/password_form_metrics_recorder.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_store_consumer.h"
#import "components/password_manager/core/browser/stub_password_manager_client.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/shared_password_controller+private.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/password_manager/ios/test_helpers.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/task_observer_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_context.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/OCMock/OCPartialMockObject.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormActivityParams;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormRendererId;
using autofill::FormRemovalParams;
using autofill::FieldRendererId;
using password_manager::PasswordForm;
using autofill::PasswordFormFillData;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;
using FillingAssistance =
    password_manager::PasswordFormMetricsRecorder::FillingAssistance;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordFormManager;
using password_manager::PasswordStoreConsumer;
using password_manager::prefs::kPasswordLeakDetectionEnabled;
using test_helpers::SetPasswordFormFillData;
using test_helpers::MakeSimpleFormData;
using testing::NiceMock;
using testing::Return;
using base::ASCIIToUTF16;
using base::SysUTF8ToNSString;
using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using testing::WithArg;
using testing::_;
using web::WebFrame;

namespace {

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext() = default;
  void IsHSTSActiveForHost(const std::string& host,
                           IsHSTSActiveForHostCallback callback) override {
    std::move(callback).Run(false);
  }
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  explicit MockPasswordManagerClient(
      password_manager::PasswordStoreInterface* store)
      : store_(store) {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(kPasswordLeakDetectionEnabled,
                                            true);
    safe_browsing::RegisterProfilePrefs(prefs_->registry());
  }

  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(IsIncognito, bool());
  MOCK_METHOD1(PromptUserToSaveOrUpdatePasswordPtr,
               void(PasswordFormManagerForUI*));
  MOCK_CONST_METHOD1(IsSavingAndFillingEnabled, bool(const GURL&));

  PrefService* GetPrefs() const override { return prefs_.get(); }

  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override {
    return store_;
  }

  network::mojom::NetworkContext* GetNetworkContext() const override {
    return &network_context_;
  }

  // Workaround for std::unique_ptr<> lacking a copy constructor.
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> manager,
      bool update_password) override {
    PromptUserToSaveOrUpdatePasswordPtr(manager.release());
    return false;
  }

 private:
  mutable FakeNetworkContext network_context_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  password_manager::PasswordStoreInterface* const store_;
};

ACTION_P(SaveToScopedPtr, scoped) {
  scoped->reset(arg0);
}

// Creates PasswordController with the given `web_state` and a mock client
// using the given `store`. If not null, `weak_client` is filled with a
// non-owning pointer to the created client. The created controller is
// returned.
PasswordController* CreatePasswordController(
    web::WebState* web_state,
    password_manager::PasswordStoreInterface* store,
    MockPasswordManagerClient** weak_client) {
  auto client = std::make_unique<NiceMock<MockPasswordManagerClient>>(store);
  if (weak_client)
    *weak_client = client.get();
  return [[PasswordController alloc] initWithWebState:web_state
                                               client:std::move(client)];
}

PasswordForm CreatePasswordForm(const char* origin_url,
                                const char* username_value,
                                const char* password_value) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.url = GURL(origin_url);
  form.signon_realm = origin_url;
  form.username_value = ASCIIToUTF16(username_value);
  form.password_value = ASCIIToUTF16(password_value);
  form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  return form;
}

// Invokes the password store consumer with a single copy of `form`, coming from
// `store`.
ACTION_P2(InvokeConsumer, store, form) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.push_back(std::make_unique<PasswordForm>(form));
  arg0->OnGetPasswordStoreResultsOrErrorFrom(store, std::move(result));
}

ACTION_P(InvokeEmptyConsumerWithForms, store) {
  arg0->OnGetPasswordStoreResultsOrErrorFrom(
      store, std::vector<std::unique_ptr<PasswordForm>>());
}

struct TestPasswordFormData {
  const char* form_name;
  const uint32_t form_renderer_id;
  const char* username_element;
  const uint32_t username_renderer_id;
  const char* password_element;
  const uint32_t password_renderer_id;
  const char* user_value;
  const char* password_value;
  // these values are used to check the expected result
  BOOL on_key_up;
  BOOL on_change;
};

}  // namespace

// Real FormSuggestionController is wrapped to register the addition of
// suggestions.
@interface PasswordsTestSuggestionController : FormSuggestionController

@property(nonatomic, copy) NSArray* suggestions;

@end

@implementation PasswordsTestSuggestionController

@synthesize suggestions = _suggestions;

- (void)updateKeyboardWithSuggestions:(NSArray*)suggestions {
  self.suggestions = suggestions;
}

@end

class PasswordControllerTest : public PlatformTest {
 public:
  PasswordControllerTest() : web_client_(std::make_unique<ChromeWebClient>()) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
  }

  ~PasswordControllerTest() override { store_->ShutdownOnUIThread(); }

  void SetUp() override {
    PlatformTest::SetUp();

    store_ =
        new testing::NiceMock<password_manager::MockPasswordStoreInterface>();
    ON_CALL(*store_, IsAbleToSavePasswords).WillByDefault(Return(true));

    // When waiting for predictions is on, it makes tests more complicated.
    // Disable wating, since most tests have nothing to do with predictions. All
    // tests that test working with prediction should explicitly turn
    // predictions on.
    PasswordFormManager::set_wait_for_server_predictions_for_filling(false);

    UniqueIDDataTabHelper::CreateForWebState(web_state());

    passwordController_ =
        CreatePasswordController(web_state(), store_.get(), &weak_client_);
    passwordController_.passwordManager->set_leak_factory(
        std::make_unique<
            NiceMock<password_manager::MockLeakDetectionCheckFactory>>());

    ON_CALL(*weak_client_, IsSavingAndFillingEnabled)
        .WillByDefault(Return(true));

    @autoreleasepool {
      // Make sure the temporary array is released after SetUp finishes,
      // otherwise [passwordController_ suggestionProvider] will be retained
      // until PlatformTest teardown, at which point all Chrome objects are
      // already gone and teardown may access invalid memory.
      suggestionController_ = [[PasswordsTestSuggestionController alloc]
          initWithWebState:web_state()
                 providers:@[ [passwordController_ suggestionProvider] ]];
      accessoryMediator_ =
          [[FormInputAccessoryMediator alloc] initWithConsumer:nil
                                                       handler:nil
                                                  webStateList:nullptr
                                           personalDataManager:nullptr
                                                 passwordStore:nullptr
                                          securityAlertHandler:nil
                                        reauthenticationModule:nil];
      [accessoryMediator_ injectWebState:web_state()];
      [accessoryMediator_ injectProvider:suggestionController_];
    }
  }

  bool SetUpUniqueIDs() {
    __block web::WebFrame* main_frame = nullptr;
    bool success =
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
          main_frame =
              web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame();
          return main_frame != nullptr;
        });
    if (!success) {
      return false;
    }
    DCHECK(main_frame);

    constexpr uint32_t next_available_id = 1;
    autofill::FormUtilJavaScriptFeature::GetInstance()
        ->SetUpForUniqueIDsWithInitialState(main_frame, next_available_id);

    // Wait for `SetUpForUniqueIDsWithInitialState` to complete.
    return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      return [web::test::ExecuteJavaScript(@"document[__gCrWeb.fill.ID_SYMBOL]",
                                           web_state()) intValue] ==
             int{next_available_id};
    });
  }

  void WaitForFormManagersCreation() {
    auto& form_managers = passwordController_.passwordManager->form_managers();
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return !form_managers.empty();
    }));
  }

  void SimulateFormActivityObserverSignal(std::string type,
                                          FormRendererId form_id,
                                          FieldRendererId field_id,
                                          std::string value) {
    std::string mainFrameID = web::GetMainWebFrameId(web_state());
    WebFrame* frame = web::GetWebFrameWithId(web_state(), mainFrameID);
    FormActivityParams params;
    params.type = type;
    params.unique_form_id = form_id;
    params.frame_id = mainFrameID;
    params.value = value;
    [passwordController_.sharedPasswordController webState:web_state()
                                   didRegisterFormActivity:params
                                                   inFrame:frame];
  }

  void SimulateFormRemovalObserverSignal(
      FormRendererId form_id,
      std::vector<FieldRendererId> field_ids) {
    std::string mainFrameID = web::GetMainWebFrameId(web_state());
    WebFrame* frame = web::GetWebFrameWithId(web_state(), mainFrameID);
    FormRemovalParams params;
    params.unique_form_id = form_id;
    params.removed_unowned_fields = field_ids;
    params.frame_id = mainFrameID;
    [passwordController_.sharedPasswordController webState:web_state()
                                    didRegisterFormRemoval:params
                                                   inFrame:frame];
  }

 protected:
  // Helper method for filling password forms and verifying filling success.
  // This method also checks whether the right suggestions are displayed.
  void FillFormAndValidate(TestPasswordFormData test_data,
                           BOOL should_succeed,
                           web::WebFrame* frame);

  // Retrieve the current suggestions from suggestionController_.
  NSArray* GetSuggestionValues() {
    NSMutableArray* suggestion_values = [NSMutableArray array];
    for (FormSuggestion* suggestion in [suggestionController_ suggestions])
      [suggestion_values addObject:suggestion.value];
    return [suggestion_values copy];
  }

  // Returns an identifier for the `form_number|th form in the page.
  std::string FormName(int form_number) {
    NSString* kFormNamingScript =
        @"__gCrWeb.form.getFormIdentifier("
         "    document.querySelectorAll('form')[%d]);";
    return base::SysNSStringToUTF8(web::test::ExecuteJavaScript(
        [NSString stringWithFormat:kFormNamingScript, form_number],
        web_state()));
  }

  void SimulateUserTyping(const std::string& form_name,
                          FormRendererId uniqueFormID,
                          const std::string& field_identifier,
                          FieldRendererId uniqueFieldID,
                          const std::string& typed_value,
                          const std::string& main_frame_id) {
    __block BOOL completion_handler_called = NO;
    FormSuggestionProviderQuery* form_query =
        [[FormSuggestionProviderQuery alloc]
            initWithFormName:SysUTF8ToNSString(form_name)
                uniqueFormID:uniqueFormID
             fieldIdentifier:SysUTF8ToNSString(field_identifier)
               uniqueFieldID:uniqueFieldID
                   fieldType:@"not_important"
                        type:@"input"
                  typedValue:SysUTF8ToNSString(typed_value)
                     frameID:SysUTF8ToNSString(main_frame_id)];
    [passwordController_.sharedPasswordController
        checkIfSuggestionsAvailableForForm:form_query
                            hasUserGesture:YES
                                  webState:web_state()
                         completionHandler:^(BOOL success) {
                           completion_handler_called = YES;
                         }];
    // Wait until the expected handler is called.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return completion_handler_called;
    }));
  }

  void LoadHtmlWithRendererInitiatedNavigation(NSString* html,
                                               GURL gurl = GURL()) {
    web::FakeNavigationContext context;
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    context.SetIsRendererInitiated(true);
    [passwordController_.sharedPasswordController webState:web_state()
                                       didFinishNavigation:&context];
    if (gurl.is_empty())
      LoadHtml(html);
    else
      LoadHtml(html, gurl);
  }

  void InjectGeneratedPassword(FormRendererId form_id,
                               FieldRendererId field_id,
                               NSString* password) {
    autofill::PasswordFormGenerationData generation_data = {form_id, field_id,
                                                            FieldRendererId()};
    [passwordController_.sharedPasswordController
        formEligibleForGenerationFound:generation_data];
    __block BOOL block_was_called = NO;
    [passwordController_.sharedPasswordController
        injectGeneratedPasswordForFormId:FormRendererId(1)
                                 inFrame:web::GetMainFrame(web_state())
                       generatedPassword:password
                       completionHandler:^() {
                         block_was_called = YES;
                       }];
    // Wait until the expected handler is called.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return block_was_called;
    }));
    ASSERT_TRUE(
        passwordController_.sharedPasswordController.isPasswordGenerated);
  }

  void LoadHtml(NSString* html) {
    web::test::LoadHtml(html, web_state());
    ASSERT_TRUE(SetUpUniqueIDs());
  }

  void LoadHtml(NSString* html, const GURL& url) {
    web::test::LoadHtml(html, url, web_state());
    ASSERT_TRUE(SetUpUniqueIDs());
  }

  [[nodiscard]] bool LoadHtml(const std::string& html) {
    web::test::LoadHtml(base::SysUTF8ToNSString(html), web_state());
    return SetUpUniqueIDs();
  }

  std::string BaseUrl() const {
    web::URLVerificationTrustLevel unused_level;
    return web_state()->GetCurrentURL(&unused_level).spec();
  }

  web::WebState* web_state() const { return web_state_.get(); }

  // This method only works if there are no more than 1 iframes per page.
  web::WebFrame* GetWebFrame(bool is_main_frame) {
    std::set<WebFrame*> all_frames =
        web_state()->GetPageWorldWebFramesManager()->GetAllWebFrames();
    for (auto* frame : all_frames) {
      if (is_main_frame == frame->IsMainFrame()) {
        return frame;
      }
    }
    return nullptr;
  }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;

  // SuggestionController for testing.
  PasswordsTestSuggestionController* suggestionController_;

  // FormInputAccessoryMediatorfor testing.
  FormInputAccessoryMediator* accessoryMediator_;

  // PasswordController for testing.
  PasswordController* passwordController_;

  scoped_refptr<password_manager::MockPasswordStoreInterface> store_;

  MockPasswordManagerClient* weak_client_;
};

struct FindPasswordFormTestData {
  NSString* html_string;
  const bool expected_form_found;
  // Expected number of fields in found form.
  const size_t expected_number_of_fields;
  // Expected form name.
  const char* expected_form_name;
  const uint32_t maxID;
};

// A script that we run after autofilling forms.  It returns
// all values for verification as a single concatenated string.
static NSString* kUsernamePasswordVerificationScript =
    @"var value = username_.value;"
     "var from = username_.selectionStart;"
     "var to = username_.selectionEnd;"
     "value.substr(0, from) + '[' + value.substr(from, to) + ']'"
     "   + value.substr(to, value.length) + '=' + password_.value"
     "   + ', onkeyup=' + onKeyUpCalled_"
     "   + ', onchange=' + onChangeCalled_;";

// A script that resets indicators used to verify that custom event
// handlers are triggered.  It also finds and the username and
// password fields and caches them for future verification.
static NSString* kUsernameAndPasswordTestPreparationScript =
    @"function findUsernameAndPasswordInFrame(win) {"
     "  username_ = win.document.getElementById(\"%@\");"
     "  password_ = win.document.getElementById(\"%@\");"
     "  if (username_ !== null) {"
     "      username_.__gCrWebAutofilled = 'false';"
     "      password_.__gCrWebAutofilled = 'false';"
     "      return;"
     "  }"
     "  var frames = win.frames;"
     "  for (var i = 0; i < frames.length; i++) {"
     "    findUsernameAndPasswordInFrame("
     "        frames[i]);"
     "  }"
     "};"
     "findUsernameAndPasswordInFrame(window);"
     "onKeyUpCalled_ = false;"
     "onChangeCalled_ = false;";

void PasswordControllerTest::FillFormAndValidate(TestPasswordFormData test_data,
                                                 BOOL should_succeed,
                                                 web::WebFrame* frame) {
  web::test::ExecuteJavaScript(
      [NSString stringWithFormat:kUsernameAndPasswordTestPreparationScript,
                                 SysUTF8ToNSString(test_data.username_element),
                                 SysUTF8ToNSString(test_data.password_element)],
      web_state());

  const std::string base_url = BaseUrl();

  PasswordFormFillData form_data;
  SetPasswordFormFillData(
      base_url, test_data.form_name, test_data.form_renderer_id,
      test_data.username_element, test_data.username_renderer_id, "user0",
      test_data.password_element, test_data.password_renderer_id, "password0",
      test_data.user_value, test_data.password_value, &form_data);

  [passwordController_.sharedPasswordController
      processPasswordFormFillData:form_data
                          inFrame:frame
                      isMainFrame:frame->IsMainFrame()
                forSecurityOrigin:frame->GetSecurityOrigin()];

  __block BOOL block_was_called = NO;

  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:SysUTF8ToNSString(test_data.form_name)
          uniqueFormID:FormRendererId(test_data.form_renderer_id)
       fieldIdentifier:SysUTF8ToNSString(test_data.username_element)
         uniqueFieldID:FieldRendererId(test_data.username_renderer_id)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:SysUTF8ToNSString(frame->GetFrameId())];

  NSString* suggestion_text = [NSString
      stringWithFormat:@"%@ ••••••••",
                       [NSString stringWithUTF8String:test_data.user_value]];

  [passwordController_.sharedPasswordController
      retrieveSuggestionsForForm:form_query
                        webState:web_state()
               completionHandler:^(NSArray* suggestions,
                                   id<FormSuggestionProvider> provider) {
                 NSMutableArray* suggestion_values = [NSMutableArray array];
                 for (FormSuggestion* suggestion in suggestions)
                   [suggestion_values addObject:suggestion.value];
                 EXPECT_NSEQ((@[
                               @"user0 ••••••••",
                               suggestion_text,
                             ]),
                             suggestion_values);
                 block_was_called = YES;
               }];
  EXPECT_TRUE(block_was_called);

  block_was_called = NO;

  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:suggestion_text
                       displayDescription:nil
                                     icon:nil
                               identifier:0
                           requiresReauth:NO];

  SuggestionHandledCompletion completion = ^{
    block_was_called = YES;

    NSString* expected_result = [NSString
        stringWithFormat:@"%@[]=%@, onkeyup=%@, onchange=%@",
                         [NSString stringWithUTF8String:test_data.user_value],
                         [NSString
                             stringWithUTF8String:test_data.password_value],
                         test_data.on_key_up ? @"true" : @"false",
                         test_data.on_change ? @"true" : @"false"];
    NSString* result = web::test::ExecuteJavaScript(
        kUsernamePasswordVerificationScript, web_state());

    if (should_succeed) {
      EXPECT_NSEQ(expected_result, result);
    } else {
      EXPECT_NSNE(expected_result, result);
    }
  };

  [passwordController_.sharedPasswordController
      didSelectSuggestion:suggestion
                     form:SysUTF8ToNSString(test_data.form_name)
             uniqueFormID:FormRendererId(test_data.form_renderer_id)
          fieldIdentifier:SysUTF8ToNSString(test_data.username_element)
            uniqueFieldID:FieldRendererId(test_data.username_renderer_id)
                  frameID:SysUTF8ToNSString(frame->GetFrameId())
        completionHandler:completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return block_was_called;
  }));
}

PasswordForm MakeSimpleForm() {
  PasswordForm form;
  form.url = GURL("http://www.google.com/a/LoginAuth");
  form.action = GURL("http://www.google.com/a/Login");
  form.username_element = u"Username";
  form.password_element = u"Passwd";
  form.username_value = u"googleuser";
  form.password_value = u"p4ssword";
  form.signon_realm = "http://www.google.com/";
  form.form_data = MakeSimpleFormData();
  form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  return form;
}

// TODO(crbug.com/403705) This test is flaky.
// Check that HTML forms are converted correctly into FormDatas.
TEST_F(PasswordControllerTest, DISABLED_FindPasswordFormsInView) {
  // clang-format off
  FindPasswordFormTestData test_data[] = {
     // Normal form: a username and a password element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user0'>"
      "<input type='password' name='pass0'>"
      "</form>",
      true, 2, "form1", 2
    },
    // User name is captured as an email address (HTML5).
    {
      @"<form name='form1'>"
      "<input type='email' name='email1'>"
      "<input type='password' name='pass1'>"
      "</form>",
      true, 2, "form1", 5
    },
    // No form found.
    {
      @"<div>",
      false, 0, nullptr, 0
    },
    // Disabled username element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user2' disabled='disabled'>"
      "<input type='password' name='pass2'>"
      "</form>",
      true, 2, "form1", 8
    },
    // No password element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user3'>"
      "</form>",
      false, 0, nullptr, 0
    },
  };
  // clang-format on

  for (const FindPasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message() << "for html_string=" << data.html_string);
    LoadHtml(data.html_string);
    __block std::vector<FormData> forms;
    __block BOOL block_was_called = NO;
    __block uint32_t maxExtractedID;
    [passwordController_.sharedPasswordController.formHelper
        findPasswordFormsInFrame:web::GetMainFrame(web_state())
               completionHandler:^(const std::vector<FormData>& result,
                                   uint32_t maxID) {
                 block_was_called = YES;
                 forms = result;
                 maxExtractedID = maxID;
               }];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));
    if (data.expected_form_found) {
      ASSERT_EQ(1U, forms.size());
      EXPECT_EQ(data.expected_number_of_fields, forms[0].fields.size());
      EXPECT_EQ(data.expected_form_name, base::UTF16ToUTF8(forms[0].name));
    } else {
      ASSERT_TRUE(forms.empty());
    }
    EXPECT_EQ(data.maxID, maxExtractedID);
  }
}

// Test HTML page. It contains several password forms.
// Tests autofill them and verify that the right ones are autofilled.
static NSString* kHtmlWithMultiplePasswordForms =
    @""
     // Basic form.
     "<form>"                                      // unique_id 1
     "<input id='un0' type='text' name='u0'>"      // unique_id 2
     "<input id='pw0' type='password' name='p0'>"  // unique_id 3
     "</form>"
     // Form with action in the same origin.
     "<form action='?query=yes#reference'>"        // unique_id 4
     "<input id='un1' type='text' name='u1'>"      // unique_id 5
     "<input id='pw1' type='password' name='p1'>"  // unique_id 6
     "</form>"
     // Form with two exactly same password fields.
     "<form>"                                      // unique_id 7
     "<input id='un2' type='text' name='u2'>"      // unique_id 8
     "<input id='pw2' type='password' name='p2'>"  // unique_id 9
     "<input id='pw2' type='password' name='p2'>"  // unique_id 10
     "</form>"
     // Forms with same names but different ids (1 of 2).
     "<form>"                                      // unique_id 11
     "<input id='un3' type='text' name='u3'>"      // unique_id 12
     "<input id='pw3' type='password' name='p3'>"  // unique_id 13
     "</form>"
     // Forms with same names but different ids (2 of 2).
     "<form>"                                      // unique_id 14
     "<input id='un4' type='text' name='u4'>"      // unique_id 15
     "<input id='pw4' type='password' name='p4'>"  // unique_id 16
     "</form>"
     // Basic form, but with quotes in the names and IDs.
     "<form name=\"f5'\">"                               // unique_id 17
     "<input id=\"un5'\" type='text' name=\"u5'\">"      // unique_id 18
     "<input id=\"pw5'\" type='password' name=\"p5'\">"  // unique_id 19
     "</form>"
     // Fields inside this form don't have name.
     "<form>"                            // unique_id 20
     "<input id='un6' type='text'>"      // unique_id 21
     "<input id='pw6' type='password'>"  // unique_id 22
     "</form>"
     // Fields in this form is attached by form's id.
     "<form id='form7'></form>"                       // unique_id 23
     "<input id='un7' type='text' form='form7'>"      // unique_id 24
     "<input id='pw7' type='password' form='form7'>"  // unique_id 25
     // Fields that are outside the <form> tag.
     "<input id='un8' type='text'>"      // unique_id 26
     "<input id='pw8' type='password'>"  // unique_id 27
     // Test forms inside iframes.
     "<iframe id='pf' name='pf'></iframe>"
     "<script>"
     "  var doc = frames['pf'].document.open();"
     // Add a form inside iframe. It should also be matched and autofilled.
     "  doc.write('<form id=\\'f10\\'><input id=\\'un10\\' type=\\'text\\' "
     "name=\\'u10\\'>');"  // unique_id 1-2
     "  doc.write('<input id=\\'pw10\\' type=\\'password\\' name=\\'p10\\'>');"
     "  doc.write('</form>');"  // unique_id 3
     "  doc.close();"
     "</script>";

// A script that resets all text fields, including those in iframes.
static NSString* kClearInputFieldsScript =
    @"function clearInputFields(win) {"
     "  var inputs = win.document.getElementsByTagName('input');"
     "  for (var i = 0; i < inputs.length; i++) {"
     "    inputs[i].value = '';"
     "  }"
     "  var frames = win.frames;"
     "  for (var i = 0; i < frames.length; i++) {"
     "    clearInputFields(frames[i]);"
     "  }"
     "}"
     "clearInputFields(window);";

struct FillPasswordFormTestData {
  const std::string origin;
  const char* name;
  uint32_t form_unique_ID;
  const char* username_field;
  uint32_t username_unique_ID;
  const char* username_value;
  const char* password_field;
  uint32_t password_unique_ID;
  const char* password_value;
  const BOOL should_succeed;
  const BOOL is_in_main_frame;
};

// Tests that filling password forms works correctly.
TEST_F(PasswordControllerTest, FillPasswordForm) {
  LoadHtml(kHtmlWithMultiplePasswordForms);

  const std::string base_url = BaseUrl();
  // clang-format off
  FillPasswordFormTestData test_data[] = {
    // Basic test: one-to-one match on the first password form.
    {
      base_url,
      "gChrome~form~0",
      1,
      "un0",
      2,
      "test_user",
      "pw0",
      3,
      "test_password",
      YES,
      YES
    },
    // The form matches despite a different action: the only difference
    // is a query and reference.
    {
      base_url,
      "gChrome~form~1",
      4,
      "un1",
      5,
      "test_user",
      "pw1",
      6,
      "test_password",
      YES,
      YES
    },
    // No match because some inputs are not in the form.
    {
      base_url,
      "gChrome~form~0",
      1,
      "un0",
      2,
      "test_user",
      "pw1",
      6,
      "test_password",
      NO,
      YES
    },
    // There are inputs with duplicate names in the form, the first of them is
    // filled.
    {
      base_url,
      "gChrome~form~2",
      7,
      "un2",
      8,
      "test_user",
      "pw2",
      9,
      "test_password",
      YES,
      YES
    },
    // Basic test, but with quotes in the names and IDs.
    {
      base_url,
      "f5'",
      17,
      "un5'",
      18,
      "test_user",
      "pw5'",
      19,
      "test_password",
      YES,
      YES
    },
    // Fields don't have name attributes so id attribute is used for fields
    // identification.
    {
      base_url,
      "gChrome~form~6",
      20,
      "un6",
      21,
      "test_user",
      "pw6",
      22,
      "test_password",
      YES,
      YES
    },
    // Fields in this form is attached by form's id.
    {
      base_url,
      "form7",
      23,
      "un7",
      24,
      "test_user",
      "pw7",
      25,
      "test_password",
      YES,
      YES
    },
    // Filling forms inside iframes.
    {
      base_url,
      "f10",
      1,
      "un10",
      2,
      "test_user",
      "pw10",
      3,
      "test_password",
      YES,
      NO
    },
    // Fields outside the form tag.
    {
      base_url,
      "",
      std::numeric_limits<uint32_t>::max(),
      "un8",
      26,
      "test_user",
      "pw8",
      27,
      "test_password",
      YES,
      YES
    },
  };
  // clang-format on

  for (const FillPasswordFormTestData& data : test_data) {
    web::test::ExecuteJavaScript(kClearInputFieldsScript, web_state());

    TestPasswordFormData form_test_data = {
        /*form_name=*/"",    data.form_unique_ID,
        data.username_field, data.username_unique_ID,
        data.password_field, data.password_unique_ID,
        data.username_value, data.password_value,
        /*on_key_up=*/NO,
        /*on_change=*/NO};

    FillFormAndValidate(form_test_data, data.should_succeed,
                        GetWebFrame(data.is_in_main_frame));
  }
}

// Check that password form is not filled if 'readonly' attribute is set
// on either username or password fields.
TEST_F(PasswordControllerTest, DontFillReadOnly) {
  TestPasswordFormData test_data = {/*form_name=*/"f0",
                                    /*form_renderer_id=*/1,
                                    /*username_element=*/"un0",
                                    /*username_renderer_id=*/2,
                                    /*password_element=*/"pw0",
                                    /*password_renderer_id=*/3,
                                    /*user_value=*/"abc",
                                    /*password_value=*/"def",
                                    /*on_key_up=*/NO,
                                    /*on_change=*/NO};

  // Control check that the fill operation will succceed with well-formed form.
  LoadHtml(@"<form id='f0'>"
            "<input id='un0' type='text' name='u0'>"
            "<input id='pw0' type='password' name='p0'>"
            "</form>");
  FillFormAndValidate(test_data,
                      /*should_succeed=*/true, web::GetMainFrame(web_state()));
  // Form fill should fail with 'readonly' attribute on username.
  LoadHtml(@"<form id='f0'>"
            "<input id='un0' type='text' name='u0' readonly='readonly'>"
            "<input id='pw0' type='password' name='p0'>"
            "</form>");
  FillFormAndValidate(test_data,
                      /*should_succeed=*/false, web::GetMainFrame(web_state()));
  // Form fill should fail with 'readonly' attribute on password.
  LoadHtml(@"<form id='f0'>"
            "<input id='un0' type='text' name='u0'>"
            "<input id='pw0' type='password' name='p0' readonly='readonly'>"
            "</form>");
  FillFormAndValidate(test_data,
                      /*should_succeed=*/false, web::GetMainFrame(web_state()));
}

// TODO(crbug.com/817755): Move them HTML const to separate HTML files.
// An HTML page without a password form.
static NSString* kHtmlWithoutPasswordForm =
    @"<h2>The rain in Spain stays <i>mainly</i> in the plain.</h2>";

// An HTML page containing one password form.  The username input field
// also has custom event handlers.  We need to verify that those event
// handlers are still triggered even though we override them with our own.
static NSString* kHtmlWithPasswordForm =
    @"<form>"
     "<input id='un' type='text' name=\"u'\""
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input id='pw' type='password' name=\"p'\""
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "</form>";

static NSString* kHtmlWithNewPasswordForm =
    @"<form>"
     "<input id='un' type='text' name=\"u'\" autocomplete=\"username\""
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input id='pw' type='password' name=\"p'\" autocomplete=\"new-password\">"
     "</form>";

// An HTML page containing two password forms.
static NSString* kHtmlWithTwoPasswordForms =
    @"<form id='f1'>"
     "<input type='text' id='u1'"
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input type='password' id='p1'>"
     "</form>"
     "<form id='f2'>"
     "<input type='text' id='u2'"
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input type='password' id='p2'>"
     "</form>";

// A script that adds a password form.
static NSString* kAddFormDynamicallyScript =
    @"var dynamicForm = document.createElement('form');"
     "dynamicForm.setAttribute('name', 'dynamic_form');"
     "var inputUsername = document.createElement('input');"
     "inputUsername.setAttribute('type', 'text');"
     "inputUsername.setAttribute('id', 'username');"
     "var inputPassword = document.createElement('input');"
     "inputPassword.setAttribute('type', 'password');"
     "inputPassword.setAttribute('id', 'password');"
     "var submitButton = document.createElement('input');"
     "submitButton.setAttribute('type', 'submit');"
     "submitButton.setAttribute('value', 'Submit');"
     "dynamicForm.appendChild(inputUsername);"
     "dynamicForm.appendChild(inputPassword);"
     "dynamicForm.appendChild(submitButton);"
     "document.body.appendChild(dynamicForm);";

static NSString* kHtmlFormlessPasswordFields =
    @"<input id='un' type='text' name=\"u'\""
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input id='pw' type='password' name=\"pw'\""
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>";

struct SuggestionTestData {
  std::string description;
  NSArray* eval_scripts;
  NSArray* expected_suggestions;
  NSString* expected_result;
};

// Tests that form activity correctly sends suggestions to the suggestion
// controller.
TEST_F(PasswordControllerTest, SuggestionUpdateTests) {
  LoadHtml(kHtmlWithPasswordForm);
  WaitForFormManagersCreation();

  const std::string base_url = BaseUrl();

  PasswordFormFillData form_data;
  SetPasswordFormFillData(base_url, "", 1, "un", 2, "user0", "pw", 3,
                          "password0", "abc", "def", &form_data);

  web::WebFrame* expected_frame = web::GetMainFrame(web_state());
  [passwordController_.sharedPasswordController
      processPasswordFormFillData:form_data
                          inFrame:expected_frame
                      isMainFrame:expected_frame->IsMainFrame()
                forSecurityOrigin:expected_frame->GetSecurityOrigin()];

  // clang-format off
  SuggestionTestData test_data[] = {
    {
      "Should show all suggestions when focusing empty username field",
      @[(@"var evt = document.createEvent('Events');"
         "username_.focus();"),
        @";"],
      @[@"user0 ••••••••", @"abc ••••••••"],
      @"[]=, onkeyup=false, onchange=false"
    },
    {
      "Should show password suggestions when focusing password field",
      @[(@"var evt = document.createEvent('Events');"
         "password_.focus();"),
        @";"],
      @[@"user0 ••••••••", @"abc ••••••••"],
      @"[]=, onkeyup=false, onchange=false"
    },
    {
      "Should not filter suggestions when focusing username field with input",
      @[(@"username_.value='ab';"
         "username_.focus();"),
        @";"],
      @[@"user0 ••••••••", @"abc ••••••••"],
      @"ab[]=, onkeyup=false, onchange=false"
    },
    {
      "Should filter suggestions when typing into a username field",
      @[(@"username_.value='ab';"
         "username_.focus();"
         // Keyup event is dispatched to simulate typing
         "var ev = new KeyboardEvent('keyup', {bubbles:true});"
         "username_.dispatchEvent(ev);"),
        @";"],
      @[@"abc ••••••••"],
      @"ab[]=, onkeyup=true, onchange=false"
    },
    {
      "Should not show suggestions when typing into a password field",
      @[(@"username_.value='abc';"
         "password_.value='••';"
         "password_.focus();"
         // Keyup event is dispatched to simulate typing.
         "var ev = new KeyboardEvent('keyup', {bubbles:true});"
         "password_.dispatchEvent(ev);"),
        @";"],
      @[],
      @"abc[]=••, onkeyup=true, onchange=false"
    },
  };
  // clang-format on

  for (const SuggestionTestData& data : test_data) {
    SCOPED_TRACE(testing::Message()
                 << "for description=" << data.description
                 << " and eval_scripts=" << data.eval_scripts);
    // Prepare the test.
    web::test::ExecuteJavaScript(
        [NSString stringWithFormat:kUsernameAndPasswordTestPreparationScript,
                                   @"un", @"pw"],
        web_state());

    for (NSString* script in data.eval_scripts) {
      // Trigger events.
      web::test::ExecuteJavaScript(script, web_state());

      // Pump the run loop so that the host can respond.
      web::test::WaitForBackgroundTasks();
    }
    // Wait until suggestions are received.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return [GetSuggestionValues() count] == [data.expected_suggestions count];
    }));

    EXPECT_NSEQ(data.expected_suggestions, GetSuggestionValues());
    EXPECT_NSEQ(data.expected_result,
                web::test::ExecuteJavaScript(
                    kUsernamePasswordVerificationScript, web_state()));
    // Clear all suggestions.
    [suggestionController_ setSuggestions:nil];
  }
}

// Tests that selecting a suggestion will fill the corresponding form and field.
TEST_F(PasswordControllerTest, SelectingSuggestionShouldFillPasswordForm) {
  LoadHtml(kHtmlWithTwoPasswordForms);
  WaitForFormManagersCreation();

  const std::string base_url = BaseUrl();

  const TestPasswordFormData kTestData[] = {{/*form_name=*/"f1",
                                             /*form_renderer_id=*/1,
                                             /*username_element=*/"u1",
                                             /*username_renderer_id=*/2,
                                             /*password_element=*/"p1",
                                             /*password_renderer_id=*/3,
                                             /*user_value=*/"abc",
                                             /*password_value=*/"def",
                                             /*on_key_up=*/YES,
                                             /*on_change=*/YES},
                                            {/*form_name=*/"f2",
                                             /*form_renderer_id=*/4,
                                             /*username_element=*/"u2",
                                             /*username_renderer_id=*/5,
                                             /*password_element=*/"p2",
                                             /*password_renderer_id=*/6,
                                             /*user_value=*/"abc",
                                             /*password_value=*/"def",
                                             /*on_key_up=*/YES,
                                             /*on_change=*/YES}};

  // Check that the right password form is filled on suggesion selection.
  for (size_t form_i = 0; form_i < std::size(kTestData); ++form_i) {
    FillFormAndValidate(kTestData[form_i], /*should_succeed=*/true,
                        web::GetMainFrame(web_state()));
  }
}

// The test cases below need a different SetUp.
class PasswordControllerTestSimple : public PlatformTest {
 public:
  PasswordControllerTestSimple() {}

  ~PasswordControllerTestSimple() override { store_->ShutdownOnUIThread(); }

  void SetUp() override {
    store_ =
        new testing::NiceMock<password_manager::MockPasswordStoreInterface>();
    ON_CALL(*store_, IsAbleToSavePasswords).WillByDefault(Return(true));

    UniqueIDDataTabHelper::CreateForWebState(&web_state_);

    passwordController_ =
        CreatePasswordController(&web_state_, store_.get(), &weak_client_);
    passwordController_.passwordManager->set_leak_factory(
        std::make_unique<
            NiceMock<password_manager::MockLeakDetectionCheckFactory>>());

    ON_CALL(*weak_client_, IsSavingAndFillingEnabled)
        .WillByDefault(Return(true));

    ON_CALL(*store_, GetLogins)
        .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web_state_.SetWebFramesManager(std::move(web_frames_manager));
  }

  base::test::TaskEnvironment task_environment_;

  PasswordController* passwordController_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> store_;
  MockPasswordManagerClient* weak_client_;
  web::FakeWebState web_state_;
  web::FakeWebFramesManager* web_frames_manager_;
};

TEST_F(PasswordControllerTestSimple, SaveOnNonHTMLLandingPage) {
  // Have a form observed and submitted.
  FormData formData = MakeSimpleFormData();
  SharedPasswordController* sharedPasswordController =
      passwordController_.sharedPasswordController;

  auto web_frame = web::FakeWebFrame::CreateMainWebFrame(GURL::EmptyGURL());
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  [sharedPasswordController formHelper:sharedPasswordController.formHelper
                         didSubmitForm:formData
                               inFrame:web::GetMainFrame(&web_state_)];

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Save password prompt shpuld be shown after navigation to a non-HTML page.
  web_state_.SetContentIsHTML(false);
  web_state_.SetCurrentURL(GURL("https://google.com/success"));
  [sharedPasswordController webState:&web_state_ didLoadPageWithSuccess:YES];

  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));

  EXPECT_EQ("http://www.google.com/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"googleuser",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"p4ssword",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Checks that when the user set a focus on a field of a password form which was
// not sent to the store then the request the the store is sent.
TEST_F(PasswordControllerTest, SendingToStoreDynamicallyAddedFormsOnFocus) {
  LoadHtml(kHtmlWithoutPasswordForm);
  web::test::ExecuteJavaScript(kAddFormDynamicallyScript, web_state());

  // The standard pattern is to use a __block variable WaitUntilCondition but
  // __block variable can't be captured in C++ lambda, so as workaround it's
  // used normal variable `get_logins_called` and pointer on it is used in a
  // block.
  bool get_logins_called = false;
  bool* p_get_logins_called = &get_logins_called;

  password_manager::PasswordFormDigest expected_form_digest(
      password_manager::PasswordForm::Scheme::kHtml, "https://chromium.test/",
      GURL("https://chromium.test/"));
  // TODO(crbug.com/949519): replace WillRepeatedly with WillOnce when the old
  // parser is gone.
  EXPECT_CALL(*store_, GetLogins(expected_form_digest, _))
      .WillRepeatedly(testing::Invoke(
          [&get_logins_called](
              const password_manager::PasswordFormDigest&,
              base::WeakPtr<password_manager::PasswordStoreConsumer>) {
            get_logins_called = true;
          }));

  // Sets a focus on a username field.
  NSString* kSetUsernameInFocusScript =
      @"document.getElementById('username').focus();";
  web::test::ExecuteJavaScript(kSetUsernameInFocusScript, web_state());

  // Wait until GetLogins is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return *p_get_logins_called;
  }));
}

// Tests that a touchend event from a button which contains in a password form
// works as a submission indicator for this password form.
TEST_F(PasswordControllerTest, TouchendAsSubmissionIndicator) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
  const char* kHtml[] = {
      "<html><body>"
      "<form name='login_form' id='login_form'>"
      "  <input type='text' name='username'>"
      "  <input type='password' name='password'>"
      "  <button id='submit_button' value='Submit'>"
      "</form>"
      "</body></html>",
      "<html><body>"
      "<form name='login_form' id='login_form'>"
      "  <input type='text' name='username'>"
      "  <input type='password' name='password'>"
      "  <button id='back' value='Back'>"
      "  <button id='submit_button' type='submit' value='Submit'>"
      "</form>"
      "</body></html>"};

  for (size_t i = 0; i < std::size(kHtml); ++i) {
    LoadHtml(SysUTF8ToNSString(kHtml[i]));
    WaitForFormManagersCreation();

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

    web::test::ExecuteJavaScript(
        @"document.getElementsByName('username')[0].value = 'user1';"
         "document.getElementsByName('password')[0].value = 'password1';"
         "var e = new UIEvent('touchend');"
         "document.getElementById('submit_button').dispatchEvent(e);",
        web_state());
    LoadHtmlWithRendererInitiatedNavigation(
        SysUTF8ToNSString("<html><body>Success</body></html>"));

    auto& form_manager_check = form_manager_to_save;
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return form_manager_check != nullptr;
    }));

    EXPECT_EQ("https://chromium.test/",
              form_manager_to_save->GetPendingCredentials().signon_realm);
    EXPECT_EQ(u"user1",
              form_manager_to_save->GetPendingCredentials().username_value);
    EXPECT_EQ(u"password1",
              form_manager_to_save->GetPendingCredentials().password_value);

    auto* form_manager =
        static_cast<PasswordFormManager*>(form_manager_to_save.get());
    EXPECT_TRUE(form_manager->is_submitted());
    EXPECT_FALSE(form_manager->IsPasswordUpdate());
  }
}

// Tests that a touchend event from a button which contains in a password form
// works as a submission indicator for this password form.
TEST_F(PasswordControllerTest, SavingFromSameOriginIframe) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  LoadHtml(@"<iframe id='frame1' name='frame1'></iframe>");
  web::test::ExecuteJavaScript(
      @"document.getElementById('frame1').contentDocument.body.innerHTML = "
       "'<form id=\"form1\">"
       "<input type=\"text\" name=\"text\" value=\"user1\" id=\"id2\">"
       "<input type=\"password\" name=\"password\" value=\"pw1\" id=\"id2\">"
       "<input type=\"submit\" id=\"submit_input\"/>"
       "</form>'",
      web_state());
  web::test::ExecuteJavaScript(
      @"document.getElementById('frame1').contentDocument.getElementById('"
      @"submit_input').click();",
      web_state());

  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"));
  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user1",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"pw1",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Tests that when a dynamic form added and the user clicks on the username
// field in this form, then the request to the Password Store is sent and
// PassworController is waiting to the response in order to show or not to show
// password suggestions.
TEST_F(PasswordControllerTest, CheckAsyncSuggestions) {
  for (bool store_has_credentials : {false, true}) {
    if (store_has_credentials) {
      PasswordForm form(CreatePasswordForm(BaseUrl().c_str(), "user", "pw"));
      // TODO(crbug.com/949519): replace WillRepeatedly with WillOnce when the
      // old parser is gone.
      EXPECT_CALL(*store_, GetLogins)
          .WillRepeatedly(WithArg<1>(InvokeConsumer(store_.get(), form)));
    } else {
      EXPECT_CALL(*store_, GetLogins)
          .WillRepeatedly(
              WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
    }
    // Do not call `LoadHtml` which will prematurely configure form ids.
    web::test::LoadHtml(kHtmlWithoutPasswordForm, web_state());
    web::test::ExecuteJavaScript(kAddFormDynamicallyScript, web_state());

    SimulateFormActivityObserverSignal("form_changed", FormRendererId(),
                                       FieldRendererId(), std::string());
    WaitForFormManagersCreation();

    __block BOOL completion_handler_success = NO;
    __block BOOL completion_handler_called = NO;

    FormRendererId form_id =
        store_has_credentials ? FormRendererId(4) : FormRendererId(1);
    FieldRendererId field_id =
        store_has_credentials ? FieldRendererId(5) : FieldRendererId(2);
    std::string mainFrameID = web::GetMainWebFrameId(web_state());

    FormSuggestionProviderQuery* form_query =
        [[FormSuggestionProviderQuery alloc]
            initWithFormName:@"dynamic_form"
                uniqueFormID:form_id
             fieldIdentifier:@"username"
               uniqueFieldID:field_id
                   fieldType:@"text"
                        type:@"focus"
                  typedValue:@""
                     frameID:SysUTF8ToNSString(mainFrameID)];
    [passwordController_.sharedPasswordController
        checkIfSuggestionsAvailableForForm:form_query
                            hasUserGesture:YES
                                  webState:web_state()
                         completionHandler:^(BOOL success) {
                           completion_handler_success = success;
                           completion_handler_called = YES;
                         }];
    // Wait until the expected handler is called.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return completion_handler_called;
    }));

    EXPECT_EQ(store_has_credentials, completion_handler_success);
    testing::Mock::VerifyAndClearExpectations(&store_);
  }
}

// Tests that when a dynamic form added and the user clicks on non username
// field in this form, then the request to the Password Store is sent but no
// suggestions are shown.
TEST_F(PasswordControllerTest, CheckNoAsyncSuggestionsOnNonUsernameField) {
  PasswordForm form(CreatePasswordForm(BaseUrl().c_str(), "user", "pw"));
  EXPECT_CALL(*store_, GetLogins)
      .WillOnce(WithArg<1>(InvokeConsumer(store_.get(), form)));

  LoadHtml(kHtmlWithoutPasswordForm);
  web::test::ExecuteJavaScript(kAddFormDynamicallyScript, web_state());

  SimulateFormActivityObserverSignal("form_changed", FormRendererId(),
                                     FieldRendererId(), std::string());
  WaitForFormManagersCreation();

  __block BOOL completion_handler_success = NO;
  __block BOOL completion_handler_called = NO;
  std::string mainFrameID = web::GetMainWebFrameId(web_state());

  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"dynamic_form"
          uniqueFormID:FormRendererId(1)
       fieldIdentifier:@"address"
         uniqueFieldID:FieldRendererId(4)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:form_query
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         completion_handler_success = success;
                         completion_handler_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return completion_handler_called;
  }));

  EXPECT_FALSE(completion_handler_success);
}

// Tests that when there are no password forms on a page and the user clicks on
// a text field the completion callback is called with no suggestions result.
TEST_F(PasswordControllerTest, CheckNoAsyncSuggestionsOnNoPasswordForms) {
  LoadHtml(kHtmlWithoutPasswordForm);

  __block BOOL completion_handler_success = NO;
  __block BOOL completion_handler_called = NO;

  EXPECT_CALL(*store_, GetLogins).Times(0);
  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:FormRendererId(1)
       fieldIdentifier:@"address"
         uniqueFieldID:FieldRendererId(2)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:form_query
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         completion_handler_success = success;
                         completion_handler_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return completion_handler_called;
  }));

  EXPECT_FALSE(completion_handler_success);
}

// Tests password generation suggestion is shown properly.
TEST_F(PasswordControllerTest, CheckPasswordGenerationSuggestion) {
  EXPECT_CALL(*store_, GetLogins)
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
  EXPECT_CALL(*weak_client_->GetPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(Return(true));

  LoadHtml(kHtmlWithNewPasswordForm);
  WaitForFormManagersCreation();

  const std::string base_url = BaseUrl();
  PasswordFormFillData form_data;
  SetPasswordFormFillData(base_url, "", 1, "un", 2, "user0", "pw", 3,
                          "password0", "abc", "def", &form_data);

  web::WebFrame* expected_frame = web::GetMainFrame(web_state());
  [passwordController_.sharedPasswordController
      processPasswordFormFillData:form_data
                          inFrame:expected_frame
                      isMainFrame:expected_frame->IsMainFrame()
                forSecurityOrigin:expected_frame->GetSecurityOrigin()];

  // clang-format off
  SuggestionTestData test_data[] = {
    {
      "Should not show suggest password when focusing username field",
      @[(@"var evt = document.createEvent('Events');"
         "username_.focus();"),
        @";"],
      @[@"user0 ••••••••", @"abc ••••••••"],
      @"[]=, onkeyup=false, onchange=false"
    },
    {
      "Should show suggest password when focusing password field",
      @[(@"var evt = document.createEvent('Events');"
         "password_.focus();"),
        @";"],
      @[@"user0 ••••••••", @"abc ••••••••", @"Suggest Password\u2026"],
      @"[]=, onkeyup=false, onchange=false"
    },
  };
  // clang-format on

  for (const SuggestionTestData& data : test_data) {
    SCOPED_TRACE(testing::Message()
                 << "for description=" << data.description
                 << " and eval_scripts=" << data.eval_scripts);
    // Prepare the test.
    web::test::ExecuteJavaScript(
        [NSString stringWithFormat:kUsernameAndPasswordTestPreparationScript,
                                   @"un", @"pw"],
        web_state());

    for (NSString* script in data.eval_scripts) {
      // Trigger events.
      web::test::ExecuteJavaScript(script, web_state());

      // Pump the run loop so that the host can respond.
      web::test::WaitForBackgroundTasks();
    }
    // Wait until suggestions are received.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return [GetSuggestionValues() count] > 0;
    }));

    EXPECT_NSEQ(data.expected_suggestions, GetSuggestionValues());
    EXPECT_NSEQ(data.expected_result,
                web::test::ExecuteJavaScript(
                    kUsernamePasswordVerificationScript, web_state()));
    // Clear all suggestions.
    [suggestionController_ setSuggestions:nil];
  }
}

// Tests that the user is prompted to save or update password on a succesful
// form submission.
TEST_F(PasswordControllerTest, ShowingSavingPromptOnSuccessfulSubmission) {
  // TODO(crbug.com/1404697): Re-enable on iOS 14. This test is flaky on iOS 14,
  // sometimes failing to finish loading the HTML.
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    return;
  }

  const char* kHtml = {"<html><body>"
                       "<form name='login_form' id='login_form'>"
                       "  <input type='text' name='username'>"
                       "  <input type='password' name='password'>"
                       "  <button id='submit_button' value='Submit'>"
                       "</form>"
                       "</body></html>"};
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  web::test::ExecuteJavaScript(
      @"document.getElementsByName('username')[0].value = 'user1';"
       "document.getElementsByName('password')[0].value = 'password1';"
       "document.getElementById('submit_button').click();",
      web_state());
  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"));
  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));
  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user1",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"password1",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Tests that the user is not prompted to save or update password on
// leaving the page before submitting the form.
TEST_F(PasswordControllerTest, NotShowingSavingPromptWithoutSubmission) {
  const char* kHtml = {"<html><body>"
                       "<form name='login_form' id='login_form'>"
                       "  <input type='text' name='username'>"
                       "  <input type='password' name='password'>"
                       "  <button id='submit_button' value='Submit'>"
                       "</form>"
                       "</body></html>"};
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);
  web::test::ExecuteJavaScript(
      @"document.getElementsByName('username')[0].value = 'user1';"
       "document.getElementsByName('password')[0].value = 'password1';",
      web_state());
  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>New page</body></html>"));
}

// Tests that the user is not prompted to save or update password on a
// succesful form submission while saving is disabled.
TEST_F(PasswordControllerTest, NotShowingSavingPromptWhileSavingIsDisabled) {
  // TODO(crbug.com/1404697): Re-enable on iOS 14. This test is flaky on iOS 14,
  // sometimes failing to finish loading the HTML.
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    return;
  }

  const char* kHtml = {"<html><body>"
                       "<form name='login_form' id='login_form'>"
                       "  <input type='text' name='username'>"
                       "  <input type='password' name='password'>"
                       "  <button id='submit_button' value='Submit'>"
                       "</form>"
                       "</body></html>"};
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
  ON_CALL(*weak_client_, IsSavingAndFillingEnabled)
      .WillByDefault(Return(false));

  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);
  web::test::ExecuteJavaScript(
      @"document.getElementsByName('username')[0].value = 'user1';"
       "document.getElementsByName('password')[0].value = 'password1';"
       "document.getElementById('submit_button').click();",
      web_state());
  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"));
}

// Tests that the user is prompted to update password on a succesful
// form submission when there's already a credential with the same
// username in the store.
TEST_F(PasswordControllerTest, ShowingUpdatePromptOnSuccessfulSubmission) {
  // TODO(crbug.com/1404697): Re-enable on iOS 14. This test is flaky on iOS 14,
  // sometimes failing to finish loading the HTML.
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    return;
  }

  PasswordForm form(MakeSimpleForm());
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeConsumer(store_.get(), form)));
  const char* kHtml = {"<html><body>"
                       "<form name='login_form' id='login_form'>"
                       "  <input type='text' name='Username'>"
                       "  <input type='password' name='Passwd'>"
                       "  <button id='submit_button' value='Submit'>"
                       "</form>"
                       "</body></html>"};

  LoadHtml(SysUTF8ToNSString(kHtml), GURL("http://www.google.com/a/LoginAuth"));
  WaitForFormManagersCreation();

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  web::test::ExecuteJavaScript(
      @"document.getElementsByName('Username')[0].value = 'googleuser';"
       "document.getElementsByName('Passwd')[0].value = 'new_password';"
       "document.getElementById('submit_button').click();",
      web_state());
  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"),
      GURL("http://www.google.com/a/Login"));

  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));
  EXPECT_EQ("http://www.google.com/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"googleuser",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"new_password",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_TRUE(form_manager->IsPasswordUpdate());
}

TEST_F(PasswordControllerTest, SavingOnNavigateMainFrame) {
  constexpr char kHtml[] = "<html><body>"
                           "<form name='login_form' id='login_form'>"
                           "  <input type='text' name='username'>"
                           "  <input type='password' name='pw'>"
                           "</form>"
                           "</body></html>";

  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
  FormRendererId form_id = FormRendererId(1);
  FieldRendererId username_id = FieldRendererId(2);
  FieldRendererId password_id = FieldRendererId(3);
  for (bool has_commited : {false, true}) {
    for (bool is_same_document : {false, true}) {
      for (bool is_renderer_initiated : {false, true}) {
        SCOPED_TRACE(testing::Message("has_commited = ")
                     << has_commited << " is_same_document=" << is_same_document
                     << " is_renderer_initiated=" << is_renderer_initiated);
        LoadHtml(SysUTF8ToNSString(kHtml));

        std::string main_frame_id = web::GetMainWebFrameId(web_state());

        SimulateUserTyping("login_form", form_id, "username", username_id,
                           "user1", main_frame_id);
        SimulateUserTyping("login_form", form_id, "pw", password_id,
                           "password1", main_frame_id);

        bool prompt_should_be_shown =
            has_commited && !is_same_document && is_renderer_initiated;

        std::unique_ptr<PasswordFormManagerForUI> form_manager;
        if (prompt_should_be_shown) {
          EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
              .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager)));
        } else {
          EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
              .Times(0);
        }
        form_id.value() += 3;
        username_id.value() += 3;
        password_id.value() += 3;
        web::FakeNavigationContext context;
        context.SetHasCommitted(has_commited);
        context.SetIsSameDocument(is_same_document);
        context.SetIsRendererInitiated(is_renderer_initiated);
        [passwordController_.sharedPasswordController webState:web_state()
                                           didFinishNavigation:&context];

        // Simulate a successful submission by loading the landing page without
        // a form.
        LoadHtml(
            SysUTF8ToNSString("<html><body>Login success page</body></html>"));

        if (prompt_should_be_shown) {
          auto& form_manager_check = form_manager;
          ASSERT_TRUE(
              WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
                return form_manager_check != nullptr;
              }));
          EXPECT_EQ(u"user1",
                    form_manager->GetPendingCredentials().username_value);
          EXPECT_EQ(u"password1",
                    form_manager->GetPendingCredentials().password_value);
        }
        testing::Mock::VerifyAndClearExpectations(weak_client_);
      }
    }
  }
}

TEST_F(PasswordControllerTest, NoSavingOnNavigateMainFrameFailedSubmission) {
  constexpr char kHtml[] = "<html><body>"
                           "<form name='login_form' id='login_form'>"
                           "  <input type='text' name='username'>"
                           "  <input type='password' name='pw'>"
                           "</form>"
                           "</body></html>";

  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();

  std::string main_frame_id = web::GetMainWebFrameId(web_state());

  SimulateUserTyping("login_form", FormRendererId(1), "username",
                     FieldRendererId(2), "user1", main_frame_id);
  SimulateUserTyping("login_form", FormRendererId(1), "pw", FieldRendererId(3),
                     "password1", main_frame_id);

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);

  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  context.SetIsRendererInitiated(true);
  [passwordController_.sharedPasswordController webState:web_state()
                                     didFinishNavigation:&context];

  // Simulate a failed submission by loading the same form again.
  LoadHtml(SysUTF8ToNSString(kHtml));
  WaitForFormManagersCreation();
}

// Tests that a form that is dynamically added to the page is found and
// that a form manager is created for it.
TEST_F(PasswordControllerTest, FindDynamicallyAddedForm2) {
  LoadHtml(kHtmlWithoutPasswordForm);
  web::test::ExecuteJavaScript(kAddFormDynamicallyScript, web_state());

  SimulateFormActivityObserverSignal("form_changed", FormRendererId(),
                                     FieldRendererId(), std::string());
  WaitForFormManagersCreation();

  auto& form_managers = passwordController_.passwordManager->form_managers();
  ASSERT_EQ(1u, form_managers.size());
  auto* password_form = form_managers[0]->observed_form();
  EXPECT_EQ(u"dynamic_form", password_form->name);
}

// Tests that submission is detected on removal of the form that had user input.
TEST_F(PasswordControllerTest, DetectSubmissionOnRemovedForm) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
  for (bool has_form_tag : {true, false}) {
    SCOPED_TRACE(testing::Message("has_form_tag = ") << has_form_tag);
    LoadHtml(has_form_tag ? kHtmlWithPasswordForm
                          : kHtmlFormlessPasswordFields);
    WaitForFormManagersCreation();

    std::string mainFrameID = web::GetMainWebFrameId(web_state());

    std::string form_name = has_form_tag ? "login_form" : "";
    FormRendererId form_id(has_form_tag ? 1 : 0);
    FieldRendererId username_id(has_form_tag ? 2 : 4);
    FieldRendererId password_id(has_form_tag ? 3 : 5);

    SimulateUserTyping(form_name, form_id, "un", username_id, "user1",
                       mainFrameID);
    SimulateUserTyping(form_name, form_id, "pw", password_id, "password1",
                       mainFrameID);

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

    std::vector<FieldRendererId> removed_ids;
    if (!has_form_tag) {
      removed_ids.push_back(FieldRendererId(4));
      removed_ids.push_back(FieldRendererId(5));
    }
    SimulateFormRemovalObserverSignal(form_id, removed_ids);

    auto& form_manager_check = form_manager_to_save;
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return form_manager_check != nullptr;
    }));
    EXPECT_EQ("https://chromium.test/",
              form_manager_to_save->GetPendingCredentials().signon_realm);
    EXPECT_EQ(u"user1",
              form_manager_to_save->GetPendingCredentials().username_value);
    EXPECT_EQ(u"password1",
              form_manager_to_save->GetPendingCredentials().password_value);

    auto* form_manager =
        static_cast<PasswordFormManager*>(form_manager_to_save.get());
    EXPECT_TRUE(form_manager->is_submitted());
    EXPECT_FALSE(form_manager->IsPasswordUpdate());
  }
}

// Tests that submission is not detected on form removal if saving is
// disabled.
TEST_F(PasswordControllerTest,
       DetectNoSubmissionOnRemovedFormIfSavingDisabled) {
  ON_CALL(*weak_client_, IsSavingAndFillingEnabled)
      .WillByDefault(Return(false));

  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
  LoadHtml(kHtmlWithPasswordForm);
  WaitForFormManagersCreation();

  std::string mainFrameID = web::GetMainWebFrameId(web_state());

  SimulateUserTyping("login_form", FormRendererId(1), "username",
                     FieldRendererId(2), "user1", mainFrameID);
  SimulateUserTyping("login_form", FormRendererId(1), "pw", FieldRendererId(3),
                     "password1", mainFrameID);

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);

  SimulateFormActivityObserverSignal("password_form_removed", FormRendererId(1),
                                     FieldRendererId(), std::string());
}

// Tests that submission is not detected on removal of the form that never
// had user input.
TEST_F(PasswordControllerTest,
       DetectNoSubmissionOnRemovedFormWithoutUserInput) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
  LoadHtml(kHtmlWithPasswordForm);
  WaitForFormManagersCreation();

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);

  SimulateFormActivityObserverSignal("password_form_removed", FormRendererId(1),
                                     FieldRendererId(), std::string());
}

// Tests that submission is detected on removal of the form that had user input.
TEST_F(PasswordControllerTest, DetectSubmissionOnIFrameDetach) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
  EXPECT_TRUE(
      LoadHtml("<script>"
               "  function FillFrame() {"
               "       var doc = frames['frame1'].document.open();"
               "       doc.write('<form id=\"form1\">');"
               "       doc.write('<input id=\"un\" type=\"text\">');"
               "       doc.write('<input id=\"pw\" type=\"password\">');"
               "       doc.write('</form>');"
               "       doc.close();"
               // This event listerer is set by Chrome, but it gets disabled
               // by document.write(). This is quite uncommon way to add
               // content to an iframe, but it is the only way for this test.
               // Reattaching it manually for test purposes.
               "       frames[0].addEventListener('unload', function(event) {"
               "  __gCrWeb.common.sendWebKitMessage('FrameBecameUnavailable',"
               "      frames[0].__gCrWeb.message.getFrameId());"
               "});"
               "}"
               "</script>"
               "<body onload='FillFrame()'>"
               "<iframe id='frame1' name='frame1'></iframe>"
               "</body>"));

  WaitForFormManagersCreation();

  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  std::set<WebFrame*> all_frames =
      web_state()->GetPageWorldWebFramesManager()->GetAllWebFrames();
  std::string iFrameID;
  for (auto* frame : all_frames) {
    if (!frame->IsMainFrame()) {
      iFrameID = frame->GetFrameId();
      break;
    }
  }

  SimulateUserTyping("form1", FormRendererId(1), "un", FieldRendererId(2),
                     "user1", iFrameID);
  SimulateUserTyping("form1", FormRendererId(1), "pw", FieldRendererId(3),
                     "password1", iFrameID);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  web::test::ExecuteJavaScript(
      @"var frame1 = document.getElementById('frame1');"
       "frame1.parentNode.removeChild(frame1);",
      web_state());
  auto& form_manager_check = form_manager_to_save;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager_check != nullptr;
  }));

  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user1",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"password1",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Tests that no submission is detected on removal of the form that had no user
// input.
TEST_F(PasswordControllerTest,
       DetectNoSubmissionOnIFrameDetachWithoutUserInput) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
  EXPECT_TRUE(
      LoadHtml("<script>"
               "  function FillFrame() {"
               "       var doc = frames['frame1'].document.open();"
               "       doc.write('<form id=\"form1\">');"
               "       doc.write('<input id=\"un\" type=\"text\">');"
               "       doc.write('<input id=\"pw\" type=\"password\">');"
               "       doc.write('</form>');"
               "       doc.close();"
               // This event listerer is set by Chrome, but it gets disabled
               // by document.write(). This is quite uncommon way to add
               // content to an iframe, but it is the only way for this test.
               // Reattaching it manually for test purposes.
               "       frames[0].addEventListener('unload', function(event) {"
               "  __gCrWeb.common.sendWebKitMessage('FrameBecameUnavailable',"
               "      frames[0].__gCrWeb.message.getFrameId());"
               "});"
               "}"
               "</script>"
               "<body onload='FillFrame()'>"
               "<iframe id='frame1' name='frame1'></iframe>"
               "</body>"));

  WaitForFormManagersCreation();

  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr).Times(0);

  web::test::ExecuteJavaScript(
      @"var frame1 = document.getElementById('frame1');"
       "frame1.parentNode.removeChild(frame1);",
      web_state());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    auto frames =
        web_state()->GetPageWorldWebFramesManager()->GetAllWebFrames();
    return frames.size() == 1;
  }));
}

TEST_F(PasswordControllerTest, PasswordMetricsNoSavedCredentials) {
  // TODO(crbug.com/1404697): Re-enable on iOS 14. This test is flaky on iOS 14,
  // sometimes failing to finish loading the HTML.
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    return;
  }

  base::HistogramTester histogram_tester;
  {
    ON_CALL(*store_, GetLogins)
        .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));
    LoadHtml(@"<html><body>"
              "<form name='login_form' id='login_form'>"
              "  <input type='text' name='username'>"
              "  <input type='password' name='password'>"
              "  <button id='submit_button' value='Submit'>"
              "</form>"
              "</body></html>");
    WaitForFormManagersCreation();

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

    std::string main_frame_id = web::GetMainWebFrameId(web_state());
    web::test::ExecuteJavaScript(
        @"document.getElementsByName('username')[0].value = 'user';"
         "document.getElementsByName('password')[0].value = 'pw';"
         "document.getElementById('submit_button').click();",
        web_state());
    LoadHtmlWithRendererInitiatedNavigation(
        @"<html><body>Success</body></html>");

    auto& form_manager_check = form_manager_to_save;
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return form_manager_check != nullptr;
    }));
  }

  histogram_tester.ExpectUniqueSample("PasswordManager.FillingAssistance",
                                      FillingAssistance::kNoSavedCredentials,
                                      1);
}

// Tests that focusing the password field containing the generated password
// is not breaking the password generation flow.
// Verifies the fix for crbug.com/1077271.
TEST_F(PasswordControllerTest, PasswordGenerationFieldFocus) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

  LoadHtml(@"<html><body>"
            "<form name='login_form' id='signup_form'>"
            "  <input type='text' name='username' id='un'>"
            "  <input type='password' name='password' id='pw'>"
            "  <button id='submit_button' value='Submit'>"
            "</form>"
            "</body></html>");
  WaitForFormManagersCreation();

  InjectGeneratedPassword(FormRendererId(1), FieldRendererId(3),
                          @"generated_password");

  // Focus the password field after password generation.
  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  __block bool block_was_called = NO;
  FormSuggestionProviderQuery* focus_query =
      [[FormSuggestionProviderQuery alloc]
          initWithFormName:@"signup_form"
              uniqueFormID:FormRendererId(1)
           fieldIdentifier:@"pw"
             uniqueFieldID:FieldRendererId(3)
                 fieldType:@"password"
                      type:@"focus"
                typedValue:@""
                   frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:focus_query
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         block_was_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return block_was_called;
  }));
  // Check that the password is still generated.
  ASSERT_TRUE(passwordController_.sharedPasswordController.isPasswordGenerated);
}

// Tests that adding input into the password field containing the generated
// password is not breaking the password generation flow.
TEST_F(PasswordControllerTest, PasswordGenerationFieldInput) {
  LoadHtml(@"<html><body>"
            "<form name='login_form' id='signup_form'>"
            "  <input type='text' name='username' id='un'>"
            "  <input type='password' name='password' id='pw'>"
            "  <button id='submit_button' value='Submit'>"
            "</form>"
            "</body></html>");
  WaitForFormManagersCreation();

  InjectGeneratedPassword(FormRendererId(1), FieldRendererId(3),
                          @"generated_password");

  // Extend the password after password generation.
  __block bool block_was_called = NO;
  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  FormSuggestionProviderQuery* extend_query =
      [[FormSuggestionProviderQuery alloc]
          initWithFormName:@"signup_form"
              uniqueFormID:FormRendererId(1)
           fieldIdentifier:@"pw"
             uniqueFieldID:FieldRendererId(3)
                 fieldType:@"password"
                      type:@"input"
                typedValue:@"generated_password_long"
                   frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:extend_query
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         block_was_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return block_was_called;
  }));
  // Check that the password is still considered generated.
  ASSERT_TRUE(passwordController_.sharedPasswordController.isPasswordGenerated);
}

// Tests that clearing the value of the password field containing
// the generated password stops the generation flow.
TEST_F(PasswordControllerTest, PasswordGenerationFieldClear) {
  LoadHtml(@"<html><body>"
            "<form name='login_form' id='signup_form'>"
            "  <input type='text' name='username' id='un'>"
            "  <input type='password' name='password' id='pw'>"
            "  <button id='submit_button' value='Submit'>"
            "</form>"
            "</body></html>");
  WaitForFormManagersCreation();

  InjectGeneratedPassword(FormRendererId(1), FieldRendererId(3),
                          @"generated_password");

  // Clear the password.
  __block bool block_was_called = NO;
  std::string mainFrameID = web::GetMainWebFrameId(web_state());
  FormSuggestionProviderQuery* clear_query =
      [[FormSuggestionProviderQuery alloc]
          initWithFormName:@"signup_form"
              uniqueFormID:FormRendererId(1)
           fieldIdentifier:@"pw"
             uniqueFieldID:FieldRendererId(3)
                 fieldType:@"password"
                      type:@"input"
                typedValue:@""
                   frameID:SysUTF8ToNSString(mainFrameID)];
  [passwordController_.sharedPasswordController
      checkIfSuggestionsAvailableForForm:clear_query
                          hasUserGesture:YES
                                webState:web_state()
                       completionHandler:^(BOOL success) {
                         block_was_called = YES;
                       }];
  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return block_was_called;
  }));
  // Check that the password is not considered generated anymore.
  ASSERT_FALSE(
      passwordController_.sharedPasswordController.isPasswordGenerated);
}

TEST_F(PasswordControllerTest, SavingPasswordsOutsideTheFormTag) {
  NSString* kHtml = @"<html><body>"
                     "<input type='text' name='username'>"
                     "<input type='password' name='pw'>"
                     "</body></html>";

  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

  LoadHtml(kHtml);
  WaitForFormManagersCreation();

  std::string main_frame_id = web::GetMainWebFrameId(web_state());
  SimulateUserTyping("", FormRendererId(), "username", FieldRendererId(1),
                     "user1", main_frame_id);
  SimulateUserTyping("", FormRendererId(), "pw", FieldRendererId(2),
                     "password1", main_frame_id);

  __block std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager)));

  // Simulate a renderer initiated navigation.
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsRendererInitiated(true);
  [passwordController_.sharedPasswordController webState:web_state()
                                     didFinishNavigation:&context];

  // Simulate a successful submission by loading the landing page without
  // a form.
  LoadHtml(@"<html><body>Login success page</body></html>");

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
    return form_manager != nullptr;
  }));
  EXPECT_EQ(u"user1", form_manager->GetPendingCredentials().username_value);
  EXPECT_EQ(u"password1", form_manager->GetPendingCredentials().password_value);
}

// Tests submission and saving of a password form located in a same origin
// iframe. The submission happens after clicking on a password form located
// in the main frame.
TEST_F(PasswordControllerTest,
       SubmittingAndSavingSameOriginIframeAfterClickingAnotherForm) {
  ON_CALL(*store_, GetLogins)
      .WillByDefault(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*weak_client_, PromptUserToSaveOrUpdatePasswordPtr)
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  LoadHtml(@""
            "<input id='un' type='text' name='u'>"
            "<input id='pw' type='password' name='p'>"
            "<iframe id='frame1' name='frame1'></iframe>");
  web::test::ExecuteJavaScript(
      @"document.getElementById('frame1').contentDocument.body.innerHTML = "
       "'<form id=\"form1\">"
       "<input type=\"text\" name=\"text\" value=\"user1\" id=\"id2\">"
       "<input type=\"password\" name=\"password\" value=\"pw1\" id=\"id2\">"
       "<input type=\"submit\" id=\"submit_input\"/>"
       "</form>'",
      web_state());
  web::test::ExecuteJavaScript(
      @"document.getElementById('un').click();"
      @"document.getElementById('frame1').contentDocument.getElementById('"
      @"submit_input').click();",
      web_state());

  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"));
  EXPECT_EQ("https://chromium.test/",
            form_manager_to_save->GetPendingCredentials().signon_realm);
  EXPECT_EQ(u"user1",
            form_manager_to_save->GetPendingCredentials().username_value);
  EXPECT_EQ(u"pw1",
            form_manager_to_save->GetPendingCredentials().password_value);

  auto* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_TRUE(form_manager->is_submitted());
  EXPECT_FALSE(form_manager->IsPasswordUpdate());
}

// Tests recording of PasswordManager.FillingAssistance metric with manual
// filling.
TEST_F(PasswordControllerTest, PasswordManagerManualFillingAssistanceMetric) {
  base::HistogramTester histogram_tester;

  PasswordForm form(CreatePasswordForm(BaseUrl().c_str(), "abc", "def"));
  EXPECT_CALL(*store_, GetLogins)
      .WillRepeatedly(WithArg<1>(InvokeConsumer(store_.get(), form)));

  LoadHtml(@""
            "<form id='ff'>"
            "  <input id='un' type='text'>"
            "  <input id='pw' type='password'>"
            "  <button id='submit_button' value='Submit'>"
            "</form>");

  WaitForFormManagersCreation();

  TestPasswordFormData test_data = {/*form_name=*/"ff",
                                    /*form_renderer_id=*/1,
                                    /*username_element=*/"un",
                                    /*username_renderer_id=*/2,
                                    /*password_element=*/"pw",
                                    /*password_renderer_id=*/3,
                                    /*user_value=*/"abc",
                                    /*password_value=*/"def",
                                    /*on_key_up=*/NO,
                                    /*on_change=*/NO};

  FillFormAndValidate(test_data, /*should_succeed=*/true,
                      web::GetMainFrame(web_state()));

  web::test::ExecuteJavaScript(
      @"var e = new UIEvent('touchend');"
       "document.getElementById('submit_button').dispatchEvent(e);",
      web_state());
  LoadHtmlWithRendererInitiatedNavigation(
      SysUTF8ToNSString("<html><body>Success</body></html>"));

  histogram_tester.ExpectUniqueSample("PasswordManager.FillingAssistance",
                                      FillingAssistance::kManual, 1);
}
