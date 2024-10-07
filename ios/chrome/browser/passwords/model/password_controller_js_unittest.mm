// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <string>

#import "base/json/json_string_value_serializer.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::SysUTF8ToNSString;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using ::testing::IsTrue;

// Unit tests for
// components/password_manager/ios/resources/password_controller.js
namespace {

// Serializes a dictionary value in a NSString.
NSString* SerializeDictValueToNSString(const base::Value::Dict& value) {
  std::string output;
  JSONStringValueSerializer serializer(&output);
  EXPECT_TRUE(serializer.Serialize(value));
  return base::SysUTF8ToNSString(output);
}

base::Value::Dict ParsedField(std::string renderer_id,
                              std::string control_type,
                              std::string identifier,
                              std::string value,
                              std::string label,
                              std::string name) {
  base::Value::Dict field = base::Value::Dict()
                                .Set("identifier", identifier)
                                .Set("name", name)
                                .Set("name_attribute", name)
                                .Set("id_attribute", "")
                                .Set("renderer_id", renderer_id)
                                .Set("form_control_type", control_type)
                                .Set("aria_label", "")
                                .Set("aria_description", "")
                                .Set("should_autocomplete", true)
                                .Set("is_focusable", true)
                                .Set("is_user_edited", true)
                                .Set("max_length", 524288)
                                .Set("is_checkable", false)
                                .Set("value", value)
                                .Set("label", label)
                                .Set("placeholder_attribute", "");
  return field;
}

// Returns the fill result payload when filling failed.
base::Value FillResultForFailure() {
  return base::Value(base::Value::Dict()
                         .Set("didAttemptFill", base::Value(false))
                         .Set("didFillUsername", base::Value(false))
                         .Set("didFillPassword", base::Value(false)));
}

// Returns the fill result payload when filling succeeded.
base::Value FillResultForSuccess(bool did_fill_username,
                                 bool did_fill_password) {
  return base::Value(
      base::Value::Dict()
          .Set("didAttemptFill", base::Value(true))
          .Set("didFillUsername", base::Value(did_fill_username))
          .Set("didFillPassword", base::Value(did_fill_password)));
}

std::unique_ptr<base::Value> ParseFormFillResult(id wk_result) {
  base::Value::Dict parsed_result;
  if (wk_result[@"didAttemptFill"]) {
    parsed_result.Set(
        "didAttemptFill",
        static_cast<bool>([wk_result[@"didAttemptFill"] boolValue]));
  }
  if (wk_result[@"didFillUsername"]) {
    parsed_result.Set(
        "didFillUsername",
        static_cast<bool>([wk_result[@"didFillUsername"] boolValue]));
  }
  if (wk_result[@"didFillPassword"]) {
    parsed_result.Set(
        "didFillPassword",
        static_cast<bool>([wk_result[@"didFillPassword"] boolValue]));
  }
  return std::make_unique<base::Value>(std::move(parsed_result));
}

// Text fixture to test password controller.
class PasswordControllerJsTest : public PlatformTest {
 public:
  PasswordControllerJsTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  bool SetUpUniqueIDs() {
    __block web::WebFrame* main_frame = nullptr;
    bool success =
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
          main_frame = GetMainWebFrame();
          return main_frame != nullptr;
        });
    if (!success) {
      return false;
    }
    DCHECK(main_frame);

    // Run password forms search to set up unique IDs.
    return FindPasswordForms() != nil;
  }

  web::WebFrame* GetMainWebFrame() {
    password_manager::PasswordManagerJavaScriptFeature* feature =
        password_manager::PasswordManagerJavaScriptFeature::GetInstance();
    return feature->GetWebFramesManager(web_state())->GetMainWebFrame();
  }

  // Finds all password forms in the window and returns for data as a JSON
  // string.
  NSString* FindPasswordForms() {
    web::WebFrame* main_frame = GetMainWebFrame();
    // Run password forms search to set up unique IDs.
    __block bool complete = false;
    __block NSString* result = nil;
    password_manager::PasswordManagerJavaScriptFeature::GetInstance()
        ->FindPasswordFormsInFrame(main_frame,
                                   base::BindOnce(^(NSString* forms) {
                                     result = forms;
                                     complete = true;
                                   }));

    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      return complete;
    }));

    return result;
  }

  std::string BaseUrl() { return web_state()->GetLastCommittedURL().spec(); }

  NSString* PageOrigin() { return SysUTF8ToNSString(BaseUrl() + "origin1"); }

  NSString* FormOrigin() { return SysUTF8ToNSString(BaseUrl() + "origin2"); }

  // Returns fill data for a form with prefilled values. Will leave the fill
  // data empty for the field if its renderer id is 0. Make sure that the names
  // used here correspond to the ones in the HTML content.
  base::Value::Dict FormFillData(int username_renderer_id,
                                 int password_renderer_id) {
    auto fill_data = base::Value::Dict()
                         .Set("action", base::SysNSStringToUTF8(PageOrigin()))
                         .Set("origin", base::SysNSStringToUTF8(FormOrigin()))
                         .Set("name", "login_form")
                         .Set("renderer_id", 1);

    auto fields = base::Value::List();

    if (username_renderer_id) {
      fields.Append(base::Value::Dict()
                        .Set("name", "username")
                        .Set("value", "username")
                        .Set("renderer_id", username_renderer_id));
    } else {
      fields.Append(base::Value::Dict()
                        .Set("name", "")
                        .Set("value", "")
                        .Set("renderer_id", 0));
    }

    if (password_renderer_id) {
      fields.Append(base::Value::Dict()
                        .Set("name", "password")
                        .Set("value", "password")
                        .Set("renderer_id", password_renderer_id));
    } else {
      fields.Append(base::Value::Dict()
                        .Set("name", "")
                        .Set("value", "")
                        .Set("renderer_id", 0));
    }

    fill_data.Set("fields", std::move(fields));
    return fill_data;
  }

 protected:
  id ExecuteJavaScript(NSString* script) {
    password_manager::PasswordManagerJavaScriptFeature* feature =
        password_manager::PasswordManagerJavaScriptFeature::GetInstance();
    return web::test::ExecuteJavaScriptForFeature(web_state(), script, feature);
  }

  web::WebState* web_state() { return web_state_.get(); }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

