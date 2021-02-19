// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/ui/password_check_referrer.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/ios/password_protection/password_protection_request_ios.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"
#include "components/variations/service/variations_service.h"
#import "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_service.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;
using password_manager::metrics_util::PasswordType;
using safe_browsing::LoginReputationClientRequest;
using safe_browsing::LoginReputationClientResponse;
using safe_browsing::PasswordProtectionTrigger;
using safe_browsing::RequestOutcome;
using safe_browsing::ReusedPasswordAccountType;
using sync_pb::UserEventSpecifics;
using safe_browsing::ReferrerChain;
using safe_browsing::WarningAction;

using InteractionResult = sync_pb::GaiaPasswordReuse::
    PasswordReuseDialogInteraction::InteractionResult;
using PasswordReuseDialogInteraction =
    sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction;
using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;
using SafeBrowsingStatus =
    sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus;
using ShowWarningCallback =
    safe_browsing::PasswordProtectionService::ShowWarningCallback;

namespace {

// Given a |web_state|, returns a timestamp of its last committed
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
    SafeBrowsingService* sb_service,
    ChromeBrowserState* browser_state)
    : safe_browsing::PasswordProtectionService(
          sb_service->GetDatabaseManager(),
          sb_service->GetURLLoaderFactory(),
          ios::HistoryServiceFactory::GetForBrowserState(
              browser_state,
              ServiceAccessType::EXPLICIT_ACCESS)),
      browser_state_(browser_state) {}

ChromePasswordProtectionService::~ChromePasswordProtectionService() = default;

// Removes ShowWarningCallbacks after requests have finished, even if they were
// not called.
void ChromePasswordProtectionService::RequestFinished(
    safe_browsing::PasswordProtectionRequest* request,
    RequestOutcome outcome,
    std::unique_ptr<LoginReputationClientResponse> response) {
  // Ensure parent class method runs before removing callback.
  PasswordProtectionService::RequestFinished(request, outcome,
                                             std::move(response));
  show_warning_callbacks_.erase(request);
}

void ChromePasswordProtectionService::ShowModalWarning(
    safe_browsing::PasswordProtectionRequest* request,
    LoginReputationClientResponse::VerdictType verdict_type,
    const std::string& verdict_token,
    ReusedPasswordAccountType password_type) {
  safe_browsing::PasswordProtectionRequestIOS* request_ios =
      static_cast<safe_browsing::PasswordProtectionRequestIOS*>(request);
  // Don't show warning again if there is already a modal warning showing.
  if (IsModalWarningShowingInWebState(request_ios->web_state()))
    return;

  auto callback = std::move(show_warning_callbacks_[request]);
  if (callback) {
    ReusedPasswordAccountType reused_password_account_type =
        GetPasswordProtectionReusedPasswordAccountType(request->password_type(),
                                                       request->username());
    std::vector<size_t> placeholder_offsets;
    const base::string16 warning_text = GetWarningDetailText(
        reused_password_account_type, &placeholder_offsets);
    // Partial bind WebState and password_type.
    auto completion_callback = base::BindOnce(
        &ChromePasswordProtectionService::OnUserAction,
        weak_factory_.GetWeakPtr(), request_ios->web_state(), password_type);
    std::move(callback).Run(warning_text, std::move(completion_callback));
  }
}

void ChromePasswordProtectionService::MaybeReportPasswordReuseDetected(
    safe_browsing::PasswordProtectionRequest* request,
    const std::string& username,
    PasswordType password_type,
    bool is_phishing_url) {
  // Enterprise reporting extension not yet supported in iOS.
}

void ChromePasswordProtectionService::ReportPasswordChanged() {
  // Enterprise reporting extension not yet supported in iOS.
}

void ChromePasswordProtectionService::FillReferrerChain(
    const GURL& event_url,
    SessionID event_tab_id,  // SessionID::InvalidValue()
                             // if tab not available.
    LoginReputationClientRequest::Frame* frame) {
  // Not yet supported in iOS.
}

