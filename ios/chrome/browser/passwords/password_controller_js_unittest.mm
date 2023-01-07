// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using web::WebFrame;

// Unit tests for
// components/password_manager/ios/resources/password_controller.js
namespace {

// Text fixture to test password controller.
class PasswordControllerJsTest : public PlatformTest {
 public:
  PasswordControllerJsTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  bool SetUpUniqueIDs() {
    __block web::WebFrame* main_frame = nullptr;
    bool success =
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
          main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
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
    success = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      return [web::test::ExecuteJavaScript(@"document[__gCrWeb.fill.ID_SYMBOL]",
                                           web_state()) intValue] ==
             int{next_available_id};
    });
    if (!success) {
      return false;
    }

    // Run password forms search to set up unique IDs.
    return FindPasswordForms() != nil;
  }

  // Finds all password forms in the window and returns for data as a JSON
  // string.
  NSString* FindPasswordForms() {
    // Run password forms search to set up unique IDs.
    __block bool complete = false;
    __block NSString* result = nil;
    password_manager::PasswordManagerJavaScriptFeature::GetInstance()
        ->FindPasswordFormsInFrame(web::GetMainFrame(web_state()),
                                   base::BindOnce(^(NSString* forms) {
                                     result = forms;
                                     complete = true;
                                   }));

    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      return complete;
    }));

    return result;
  }

  std::string BaseUrl() {
    web::URLVerificationTrustLevel unused_level;
    return web_state()->GetCurrentURL(&unused_level).spec();
  }

 protected:
  void SetUp() override { PlatformTest::SetUp(); }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
};

// IDs used in the Username and Password <input> elements.
NSString* const kEmailInputID = @"Email";
NSString* const kPasswordInputID = @"Passwd";

// Returns an autoreleased string of an HTML form that is similar to the
// Google Accounts sign in form. `email` may be nil if the form does not
// need to be pre-filled with the username. Use `isReadOnly` flag to indicate
// if the email field should be read-only.
NSString* GAIASignInForm(NSString* formAction,
                         NSString* email,
                         BOOL isReadOnly,
                         BOOL isDisabled) {
  return [NSString
      stringWithFormat:
          @"<html><body>"
           "<form novalidate action=\"%@\" "
           "id=\"gaia_loginform\">"
           "  <input name=\"GALX\" type=\"hidden\" value=\"abcdefghij\">"
           "  <input name=\"service\" type=\"hidden\" value=\"mail\">"
           "  <input id=\"%@\" name=\"Email\" type=\"email\" value=\"%@\" %@ "
           "%@>"
           "  <input id=\"%@\" name=\"Passwd\" type=\"password\" "
           "    placeholder=\"Password\">"
           "</form></body></html>",
          formAction, kEmailInputID, email ? email : @"",
          isReadOnly ? @"readonly" : @"", isDisabled ? @"disabled" : @"",
          kPasswordInputID];
}

// Returns an autoreleased string of JSON for a parsed form.
NSString* GAIASignInFormData(NSString* formOrigin, NSString* formName) {
  return [NSString
      stringWithFormat:
          @"{"
           "  \"origin\":\"%@\","
           "  \"name\":\"%@\","
           "  \"unique_renderer_id\":1,"
           "  \"fields\":["
           "    {\"name\":\"%@\", \"value\":\"\", \"unique_renderer_id\":2},"
           "    {\"name\":\"%@\",\"value\":\"\", \"unique_renderer_id\":3},"
           "  ]"
           "}",
          formOrigin, formName, kEmailInputID, kPasswordInputID];
}

// Loads a page with a password form containing a username value already.
// Checks that an attempt to fill in credentials with the same username
// succeeds.
TEST_F(PasswordControllerJsTest,
       FillPasswordFormWithPrefilledUsername_SucceedsWhenUsernameMatches) {
  const std::string origin = "https://accounts.google.com/ServiceLoginAuth";
  NSString* const formOrigin = [NSString stringWithUTF8String:origin.c_str()];
  NSString* const formName = @"gaia_loginform";
  NSString* const username = @"john.doe@gmail.com";
  NSString* const password = @"super!secret";
  web::test::LoadHtml(GAIASignInForm(formOrigin, username, /*isReadOnly=*/YES,
                                     /*isDisabled=*/NO),
                      GURL(origin), web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString stringWithFormat:
                        @"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                        GAIASignInFormData(formOrigin, formName), username,
                        password],
          web_state()));

  // Verifies that the sign-in form has been filled with username/password.
  NSString* email_js = [NSString
      stringWithFormat:@"document.getElementById('%@').value", kEmailInputID];
  EXPECT_NSEQ(username, web::test::ExecuteJavaScript(email_js, web_state()));

  NSString* password_js =
      [NSString stringWithFormat:@"document.getElementById('%@').value",
                                 kPasswordInputID];
  EXPECT_NSEQ(password, web::test::ExecuteJavaScript(password_js, web_state()));
}