// IDs used in the Username and Password <input> elements.
NSString* const kEmailInputID = @"Email";
NSString* const kPasswordInputID = @"Passwd";

// Fake username and password used for sign-in.
NSString* const kUsername = @"john.doe@gmail.com";
NSString* const kPassword = @"super!secret";

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
  return
      [NSString stringWithFormat:
                    @"{"
                     "  \"origin\":\"%@\","
                     "  \"name\":\"%@\","
                     "  \"renderer_id\":1,"
                     "  \"fields\":["
                     "    {\"name\":\"%@\", \"value\":\"\", \"renderer_id\":2},"
                     "    {\"name\":\"%@\",\"value\":\"\", \"renderer_id\":3},"
                     "  ]"
                     "}",
                    formOrigin, formName, kEmailInputID, kPasswordInputID];
}

// Tests that an attempt to fill in credentials when the username field is
// read-only succeeds, but by only filling the password while skipping filling
// the username.
TEST_F(PasswordControllerJsTest,
       FillPasswordFormWithPrefilledUsername_SucceedsWhenReadOnly) {
  const std::string origin = "https://accounts.google.com/ServiceLoginAuth";
  NSString* const formOrigin = [NSString stringWithUTF8String:origin.c_str()];
  NSString* const formName = @"gaia_loginform";
  NSString* const username2 = @"jane.doe@gmail.com";
  web::test::LoadHtml(GAIASignInForm(formOrigin, kUsername, /*isReadOnly=*/YES,
                                     /*isDisabled=*/NO),
                      GURL(origin), web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  // Expect the attempt to succeeds where the username is skipped and the
  // password filled.
  auto expected_result =
      base::Value(base::Value::Dict()
                      .Set("didAttemptFill", base::Value(true))
                      .Set("didFillUsername", base::Value(false))
                      .Set("didFillPassword", base::Value(true)));

  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                       GAIASignInFormData(formOrigin, formName), username2,
                       kPassword]));
  // Expect success but without filling the username field.
  EXPECT_EQ(FillResultForSuccess(/*did_fill_username=*/false,
                                 /*did_fill_password=*/true),
            *result);

  // Verifies that the sign-in form has been filled with username/password.
  NSString* email_js = [NSString
      stringWithFormat:@"document.getElementById('%@').value", kEmailInputID];
  EXPECT_NSEQ(kUsername, ExecuteJavaScript(email_js));

  NSString* password_js =
      [NSString stringWithFormat:@"document.getElementById('%@').value",
                                 kPasswordInputID];
  EXPECT_NSEQ(kPassword, ExecuteJavaScript(password_js));
}

// Loads a page with a password form containing a username value already.
// Checks that an attempt to fill in credentials with a different username
// succeeds, as long as the field is editable.
TEST_F(
    PasswordControllerJsTest,
    FillPasswordFormWithPrefilledUsername_SucceedsByOverridingUsernameWhenEditable) {
  const std::string origin = "https://accounts.google.com/ServiceLoginAuth";
  NSString* const formOrigin = [NSString stringWithUTF8String:origin.c_str()];
  NSString* const formName = @"gaia_loginform";
  NSString* const username2 = @"jane.doe@gmail.com";
  web::test::LoadHtml(GAIASignInForm(formOrigin, kUsername, /*isReadOnly=*/NO,
                                     /*isDisabled=*/NO),
                      GURL(origin), web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                       GAIASignInFormData(formOrigin, formName), username2,
                       kPassword]));
  // Expect success with both fields filled.
  EXPECT_EQ(FillResultForSuccess(/*did_fill_username=*/true,
                                 /*did_fill_password=*/true),
            *result);

  // Verifies that the sign-in form has been filled with the new username
  // and password.

  NSString* email_js = [NSString
      stringWithFormat:@"document.getElementById('%@').value", kEmailInputID];
  EXPECT_NSEQ(username2, ExecuteJavaScript(email_js));

  NSString* password_js =
      [NSString stringWithFormat:@"document.getElementById('%@').value",
                                 kPasswordInputID];
  EXPECT_NSEQ(kPassword, ExecuteJavaScript(password_js));
}

