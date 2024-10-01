// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_manager_client.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/types/optional_util.h"
#import "components/autofill/core/browser/logging/log_manager.h"
#import "components/autofill/core/browser/logging/log_router.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "components/password_manager/core/browser/password_manager_driver.h"
#import "components/password_manager/core/browser/password_requirements_service.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/password_manager/ios/password_manager_ios_util.h"
#import "components/sync/service/sync_service.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_reuse_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_password_requirements_service_factory.h"
#import "ios/chrome/browser/passwords/model/password_manager_log_router_factory.h"
#import "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service.h"
#import "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "net/cert/cert_status_flags.h"
#import "services/metrics/public/cpp/ukm_recorder.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "url/gurl.h"

using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordManagerMetricsRecorder;
using password_manager::PasswordStore;
using password_manager::PasswordStoreInterface;
using password_manager::metrics_util::PasswordType;

IOSChromePasswordManagerClient::IOSChromePasswordManagerClient(
    id<IOSChromePasswordManagerClientBridge> bridge)
    : bridge_(bridge),
      password_feature_manager_(
          GetPrefs(),
          GetLocalStatePrefs(),
          SyncServiceFactory::GetForProfileIfExists(bridge_.profile)),
      credentials_filter_(this),
      helper_(this) {
  saving_passwords_enabled_.Init(
      password_manager::prefs::kCredentialsEnableService, GetPrefs());
  log_manager_ = autofill::LogManager::Create(
      ios::PasswordManagerLogRouterFactory::GetForProfile(bridge_.profile),
      base::RepeatingClosure());
}

IOSChromePasswordManagerClient::~IOSChromePasswordManagerClient() = default;

bool IOSChromePasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin,
    CredentialsCallback callback) {
  NOTIMPLEMENTED();
  return false;
}

bool IOSChromePasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  if (form_to_save->IsBlocklisted()) {
    return false;
  }

  [bridge_ removePasswordInfoBarManualFallback:YES];

  if (update_password) {
    [bridge_ showUpdatePasswordInfoBar:std::move(form_to_save) manual:NO];
  } else {
    [bridge_ showSavePasswordInfoBar:std::move(form_to_save) manual:NO];
  }

  return true;
}

void IOSChromePasswordManagerClient::PromptUserToMovePasswordToAccount(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move) {
  NOTIMPLEMENTED();
}

void IOSChromePasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
  if (is_update) {
    [bridge_ showUpdatePasswordInfoBar:std::move(form_to_save) manual:YES];
  } else {
    [bridge_ showSavePasswordInfoBar:std::move(form_to_save) manual:YES];
  }
}

void IOSChromePasswordManagerClient::HideManualFallbackForSaving() {
  [bridge_ removePasswordInfoBarManualFallback:YES];
}

void IOSChromePasswordManagerClient::FocusedInputChanged(
    password_manager::PasswordManagerDriver* driver,
    autofill::FieldRendererId focused_field_id,
    autofill::mojom::FocusedFieldType focused_field_type) {
  NOTIMPLEMENTED();
}

void IOSChromePasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> saved_form_manager,
    bool is_update_confirmation) {
  NOTIMPLEMENTED();
}

void IOSChromePasswordManagerClient::PromptUserToEnableAutosignin() {
  // TODO(crbug.com/40394758): Implement this method.
  NOTIMPLEMENTED();
}

bool IOSChromePasswordManagerClient::IsOffTheRecord() const {
  return (bridge_.profile)->IsOffTheRecord();
}

const password_manager::PasswordManager*
IOSChromePasswordManagerClient::GetPasswordManager() const {
  return bridge_.passwordManager;
}

const password_manager::PasswordFeatureManager*
IOSChromePasswordManagerClient::GetPasswordFeatureManager() const {
  return &password_feature_manager_;
}

PrefService* IOSChromePasswordManagerClient::GetPrefs() const {
  return (bridge_.profile)->GetPrefs();
}

PrefService* IOSChromePasswordManagerClient::GetLocalStatePrefs() const {
  return GetApplicationContext()->GetLocalState();
}

const syncer::SyncService* IOSChromePasswordManagerClient::GetSyncService()
    const {
  return SyncServiceFactory::GetForProfileIfExists(bridge_.profile);
}

affiliations::AffiliationService*
IOSChromePasswordManagerClient::GetAffiliationService() {
  // Not used on IOS platform.
  return nullptr;
}