// Loads a page with a password form containing a username value already.
// Checks that an attempt to fill in credentials with a different username
// succeeds, as long as the field is writeable.
TEST_F(PasswordControllerJsTest,
       FillPasswordFormWithPrefilledUsername_SucceedsByOverridingUsername) {
  const std::string origin = "https://accounts.google.com/ServiceLoginAuth";
  NSString* const formOrigin = [NSString stringWithUTF8String:origin.c_str()];
  NSString* const formName = @"gaia_loginform";
  NSString* const username1 = @"john.doe@gmail.com";
  NSString* const username2 = @"jane.doe@gmail.com";
  NSString* const password = @"super!secret";
  web::test::LoadHtml(GAIASignInForm(formOrigin, username1, /*isReadOnly=*/NO,
                                     /*isDisabled=*/NO),
                      GURL(origin), web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString stringWithFormat:
                        @"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                        GAIASignInFormData(formOrigin, formName), username2,
                        password],
          web_state()));
  // Verifies that the sign-in form has been filled with the new username
  // and password.

  NSString* email_js = [NSString
      stringWithFormat:@"document.getElementById('%@').value", kEmailInputID];
  EXPECT_NSEQ(username2, web::test::ExecuteJavaScript(email_js, web_state()));

  NSString* password_js =
      [NSString stringWithFormat:@"document.getElementById('%@').value",
                                 kPasswordInputID];
  EXPECT_NSEQ(password, web::test::ExecuteJavaScript(password_js, web_state()));
}

// Loads a page with a password form containing a disabled input with username
// value already. Checks that an attempt to fill in credentials succeeds, and
// the password is filled.
TEST_F(PasswordControllerJsTest,
       FillPasswordFormWithPrefilledUsername_SucceedsWithDisabledUsername) {
  const std::string origin = "https://accounts.google.com/ServiceLoginAuth";
  NSString* const formOrigin = [NSString stringWithUTF8String:origin.c_str()];
  NSString* const formName = @"gaia_loginform";
  NSString* const username = @"john.doe@gmail.com";
  NSString* const password = @"super!secret";
  web::test::LoadHtml(GAIASignInForm(formOrigin, username, /*isReadOnly=*/NO,
                                     /*isDisabled=*/YES),
                      GURL(origin), web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString stringWithFormat:
                        @"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                        GAIASignInFormData(formOrigin, formName), username,
                        password],
          web_state()));

  // Verifies that the sign-in form has been filled with password and username
  // value remained the same.
  NSString* email_js = [NSString
      stringWithFormat:@"document.getElementById('%@').value", kEmailInputID];
  EXPECT_NSEQ(username, web::test::ExecuteJavaScript(email_js, web_state()));

  NSString* password_js =
      [NSString stringWithFormat:@"document.getElementById('%@').value",
                                 kPasswordInputID];
  EXPECT_NSEQ(password, web::test::ExecuteJavaScript(password_js, web_state()));
}

// Loads a page with a password form containing a disabled input with the value
// that doesn't match credential username. Checks that an attempt to fill in
// credentials succeeds, and the password is filled, and username value remained
// the same.
TEST_F(
    PasswordControllerJsTest,
    FillPasswordFormWithPrefilledUsername_SucceedsWithDisabledUsernameWithAnotherValue) {
  const std::string origin = "https://accounts.google.com/ServiceLoginAuth";
  NSString* const formOrigin = [NSString stringWithUTF8String:origin.c_str()];
  NSString* const formName = @"gaia_loginform";
  NSString* const initialInputValue = @"dummy_username_field";
  NSString* const username = @"john.doe@gmail.com";
  NSString* const password = @"super!secret";
  web::test::LoadHtml(
      GAIASignInForm(formOrigin, initialInputValue, /*isReadOnly=*/NO,
                     /*isDisabled=*/YES),
      GURL(origin), web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString stringWithFormat:
                        @"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                        GAIASignInFormData(formOrigin, formName), username,
                        password],
          web_state()));

  // Verifies that the sign-in form has been filled with password and username
  // value remained the same.
  NSString* email_js = [NSString
      stringWithFormat:@"document.getElementById('%@').value", kEmailInputID];
  EXPECT_NSEQ(initialInputValue,
              web::test::ExecuteJavaScript(email_js, web_state()));

  NSString* password_js =
      [NSString stringWithFormat:@"document.getElementById('%@').value",
                                 kPasswordInputID];
  EXPECT_NSEQ(password, web::test::ExecuteJavaScript(password_js, web_state()));
}