// Loads a page with a password form containing a disabled input with username
// value already. Checks that an attempt to fill in credentials succeeds, and
// the password is filled while the skipping filling the username.
TEST_F(
    PasswordControllerJsTest,
    FillPasswordFormWithPrefilledUsername_SucceedsWhenUsernameMatchesAndIsDisabled) {
  const std::string origin = "https://accounts.google.com/ServiceLoginAuth";
  NSString* const formOrigin = [NSString stringWithUTF8String:origin.c_str()];
  NSString* const formName = @"gaia_loginform";
  NSString* const username2 = @"jane.doe@gmail.com";
  web::test::LoadHtml(GAIASignInForm(formOrigin, kUsername, /*isReadOnly=*/NO,
                                     /*isDisabled=*/YES),
                      GURL(origin), web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                       GAIASignInFormData(formOrigin, formName), username2,
                       kPassword]));
  // Expect success without filling the username field.
  EXPECT_EQ(FillResultForSuccess(/*did_fill_username=*/false,
                                 /*did_fill_password=*/true),
            *result);

  // Verifies that the sign-in form has been filled with password and username
  // value remained the same.
  NSString* email_js = [NSString
      stringWithFormat:@"document.getElementById('%@').value", kEmailInputID];
  EXPECT_NSEQ(kUsername, ExecuteJavaScript(email_js));

  NSString* password_js =
      [NSString stringWithFormat:@"document.getElementById('%@').value",
                                 kPasswordInputID];
  EXPECT_NSEQ(kPassword, ExecuteJavaScript(password_js));
}

// Check that one password form is identified and serialized correctly.
TEST_F(PasswordControllerJsTest, GetPasswordForms_SingleFrameAndSingleForm) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form action='/generic_submit' method='post' name='login_form'>"
       "  Name: <input type='text' name='username'>"
       "  Password: <input type='password' name='password'>"
       "  <input type='submit' value='Submit'>"
       "</form>"
       "</body></html>",
      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  auto expected_form =
      base::Value::Dict()
          .Set("name", "login_form")
          .Set("origin", BaseUrl())
          .Set("action", base::StrCat({BaseUrl(), "generic_submit"}))
          .Set("name_attribute", "login_form")
          .Set("id_attribute", "")
          .Set("renderer_id", "1")
          .Set("host_frame", GetMainWebFrame()->GetFrameId());
  base::Value::Dict expected_username_field =
      ParsedField(/*renderer_id=*/"2", /*contole_type=*/"text",
                  /*identifier=*/"username", /*value=*/"",
                  /*label=*/"Name:", /*name=*/"username");
  base::Value::Dict expected_password_field = ParsedField(
      /*renderer_id=*/"3", /*contole_type=*/"password",
      /*identifier=*/"password", /*value=*/"",
      /*label=*/"Password:", /*name=*/"password");
  auto expected_fields = base::Value::List()
                             .Append(std::move(expected_username_field))
                             .Append(std::move(expected_password_field));
  expected_form.Set("fields", std::move(expected_fields));
  base::Value::List expected_results =
      base::Value::List().Append(std::move(expected_form));

  std::unique_ptr<base::Value> results =
      autofill::ParseJson(FindPasswordForms());
  ASSERT_TRUE(results);

  EXPECT_EQ(expected_results, *results);
}

