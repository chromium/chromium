// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"

#include <memory>
#include <utility>

#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/ios/credential_manager_util.h"
#import "ios/web/public/web_state.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_log_router_factory.h"
#include "ios/web_view/internal/passwords/web_view_password_store_factory.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordManagerMetricsRecorder;
using password_manager::PasswordStore;
using password_manager::SyncState;

namespace {

const syncer::SyncService* GetSyncService(
    ios_web_view::WebViewBrowserState* browser_state) {
  return ios_web_view::WebViewProfileSyncServiceFactory::GetForBrowserState(
      browser_state);
}

}  // namespace

namespace ios_web_view {

WebViewPasswordManagerClient::WebViewPasswordManagerClient(
    id<CWVPasswordManagerClientDelegate> delegate)
    : delegate_(delegate),
      credentials_filter_(
          this,
          base::BindRepeating(&GetSyncService, delegate_.browserState)),
      log_manager_(autofill::LogManager::Create(
          ios_web_view::WebViewPasswordManagerLogRouterFactory::
              GetForBrowserState(delegate_.browserState),
          base::RepeatingClosure())),
      helper_(this) {
  saving_passwords_enabled_.Init(
      password_manager::prefs::kCredentialsEnableService, GetPrefs());
}

WebViewPasswordManagerClient::~WebViewPasswordManagerClient() = default;

SyncState WebViewPasswordManagerClient::GetPasswordSyncState() const {
  const syncer::SyncService* sync_service =
      GetSyncService(delegate_.browserState);
  return password_manager_util::GetPasswordSyncState(sync_service);
}

bool WebViewPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin,
    const CredentialsCallback& callback) {
  NOTIMPLEMENTED();
  return false;
}

bool WebViewPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  if (form_to_save->IsBlacklisted()) {
    return false;
  }

  if (update_password) {
    [delegate_ showUpdatePasswordInfoBar:std::move(form_to_save)];
  } else {
    [delegate_ showSavePasswordInfoBar:std::move(form_to_save)];
  }

  return true;
}

bool WebViewPasswordManagerClient::ShowOnboarding(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save) {
  return false;
}

void WebViewPasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::HideManualFallbackForSaving() {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::FocusedInputChanged(
    password_manager::PasswordManagerDriver* driver,
    autofill::mojom::FocusedFieldType focused_field_type) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> saved_form_manager) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::PromptUserToEnableAutosignin() {
  // TODO(crbug.com/435048): Implement this method.
  NOTIMPLEMENTED();
}

bool WebViewPasswordManagerClient::IsIncognito() const {
  return delegate_.browserState->IsOffTheRecord();
}

const password_manager::PasswordManager*
WebViewPasswordManagerClient::GetPasswordManager() const {
  return delegate_.passwordManager;
}

const password_manager::PasswordFeatureManager*
WebViewPasswordManagerClient::GetPasswordFeatureManager() const {
  return &password_feature_manager_;
}

bool WebViewPasswordManagerClient::IsMainFrameSecure() const {
  return password_manager::WebStateContentIsSecureHtml(delegate_.webState);
}

PrefService* WebViewPasswordManagerClient::GetPrefs() const {
  return delegate_.browserState->GetPrefs();
}

PasswordStore* WebViewPasswordManagerClient::GetProfilePasswordStore() const {
  return ios_web_view::WebViewPasswordStoreFactory::GetForBrowserState(
             delegate_.browserState, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

PasswordStore* WebViewPasswordManagerClient::GetAccountPasswordStore() const {
  // Account password stores aren't currently supported in iOS webviews.
  return nullptr;
}

void WebViewPasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin) {
  DCHECK(!local_forms.empty());
  helper_.NotifyUserAutoSignin();
  [delegate_ showAutosigninNotification:std::move(local_forms[0])];
}

void WebViewPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<autofill::PasswordForm> form) {
  helper_.NotifyUserCouldBeAutoSignedIn(std::move(form));
}

void WebViewPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    const autofill::PasswordForm& form) {
  helper_.NotifySuccessfulLoginWithExistingPassword(form);
}

void WebViewPasswordManagerClient::NotifyStorePasswordCalled() {
  helper_.NotifyStorePasswordCalled();
}

bool WebViewPasswordManagerClient::IsSavingAndFillingEnabled(
    const GURL& url) const {
  return *saving_passwords_enabled_ && !IsIncognito() &&
         !net::IsCertStatusError(GetMainFrameCertStatus()) &&
         IsFillingEnabled(url);
}

const GURL& WebViewPasswordManagerClient::GetLastCommittedEntryURL() const {
  return delegate_.lastCommittedURL;
}

const password_manager::CredentialsFilter*
WebViewPasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

const autofill::LogManager* WebViewPasswordManagerClient::GetLogManager()
    const {
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
  return WebViewIdentityManagerFactory::GetForBrowserState(
      delegate_.browserState);
}

scoped_refptr<network::SharedURLLoaderFactory>
WebViewPasswordManagerClient::GetURLLoaderFactory() {
  return (delegate_.browserState)->GetSharedURLLoaderFactory();
}

bool WebViewPasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  return false;
}

bool WebViewPasswordManagerClient::IsNewTabPage() const {
  return false;
}

password_manager::FieldInfoManager*
WebViewPasswordManagerClient::GetFieldInfoManager() const {
  return nullptr;
}

}  // namespace ios_web_view