// Check that one password form is identified and serialized correctly.
TEST_F(PasswordControllerJsTest,
       FindAndPreparePasswordFormsSingleFrameSingleForm) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form action='/generic_submit' method='post' name='login_form'>"
       "  Name: <input type='text' name='name'>"
       "  Password: <input type='password' name='password'>"
       "  <input type='submit' value='Submit'>"
       "</form>"
       "</body></html>",
      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  const std::string base_url = BaseUrl();
  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  std::string mainFrameID = main_frame->GetFrameId();
  NSString* result = [NSString
      stringWithFormat:
          @"[{\"name\":\"login_form\",\"origin\":\"%s\",\"action\":\"https://"
          @"chromium.test/generic_submit\",\"name_attribute\":\"login_form\","
          @"\"id_attribute\":\"\",\"unique_renderer_id\":\"1\","
          @"\"frame_id\":\"%s\","
          @"\"fields\":[{"
          @"\"identifier\":\"name\","
          @"\"name\":\"name\",\"name_attribute\":\"name\",\"id_attribute\":"
          @"\"\",\"unique_renderer_id\":\"2\",\"form_control_type\":\"text\","
          @"\"aria_label\":\"\","
          @"\"aria_description\":\"\",\"should_autocomplete\":true,"
          @"\"is_focusable\":true,\"max_length\":524288,\"is_checkable\":false,"
          @"\"value\":\"\",\"label\":\"Name:\"},{\"identifier\":"
          @"\"password\",\"name\":\"password\",\"name_attribute\":\"password\","
          @"\"id_attribute\":\"\",\"unique_renderer_id\":\"3\",\"form_control_"
          @"type\":\"password\","
          @"\"aria_label\":\"\",\"aria_description\":\"\","
          @"\"should_autocomplete\":true,\"is_focusable\":true,"
          @"\"max_length\":524288,\"is_checkable\":false,\"value\":\"\","
          @"\"label\":\"Password:\"}]}]",
          base_url.c_str(), mainFrameID.c_str()];
  EXPECT_NSEQ(result, FindPasswordForms());
}