// Check that multiple password forms are identified and parsed & serialized
// correctly.
TEST_F(PasswordControllerJsTest, GetPasswordForms_SingleFrameAndMultipleForms) {
  web::test::LoadHtml(@"<html><body>"
                       "<form action='/generic_submit1' name='login_form1'>"
                       "  Name: <input type='text' name='username'>"
                       "  Password: <input type='password' name='password'>"
                       "  <input type='submit' value='Submit'>"
                       "</form>"
                       "<form action='/generic_submit2' name='login_form2'>"
                       "  Name: <input type='text' name='username2'>"
                       "  Password: <input type='password' name='password2'>"
                       "  <input type='submit' value='Submit'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  // Build expected parsed forms results.
  base::Value::List expected_results = base::Value::List();
  // Set expected form 1.
  {
    auto expected_form =
        base::Value::Dict()
            .Set("name", "login_form1")
            .Set("origin", BaseUrl())
            .Set("action", base::StrCat({BaseUrl(), "generic_submit1"}))
            .Set("name_attribute", "login_form1")
            .Set("id_attribute", "")
            .Set("renderer_id", "1")
            .Set("host_frame", GetMainWebFrame()->GetFrameId());

    base::Value::Dict expected_username_field =
        ParsedField(/*renderer_id=*/"2", /*contole_type=*/"text",
                    /*identifier=*/"username", /*value=*/"",
                    /*label=*/"Name:", /*name=*/"username");
    base::Value::Dict expected_password_field = ParsedField(
        /*renderer_id=*/"3", /*contole_type=*/"password",
        /*identifier=*/"password", /*value=*/"",
        /*label=*/"Password:", /*name=*/"password");
    auto expected_fields = base::Value::List()
                               .Append(std::move(expected_username_field))
                               .Append(std::move(expected_password_field));
    expected_form.Set("fields", std::move(expected_fields));

    expected_results.Append(std::move(expected_form));
  }
  // Set expected form 2.
  {
    auto expected_form =
        base::Value::Dict()
            .Set("name", "login_form2")
            .Set("origin", BaseUrl())
            .Set("action", base::StrCat({BaseUrl(), "generic_submit2"}))
            .Set("name_attribute", "login_form2")
            .Set("id_attribute", "")
            .Set("renderer_id", "4")
            .Set("host_frame", GetMainWebFrame()->GetFrameId());
    base::Value::Dict expected_username_field =
        ParsedField(/*renderer_id=*/"5", /*contole_type=*/"text",
                    /*identifier=*/"username2", /*value=*/"",
                    /*label=*/"Name:", /*name=*/"username2");
    base::Value::Dict expected_password_field = ParsedField(
        /*renderer_id=*/"6", /*contole_type=*/"password",
        /*identifier=*/"password2", /*value=*/"",
        /*label=*/"Password:", /*name=*/"password2");
    auto expected_fields = base::Value::List()
                               .Append(std::move(expected_username_field))
                               .Append(std::move(expected_password_field));
    expected_form.Set("fields", std::move(expected_fields));

    expected_results.Append(std::move(expected_form));
  }

  std::unique_ptr<base::Value> results =
      autofill::ParseJson(FindPasswordForms());
  ASSERT_TRUE(results);

  EXPECT_EQ(expected_results, *results);
}

// Test serializing of password forms by directly calling the javascript code.
TEST_F(PasswordControllerJsTest, GetPasswordForms_DirectJsCall) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='login_form' action='/generic_submit'>"
                       "  Name: <input type='text' name='username'>"
                       "  Password: <input type='password' name='password'>"
                       "  <input type='submit' value='Submit'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  auto expected_form =
      base::Value::Dict()
          .Set("name", "login_form")
          .Set("origin", BaseUrl())
          .Set("action", base::StrCat({BaseUrl(), "generic_submit"}))
          .Set("name_attribute", "login_form")
          .Set("id_attribute", "")
          .Set("renderer_id", "1")
          .Set("host_frame", GetMainWebFrame()->GetFrameId())
          .Set("fields", base::Value::List());

  base::Value::Dict expected_username_field =
      ParsedField(/*renderer_id=*/"2", /*contole_type=*/"text",
                  /*identifier=*/"username", /*value=*/"",
                  /*label=*/"Name:", /*name=*/"username");
  base::Value::Dict expected_password_field = ParsedField(
      /*renderer_id=*/"3", /*contole_type=*/"password",
      /*identifier=*/"password", /*value=*/"",
      /*label=*/"Password:", /*name=*/"password");
  auto expected_fields = base::Value::List()
                             .Append(std::move(expected_username_field))
                             .Append(std::move(expected_password_field));
  expected_form.Set("fields", std::move(expected_fields));

  NSString* parameter = @"window.document.getElementsByTagName('form')[0]";

  std::unique_ptr<base::Value> results = autofill::ParseJson(ExecuteJavaScript(
      [NSString stringWithFormat:@"__gCrWeb.stringify(__gCrWeb.passwords."
                                 @"getPasswordFormData(%@, window))",
                                 parameter]));
  ASSERT_TRUE(results);

  EXPECT_EQ(expected_form, *results);
}

// Check that if a form action is not set then the action is parsed to the
// current url.
TEST_F(PasswordControllerJsTest, GetPasswordForms_FormActionIsNotSet) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='login_form'>"
                       "  Name: <input type='text' name='username'>"
                       "  Password: <input type='password' name='password'>"
                       "  <input type='submit' value='Submit'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  auto expected_form = base::Value::Dict()
                           .Set("name", "login_form")
                           .Set("origin", BaseUrl())
                           .Set("action", BaseUrl())
                           .Set("name_attribute", "login_form")
                           .Set("id_attribute", "")
                           .Set("renderer_id", "1")
                           .Set("host_frame", GetMainWebFrame()->GetFrameId());
  base::Value::Dict expected_username_field =
      ParsedField(/*renderer_id=*/"2", /*contole_type=*/"text",
                  /*identifier=*/"username", /*value=*/"",
                  /*label=*/"Name:", /*name=*/"username");
  base::Value::Dict expected_password_field = ParsedField(
      /*renderer_id=*/"3", /*contole_type=*/"password",
      /*identifier=*/"password", /*value=*/"",
      /*label=*/"Password:", /*name=*/"password");
  auto expected_fields = base::Value::List()
                             .Append(std::move(expected_username_field))
                             .Append(std::move(expected_password_field));
  expected_form.Set("fields", std::move(expected_fields));
  base::Value::List expected_results =
      base::Value::List().Append(std::move(expected_form));

  std::unique_ptr<base::Value> results =
      autofill::ParseJson(FindPasswordForms());
  ASSERT_TRUE(results);

  EXPECT_EQ(expected_results, *results);
}

