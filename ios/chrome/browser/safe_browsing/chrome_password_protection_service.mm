// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;

namespace safe_browsing {

ChromePasswordProtectionService::ChromePasswordProtectionService(
    ChromeBrowserState* browser_state)
    : PasswordProtectionService(nullptr, nullptr, nullptr),
      browser_state_(browser_state) {}

ChromePasswordProtectionService::~ChromePasswordProtectionService() = default;

void ChromePasswordProtectionService::ShowModalWarning(
    PasswordProtectionRequest* request,
    LoginReputationClientResponse::VerdictType verdict_type,
    const std::string& verdict_token,
    ReusedPasswordAccountType password_type) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
}

void ChromePasswordProtectionService::MaybeReportPasswordReuseDetected(
    PasswordProtectionRequest* request,
    const std::string& username,
    PasswordType password_type,
    bool is_phishing_url) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
}

void ChromePasswordProtectionService::ReportPasswordChanged() {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
}

void ChromePasswordProtectionService::FillReferrerChain(
    const GURL& event_url,
    SessionID event_tab_id,  // SessionID::InvalidValue()
                             // if tab not available.
    LoginReputationClientRequest::Frame* frame) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
}

void ChromePasswordProtectionService::SanitizeReferrerChain(
    ReferrerChain* referrer_chain) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
}

void ChromePasswordProtectionService::PersistPhishedSavedPasswordCredential(
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials) {
  if (!browser_state_)
    return;

  for (const auto& credential : matching_reused_credentials) {
    password_manager::PasswordStore* password_store =
        GetStoreForReusedCredential(credential);
    // Password store can be null in tests.
    if (!password_store) {
      continue;
    }
    password_store->AddCompromisedCredentials(
        password_manager::CompromisedCredentials(
            credential.signon_realm, credential.username, base::Time::Now(),
            password_manager::InsecureType::kPhished,
            password_manager::IsMuted(false)));
  }
}

void ChromePasswordProtectionService::RemovePhishedSavedPasswordCredential(
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials) {
  if (!browser_state_)
    return;

  for (const auto& credential : matching_reused_credentials) {
    password_manager::PasswordStore* password_store =
        GetStoreForReusedCredential(credential);
    // Password store can be null in tests.
    if (!password_store) {
      continue;
    }
    password_store->RemoveCompromisedCredentials(
        credential.signon_realm, credential.username,
        password_manager::RemoveInsecureCredentialsReason::
            kMarkSiteAsLegitimate);
  }
}

RequestOutcome ChromePasswordProtectionService::GetPingNotSentReason(
    LoginReputationClientRequest::TriggerType trigger_type,
    const GURL& url,
    ReusedPasswordAccountType password_type) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return RequestOutcome::DISABLED_DUE_TO_USER_POPULATION;
}

void ChromePasswordProtectionService::
    RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
        bool all_history,
        const history::URLRows& deleted_rows) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
}

bool ChromePasswordProtectionService::UserClickedThroughSBInterstitial(
    PasswordProtectionRequest* request) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

PasswordProtectionTrigger
ChromePasswordProtectionService::GetPasswordProtectionWarningTriggerPref(
    ReusedPasswordAccountType password_type) const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return PHISHING_REUSE;
}

LoginReputationClientRequest::UrlDisplayExperiment
ChromePasswordProtectionService::GetUrlDisplayExperiment() const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment experiment;
  return experiment;
}

const policy::BrowserPolicyConnector*
ChromePasswordProtectionService::GetBrowserPolicyConnector() const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return nullptr;
}

AccountInfo ChromePasswordProtectionService::GetAccountInfo() const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return AccountInfo();
}

AccountInfo ChromePasswordProtectionService::GetSignedInNonSyncAccount(
    const std::string& username) const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return AccountInfo();
}

LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
ChromePasswordProtectionService::GetSyncAccountType() const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return PasswordReuseEvent::NOT_SIGNED_IN;
}

bool ChromePasswordProtectionService::CanShowInterstitial(
    ReusedPasswordAccountType password_type,
    const GURL& main_frame_url) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsURLAllowlistedForPasswordEntry(
    const GURL& url) const {
  if (!browser_state_)
    return false;

  PrefService* prefs = browser_state_->GetPrefs();
  return IsURLAllowlistedByPolicy(url, *prefs) ||
         MatchesPasswordProtectionChangePasswordURL(url, *prefs) ||
         MatchesPasswordProtectionLoginURL(url, *prefs);
}

bool ChromePasswordProtectionService::IsInPasswordAlertMode(
    ReusedPasswordAccountType password_type) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::CanSendSamplePing() {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsPingingEnabled(
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type) {
  if (!IsSafeBrowsingEnabled())
    return false;

  // Currently, pinging is only enabled for saved passwords reuse events in iOS.
  if (trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT &&
      password_type.account_type() ==
          ReusedPasswordAccountType::SAVED_PASSWORD) {
    return true;
  }
  return false;
}

bool ChromePasswordProtectionService::IsIncognito() {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsExtendedReporting() {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsEnhancedProtection() {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsUserMBBOptedIn() {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsHistorySyncEnabled() {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsPrimaryAccountSyncing() const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsPrimaryAccountSignedIn() const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return true;
}

bool ChromePasswordProtectionService::IsPrimaryAccountGmail() const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsOtherGaiaAccountGmail(
    const std::string& username) const {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

bool ChromePasswordProtectionService::IsInExcludedCountry() {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseLookupEvent(
    web::WebState* web_state,
    RequestOutcome outcome,
    PasswordType password_type,
    const LoginReputationClientResponse* response) {}

password_manager::PasswordStore*
ChromePasswordProtectionService::GetStoreForReusedCredential(
    const password_manager::MatchingReusedCredential& reused_credential) {
  if (!browser_state_)
    return nullptr;
  return reused_credential.in_store ==
                 password_manager::PasswordForm::Store::kAccountStore
             ? GetAccountPasswordStore()
             : GetProfilePasswordStore();
}

password_manager::PasswordStore*
ChromePasswordProtectionService::GetProfilePasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  return IOSChromePasswordStoreFactory::GetForBrowserState(
             browser_state_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStore*
ChromePasswordProtectionService::GetAccountPasswordStore() const {
  // AccountPasswordStore is currenly not supported on iOS.
  return nullptr;
}

PrefService* ChromePasswordProtectionService::GetPrefs() {
  return browser_state_->GetPrefs();
}

bool ChromePasswordProtectionService::IsSafeBrowsingEnabled() {
  return ::safe_browsing::IsSafeBrowsingEnabled(*GetPrefs());
}

}  // namespace safe_browsing
