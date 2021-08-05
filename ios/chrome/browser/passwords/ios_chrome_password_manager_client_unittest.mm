// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_manager_client.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/passwords/password_controller.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordFormManager;
using password_manager::PasswordManagerClient;
using password_manager::prefs::kCredentialsEnableService;
using testing::Return;

// TODO(crbug.com/958833): this file is initiated because of needing test for
// ios policy. More unit test of the client should be added.
class IOSChromePasswordManagerClientTest : public ChromeWebTest {
 public:
  IOSChromePasswordManagerClientTest()
      : ChromeWebTest(std::make_unique<ChromeWebClient>()),
        store_(new testing::NiceMock<
               password_manager::MockPasswordStoreInterface>()) {}

  ~IOSChromePasswordManagerClientTest() override {
    store_->ShutdownOnUIThread();
  }

  void SetUp() override {
    ChromeWebTest::SetUp();
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