// Check that a single username form can be extracted when its input has a
// "username" autocomplete value.
TEST_F(PasswordControllerJsTest,
       GetPasswordForms_UsernameField_WithUsernameAutocompleteAttribute) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='login_form' action='/generic_submit'>"
                       "  Name: <input type='text' name='username' "
                       "autocomplete='username'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  auto expected_form =
      base::Value::Dict()
          .Set("name", "login_form")
          .Set("origin", BaseUrl())
          .Set("action", base::StrCat({BaseUrl(), "generic_submit"}))
          .Set("name_attribute", "login_form")
          .Set("id_attribute", "")
          .Set("renderer_id", "1")
          .Set("host_frame", GetMainWebFrame()->GetFrameId());
  base::Value::Dict expected_username_field =
      ParsedField(/*renderer_id=*/"2", /*contole_type=*/"text",
                  /*identifier=*/"username", /*value=*/"",
                  /*label=*/"Name:", /*name=*/"username");
  // Set the autocomplete_attribute key here which is only set if there is an
  // autocomplete attribute in the input field.
  expected_username_field.Set("autocomplete_attribute", "username");
  auto expected_fields =
      base::Value::List().Append(std::move(expected_username_field));
  expected_form.Set("fields", std::move(expected_fields));
  base::Value::List expected_results =
      base::Value::List().Append(std::move(expected_form));

  std::unique_ptr<base::Value> results =
      autofill::ParseJson(FindPasswordForms());
  ASSERT_TRUE(results);

  EXPECT_EQ(expected_results, *results);
}

// Check that the form can be extracted when one of its input has a "webauthn"
// autocomplete value.
TEST_F(PasswordControllerJsTest,
       GetPasswordForms_UsernameField_WithWebauthnAutocompleteAttribute) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='login_form' action='/generic_submit'>"
                       "  Name: <input type='text' name='username' "
                       "autocomplete='webauthn'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  auto expected_form =
      base::Value::Dict()
          .Set("name", "login_form")
          .Set("origin", BaseUrl())
          .Set("action", base::StrCat({BaseUrl(), "generic_submit"}))
          .Set("name_attribute", "login_form")
          .Set("id_attribute", "")
          .Set("renderer_id", "1")
          .Set("host_frame", GetMainWebFrame()->GetFrameId());
  base::Value::Dict expected_username_field =
      ParsedField(/*renderer_id=*/"2", /*contole_type=*/"text",
                  /*identifier=*/"username", /*value=*/"",
                  /*label=*/"Name:", /*name=*/"username");
  // Set the autocomplete_attribute field here which is only set if there is
  // an autocomplete attribute in the field.
  expected_username_field.Set("autocomplete_attribute", "webauthn");
  auto expected_fields =
      base::Value::List().Append(std::move(expected_username_field));
  expected_form.Set("fields", std::move(expected_fields));
  base::Value::List expected_results =
      base::Value::List().Append(std::move(expected_form));

  std::unique_ptr<base::Value> results =
      autofill::ParseJson(FindPasswordForms());
  ASSERT_TRUE(results);

  EXPECT_EQ(expected_results, *results);
}

// Check that a form that isn't identified as a password form is rejected.
TEST_F(PasswordControllerJsTest,
       GetPasswordForms_UsernameField_NonPwdFormRejected) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='np' id='np1' action='/generic_submit'>"
                       "  Name: <input type='text' name='name' "
                       "autocomplete='address'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  std::string mainFrameID = GetMainWebFrame()->GetFrameId();
  NSString* expected_result = [NSString stringWithFormat:@"[]"];

  std::unique_ptr<base::Value> expected_result_json =
      autofill::ParseJson(expected_result);
  ASSERT_TRUE(expected_result);

  std::unique_ptr<base::Value> result_json =
      autofill::ParseJson(FindPasswordForms());
  ASSERT_TRUE(result_json);

  EXPECT_EQ(*expected_result_json, *result_json);
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

  // Replace __gCrWeb.common.sendWebKitMessage with mock method for checking of
  // call arguments.
  ExecuteJavaScript(
      @"var submittedFormData = null;"
       "var submittedFormMessageCalls = 0;"
       "__gCrWeb.common.sendWebKitMessage = function(messageName, messageData) "
       "{"
       "  if (messageName == 'PasswordFormSubmitButtonClick') {"
       "    submittedFormData = messageData;"
       "    submittedFormMessageCalls++;"
       "  }"
       "}");

  // Simulate touchend event on the button.
  ExecuteJavaScript(
      @"document.getElementsByName('username')[0].value = 'user1';"
       "document.getElementsByName('password')[0].value = 'password1';"
       "var e = new UIEvent('touchend');"
       "document.getElementsByTagName('button')[0].dispatchEvent(e);");

  // Check that there was only 1 call for sendWebKitMessage.
  EXPECT_NSEQ(@1, ExecuteJavaScript(@"submittedFormMessageCalls"));

  auto expected_form = base::Value::Dict()
                           .Set("name", "login_form")
                           .Set("origin", BaseUrl())
                           .Set("action", BaseUrl())
                           .Set("name_attribute", "login_form")
                           .Set("id_attribute", "login_form")
                           .Set("renderer_id", "1")
                           .Set("host_frame", GetMainWebFrame()->GetFrameId());
  base::Value::Dict expected_username_field = ParsedField(
      /*renderer_id=*/"2", /*contole_type=*/"text",
      /*identifier=*/"username", /*value=*/"user1",
      /*label=*/"Name:", /*name=*/"username");
  base::Value::Dict expected_password_field = ParsedField(
      /*renderer_id=*/"3", /*contole_type=*/"password",
      /*identifier=*/"password", /*value=*/"password1",
      /*label=*/"Password:", /*name=*/"password");
  auto expected_fields = base::Value::List()
                             .Append(std::move(expected_username_field))
                             .Append(std::move(expected_password_field));
  expected_form.Set("fields", std::move(expected_fields));

  std::unique_ptr<base::Value> results = autofill::ParseJson(
      ExecuteJavaScript(@"__gCrWeb.stringify(submittedFormData)"));
  ASSERT_TRUE(results);

  // Check that sendWebKitMessage was called with the correct argument.
  EXPECT_EQ(expected_form, *results);
}

