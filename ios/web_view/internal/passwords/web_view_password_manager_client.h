// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_MANAGER_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_MANAGER_CLIENT_H_

#import <Foundation/Foundation.h>

#include <memory>

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/logging/log_manager.h"
#import "components/password_manager/core/browser/password_feature_manager.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_manager_client_helper.h"
#import "components/password_manager/core/browser/password_manager_driver.h"
#import "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#import "components/password_manager/core/browser/password_requirements_service.h"
#import "components/password_manager/core/browser/password_reuse_manager.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/sync_credentials_filter.h"
#import "components/password_manager/ios/password_manager_client_bridge.h"
#import "components/prefs/pref_member.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/passwords/web_view_password_feature_manager.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "url/gurl.h"

namespace autofill {
class LogRouter;
}  // namespace autofill

namespace ios_web_view {
// An //ios/web_view implementation of password_manager::PasswordManagerClient.
class WebViewPasswordManagerClient
    : public password_manager::PasswordManagerClient {
 public:
  // Convenience factory method for creating a WebViewPasswordManagerClient.
  static std::unique_ptr<WebViewPasswordManagerClient> Create(
      web::WebState* web_state,
      WebViewBrowserState* browser_state);

  explicit WebViewPasswordManagerClient(
      web::WebState* web_state,
      syncer::SyncService* sync_service,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      autofill::LogRouter* log_router,
      password_manager::PasswordStoreInterface* profile_store,
      password_manager::PasswordStoreInterface* account_store,
      password_manager::PasswordReuseManager* reuse_manager,
      password_manager::PasswordRequirementsService* requirements_service);

  WebViewPasswordManagerClient(const WebViewPasswordManagerClient&) = delete;
  WebViewPasswordManagerClient& operator=(const WebViewPasswordManagerClient&) =
      delete;

  ~WebViewPasswordManagerClient() override;

  // password_manager::PasswordManagerClient implementation.
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
          saved_form_manager,
      bool is_update_confirmation) override;
  void PromptUserToEnableAutosignin() override;
  bool IsOffTheRecord() const override;
  const password_manager::PasswordManager* GetPasswordManager() const override;
  const password_manager::PasswordFeatureManager* GetPasswordFeatureManager()
      const override;
  PrefService* GetPrefs() const override;
  PrefService* GetLocalStatePrefs() const override;
  const syncer::SyncService* GetSyncService() const override;
  affiliations::AffiliationService* GetAffiliationService() override;
  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override;
  password_manager::PasswordStoreInterface* GetAccountPasswordStore()
      const override;
  password_manager::PasswordReuseManager* GetPasswordReuseManager()
      const override;
  password_manager::PasswordChangeServiceInterface* GetPasswordChangeService()
      const override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<password_manager::PasswordForm> form) override;
  void NotifySuccessfulLoginWithExistingPassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          submitted_manager) override;
  bool IsPasswordChangeOngoing() override;
  void NotifyStorePasswordCalled() override;
  void NotifyUserCredentialsWereLeaked(
      password_manager::LeakedPasswordDetails details) override;
  void NotifyKeychainError() override;
  bool IsSavingAndFillingEnabled(const GURL& url) const override;
  bool IsCommittedMainFrameSecure() const override;
  const GURL& GetLastCommittedURL() const override;
  url::Origin GetLastCommittedOrigin() const override;
  const password_manager::CredentialsFilter* GetStoreResultFilter()
      const override;
  autofill::LogManager* GetCurrentLogManager() override;
  ukm::SourceId GetUkmSourceId() override;
  password_manager::PasswordManagerMetricsRecorder* GetMetricsRecorder()
      override;
  signin::IdentityManager* GetIdentityManager() override;
  const signin::IdentityManager* GetIdentityManager() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  password_manager::PasswordRequirementsService*
  GetPasswordRequirementsService() override;
  bool IsIsolationForPasswordSitesEnabled() const override;
  bool IsNewTabPage() const override;

  void set_bridge(id<PasswordManagerClientBridge> bridge) { bridge_ = bridge; }

  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;

  password_manager::LeakDetectionInitiator GetLeakDetectionInitiator() override;

 private:
  __weak id<PasswordManagerClientBridge> bridge_;

  web::WebState* web_state_;
  syncer::SyncService* sync_service_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<autofill::LogRouter> log_router_;
  std::unique_ptr<autofill::LogManager> log_manager_;
  password_manager::PasswordStoreInterface* profile_store_;
  password_manager::PasswordStoreInterface* account_store_;
  password_manager::PasswordReuseManager* reuse_manager_;
  WebViewPasswordFeatureManager password_feature_manager_;
  const password_manager::SyncCredentialsFilter credentials_filter_;
  password_manager::PasswordRequirementsService* requirements_service_;

  // The preference associated with
  // password_manager::prefs::kCredentialsEnableService.
  BooleanPrefMember saving_passwords_enabled_;

  // Helper for performing logic that is common between
  // ChromePasswordManagerClient and IOSChromePasswordManagerClient.
  password_manager::PasswordManagerClientHelper helper_;
};
}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_MANAGER_CLIENT_H_