// Check that multiple password forms are identified and serialized correctly.
TEST_F(PasswordControllerJsTest,
       FindAndPreparePasswordFormsSingleFrameMultipleForms) {
  web::test::LoadHtml(@"<html><body>"
                       "<form action='/generic_submit' id='login_form1'>"
                       "  Name: <input type='text' name='name'>"
                       "  Password: <input type='password' name='password'>"
                       "  <input type='submit' value='Submit'>"
                       "</form>"
                       "<form action='/generic_s2' name='login_form2'>"
                       "  Name: <input type='text' name='name2'>"
                       "  Password: <input type='password' name='password2'>"
                       "  <input type='submit' value='Submit'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  const std::string base_url = BaseUrl();
  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  std::string mainFrameID = main_frame->GetFrameId();
  NSString* result = [NSString
      stringWithFormat:
          @"[{\"name\":\"login_form1\",\"origin\":\"%s\",\"action\":\"%s"
          @"generic_submit\",\"name_attribute\":\"\",\"id_attribute\":"
          @"\"login_form1\",\"unique_renderer_id\":\"1\","
          @"\"frame_id\":\"%s\","
          @"\"fields\":[{"
          @"\"identifier\":\"name\","
          @"\"name\":\"name\",\"name_attribute\":\"name\",\"id_attribute\":"
          @"\"\",\"unique_renderer_id\":\"2\",\"form_control_type\":\"text\","
          @"\"aria_label\":\"\","
          @"\"aria_description\":\"\",\"should_autocomplete\":"
          @"true,\"is_focusable\":true,\"max_length\":524288,\"is_checkable\":"
          @"false,\"value\":\"\",\"label\":\"Name:\"},{\"identifier\":"
          @"\"password\",\"name\":\"password\",\"name_attribute\":\"password\","
          @"\"id_attribute\":\"\",\"unique_renderer_id\":\"3\",\"form_control_"
          @"type\":\"password\","
          @"\"aria_label\":\"\",\"aria_description\":\"\","
          @"\"should_autocomplete\":true,\"is_focusable\":true,"
          @"\"max_length\":524288,\"is_checkable\":false,\"value\":\"\","
          @"\"label\":\"Password:\"}]},{\"name\":\"login_form2\",\"origin\":"
          @"\"https://chromium.test/\",\"action\":\"https://chromium.test/"
          @"generic_s2\",\"name_attribute\":\"login_form2\","
          @"\"id_attribute\":\"\",\"unique_renderer_id\":\"4\","
          @"\"frame_id\":\"%s\","
          @"\"fields\":[{"
          @"\"identifier\":\"name2\","
          @"\"name\":\"name2\",\"name_attribute\":\"name2\",\"id_attribute\":"
          @"\"\",\"unique_renderer_id\":\"5\",\"form_control_type\":\"text\","
          @"\"aria_label\":\"\","
          @"\"aria_description\":\"\",\"should_autocomplete\":"
          @"true,\"is_focusable\":true,\"max_length\":524288,\"is_checkable\":"
          @"false,\"value\":\"\",\"label\":\"Name:\"},{\"identifier\":"
          @"\"password2\",\"name\":\"password2\",\"name_attribute\":"
          @"\"password2\",\"id_attribute\":\"\",\"unique_renderer_id\":\"6\","
          @"\"form_control_type\":"
          @"\"password\",\"aria_label\":\"\",\"aria_description\":\"\","
          @"\"should_autocomplete\":true,\"is_focusable\":true,"
          @"\"max_length\":524288,\"is_checkable\":false,"
          @"\"value\":\"\","
          @"\"label\":\"Password:\"}]}]",
          base_url.c_str(), base_url.c_str(), mainFrameID.c_str(),
          mainFrameID.c_str()];

  EXPECT_NSEQ(result, FindPasswordForms());
}

// Test serializing of password forms.
TEST_F(PasswordControllerJsTest, GetPasswordFormData) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='np' id='np1' action='/generic_submit'>"
                       "  Name: <input type='text' name='name'>"
                       "  Password: <input type='password' name='password'>"
                       "  <input type='submit' value='Submit'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  const std::string base_url = BaseUrl();
  NSString* parameter = @"window.document.getElementsByTagName('form')[0]";

  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  std::string mainFrameID = main_frame->GetFrameId();
  NSString* result = [NSString
      stringWithFormat:
          @"{\"name\":\"np\",\"origin\":\"%s\",\"action\":\"%sgeneric_submit\","
          @"\"name_attribute\":\"np\",\"id_attribute\":\"np1\",\"unique_"
          @"renderer_id\":\"1\","
          @"\"frame_id\":\"%s\","
          @"\"fields\":[{\"identifier\":\"name\",\"name\":\"name\","
          @"\"name_attribute\":\"name\",\"id_attribute\":\"\",\"unique_"
          @"renderer_id\":\"2\",\"form_"
          @"control_type\":\"text\",\"aria_label\":\"\","
          @"\"aria_description\":\"\",\"should_autocomplete\":true,\"is_"
          @"focusable\":true,\"max_length\":524288,\"is_checkable\":false,"
          @"\"value\":\"\",\"label\":\"Name:\"},{\"identifier\":\"password\","
          @"\"name\":\"password\",\"name_attribute\":\"password\","
          @"\"id_attribute\":\"\",\"unique_renderer_id\":\"3\",\"form_control_"
          @"type\":\"password\","
          @"\"aria_label\":\"\",\"aria_description\":\"\","
          @"\"should_autocomplete\":true,\"is_focusable\":true,"
          @"\"max_length\":524288,"
          @"\"is_checkable\":false,\"value\":\"\",\"label\":\"Password:\"}]}",
          base_url.c_str(), base_url.c_str(), mainFrameID.c_str()];

  EXPECT_NSEQ(
      result,
      web::test::ExecuteJavaScript(
          [NSString stringWithFormat:@"__gCrWeb.stringify(__gCrWeb.passwords."
                                     @"getPasswordFormData(%@, window))",
                                     parameter],
          web_state()));
}