// Check that a form is filled if url of a page and url in form fill data are
// different only in paths.
TEST_F(PasswordControllerJsTest, OriginsAreDifferentInPaths) {
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

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/2, /*password_renderer_id=*/3));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                       form_fill_data, kUsername, kPassword]));
  // Expect success with both fields filled.
  EXPECT_EQ(FillResultForSuccess(/*did_fill_username=*/true,
                                 /*did_fill_password=*/true),
            *result);

  // Verifies that the sign-in form has been filled with username/password.
  EXPECT_NSEQ(kUsername,
              ExecuteJavaScript(@"document.getElementById('name').value"));
  EXPECT_NSEQ(kPassword,
              ExecuteJavaScript(@"document.getElementById('password').value"));
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
  uint32_t newPasswordIdentifier = 2;
  EXPECT_NSEQ(
      @NO, ExecuteJavaScript([NSString
               stringWithFormat:
                   @"__gCrWeb.passwords."
                   @"fillPasswordFormWithGeneratedPassword(%d, %d, %d, '%@')",
                   formIdentifier, newPasswordIdentifier, 0, kPassword]));
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
  uint32_t const newPasswordIdentifier = 4;
  uint32_t const confirmPasswordIdentifier = 5;
  EXPECT_NSEQ(
      @NO, ExecuteJavaScript([NSString
               stringWithFormat:
                   @"__gCrWeb.passwords."
                   @"fillPasswordFormWithGeneratedPassword(%d, %d, %d, '%@')",
                   formIdentifier, newPasswordIdentifier,
                   confirmPasswordIdentifier, kPassword]));
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
  uint32_t const newPasswordIdentifier = 3;
  uint32_t const confirmPasswordIdentifier = 4;
  EXPECT_NSEQ(
      @YES, ExecuteJavaScript([NSString
                stringWithFormat:
                    @"__gCrWeb.passwords."
                    @"fillPasswordFormWithGeneratedPassword(%u, %u, %u, '%@')",
                    formIdentifier, newPasswordIdentifier,
                    confirmPasswordIdentifier, kPassword]));
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('ps1').value == '%@'",
                           kPassword]));
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('ps2').value == '%@'",
                           kPassword]));
  EXPECT_NSEQ(
      @NO,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('user').value == '%@'",
                           kPassword]));
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
  uint32_t const newPasswordIdentifier = 3;
  EXPECT_NSEQ(
      @YES, ExecuteJavaScript([NSString
                stringWithFormat:
                    @"__gCrWeb.passwords."
                    @"fillPasswordFormWithGeneratedPassword(%u, %u, %u, '%@')",
                    formIdentifier, newPasswordIdentifier, 0, kPassword]));
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('ps1').value == '%@'",
                           kPassword]));
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('ps2').value == '%@'",
                           @""]));
  EXPECT_NSEQ(
      @NO,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('user').value == '%@'",
                           kPassword]));
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
  uint32_t const confirmPasswordIdentifier = 4;
  EXPECT_NSEQ(
      @NO, ExecuteJavaScript([NSString
               stringWithFormat:
                   @"__gCrWeb.passwords."
                   @"fillPasswordFormWithGeneratedPassword(%u, %u, %u, '%@')",
                   formIdentifier, 0, confirmPasswordIdentifier, kPassword]));
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(@"document.getElementById('ps1').value == ''"));
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(@"document.getElementById('ps2').value == ''"));
  EXPECT_NSEQ(
      @YES, ExecuteJavaScript(@"document.getElementById('user').value == ''"));
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
  EXPECT_NSEQ(@NO, ExecuteJavaScript([NSString
                       stringWithFormat:@"__gCrWeb.passwords."
                                        @"fillPasswordFormWithGeneratedPasswo"
                                        @"rd(%u, '%@', null, '%@')",
                                        formIdentifier, @"hello", kPassword]));
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(@"document.getElementById('ps1').value == ''"));
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(@"document.getElementById('ps2').value == ''"));
  EXPECT_NSEQ(
      @NO,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('user').value == '%@'",
                           kPassword]));
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

  uint32_t const newPasswordIdentifier = 2;
  uint32_t const confirmPasswordIdentifier = 3;
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript([NSString
          stringWithFormat:
              @"__gCrWeb.passwords."
              @"fillPasswordFormWithGeneratedPassword(0, %u, %u, '%@')",
              newPasswordIdentifier, confirmPasswordIdentifier, kPassword]));
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('ps1').value == '%@'",
                           kPassword]));
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('ps2').value == '%@'",
                           kPassword]));
  EXPECT_NSEQ(
      @NO,
      ExecuteJavaScript([NSString
          stringWithFormat:@"document.getElementById('user').value == '%@'",
                           kPassword]));
}

