// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"

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
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
}

void ChromePasswordProtectionService::RemovePhishedSavedPasswordCredential(
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
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
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
  return false;
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
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
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

}  // namespace safe_browsing