// Check that if a form action is not set then the action is parsed to the
// current url.
TEST_F(PasswordControllerJsTest, FormActionIsNotSet) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='login_form'>"
                       "  Name: <input type='text' name='name'>"
                       "  Password: <input type='password' name='password'>"
                       "  <input type='submit' value='Submit'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  const std::string base_url = BaseUrl();
  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  std::string mainFrameID = main_frame->GetFrameId();
  NSString* result = [NSString
      stringWithFormat:
          @"[{\"name\":\"login_form\",\"origin\":\"%s\",\"action\":\"%s\","
          @"\"name_attribute\":\"login_form\",\"id_attribute\":\"\",\"unique_"
          @"renderer_id\":\"1\","
          @"\"frame_id\":\"%s\","
          @"\"fields\":[{\"identifier\":\"name\",\"name\":\"name\","
          @"\"name_attribute\":\"name\",\"id_attribute\":\"\",\"unique_"
          @"renderer_id\":\"2\",\"form_"
          @"control_type\":\"text\",\"aria_label\":\"\","
          @"\"aria_description\":\"\",\"should_autocomplete\":true,\"is_"
          @"focusable\":true,\"max_length\":524288,\"is_checkable\":false,"
          @"\"value\":\"\",\"label\":\"Name:\"},{\"identifier\":\"password\","
          @"\"name\":\"password\",\"name_attribute\":\"password\","
          @"\"id_attribute\":\"\",\"unique_renderer_id\":\"3\",\"form_control_"
          @"type\":\"password\","
          @"\"aria_label\":\"\",\"aria_description\":\"\","
          @"\"should_autocomplete\":true,\"is_focusable\":true,"
          @"\"max_length\":524288,"
          @"\"is_checkable\":false,\"value\":\"\",\"label\":\"Password:\"}]}]",
          base_url.c_str(), base_url.c_str(), mainFrameID.c_str()];
  EXPECT_NSEQ(result, FindPasswordForms());
}

// Checks that a touchend event from a button which contains in a password form
// works as a submission indicator for this password form.
TEST_F(PasswordControllerJsTest, TouchendAsSubmissionIndicator) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='login_form' id='login_form'>"
                       "  Name: <input type='text' name='username'>"
                       "  Password: <input type='password' name='password'>"
                       "  <button id='submit_button' value='Submit'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  // Call __gCrWeb.passwords.findPasswordForms in order to set an event handler
  // on the button touchend event.
  FindPasswordForms();

  // Replace __gCrWeb.message.invokeOnHost with mock method for checking of call
  // arguments.
  web::test::ExecuteJavaScript(
      @"var invokeOnHostArgument = null;"
       "var invokeOnHostCalls = 0;"
       "__gCrWeb.message.invokeOnHost = function(command) {"
       "  invokeOnHostArgument = command;"
       "  invokeOnHostCalls++;"
       "}",
      web_state());

  // Simulate touchend event on the button.
  web::test::ExecuteJavaScript(
      @"document.getElementsByName('username')[0].value = 'user1';"
       "document.getElementsByName('password')[0].value = 'password1';"
       "var e = new UIEvent('touchend');"
       "document.getElementsByTagName('button')[0].dispatchEvent(e);",
      web_state());

  // Check that there was only 1 call for invokeOnHost.
  EXPECT_NSEQ(@1,
              web::test::ExecuteJavaScript(@"invokeOnHostCalls", web_state()));

  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  std::string mainFrameID = main_frame->GetFrameId();
  NSString* expected_command = [NSString
      stringWithFormat:
          @"{\"name\":\"login_form\",\"origin\":\"https://chromium.test/"
          @"\",\"action\":\"%s\",\"name_attribute\":\"login_form\","
          @"\"id_attribute\":\"login_form\",\"unique_renderer_id\":\"1\","
          @"\"frame_id\":\"%s\","
          @"\"fields\":"
          @"[{\"identifier\":\"username\","
          @"\"name\":\"username\",\"name_attribute\":\"username\","
          @"\"id_attribute\":\"\",\"unique_renderer_id\":\"2\",\"form_control_"
          @"type\":\"text\","
          @"\"aria_label\":\"\",\"aria_description\":\"\","
          @"\"should_autocomplete\":true,\"is_focusable\":true,"
          @"\"max_length\":524288,"
          @"\"is_checkable\":false,\"value\":\"user1\",\"label\":\"Name:\"},{"
          @"\"identifier\":\"password\",\"name\":\"password\","
          @"\"name_attribute\":\"password\",\"id_attribute\":\"\",\"unique_"
          @"renderer_id\":\"3\","
          @"\"form_control_type\":\"password\","
          @"\"aria_label\":\"\",\"aria_description\":\"\","
          @"\"should_autocomplete\":true,"
          @"\"is_focusable\":true,\"max_length\":524288,\"is_checkable\":false,"
          @"\"value\":\"password1\",\"label\":\"Password:\"}],"
          @"\"command\":\"passwordForm.submitButtonClick\"}",
          BaseUrl().c_str(), mainFrameID.c_str()];

  // Check that invokeOnHost was called with the correct argument.
  EXPECT_NSEQ(expected_command,
              web::test::ExecuteJavaScript(
                  @"__gCrWeb.stringify(invokeOnHostArgument)", web_state()));
}