// Check that a form with only a password field (i.e. w/o username) is filled.
TEST_F(PasswordControllerJsTest, FillPasswordField_Alone) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Password: <input type='password' name='password' id='password'>"
       "</form>"
       "</body></html>",
      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  // Expect the attempt to succeeds where only the password is filled.
  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/0, /*password_renderer_id=*/2));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '', '%@')",
                       form_fill_data, kPassword]));
  // Expect success without filling the username field.
  EXPECT_EQ(FillResultForSuccess(/*did_fill_username=*/false,
                                 /*did_fill_password=*/true),
            *result);

  // Verifies that the sign-in form has been filled with `kPassword`.
  EXPECT_NSEQ(kPassword,
              ExecuteJavaScript(@"document.getElementById('password').value"));
}

// Check that no fill attempt is made if the password input field is disabled.
TEST_F(PasswordControllerJsTest, FillPasswordField_InputDisabled) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='login_form' action='action1'>"
                       "  Password: <input type='password' name='password' "
                       "id='password' disabled>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/0, /*password_renderer_id=*/2));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '', '%@')",
                       form_fill_data, kPassword]));
  // Expect fill to fail.
  EXPECT_EQ(FillResultForFailure(), *result);

  // Verifies that the password field isn't filled.
  EXPECT_NSEQ(@"",
              ExecuteJavaScript(@"document.getElementById('password').value"));
}

// Check that no fill attempt is made if the password input field isn't of
// password type.
TEST_F(PasswordControllerJsTest, FillPasswordField_NotPasswordInput) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Password: <input type='text' name='password' id='password'>"
       "</form>"
       "</body></html>",
      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/0, /*password_renderer_id=*/2));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '', '%@')",
                       form_fill_data, kPassword]));
  // Expect fill to fail.
  EXPECT_EQ(FillResultForFailure(), *result);

  // Verifies that the password field isn't filled.
  EXPECT_NSEQ(@"",
              ExecuteJavaScript(@"document.getElementById('password').value"));
}

// Check that no fill attempt is made if the password field ID doesn't match.
TEST_F(PasswordControllerJsTest, FillPasswordField_NoMatchForID) {
  web::test::LoadHtml(@"<html><body>"
                       "<form name='login_form' action='action1'>"
                       "  Password: "
                       "<input type='password' name='password' id='password'>"
                       "</form>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/0, /*password_renderer_id=*/3));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '', '%@')",
                       form_fill_data, kPassword]));
  // Expect fill to fail.
  EXPECT_EQ(FillResultForFailure(), *result);

  // Verifies that the password field isn't filled.
  EXPECT_NSEQ(@"",
              ExecuteJavaScript(@"document.getElementById('password').value"));
}

// Check that filling aborts if the username input field isn't of text type.
TEST_F(PasswordControllerJsTest, FillUsernameField_NonText) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Username: <input type='radio' name='username' id='username'>"
       "  Password: <input type='password' name='password' id='password'>"
       "</form>"
       "</body></html>",
      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/2, /*password_renderer_id=*/0));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '')",
                       form_fill_data, kUsername]));
  EXPECT_EQ(FillResultForFailure(), *result);

  // Verifies that the input value is still the default value for the type.
  EXPECT_NSEQ(@"on",
              ExecuteJavaScript(@"document.getElementById('username').value"));
}

// Check that the username field isn't filled if disabled in a single username
// form.
TEST_F(PasswordControllerJsTest,
       SingleUsername_FillUsernameField_InputDisabled) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Username: <input type='text' name='username' id='username' disabled>"
       "</form>"
       "</body></html>",
      web_state());

  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/2, /*password_renderer_id=*/0));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '')",
                       form_fill_data, kUsername]));
  // Expect fill to succeeds despite no fields being filled.
  EXPECT_EQ(FillResultForSuccess(/*did_fill_username=*/false,
                                 /*did_fill_password=*/false),
            *result);

  // Verifies that the username field was filled.
  EXPECT_NSEQ(@"",
              ExecuteJavaScript(@"document.getElementById('username').value"));
}

