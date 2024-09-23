// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_UNITTEST_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_UNITTEST_H_

#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"

#import <memory>
#import <utility>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/password_form_classification.h"
#import "components/autofill/core/browser/test_autofill_manager_waiter.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/task_observer_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill {

namespace {

class TestAutofillManager : public BrowserAutofillManager {
 public:
  explicit TestAutofillManager(AutofillDriverIOS* driver)
      : BrowserAutofillManager(driver, "en-US") {}

  TestAutofillManagerWaiter& waiter() { return waiter_; }

  const FormStructure* WaitForMatchingForm(
      base::RepeatingCallback<bool(const FormStructure&)> pred) {
    return autofill::WaitForMatchingForm(this, std::move(pred),
                                         base::Seconds(2));
  }

 private:
  TestAutofillManagerWaiter waiter_{*this, {AutofillManagerEvent::kFormsSeen}};
};

}  //  namespace

class ChromeAutofillClientIOSTest : public PlatformTest {
 public:
  ChromeAutofillClientIOSTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    AutofillAgent* autofill_agent =
        [[AutofillAgent alloc] initWithPrefService:profile_->GetPrefs()
                                          webState:web_state_.get()];
    InfoBarManagerImpl::CreateForWebState(web_state_.get());
    autofill_client_ = std::make_unique<ChromeAutofillClientIOS>(
        profile_.get(), web_state_.get(),
        InfoBarManagerImpl::FromWebState(web_state_.get()), autofill_agent);
    autofill::AutofillDriverIOSFactory::CreateForWebState(
        web_state_.get(), autofill_client_.get(), autofill_agent, "en");
    autofill_manager_injector_ =
        std::make_unique<TestAutofillManagerInjector<TestAutofillManager>>(
            web_state_.get());
  }

  void TearDown() override {
    web::test::WaitForBackgroundTasks();
    PlatformTest::TearDown();
  }

 protected:
  bool LoadHtmlAndWaitForFormsSeen(NSString* html,
                                   size_t expected_number_of_forms) {
    web::test::LoadHtml(html, web_state_.get());
    return main_frame_manager()->waiter().Wait(1) &&
           main_frame_manager()->form_structures().size() ==
               expected_number_of_forms;
  }

  ChromeAutofillClientIOS& client() { return *autofill_client_; }

  TestAutofillManager* main_frame_manager() {
    return autofill_manager_injector_->GetForMainFrame();
  }

  web::WebState* web_state() { return web_state_.get(); }

 private:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<ChromeAutofillClientIOS> autofill_client_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<TestAutofillManagerInjector<TestAutofillManager>>
      autofill_manager_injector_;
};

// Tests that ClassifyAsPasswordForm correctly classifies a login form.
TEST_F(ChromeAutofillClientIOSTest, ClassifyAsPasswordForm) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormsSeen(
      @"<form>"
       "<input name='username' autocomplete='username'>"
       "<input type='password' name='password' autocomplete='current-password'>"
       "</form>",
      1));
  const FormStructure& form =
      *(main_frame_manager()->form_structures().begin()->second);
  FormData form_data = form.ToFormData();
  const auto expected = PasswordFormClassification{
      .type = PasswordFormClassification::Type::kLoginForm,
      .username_field = form_data.fields()[0].global_id()};
  EXPECT_EQ(client().ClassifyAsPasswordForm(*main_frame_manager(),
                                            form_data.global_id(),
                                            form_data.fields()[0].global_id()),
            expected);
}

// Tests that `ClassifyAsPasswordForm()` correctly classifies a login renderer
// form that is part of a bigger browser form that stretches across multiple
// frames. Also tests that non-login renderer forms aren't classified as such.
TEST_F(ChromeAutofillClientIOSTest, ClassifyAsPasswordForm_AcrossFrames) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillAcrossIframesIos);

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
      .username_field = browser_form.fields()[0].global_id()};
  EXPECT_EQ(client().ClassifyAsPasswordForm(
                *main_frame_manager(), browser_form.global_id(),
                browser_form.fields()[0].global_id()),
            expected);
}

// Tests that `ClassifyAsPasswordForm()` doesn't classify non-login forms.
TEST_F(ChromeAutofillClientIOSTest,
       ClassifyAsPasswordForm_AcrossFrames_NonLoginForm) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillAcrossIframesIos);

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

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_UNITTEST_H_