void ChromePasswordProtectionService::SanitizeReferrerChain(
    ReferrerChain* referrer_chain) {
  // Sample pings not yet supported in iOS.
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
    password_store->AddInsecureCredential(password_manager::InsecureCredential(
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
    password_store->RemoveInsecureCredentials(
        credential.signon_realm, credential.username,
        password_manager::RemoveInsecureCredentialsReason::
            kMarkSiteAsLegitimate);
  }
}

RequestOutcome ChromePasswordProtectionService::GetPingNotSentReason(
    LoginReputationClientRequest::TriggerType trigger_type,
    const GURL& url,
    ReusedPasswordAccountType password_type) {
  DCHECK(!CanSendPing(trigger_type, url, password_type));
  if (IsInExcludedCountry()) {
    return RequestOutcome::EXCLUDED_COUNTRY;
  }
  if (!IsSafeBrowsingEnabled()) {
    return RequestOutcome::SAFE_BROWSING_DISABLED;
  }
  if (IsIncognito()) {
    return RequestOutcome::DISABLED_DUE_TO_INCOGNITO;
  }
  if (trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT &&
      password_type.account_type() !=
          ReusedPasswordAccountType::SAVED_PASSWORD &&
      GetPasswordProtectionWarningTriggerPref(password_type) ==
          safe_browsing::PASSWORD_PROTECTION_OFF) {
    return RequestOutcome::TURNED_OFF_BY_ADMIN;
  }
  PrefService* prefs = browser_state_->GetPrefs();
  if (safe_browsing::IsURLAllowlistedByPolicy(url, *prefs)) {
    return RequestOutcome::MATCHED_ENTERPRISE_ALLOWLIST;
  }
  if (safe_browsing::MatchesPasswordProtectionChangePasswordURL(url, *prefs)) {
    return RequestOutcome::MATCHED_ENTERPRISE_CHANGE_PASSWORD_URL;
  }
  if (safe_browsing::MatchesPasswordProtectionLoginURL(url, *prefs)) {
    return RequestOutcome::MATCHED_ENTERPRISE_LOGIN_URL;
  }
  if (IsInPasswordAlertMode(password_type)) {
    return RequestOutcome::PASSWORD_ALERT_MODE;
  }
  return RequestOutcome::DISABLED_DUE_TO_USER_POPULATION;
}

void ChromePasswordProtectionService::
    RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
        bool all_history,
        const history::URLRows& deleted_rows) {
  // Sync password not yet supported in iOS.
}

bool ChromePasswordProtectionService::UserClickedThroughSBInterstitial(
    safe_browsing::PasswordProtectionRequest* request) {
  // Not yet supported in iOS.
  return false;
}

PasswordProtectionTrigger
ChromePasswordProtectionService::GetPasswordProtectionWarningTriggerPref(
    ReusedPasswordAccountType password_type) const {
  if (password_type.account_type() ==
          ReusedPasswordAccountType::SAVED_PASSWORD &&
      base::FeatureList::IsEnabled(
          safe_browsing::kPasswordProtectionForSavedPasswords))
    return safe_browsing::PHISHING_REUSE;

  bool is_policy_managed =
      GetPrefs()->HasPrefPath(prefs::kPasswordProtectionWarningTrigger);
  PasswordProtectionTrigger trigger_level =
      static_cast<PasswordProtectionTrigger>(
          GetPrefs()->GetInteger(prefs::kPasswordProtectionWarningTrigger));
  return is_policy_managed ? trigger_level : safe_browsing::PHISHING_REUSE;
}

LoginReputationClientRequest::UrlDisplayExperiment
ChromePasswordProtectionService::GetUrlDisplayExperiment() const {
  safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment experiment;
  experiment.set_simplified_url_display_enabled(
      base::FeatureList::IsEnabled(safe_browsing::kSimplifiedUrlDisplay));
  // Delayed warnings parameters:
  experiment.set_delayed_warnings_enabled(
      base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings));
  experiment.set_delayed_warnings_mouse_clicks_enabled(
      safe_browsing::kDelayedWarningsEnableMouseClicks.Get());
  // Actual URL display experiments:
  experiment.set_reveal_on_hover(base::FeatureList::IsEnabled(
      omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover));
  experiment.set_hide_on_interaction(base::FeatureList::IsEnabled(
      omnibox::kHideSteadyStateUrlPathQueryAndRefOnInteraction));
  experiment.set_elide_to_registrable_domain(
      base::FeatureList::IsEnabled(omnibox::kMaybeElideToRegistrableDomain));
  return experiment;
}