PasswordStoreInterface*
IOSChromePasswordManagerClient::GetProfilePasswordStore() const {
  return IOSChromeProfilePasswordStoreFactory::GetForProfile(
             bridge_.profile, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

PasswordStoreInterface*
IOSChromePasswordManagerClient::GetAccountPasswordStore() const {
  return IOSChromeAccountPasswordStoreFactory::GetForProfile(
             bridge_.profile, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordReuseManager*
IOSChromePasswordManagerClient::GetPasswordReuseManager() const {
  return IOSChromePasswordReuseManagerFactory::GetForProfile(bridge_.profile);
}

void IOSChromePasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin) {
  DCHECK(!local_forms.empty());
  helper_.NotifyUserAutoSignin();
  [bridge_ showAutosigninNotification:std::move(local_forms[0])];
}

void IOSChromePasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<password_manager::PasswordForm> form) {
  helper_.NotifyUserCouldBeAutoSignedIn(std::move(form));
}

void IOSChromePasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI>
        submitted_manager) {
  helper_.NotifySuccessfulLoginWithExistingPassword(
      std::move(submitted_manager));
  [bridge_
      showCredentialProviderPromo:CredentialProviderPromoTrigger::
                                      SuccessfulLoginUsingExistingPassword];
}

void IOSChromePasswordManagerClient::NotifyStorePasswordCalled() {
  helper_.NotifyStorePasswordCalled();
}

void IOSChromePasswordManagerClient::NotifyUserCredentialsWereLeaked(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin,
    const std::u16string& username,
    bool in_account_store) {
  [bridge_ showPasswordBreachForLeakType:leak_type
                                     URL:origin
                                username:username];
}

void IOSChromePasswordManagerClient::NotifyKeychainError() {}

bool IOSChromePasswordManagerClient::IsSavingAndFillingEnabled(
    const GURL& url) const {
  return *saving_passwords_enabled_ && !IsOffTheRecord() &&
         !net::IsCertStatusError(GetMainFrameCertStatus()) &&
         IsFillingEnabled(url);
}

bool IOSChromePasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  return url.DeprecatedGetOriginAsURL() !=
         GURL(password_manager::kPasswordManagerAccountDashboardURL);
}

bool IOSChromePasswordManagerClient::IsCommittedMainFrameSecure() const {
  return password_manager::WebStateContentIsSecureHtml(bridge_.webState);
}

const GURL& IOSChromePasswordManagerClient::GetLastCommittedURL() const {
  return bridge_.lastCommittedURL;
}

url::Origin IOSChromePasswordManagerClient::GetLastCommittedOrigin() const {
  return url::Origin::Create(bridge_.lastCommittedURL);
}

autofill::LanguageCode IOSChromePasswordManagerClient::GetPageLanguage() const {
  // TODO(crbug.com/41430413): Add WebState to the
  // IOSChromePasswordManagerClient to be able to get the pages LanguageState
  // from the TranslateManager.
  return autofill::LanguageCode();
}

const password_manager::CredentialsFilter*
IOSChromePasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

autofill::LogManager* IOSChromePasswordManagerClient::GetLogManager() {
  return log_manager_.get();
}

ukm::SourceId IOSChromePasswordManagerClient::GetUkmSourceId() {
  return bridge_.webState
             ? ukm::GetSourceIdForWebStateDocument(bridge_.webState)
             : ukm::kInvalidSourceId;
}

PasswordManagerMetricsRecorder*
IOSChromePasswordManagerClient::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    metrics_recorder_.emplace(GetUkmSourceId());
  }
  return base::OptionalToPtr(metrics_recorder_);
}

signin::IdentityManager* IOSChromePasswordManagerClient::GetIdentityManager() {
  return IdentityManagerFactory::GetForProfile(bridge_.profile);
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromePasswordManagerClient::GetURLLoaderFactory() {
  return (bridge_.profile)->GetSharedURLLoaderFactory();
}

password_manager::PasswordRequirementsService*
IOSChromePasswordManagerClient::GetPasswordRequirementsService() {
  return IOSPasswordRequirementsServiceFactory::GetForProfile(
      bridge_.profile, ServiceAccessType::EXPLICIT_ACCESS);
}

void IOSChromePasswordManagerClient::UpdateFormManagers() {
  bridge_.passwordManager->UpdateFormManagers();
}

bool IOSChromePasswordManagerClient::IsIsolationForPasswordSitesEnabled()
    const {
  return false;
}

bool IOSChromePasswordManagerClient::IsNewTabPage() const {
  return false;
}

safe_browsing::PasswordProtectionService*
IOSChromePasswordManagerClient::GetPasswordProtectionService() const {
  return ChromePasswordProtectionServiceFactory::GetForProfile(bridge_.profile);
}
