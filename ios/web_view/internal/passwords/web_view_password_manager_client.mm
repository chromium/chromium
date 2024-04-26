// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/ios/password_manager_ios_util.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_log_router_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_requirements_service_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_reuse_manager_factory.h"
#import "ios/web_view/internal/passwords/web_view_profile_password_store_factory.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_service_factory.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordManagerMetricsRecorder;
using password_manager::PasswordStoreInterface;

namespace ios_web_view {

// static
std::unique_ptr<WebViewPasswordManagerClient>
WebViewPasswordManagerClient::Create(web::WebState* web_state,
                                     WebViewBrowserState* browser_state) {
  syncer::SyncService* sync_service =
      ios_web_view::WebViewSyncServiceFactory::GetForBrowserState(
          browser_state);
  signin::IdentityManager* identity_manager =
      ios_web_view::WebViewIdentityManagerFactory::GetForBrowserState(
          browser_state);
  autofill::LogRouter* logRouter =
      ios_web_view::WebViewPasswordManagerLogRouterFactory::GetForBrowserState(
          browser_state);
  auto log_manager =
      autofill::LogManager::Create(logRouter, base::RepeatingClosure());
  scoped_refptr<password_manager::PasswordStoreInterface> profile_store =
      ios_web_view::WebViewProfilePasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  scoped_refptr<password_manager::PasswordStoreInterface> account_store =
      ios_web_view::WebViewAccountPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  password_manager::PasswordReuseManager* reuse_manager =
      ios_web_view::WebViewPasswordReuseManagerFactory::GetForBrowserState(
          browser_state);
  password_manager::PasswordRequirementsService* requirements_service =
      WebViewPasswordRequirementsServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  return std::make_unique<ios_web_view::WebViewPasswordManagerClient>(
      web_state, sync_service, browser_state->GetPrefs(), identity_manager,
      std::move(log_manager), profile_store.get(), account_store.get(),
      reuse_manager, requirements_service);
}

WebViewPasswordManagerClient::WebViewPasswordManagerClient(
    web::WebState* web_state,
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<autofill::LogManager> log_manager,
    PasswordStoreInterface* profile_store,
    PasswordStoreInterface* account_store,
    password_manager::PasswordReuseManager* reuse_manager,
    password_manager::PasswordRequirementsService* requirements_service)
    : web_state_(web_state),
      sync_service_(sync_service),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      log_manager_(std::move(log_manager)),
      profile_store_(profile_store),
      account_store_(account_store),
      reuse_manager_(reuse_manager),
      password_feature_manager_(pref_service, sync_service),
      credentials_filter_(this),
      requirements_service_(requirements_service),
      helper_(this) {
  saving_passwords_enabled_.Init(
      password_manager::prefs::kCredentialsEnableService, GetPrefs());
}

WebViewPasswordManagerClient::~WebViewPasswordManagerClient() = default;

bool WebViewPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin,
    CredentialsCallback callback) {
  NOTIMPLEMENTED();
  return false;
}

bool WebViewPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  if (form_to_save->IsBlocklisted()) {
    return false;
  }
  if (!password_feature_manager_.IsOptedInForAccountStorage()) {
    return false;
  }

  if (update_password) {
    [bridge_ showUpdatePasswordInfoBar:std::move(form_to_save) manual:NO];
  } else {
    [bridge_ showSavePasswordInfoBar:std::move(form_to_save) manual:NO];
  }

  return true;
}

void WebViewPasswordManagerClient::PromptUserToMovePasswordToAccount(
    std::unique_ptr<PasswordFormManagerForUI> form_to_move) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
  // No op. We only show save dialogues after successful form submissions.
}

void WebViewPasswordManagerClient::HideManualFallbackForSaving() {
  // No op. We only show save dialogues after successful form submissions.
}

void WebViewPasswordManagerClient::FocusedInputChanged(
    password_manager::PasswordManagerDriver* driver,
    autofill::FieldRendererId focused_field_id,
    autofill::mojom::FocusedFieldType focused_field_type) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> saved_form_manager,
    bool is_update_confirmation) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::PromptUserToEnableAutosignin() {
  // TODO(crbug.com/40394758): Implement this method.
  NOTIMPLEMENTED();
}