// Check that the username field isn't filled if readonly in a single username
// form.
TEST_F(PasswordControllerJsTest, SingleUsername_FillUsernameField_ReadOnly) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Username: <input type='text' name='username' id='username' readonly>"
       "</form>"
       "</body></html>",
      web_state());

  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/2, /*password_renderer_id=*/0));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '')",
                       form_fill_data, kUsername]));
  // Expect fill to succeeds despite no fields being filled.
  EXPECT_EQ(FillResultForSuccess(/*did_fill_username=*/false,
                                 /*did_fill_password=*/false),
            *result);

  // Verifies that the username field was filled.
  EXPECT_NSEQ(@"",
              ExecuteJavaScript(@"document.getElementById('username').value"));
}

// Check that the username can be filled in single username form.
TEST_F(PasswordControllerJsTest, SingleUsername_FillUsernameField) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Username: <input type='text' name='username' id='username'>"
       "</form>"
       "</body></html>",
      web_state());

  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/2, /*password_renderer_id=*/0));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '')",
                       form_fill_data, kUsername]));
  EXPECT_EQ(FillResultForSuccess(/*did_fill_username=*/true,
                                 /*did_fill_password=*/false),
            *result);

  // Verifies that the username field was filled.
  EXPECT_NSEQ(kUsername,
              ExecuteJavaScript(@"document.getElementById('username').value"));
}

// Check that filling is skipped alltogether when the password input to fill is
// missing, and this, even if the username input can be filled. Partial filling
// isn't an option in that case.
TEST_F(PasswordControllerJsTest, FillUsernameAndPassword_MissingPasswordInput) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Username: <input type='text' name='username' id='username'>"
       "</form>"
       "</body></html>",
      web_state());

  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/2, /*password_renderer_id=*/3));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                       form_fill_data, kUsername, kPassword]));
  EXPECT_EQ(FillResultForFailure(), *result);

  // Verifies that the username fields wasn't filled.
  EXPECT_NSEQ(@"",
              ExecuteJavaScript(@"document.getElementById('username').value"));
}

// Check that filling is skipped alltogether when the username input to fill is
// missing, and this, even if the password input can be filled. Partial filling
// isn't an option in that case.
TEST_F(PasswordControllerJsTest, FillUsernameAndPassword_MissingUsernameInput) {
  web::test::LoadHtml(
      @"<html><body>"
       "<form name='login_form' action='action1'>"
       "  Password: <input type='password' name='password' id='password'>"
       "</form>"
       "</body></html>",
      web_state());

  ASSERT_TRUE(SetUpUniqueIDs());

  NSString* form_fill_data = SerializeDictValueToNSString(
      FormFillData(/*username_renderer_id=*/3, /*password_renderer_id=*/2));
  auto result = ParseFormFillResult(ExecuteJavaScript([NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, '%@', '%@')",
                       form_fill_data, kUsername, kPassword]));
  EXPECT_EQ(FillResultForFailure(), *result);

  // Verifies that the password field wasn't filled.
  EXPECT_NSEQ(@"",
              ExecuteJavaScript(@"document.getElementById('password').value"));
}

// Check that password form outside the <form> tag is extracted correctly.
TEST_F(PasswordControllerJsTest, ExtractFormOutsideTheFormTag) {
  web::test::LoadHtml(@"<html><body>"
                       "  Name: <input type='text' name='username'>"
                       "  Password: <input type='password' name='password'>"
                       "  <input type='submit' value='Submit'>"
                       "</body></html>",
                      web_state());
  ASSERT_TRUE(SetUpUniqueIDs());

  auto expected_form = base::Value::Dict()
                           .Set("name", "")
                           .Set("origin", BaseUrl())
                           .Set("action", "");
  base::Value::Dict expected_username_field =
      ParsedField(/*renderer_id=*/"1", /*contole_type=*/"text",
                  /*identifier=*/"gChrome~field~~INPUT~0", /*value=*/"",
                  /*label=*/"Name:", /*name=*/"username");
  base::Value::Dict expected_password_field = ParsedField(
      /*renderer_id=*/"2", /*contole_type=*/"password",
      /*identifier=*/"gChrome~field~~INPUT~1", /*value=*/"",
      /*label=*/"Password:", /*name=*/"password");
  auto expected_fields = base::Value::List()
                             .Append(std::move(expected_username_field))
                             .Append(std::move(expected_password_field));
  expected_form.Set("fields", std::move(expected_fields));

  std::unique_ptr<base::Value> results = autofill::ParseJson(
      ExecuteJavaScript(@"__gCrWeb.passwords.getPasswordFormDataAsString(0)"));
  ASSERT_TRUE(results);
  // Verify that the returned `results` correspond to a dictionary with
  // key/value pairs.
  ASSERT_TRUE(results->is_dict());

  base::Value::Dict& results_content = results->GetDict();

  // Verify that there is the "host_frame" key in the returned `results`.
  const std::string* results_host_frame =
      results_content.FindString("host_frame");
  ASSERT_TRUE(results_host_frame);
  ASSERT_THAT(autofill::DeserializeJavaScriptFrameId(*results_host_frame),
              IsTrue());

  // Remove the key as it was already verified to make the expected results and
  // the actual results comparable, since the host_frame is randomly generated.
  results_content.Remove("host_frame");

  EXPECT_EQ(expected_form, *results);
}

}  // namespace
