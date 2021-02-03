// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>

#include "base/time/time.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/protocol/gaia_password_reuse.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using sync_pb::GaiaPasswordReuse;
using sync_pb::UserEventSpecifics;
using InteractionResult =
    GaiaPasswordReuse::PasswordReuseDialogInteraction::InteractionResult;
using PasswordReuseDialogInteraction =
    GaiaPasswordReuse::PasswordReuseDialogInteraction;
using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;
using SafeBrowsingStatus =
    GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus;

namespace safe_browsing {

namespace {

// Given a |web_state|, returns a timestemp of its last committed
// navigation.
int64_t GetLastCommittedNavigationTimestamp(web::WebState* web_state) {
  if (!web_state)
    return 0;
  web::NavigationItem* navigation =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  return navigation ? navigation->GetTimestamp()
                          .ToDeltaSinceWindowsEpoch()
                          .InMicroseconds()
                    : 0;
}

// Return a new UserEventSpecifics w/o the navigation_id populated
std::unique_ptr<UserEventSpecifics> GetNewUserEventSpecifics() {
  auto specifics = std::make_unique<UserEventSpecifics>();
  specifics->set_event_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  return specifics;
}

// Return a new UserEventSpecifics w/ the navigation_id populated
std::unique_ptr<UserEventSpecifics> GetUserEventSpecificsWithNavigationId(
    int64_t navigation_id) {
  if (navigation_id <= 0)
    return nullptr;

  auto specifics = GetNewUserEventSpecifics();
  specifics->set_navigation_id(navigation_id);
  return specifics;
}

// Return a new UserEventSpecifics populated from the web_state
std::unique_ptr<UserEventSpecifics> GetUserEventSpecifics(
    web::WebState* web_state) {
  return GetUserEventSpecificsWithNavigationId(
      GetLastCommittedNavigationTimestamp(web_state));
}

}  // namespace

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
    const LoginReputationClientResponse* response) {
  // TODO(crbug.com/1147967): Complete PhishGuard iOS implementation.
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseDetectedEvent(
    web::WebState* web_state) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  if (IsIncognito())
    return;

  syncer::UserEventService* user_event_service =
      IOSUserEventServiceFactory::GetForBrowserState(browser_state_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecifics(web_state);
  if (!specifics)
    return;

  auto* const status = specifics->mutable_gaia_password_reuse_event()
                           ->mutable_reuse_detected()
                           ->mutable_status();
  status->set_enabled(IsSafeBrowsingEnabled());

  status->set_safe_browsing_reporting_population(SafeBrowsingStatus::NONE);

  user_event_service->RecordUserEvent(std::move(specifics));
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseDialogInteraction(
    int64_t navigation_id,
    InteractionResult interaction_result) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  if (IsIncognito())
    return;

  syncer::UserEventService* user_event_service =
      IOSUserEventServiceFactory::GetForBrowserState(browser_state_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecificsWithNavigationId(navigation_id);
  if (!specifics)
    return;

  PasswordReuseDialogInteraction* const dialog_interaction =
      specifics->mutable_gaia_password_reuse_event()
          ->mutable_dialog_interaction();
  dialog_interaction->set_interaction_result(interaction_result);

  user_event_service->RecordUserEvent(std::move(specifics));
}

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
