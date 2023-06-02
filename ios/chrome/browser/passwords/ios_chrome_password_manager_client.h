// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/common/language_code.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_feature_manager_impl.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_client_helper.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/sync_credentials_filter.h"
#include "components/password_manager/ios/password_manager_client_bridge.h"
#include "components/prefs/pref_member.h"
#include "components/sync/service/sync_service.h"
#import "ios/chrome/browser/safe_browsing/input_event_observer.h"
#import "ios/chrome/browser/safe_browsing/password_protection_java_script_feature.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class ChromeBrowserState;

namespace autofill {
class LogManager;
}

namespace password_manager {
class PasswordFormManagerForUI;
class PasswordManagerDriver;
}  // namespace password_manager

namespace safe_browsing {
enum class WarningAction;
}

@protocol IOSChromePasswordManagerClientBridge <PasswordManagerClientBridge>

@property(readonly, nonatomic) ChromeBrowserState* browserState;

// Shows UI to notify the user about auto sign in.
- (void)showAutosigninNotification:
    (std::unique_ptr<password_manager::PasswordForm>)formSignedIn;

@end

// An iOS implementation of password_manager::PasswordManagerClient.
// TODO(crbug.com/958833): write unit tests for this class.
class IOSChromePasswordManagerClient
    : public password_manager::PasswordManagerClient {
 public:
  explicit IOSChromePasswordManagerClient(
      id<IOSChromePasswordManagerClientBridge> bridge);

  IOSChromePasswordManagerClient(const IOSChromePasswordManagerClient&) =
      delete;
  IOSChromePasswordManagerClient& operator=(
      const IOSChromePasswordManagerClient&) = delete;

  ~IOSChromePasswordManagerClient() override;

  // password_manager::PasswordManagerClient implementation.
  password_manager::SyncState GetPasswordSyncState() const override;
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool update_password) override;
  void PromptUserToMovePasswordToAccount(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move)
      override;
  void ShowManualFallbackForSaving(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool is_update) override;
  void HideManualFallbackForSaving() override;
  void FocusedInputChanged(
      password_manager::PasswordManagerDriver* driver,
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin,
      CredentialsCallback callback) override;
  void AutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          saved_form_manager) override;
  void PromptUserToEnableAutosignin() override;
  bool IsOffTheRecord() const override;
  const password_manager::PasswordManager* GetPasswordManager() const override;
  const password_manager::PasswordFeatureManager* GetPasswordFeatureManager()
      const override;
  PrefService* GetPrefs() const override;
  PrefService* GetLocalStatePrefs() const override;
  const syncer::SyncService* GetSyncService() const override;
  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override;
  password_manager::PasswordStoreInterface* GetAccountPasswordStore()
      const override;
  password_manager::PasswordReuseManager* GetPasswordReuseManager()
      const override;
  password_manager::PasswordChangeSuccessTracker*
  GetPasswordChangeSuccessTracker() override;

  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<password_manager::PasswordForm> form) override;
  void NotifySuccessfulLoginWithExistingPassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          submitted_manager) override;
  void NotifyStorePasswordCalled() override;
  void NotifyUserCredentialsWereLeaked(
      password_manager::CredentialLeakType leak_type,
      const GURL& origin,
      const std::u16string& username) override;
  bool IsSavingAndFillingEnabled(const GURL& url) const override;
  bool IsFillingEnabled(const GURL& url) const override;
  bool IsCommittedMainFrameSecure() const override;
  const GURL& GetLastCommittedURL() const override;
  url::Origin GetLastCommittedOrigin() const override;
  autofill::LanguageCode GetPageLanguage() const override;
  const password_manager::CredentialsFilter* GetStoreResultFilter()
      const override;
  autofill::LogManager* GetLogManager() override;
  ukm::SourceId GetUkmSourceId() override;
  password_manager::PasswordManagerMetricsRecorder* GetMetricsRecorder()
      override;
  signin::IdentityManager* GetIdentityManager() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  password_manager::PasswordRequirementsService*
  GetPasswordRequirementsService() override;
  void UpdateFormManagers() override;
  bool IsIsolationForPasswordSitesEnabled() const override;
  bool IsNewTabPage() const override;
  password_manager::FieldInfoManager* GetFieldInfoManager() const override;

  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;

 private:
  __weak id<IOSChromePasswordManagerClientBridge> bridge_;

  password_manager::PasswordFeatureManagerImpl password_feature_manager_;

  // The preference associated with
  // password_manager::prefs::kCredentialsEnableService.
  BooleanPrefMember saving_passwords_enabled_;

  const password_manager::SyncCredentialsFilter credentials_filter_;

  std::unique_ptr<autofill::LogManager> log_manager_;

  // Recorder of metrics that is associated with the last committed navigation
  // of the tab owning this ChromePasswordManagerClient. May be unset at
  // times. Sends statistics on destruction.
  absl::optional<password_manager::PasswordManagerMetricsRecorder>
      metrics_recorder_;

  // Helper for performing logic that is common between
  // ChromePasswordManagerClient and IOSChromePasswordManagerClient.
  password_manager::PasswordManagerClientHelper helper_;

  base::WeakPtrFactory<IOSChromePasswordManagerClient> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_CLIENT_H_