bool WebViewPasswordManagerClient::IsOffTheRecord() const {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

const password_manager::PasswordManager*
WebViewPasswordManagerClient::GetPasswordManager() const {
  return bridge_.passwordManager;
}

const password_manager::PasswordFeatureManager*
WebViewPasswordManagerClient::GetPasswordFeatureManager() const {
  return &password_feature_manager_;
}

PrefService* WebViewPasswordManagerClient::GetPrefs() const {
  return pref_service_;
}

PrefService* WebViewPasswordManagerClient::GetLocalStatePrefs() const {
  return ApplicationContext::GetInstance()->GetLocalState();
}

const syncer::SyncService* WebViewPasswordManagerClient::GetSyncService()
    const {
  return sync_service_;
}

affiliations::AffiliationService*
WebViewPasswordManagerClient::GetAffiliationService() {
  // Not used on IOS web view.
  return nullptr;
}

PasswordStoreInterface* WebViewPasswordManagerClient::GetProfilePasswordStore()
    const {
  return profile_store_;
}

PasswordStoreInterface* WebViewPasswordManagerClient::GetAccountPasswordStore()
    const {
  return account_store_;
}

password_manager::PasswordReuseManager*
WebViewPasswordManagerClient::GetPasswordReuseManager() const {
  return reuse_manager_;
}

void WebViewPasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin) {
  DCHECK(!local_forms.empty());
  helper_.NotifyUserAutoSignin();
  // TODO(crbug.com/40585559): Implement remaining logic.
}

void WebViewPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<password_manager::PasswordForm> form) {
  helper_.NotifyUserCouldBeAutoSignedIn(std::move(form));
}

void WebViewPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI>
        submitted_manager) {
  helper_.NotifySuccessfulLoginWithExistingPassword(
      std::move(submitted_manager));
}

void WebViewPasswordManagerClient::NotifyStorePasswordCalled() {
  helper_.NotifyStorePasswordCalled();
}

void WebViewPasswordManagerClient::NotifyUserCredentialsWereLeaked(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin,
    const std::u16string& username,
    bool in_account_store) {
  [bridge_ showPasswordBreachForLeakType:leak_type
                                     URL:origin
                                username:username];
}

void WebViewPasswordManagerClient::NotifyKeychainError() {}

bool WebViewPasswordManagerClient::IsSavingAndFillingEnabled(
    const GURL& url) const {
  return *saving_passwords_enabled_ && !IsOffTheRecord() &&
         !net::IsCertStatusError(GetMainFrameCertStatus()) &&
         IsFillingEnabled(url);
}

bool WebViewPasswordManagerClient::IsCommittedMainFrameSecure() const {
  return password_manager::WebStateContentIsSecureHtml(web_state_);
}

const GURL& WebViewPasswordManagerClient::GetLastCommittedURL() const {
  return bridge_.lastCommittedURL;
}

url::Origin WebViewPasswordManagerClient::GetLastCommittedOrigin() const {
  return url::Origin::Create(bridge_.lastCommittedURL);
}

const password_manager::CredentialsFilter*
WebViewPasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

autofill::LogManager* WebViewPasswordManagerClient::GetLogManager() {
  return log_manager_.get();
}

ukm::SourceId WebViewPasswordManagerClient::GetUkmSourceId() {
  // We don't collect UKM metrics from //ios/web_view.
  return ukm::kInvalidSourceId;
}

PasswordManagerMetricsRecorder*
WebViewPasswordManagerClient::GetMetricsRecorder() {
  // We don't collect UKM metrics from //ios/web_view.
  return nullptr;
}

signin::IdentityManager* WebViewPasswordManagerClient::GetIdentityManager() {
  return identity_manager_;
}

scoped_refptr<network::SharedURLLoaderFactory>
WebViewPasswordManagerClient::GetURLLoaderFactory() {
  return web_state_->GetBrowserState()->GetSharedURLLoaderFactory();
}

password_manager::PasswordRequirementsService*
WebViewPasswordManagerClient::GetPasswordRequirementsService() {
  return requirements_service_;
}

bool WebViewPasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  return false;
}

bool WebViewPasswordManagerClient::IsNewTabPage() const {
  return false;
}

safe_browsing::PasswordProtectionService*
WebViewPasswordManagerClient::GetPasswordProtectionService() const {
  // TODO(crbug.com/40731177): Enable PhishGuard in web_view.
  return nullptr;
}

}  // namespace ios_web_view