// Check that a form is filled if url of a page and url in form fill data are
// different only in pathes.
TEST_F(PasswordControllerJsTest, OriginsAreDifferentInPathes) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Name: <input type='text' name='name' id='name'>"
       "  Password: <input type='password' name='password' id='password'>"
       "  <input type='submit' value='Submit'>"
       "</form>"
       "</body></html>",
      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* const username = @"john.doe@gmail.com";
  NSString* const password = @"super!secret";
  std::string page_origin = BaseUrl() + "origin1";
  std::string form_fill_data_origin = BaseUrl() + "origin2";

  NSString* form_fill_data = [NSString
      stringWithFormat:@"{"
                        "  \"action\":\"%s\","
                        "  \"origin\":\"%s\","
                        "  \"name\":\"login_form\","
                        "  \"unique_renderer_id\":1,"
                        "  \"fields\":["
                        "    {\"name\":\"name\", \"value\":\"name\", "
                        "\"unique_renderer_id\":2},"
                        "    {\"name\":\"password\",\"value\":\"password\", "
                        "\"unique_renderer_id\":3}"
                        "  ]"
                        "}",
                       page_origin.c_str(), form_fill_data_origin.c_str()];
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString stringWithFormat:
                        @"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                        form_fill_data, username, password],
          web_state()));
  // Verifies that the sign-in form has been filled with username/password.
  EXPECT_NSEQ(username,
              web::test::ExecuteJavaScript(
                  @"document.getElementById('name').value", web_state()));
  EXPECT_NSEQ(password,
              web::test::ExecuteJavaScript(
                  @"document.getElementById('password').value", web_state()));
}

// Check that when instructed to fill a form named "bar", a form named "foo"
// is not filled with generated password.
TEST_F(PasswordControllerJsTest,
       FillPasswordFormWithGeneratedPassword_FailsWhenFormNotFound) {
  web::test::LoadHtml(@"<html>"
                       "  <body>"
                       "    <form name=\"foo\">"
                       "      <input type=\"password\" id=\"ps1\" name=\"ps\">"
                       "    </form>"
                       "  </body"
                       "</html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  uint32_t formIdentifier = 404;
  NSString* const password = @"abc";
  uint32_t newPasswordIdentifier = 2;
  EXPECT_NSEQ(
      @NO,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:
                  @"__gCrWeb.passwords."
                  @"fillPasswordFormWithGeneratedPassword(%d, %d, %d, '%@')",
                  formIdentifier, newPasswordIdentifier, 0, password],
          web_state()));
}

// Check that filling a form without password fields fails.
TEST_F(PasswordControllerJsTest,
       FillPasswordFormWithGeneratedPassword_FailsWhenNoPasswordFields) {
  web::test::LoadHtml(@"<html>"
                       "  <body>"
                       "    <form name=\"foo\">"
                       "      <input type=\"text\" name=\"user\">"
                       "      <input type=\"submit\" name=\"go\">"
                       "    </form>"
                       "  </body"
                       "</html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  uint32_t formIdentifier = 1;
  NSString* const password = @"abc";
  uint32_t const newPasswordIdentifier = 3;
  uint32_t const confirmPasswordIdentifier = 4;
  EXPECT_NSEQ(
      @NO,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:
                  @"__gCrWeb.passwords."
                  @"fillPasswordFormWithGeneratedPassword(%d, %d, %d, '%@')",
                  formIdentifier, newPasswordIdentifier,
                  confirmPasswordIdentifier, password],
          web_state()));
}

// Check that a matching and complete password form is successfully filled
// with the generated password.
TEST_F(PasswordControllerJsTest,
       FillPasswordFormWithGeneratedPassword_SucceedsWhenFieldsFilled) {
  web::test::LoadHtml(@"<html>"
                       "  <body>"
                       "    <form name=\"foo\">"
                       "      <input type=\"text\" id=\"user\" name=\"user\">"
                       "      <input type=\"password\" id=\"ps1\" name=\"ps1\">"
                       "      <input type=\"password\" id=\"ps2\" name=\"ps2\">"
                       "      <input type=\"submit\" name=\"go\">"
                       "    </form>"
                       "  </body"
                       "</html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  uint32_t formIdentifier = 1;
  NSString* const password = @"abc";
  uint32_t const newPasswordIdentifier = 3;
  uint32_t const confirmPasswordIdentifier = 4;
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:
                  @"__gCrWeb.passwords."
                  @"fillPasswordFormWithGeneratedPassword(%u, %u, %u, '%@')",
                  formIdentifier, newPasswordIdentifier,
                  confirmPasswordIdentifier, password],
          web_state()));
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('ps1').value == '%@'",
                               password],
          web_state()));
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('ps2').value == '%@'",
                               password],
          web_state()));
  EXPECT_NSEQ(
      @NO,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('user').value == '%@'",
                               password],
          web_state()));
}

