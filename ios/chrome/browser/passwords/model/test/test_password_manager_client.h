// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_TEST_TEST_PASSWORD_MANAGER_CLIENT_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_TEST_TEST_PASSWORD_MANAGER_CLIENT_H_

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include "components/password_manager/core/browser/password_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {
class PasswordFormManagerForUI;
class TestPasswordStore;
}  // namespace password_manager
class TestingPrefServiceSimple;

using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordManager;
using password_manager::PasswordStoreInterface;
using password_manager::TestPasswordStore;

// Test PasswordManagerClient.
class TestPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  TestPasswordManagerClient();

  TestPasswordManagerClient(const TestPasswordManagerClient&) = delete;
  TestPasswordManagerClient& operator=(const TestPasswordManagerClient&) =
      delete;

  ~TestPasswordManagerClient() override;

  // PromptUserTo*Ptr functions allow to both override PromptUserTo* methods
  // and expect calls.
  MOCK_METHOD1(PromptUserToSavePasswordPtr, void(PasswordFormManagerForUI*));
  MOCK_METHOD3(
      PromptUserToChooseCredentialsPtr,
      bool(const std::vector<password_manager::PasswordForm*>& local_forms,
           const url::Origin& origin,
           CredentialsCallback callback));

  scoped_refptr<TestPasswordStore> password_store() const;
  void set_password_store(scoped_refptr<TestPasswordStore> store);

  PasswordFormManagerForUI* pending_manager() const;

  void set_current_url(const GURL& current_url);

 private:
  // PasswordManagerClient:
  PrefService* GetPrefs() const override;
  PasswordStoreInterface* GetProfilePasswordStore() const override;
  const PasswordManager* GetPasswordManager() const override;
  url::Origin GetLastCommittedOrigin() const override;
  // Stores `manager` into `manager_`. Save() should be
  // called manually in test. To put expectation on this function being called,
  // use PromptUserToSavePasswordPtr.
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> manager,
      bool update_password) override;
  // Mocks choosing a credential by the user. To put expectation on this
  // function being called, use PromptUserToChooseCredentialsPtr.
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin,
      CredentialsCallback callback) override;

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  GURL last_committed_url_;
  PasswordManager password_manager_;
  std::unique_ptr<PasswordFormManagerForUI> manager_;
  scoped_refptr<TestPasswordStore> store_;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_TEST_TEST_PASSWORD_MANAGER_CLIENT_H_