const policy::BrowserPolicyConnector*
ChromePasswordProtectionService::GetBrowserPolicyConnector() const {
  return GetApplicationContext()->GetBrowserPolicyConnector();
}

AccountInfo ChromePasswordProtectionService::GetAccountInfo() const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state_);
  if (!identity_manager)
    return AccountInfo();
  base::Optional<AccountInfo> primary_account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync));
  return primary_account_info.value_or(AccountInfo());
}

AccountInfo ChromePasswordProtectionService::GetSignedInNonSyncAccount(
    const std::string& username) const {
  auto* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state_);
  if (!identity_manager)
    return AccountInfo();
  std::vector<CoreAccountInfo> signed_in_accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  auto account_iterator =
      std::find_if(signed_in_accounts.begin(), signed_in_accounts.end(),
                   [username](const auto& account) {
                     return password_manager::AreUsernamesSame(
                         account.email,
                         /*is_username1_gaia_account=*/true, username,
                         /*is_username2_gaia_account=*/true);
                   });
  if (account_iterator == signed_in_accounts.end())
    return AccountInfo();

  return identity_manager
      ->FindExtendedAccountInfoForAccountWithRefreshToken(*account_iterator)
      .value_or(AccountInfo());
}

LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
ChromePasswordProtectionService::GetSyncAccountType() const {
  const AccountInfo account_info = GetAccountInfo();
  if (!IsPrimaryAccountSignedIn()) {
    return PasswordReuseEvent::NOT_SIGNED_IN;
  }

  // For gmail or googlemail account, the hosted_domain will always be
  // kNoHostedDomainFound.
  return account_info.hosted_domain == kNoHostedDomainFound
             ? PasswordReuseEvent::GMAIL
             : PasswordReuseEvent::GSUITE;
}

bool ChromePasswordProtectionService::CanShowInterstitial(
    ReusedPasswordAccountType password_type,
    const GURL& main_frame_url) {
  // Not yet supported in iOS.
  return false;
}

bool ChromePasswordProtectionService::IsURLAllowlistedForPasswordEntry(
    const GURL& url) const {
  if (!browser_state_)
    return false;

  PrefService* prefs = GetPrefs();
  return safe_browsing::IsURLAllowlistedByPolicy(url, *prefs) ||
         safe_browsing::MatchesPasswordProtectionChangePasswordURL(url,
                                                                   *prefs) ||
         safe_browsing::MatchesPasswordProtectionLoginURL(url, *prefs);
}

bool ChromePasswordProtectionService::IsInPasswordAlertMode(
    ReusedPasswordAccountType password_type) {
  return GetPasswordProtectionWarningTriggerPref(password_type) ==
         safe_browsing::PASSWORD_REUSE;
}

bool ChromePasswordProtectionService::CanSendSamplePing() {
  // Sample pings not yet enabled in iOS.
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
  return browser_state_->IsOffTheRecord();
}

bool ChromePasswordProtectionService::IsExtendedReporting() {
  // Not yet supported in iOS.
  return false;
}

bool ChromePasswordProtectionService::IsEnhancedProtection() {
  // Not yet supported in iOS.
  return false;
}

bool ChromePasswordProtectionService::IsUserMBBOptedIn() {
  // Not yet supported in iOS.
  return false;
}

bool ChromePasswordProtectionService::IsHistorySyncEnabled() {
  syncer::SyncService* sync =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state_);
  return sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled() &&
         sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
}

bool ChromePasswordProtectionService::IsPrimaryAccountSyncing() const {
  syncer::SyncService* sync =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state_);
  return sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled();
}

bool ChromePasswordProtectionService::IsPrimaryAccountSignedIn() const {
  return !GetAccountInfo().account_id.empty() &&
         !GetAccountInfo().hosted_domain.empty();
}

bool ChromePasswordProtectionService::IsPrimaryAccountGmail() const {
  return GetAccountInfo().hosted_domain == kNoHostedDomainFound;
}

