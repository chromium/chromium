// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_manager_client.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#import "components/password_manager/core/browser/mock_password_store_interface.h"
#import "components/password_manager/core/browser/password_form_manager.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/passwords/password_controller.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordFormManager;
using password_manager::PasswordManagerClient;
using password_manager::prefs::kCredentialsEnableService;
using testing::Return;

// TODO(crbug.com/958833): this file is initiated because of needing test for
// ios policy. More unit test of the client should be added.
class IOSChromePasswordManagerClientTest : public PlatformTest {
 public:
  IOSChromePasswordManagerClientTest()
      : web_client_(std::make_unique<ChromeWebClient>()),
        store_(new testing::NiceMock<
               password_manager::MockPasswordStoreInterface>()) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  ~IOSChromePasswordManagerClientTest() override {
    store_->ShutdownOnUIThread();
  }

  void SetUp() override {
    PlatformTest::SetUp();
    ON_CALL(*store_, IsAbleToSavePasswords).WillByDefault(Return(true));

    // When waiting for predictions is on, it makes tests more complicated.
    // Disable waiting, since most tests have nothing to do with predictions.
    // All tests that test working with prediction should explicitly turn
    // predictions on.
    PasswordFormManager::set_wait_for_server_predictions_for_filling(false);

    UniqueIDDataTabHelper::CreateForWebState(web_state());
    passwordController_ =
        [[PasswordController alloc] initWithWebState:web_state()];
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;

  // PasswordController for testing.
  PasswordController* passwordController_;

  scoped_refptr<password_manager::MockPasswordStoreInterface> store_;
};

// Tests that saving password behaves properly with the
// kCredentialsEnableService pref.
TEST_F(IOSChromePasswordManagerClientTest, PasswordManagerEnabledPolicyTest) {
  PasswordManagerClient* client = passwordController_.passwordManagerClient;
  GURL url = GURL("http://foo.example.com");

  // Password Manager is enabled by default. IsSavingAndFillingEnabled should be
  // true when PasswordManagerEnabled policy is not set.
  EXPECT_TRUE(client->IsSavingAndFillingEnabled(url));

  // The pref kCredentialsEnableService should be false when disable the policy.
  client->GetPrefs()->SetBoolean(kCredentialsEnableService, false);
  // IsSavingAndFillingEnabled should return false, which means the password
  // won't be saved anymore.
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(url));

  // The pref kCredentialsEnableService should be true when enable the policy.
  client->GetPrefs()->SetBoolean(kCredentialsEnableService, true);
  // IsSavingAndFillingEnabled should return true, which means the password
  // should be saved.
  EXPECT_TRUE(client->IsSavingAndFillingEnabled(url));
}