// Check that a matching and complete password field is successfully filled
// with the generated password and that confirm field is untouched.
TEST_F(
    PasswordControllerJsTest,
    FillPasswordFormWithGeneratedPassword_SucceedsWhenOnlyNewPasswordFilled) {
  web::test::LoadHtml(@"<html>"
                       "  <body>"
                       "    <form name=\"foo\">"
                       "      <input type=\"text\" id=\"user\" name=\"user\">"
                       "      <input type=\"password\" id=\"ps1\" name=\"ps1\">"
                       "      <input type=\"password\" id=\"ps2\" name=\"ps2\">"
                       "      <input type=\"submit\" name=\"go\">"
                       "    </form>"
                       "  </body"
                       "</html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  uint32_t formIdentifier = 1;
  NSString* const password = @"abc";
  uint32_t const newPasswordIdentifier = 3;
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:
                  @"__gCrWeb.passwords."
                  @"fillPasswordFormWithGeneratedPassword(%u, %u, %u, '%@')",
                  formIdentifier, newPasswordIdentifier, 0, password],
          web_state()));
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('ps1').value == '%@'",
                               password],
          web_state()));
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('ps2').value == '%@'",
                               @""],
          web_state()));
  EXPECT_NSEQ(
      @NO,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('user').value == '%@'",
                               password],
          web_state()));
}

// Check that a matching and complete confirm password field is successfully
// filled with the generated password and that new password field is untouched.
TEST_F(
    PasswordControllerJsTest,
    FillPasswordFormWithGeneratedPassword_FailsWhenOnlyConfirmPasswordFilled) {
  web::test::LoadHtml(@"<html>"
                       "  <body>"
                       "    <form name=\"foo\">"
                       "      <input type=\"text\" id=\"user\" name=\"user\">"
                       "      <input type=\"password\" id=\"ps1\" name=\"ps1\">"
                       "      <input type=\"password\" id=\"ps2\" name=\"ps2\">"
                       "      <input type=\"submit\" name=\"go\">"
                       "    </form>"
                       "  </body"
                       "</html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  uint32_t formIdentifier = 1;
  NSString* const password = @"abc";
  uint32_t const confirmPasswordIdentifier = 4;
  EXPECT_NSEQ(
      @NO,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:
                  @"__gCrWeb.passwords."
                  @"fillPasswordFormWithGeneratedPassword(%u, %u, %u, '%@')",
                  formIdentifier, 0, confirmPasswordIdentifier, password],
          web_state()));
  EXPECT_NSEQ(@YES,
              web::test::ExecuteJavaScript(
                  @"document.getElementById('ps1').value == ''", web_state()));
  EXPECT_NSEQ(@YES,
              web::test::ExecuteJavaScript(
                  @"document.getElementById('ps2').value == ''", web_state()));
  EXPECT_NSEQ(@YES,
              web::test::ExecuteJavaScript(
                  @"document.getElementById('user').value == ''", web_state()));
}

// Check that unknown or null identifiers are handled gracefully.
TEST_F(
    PasswordControllerJsTest,
    FillPasswordFormWithGeneratedPassword_SucceedsOnUnknownOrNullIdentifiers) {
  web::test::LoadHtml(@"<html>"
                       "  <body>"
                       "    <form name=\"foo\">"
                       "      <input type=\"text\" id=\"user\" name=\"user\">"
                       "      <input type=\"password\" id=\"ps1\" name=\"ps1\">"
                       "      <input type=\"password\" id=\"ps2\" name=\"ps2\">"
                       "      <input type=\"submit\" name=\"go\">"
                       "    </form>"
                       "  </body"
                       "</html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  uint32_t formIdentifier = 1;
  NSString* const password = @"abc";
  EXPECT_NSEQ(
      @NO, web::test::ExecuteJavaScript(
               [NSString stringWithFormat:@"__gCrWeb.passwords."
                                          @"fillPasswordFormWithGeneratedPasswo"
                                          @"rd(%u, '%@', null, '%@')",
                                          formIdentifier, @"hello", password],
               web_state()));
  EXPECT_NSEQ(@YES,
              web::test::ExecuteJavaScript(
                  @"document.getElementById('ps1').value == ''", web_state()));
  EXPECT_NSEQ(@YES,
              web::test::ExecuteJavaScript(
                  @"document.getElementById('ps2').value == ''", web_state()));
  EXPECT_NSEQ(
      @NO,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('user').value == '%@'",
                               password],
          web_state()));
}