bool ChromePasswordProtectionService::IsOtherGaiaAccountGmail(
    const std::string& username) const {
  return GetSignedInNonSyncAccount(username).hosted_domain ==
         kNoHostedDomainFound;
}

bool ChromePasswordProtectionService::IsInExcludedCountry() {
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  if (!variations_service)
    return false;
  return base::Contains(safe_browsing::GetExcludedCountries(),
                        variations_service->GetStoredPermanentCountry());
}

void ChromePasswordProtectionService::MaybeStartProtectedPasswordEntryRequest(
    web::WebState* web_state,
    const GURL& main_frame_url,
    const std::string& username,
    PasswordType password_type,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    bool password_field_exists,
    ShowWarningCallback show_warning_callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  LoginReputationClientRequest::TriggerType trigger_type =
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT;
  ReusedPasswordAccountType reused_password_account_type =
      GetPasswordProtectionReusedPasswordAccountType(password_type, username);

  if (IsSupportedPasswordTypeForPinging(password_type)) {
    if (CanSendPing(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                    main_frame_url, reused_password_account_type)) {
      saved_passwords_matching_reused_credentials_ =
          matching_reused_credentials;
      StartRequest(web_state, main_frame_url, username, password_type,
                   matching_reused_credentials,
                   LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                   password_field_exists, std::move(show_warning_callback));
    } else {
      RequestOutcome reason = GetPingNotSentReason(
          trigger_type, main_frame_url, reused_password_account_type);
      LogNoPingingReason(trigger_type, reason, reused_password_account_type);

      if (reused_password_account_type.is_account_syncing())
        MaybeLogPasswordReuseLookupEvent(web_state, reason, password_type,
                                         nullptr);
    }
  }
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

base::string16 ChromePasswordProtectionService::GetWarningDetailText(
    ReusedPasswordAccountType password_type,
    std::vector<size_t>* placeholder_offsets) const {
  DCHECK(password_type.account_type() ==
         ReusedPasswordAccountType::SAVED_PASSWORD);
  DCHECK(base::FeatureList::IsEnabled(
      safe_browsing::kPasswordProtectionForSavedPasswords));
  return GetWarningDetailTextForSavedPasswords(placeholder_offsets);
}
base::string16
ChromePasswordProtectionService::GetWarningDetailTextForSavedPasswords(
    std::vector<size_t>* placeholder_offsets) const {
  std::vector<base::string16> placeholders =
      GetPlaceholdersForSavedPasswordWarningText();
  // The default text is a complete sentence without placeholders.
  return placeholders.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED)
             : GetWarningDetailTextToCheckSavedPasswords(placeholder_offsets);
}

base::string16
ChromePasswordProtectionService::GetWarningDetailTextToCheckSavedPasswords(
    std::vector<size_t>* placeholder_offsets) const {
  std::vector<base::string16> placeholders =
      GetPlaceholdersForSavedPasswordWarningText();
  if (placeholders.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_1_DOMAIN, placeholders,
        placeholder_offsets);
  } else if (placeholders.size() == 2) {
    return l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_2_DOMAIN, placeholders,
        placeholder_offsets);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_3_DOMAIN, placeholders,
        placeholder_offsets);
  }
}

