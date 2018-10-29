// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include "base/macros.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_client_helper.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/sync_credentials_filter.h"
#include "components/prefs/pref_member.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ios {
class ChromeBrowserState;
class LogManager;
}

namespace password_manager {
class PasswordFormManagerForUI;
}

@protocol PasswordManagerClientDelegate

// Shows UI to prompt the user to save the password.
- (void)showSavePasswordInfoBar:
    (std::unique_ptr<password_manager::PasswordFormManagerForUI>)formToSave;

// Shows UI to prompt the user to update the password.
- (void)showUpdatePasswordInfoBar:
    (std::unique_ptr<password_manager::PasswordFormManagerForUI>)formToUpdate;

// Shows UI to notify the user about auto sign in.
- (void)showAutosigninNotification:
    (std::unique_ptr<autofill::PasswordForm>)formSignedIn;

@property(readonly, nonatomic) ios::ChromeBrowserState* browserState;

@property(readonly) password_manager::PasswordManager* passwordManager;

@property(readonly, nonatomic) const GURL& lastCommittedURL;

@property(readonly, nonatomic) ukm::SourceId ukmSourceId;

@end

// An iOS implementation of password_manager::PasswordManagerClient.
class IOSChromePasswordManagerClient
    : public password_manager::PasswordManagerClient,
      public password_manager::PasswordManagerClientHelperDelegate {
 public:
  explicit IOSChromePasswordManagerClient(
      id<PasswordManagerClientDelegate> delegate);

  ~IOSChromePasswordManagerClient() override;

  // password_manager::PasswordManagerClient implementation.
  password_manager::SyncState GetPasswordSyncState() const override;
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool update_password) override;
  void ShowManualFallbackForSaving(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool is_update) override;
  void HideManualFallbackForSaving() override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin,
      const CredentialsCallback& callback) override;
  void AutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          saved_form_manager) override;
  bool IsIncognito() const override;
  const password_manager::PasswordManager* GetPasswordManager() const override;
  PrefService* GetPrefs() const override;
  password_manager::PasswordStore* GetPasswordStore() const override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm> form) override;
  void NotifySuccessfulLoginWithExistingPassword(
      const autofill::PasswordForm& form) override;
  void NotifyStorePasswordCalled() override;
  bool IsSavingAndFillingEnabledForCurrentPage() const override;
  const GURL& GetLastCommittedEntryURL() const override;
  const password_manager::CredentialsFilter* GetStoreResultFilter()
      const override;
  const password_manager::LogManager* GetLogManager() const override;
  ukm::SourceId GetUkmSourceId() override;
  password_manager::PasswordManagerMetricsRecorder* GetMetricsRecorder()
      override;

 private:
  // password_manager::PasswordManagerClientHelperDelegate implementation.
  void PromptUserToEnableAutosignin() override;
  password_manager::PasswordManager* GetPasswordManager() override;

  id<PasswordManagerClientDelegate> delegate_;  // (weak)

  // The preference associated with
  // password_manager::prefs::kCredentialsEnableService.
  BooleanPrefMember saving_passwords_enabled_;

  const password_manager::SyncCredentialsFilter credentials_filter_;

  std::unique_ptr<password_manager::LogManager> log_manager_;

  // Recorder of metrics that is associated with the last committed navigation
  // of the tab owning this ChromePasswordManagerClient. May be unset at
  // times. Sends statistics on destruction.
  base::Optional<password_manager::PasswordManagerMetricsRecorder>
      metrics_recorder_;

  // Helper for performing logic that is common between
  // ChromePasswordManagerClient and IOSChromePasswordManagerClient.
  password_manager::PasswordManagerClientHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromePasswordManagerClient);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_