// Check that a matching and complete password form is successfully filled
// with the generated password.
TEST_F(PasswordControllerJsTest,
       FillPasswordFormWithGeneratedPassword_SucceedsOutsideFormTag) {
  web::test::LoadHtml(@"<html>"
                       "  <body>"
                       "    <input type=\"text\" id=\"user\" name=\"user\">"
                       "    <input type=\"password\" id=\"ps1\" name=\"ps1\">"
                       "    <input type=\"password\" id=\"ps2\" name=\"ps2\">"
                       "    <input type=\"submit\" name=\"go\">"
                       "  </body>"
                       "</html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* const password = @"abc";
  uint32_t const newPasswordIdentifier = 2;
  uint32_t const confirmPasswordIdentifier = 3;
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:
                  @"__gCrWeb.passwords."
                  @"fillPasswordFormWithGeneratedPassword(0, %u, %u, '%@')",
                  newPasswordIdentifier, confirmPasswordIdentifier, password],
          web_state()));
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('ps1').value == '%@'",
                               password],
          web_state()));
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('ps2').value == '%@'",
                               password],
          web_state()));
  EXPECT_NSEQ(
      @NO,
      web::test::ExecuteJavaScript(
          [NSString
              stringWithFormat:@"document.getElementById('user').value == '%@'",
                               password],
          web_state()));
}

// Check that a form with only password field (i.e. w/o username) is filled.
TEST_F(PasswordControllerJsTest, FillOnlyPasswordField) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Password: <input type='password' name='password' id='password'>"
       "</form>"
       "</body></html>",
      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* const password = @"super!secret";
  std::string page_origin = BaseUrl() + "origin1";
  std::string form_fill_data_origin = BaseUrl() + "origin2";

  NSString* form_fill_data = [NSString
      stringWithFormat:
          @"{"
           "  \"action\":\"%s\","
           "  \"origin\":\"%s\","
           "  \"name\":\"login_form\","
           "  \"unique_renderer_id\":1,"
           "  \"fields\":["
           "    {\"name\":\"\", \"value\":\"\", \"unique_renderer_id\":0},"
           "    {\"name\":\"password\",\"value\":\"password\", "
           "\"unique_renderer_id\":2}"
           "  ]"
           "}",
          page_origin.c_str(), form_fill_data_origin.c_str()];
  EXPECT_NSEQ(
      @YES,
      web::test::ExecuteJavaScript(
          [NSString stringWithFormat:
                        @"__gCrWeb.passwords.fillPasswordForm(%@, '', '%@')",
                        form_fill_data, password],
          web_state()));
  // Verifies that the sign-in form has been filled with `password`.
  EXPECT_NSEQ(password,
              web::test::ExecuteJavaScript(
                  @"document.getElementById('password').value", web_state()));
}

// Check that password form outside the <form> tag is extracted correctly.
TEST_F(PasswordControllerJsTest, ExtractFormOutsideTheFormTag) {
  web::test::LoadHtml(@"<html><body>"
                       "  Name: <input type='text' name='name'>"
                       "  Password: <input type='password' name='password'>"
                       "  <input type='submit' value='Submit'>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  const std::string base_url = BaseUrl();
  NSString* result = [NSString
      stringWithFormat:
          @"{\"name\":\"\",\"origin\":\"%s\",\"action\":\"\","
          @"\"is_form_tag\":false,\"fields\":[{"
          @"\"identifier\":\"gChrome~field~~INPUT~0\","
          @"\"name\":\"name\",\"name_attribute\":\"name\",\"id_attribute\":"
          @"\"\",\"unique_renderer_id\":\"1\",\"form_control_type\":\"text\","
          @"\"aria_label\":\"\","
          @"\"aria_description\":\"\",\"should_autocomplete\":true,"
          @"\"is_focusable\":true,\"max_length\":524288,\"is_checkable\":false,"
          @"\"value\":\"\",\"label\":\"Name:\"},{\"identifier\":"
          @"\"gChrome~field~~INPUT~1\",\"name\":\"password\",\"name_"
          @"attribute\":\"password\","
          @"\"id_attribute\":\"\",\"unique_renderer_id\":\"2\",\"form_control_"
          @"type\":\"password\","
          @"\"aria_label\":\"\",\"aria_description\":\"\","
          @"\"should_autocomplete\":true,\"is_focusable\":true,"
          @"\"max_length\":524288,\"is_checkable\":false,\"value\":\"\","
          @"\"label\":\"Password:\"}]}",
          base_url.c_str()];
  EXPECT_NSEQ(result, web::test::ExecuteJavaScript(
                          @"__gCrWeb.passwords.getPasswordFormDataAsString(0)",
                          web_state()));
}

}  // namespace