std::vector<base::string16>
ChromePasswordProtectionService::GetPlaceholdersForSavedPasswordWarningText()
    const {
  const std::vector<std::string>& matching_domains =
      saved_passwords_matching_domains();
  const std::list<std::string>& spoofed_domains = common_spoofed_domains();

  // Show most commonly spoofed domains first.
  // This looks through the top priority spoofed domains and then checks to see
  // if it's in the matching domains.
  std::vector<base::string16> placeholders;
  for (auto priority_domain_iter = spoofed_domains.begin();
       priority_domain_iter != spoofed_domains.end(); ++priority_domain_iter) {
    std::string matching_domain;

    // Check if any of the matching domains is equal or a suffix to the current
    // priority domain.
    if (std::find_if(matching_domains.begin(), matching_domains.end(),
                     [priority_domain_iter,
                      &matching_domain](const std::string& domain) {
                       // Assigns the matching_domain to add into the priority
                       // placeholders. This value is only used if the return
                       // value of this function is true.
                       matching_domain = domain;
                       const base::StringPiece domainStringPiece(domain);
                       // Checks for two cases:
                       // 1. if the matching domain is equal to the current
                       // priority domain or
                       // 2. if "," + the current priority is a suffix of the
                       // matching domain The second case covers eTLD+1.
                       return (domain == *priority_domain_iter) ||
                              base::EndsWith(domainStringPiece,
                                             "." + *priority_domain_iter);
                     }) != matching_domains.end()) {
      placeholders.push_back(base::UTF8ToUTF16(matching_domain));
    }
  }

  // If there are less than 3 saved default domains, check the saved
  //  password domains to see if there are more that can be added to the
  //  warning text.
  int domains_idx = placeholders.size();
  for (size_t idx = 0; idx < matching_domains.size() && domains_idx < 3;
       idx++) {
    // Do not add duplicate domains if it was already in the default domains.
    if (std::find(placeholders.begin(), placeholders.end(),
                  base::UTF8ToUTF16(matching_domains[idx])) !=
        placeholders.end()) {
      continue;
    }
    placeholders.push_back(base::UTF8ToUTF16(matching_domains[idx]));
    domains_idx++;
  }
  return placeholders;
}

void ChromePasswordProtectionService::StartRequest(
    web::WebState* web_state,
    const GURL& main_frame_url,
    const std::string& username,
    PasswordType password_type,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    LoginReputationClientRequest::TriggerType trigger_type,
    bool password_field_exists,
    ShowWarningCallback show_warning_callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  scoped_refptr<safe_browsing::PasswordProtectionRequest> request(
      new safe_browsing::PasswordProtectionRequestIOS(
          web_state, main_frame_url, web_state->GetContentsMimeType(), username,
          password_type, matching_reused_credentials, trigger_type,
          password_field_exists, this, GetRequestTimeoutInMS()));
  request->Start();
  show_warning_callbacks_[request.get()] = std::move(show_warning_callback);
  pending_requests_.insert(std::move(request));
}

void ChromePasswordProtectionService::OnUserAction(
    web::WebState* web_state,
    ReusedPasswordAccountType password_type,
    WarningAction action) {
  // Only SAVED_PASSWORD is supported in iOS.
  DCHECK_EQ(ReusedPasswordAccountType::SAVED_PASSWORD,
            password_type.account_type());
  LogWarningAction(safe_browsing::WarningUIType::MODAL_DIALOG, action,
                   password_type);
  switch (action) {
    case WarningAction::CHANGE_PASSWORD:
      RecordAction(UserMetricsAction(
          "PasswordProtection.ModalWarning.ChangePasswordButtonClicked"));
      password_manager::LogPasswordCheckReferrer(
          password_manager::PasswordCheckReferrer::kPhishGuardDialog);
      break;
    case WarningAction::CLOSE:
      RecordAction(
          UserMetricsAction("PasswordProtection.ModalWarning.CloseWarning"));
      break;
    default:
      NOTREACHED();
      break;
  }
  RemoveWarningRequestsByWebState(web_state);
}

bool ChromePasswordProtectionService::IsModalWarningShowingInWebState(
    web::WebState* web_state) {
  for (const auto& request : warning_requests_) {
    safe_browsing::PasswordProtectionRequestIOS* request_ios =
        static_cast<safe_browsing::PasswordProtectionRequestIOS*>(
            request.get());
    if (request_ios->web_state() == web_state)
      return true;
  }
  return false;
}

void ChromePasswordProtectionService::RemoveWarningRequestsByWebState(
    web::WebState* web_state) {
  for (auto it = warning_requests_.begin(); it != warning_requests_.end();) {
    safe_browsing::PasswordProtectionRequestIOS* request_ios =
        static_cast<safe_browsing::PasswordProtectionRequestIOS*>(it->get());
    if (request_ios->web_state() == web_state)
      it = warning_requests_.erase(it);
    else
      ++it;
  }
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

PrefService* ChromePasswordProtectionService::GetPrefs() const {
  return browser_state_->GetPrefs();
}

bool ChromePasswordProtectionService::IsSafeBrowsingEnabled() {
  return ::safe_browsing::IsSafeBrowsingEnabled(*GetPrefs());
}

