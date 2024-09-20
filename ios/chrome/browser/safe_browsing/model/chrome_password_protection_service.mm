// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service.h"

#import <memory>

#import "base/command_line.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/ranges/algorithm.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/password_manager/core/browser/insecure_credentials_helper.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#import "components/safe_browsing/core/browser/user_population.h"
#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/core/common/safebrowsing_constants.h"
#import "components/safe_browsing/core/common/utils.h"
#import "components/safe_browsing/ios/browser/password_protection/password_protection_request_ios.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/data_type.h"
#import "components/sync/protocol/user_event_specifics.pb.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_user_events/user_event_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/safe_browsing/model/user_population_helper.h"
#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using base::RecordAction;
using base::UserMetricsAction;
using password_manager::metrics_util::PasswordType;
using safe_browsing::ChromeUserPopulation;
using safe_browsing::LoginReputationClientRequest;
using safe_browsing::LoginReputationClientResponse;
using safe_browsing::PasswordProtectionTrigger;
using safe_browsing::ReferrerChain;
using safe_browsing::RequestOutcome;
using safe_browsing::ReusedPasswordAccountType;
using safe_browsing::WarningAction;
using sync_pb::UserEventSpecifics;

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

// Returns true if the command line has an artificial unsafe cached verdict.
bool HasArtificialCachedVerdict() {
  std::string phishing_url_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          safe_browsing::kArtificialCachedPhishGuardVerdictFlag);
  return !phishing_url_string.empty();
}

// Given a `web_state`, returns a timestamp of its last committed
// navigation.
int64_t GetLastCommittedNavigationTimestamp(web::WebState* web_state) {
  if (!web_state) {
    return 0;
  }
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
  if (navigation_id <= 0) {
    return nullptr;
  }

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
    ProfileIOS* profile,
    history::HistoryService* history_service,
    safe_browsing::SafeBrowsingMetricsCollector*
        safe_browsing_metrics_collector,
    ChangePhishedCredentialsCallback add_phished_credentials,
    ChangePhishedCredentialsCallback remove_phished_credentials)
    : safe_browsing::PasswordProtectionService(
          sb_service->GetDatabaseManager(),
          sb_service->GetURLLoaderFactory(),
          history_service,
          /*pref_service=*/nullptr,
          /*token_fetcher=*/nullptr,
          profile->IsOffTheRecord(),
          /*identity_manager=*/nullptr,
          /*try_token_fetch=*/false,
          safe_browsing_metrics_collector),
      profile_(profile),
      add_phished_credentials_(std::move(add_phished_credentials)),
      remove_phished_credentials_(std::move(remove_phished_credentials)) {}

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
  if (IsModalWarningShowingInWebState(request_ios->web_state())) {
    return;
  }

  auto callback = std::move(show_warning_callbacks_[request]);
  if (callback) {
    ReusedPasswordAccountType reused_password_account_type =
        GetPasswordProtectionReusedPasswordAccountType(request->password_type(),
                                                       request->username());
    const std::u16string warning_text =
        GetWarningDetailText(reused_password_account_type);
    // Partial bind WebState and password_type.
    auto completion_callback = base::BindOnce(
        &ChromePasswordProtectionService::OnUserAction,
        weak_factory_.GetWeakPtr(), request_ios->web_state(), password_type);
    std::move(callback).Run(warning_text, std::move(completion_callback));
  }
}

void ChromePasswordProtectionService::CacheVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type,
    const LoginReputationClientResponse& verdict,
    const base::Time& receive_time) {
  if (!CanGetReputationOfURL(url) || IsIncognito()) {
    return;
  }
  VerdictCacheManagerFactory::GetForProfile(profile_)->CachePhishGuardVerdict(
      trigger_type, password_type, verdict, receive_time);
}

LoginReputationClientResponse::VerdictType
ChromePasswordProtectionService::GetCachedVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type,
    LoginReputationClientResponse* out_response) {
  if (HasArtificialCachedVerdict() ||
      (url.is_valid() && CanGetReputationOfURL(url))) {
    return VerdictCacheManagerFactory::GetForProfile(profile_)
        ->GetCachedPhishGuardVerdict(url, trigger_type, password_type,
                                     out_response);
  }
  return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
}

int ChromePasswordProtectionService::GetStoredVerdictCount(
    LoginReputationClientRequest::TriggerType trigger_type) {
  return VerdictCacheManagerFactory::GetForProfile(profile_)
      ->GetStoredPhishGuardVerdictCount(trigger_type);
}

void ChromePasswordProtectionService::MaybeReportPasswordReuseDetected(
    const GURL& main_frame_url,
    const std::string& username,
    PasswordType password_type,
    bool is_phishing_url,
    bool warning_shown) {
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
  if (!profile_) {
    return;
  }

  for (const auto& credential : matching_reused_credentials) {
    password_manager::PasswordStoreInterface* password_store =
        GetStoreForReusedCredential(credential);
    // Password store can be null in tests.
    if (!password_store) {
      continue;
    }
    add_phished_credentials_.Run(password_store, credential);
  }
}

void ChromePasswordProtectionService::RemovePhishedSavedPasswordCredential(
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials) {
  if (!profile_) {
    return;
  }

  for (const auto& credential : matching_reused_credentials) {
    password_manager::PasswordStoreInterface* password_store =
        GetStoreForReusedCredential(credential);
    // Password store can be null in tests.
    if (!password_store) {
      continue;
    }
    remove_phished_credentials_.Run(password_store, credential);
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
  PrefService* prefs = profile_->GetPrefs();
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
      ReusedPasswordAccountType::SAVED_PASSWORD) {
    return safe_browsing::PHISHING_REUSE;
  }

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
  return experiment;
}

AccountInfo ChromePasswordProtectionService::GetAccountInfo() const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    return AccountInfo();
  }
  return identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
}

safe_browsing::ChromeUserPopulation::UserPopulation
ChromePasswordProtectionService::GetUserPopulationPref() const {
  return safe_browsing::GetUserPopulationPref(profile_->GetPrefs());
}

AccountInfo ChromePasswordProtectionService::GetAccountInfoForUsername(
    const std::string& username) const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    return AccountInfo();
  }
  std::vector<CoreAccountInfo> signed_in_accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  auto account_iterator = base::ranges::find_if(
      signed_in_accounts, [username](const auto& account) {
        return password_manager::AreUsernamesSame(
            account.email,
            /*is_username1_gaia_account=*/true, username,
            /*is_username2_gaia_account=*/true);
      });
  if (account_iterator == signed_in_accounts.end()) {
    return AccountInfo();
  }

  return identity_manager->FindExtendedAccountInfo(*account_iterator);
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
  if (!profile_) {
    return false;
  }

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
  if (!IsSafeBrowsingEnabled()) {
    return false;
  }

  // Currently, pinging is only enabled for saved passwords reuse events in iOS.
  if (trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT &&
      password_type.account_type() ==
          ReusedPasswordAccountType::SAVED_PASSWORD) {
    return true;
  }
  return false;
}

bool ChromePasswordProtectionService::IsIncognito() {
  return profile_->IsOffTheRecord();
}

bool ChromePasswordProtectionService::IsExtendedReporting() {
  // Not yet supported in iOS.
  return false;
}

bool ChromePasswordProtectionService::IsPrimaryAccountSyncingHistory() const {
  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile_);
  return sync &&
         sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES) &&
         !sync->IsLocalSyncEnabled();
}

bool ChromePasswordProtectionService::IsPrimaryAccountSignedIn() const {
  return !GetAccountInfo().account_id.empty() &&
         !GetAccountInfo().hosted_domain.empty();
}

bool ChromePasswordProtectionService::IsAccountGmail(
    const std::string& username) const {
  return GetAccountInfoForUsername(username).hosted_domain ==
         kNoHostedDomainFound;
}

bool ChromePasswordProtectionService::IsInExcludedCountry() {
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  if (!variations_service) {
    return false;
  }
  return base::Contains(safe_browsing::GetExcludedCountries(),
                        variations_service->GetLatestCountry());
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

      if (reused_password_account_type.is_account_syncing()) {
        MaybeLogPasswordReuseLookupEvent(web_state, reason, password_type,
                                         nullptr);
      }
    }
  }
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseLookupEvent(
    web::WebState* web_state,
    RequestOutcome outcome,
    PasswordType password_type,
    const LoginReputationClientResponse* response) {
  // TODO(crbug.com/40731022): Complete PhishGuard iOS implementation.
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseDetectedEvent(
    web::WebState* web_state) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  if (IsIncognito()) {
    return;
  }

  syncer::UserEventService* user_event_service =
      IOSUserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service) {
    return;
  }

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecifics(web_state);
  if (!specifics) {
    return;
  }

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

  if (IsIncognito()) {
    return;
  }

  syncer::UserEventService* user_event_service =
      IOSUserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service) {
    return;
  }

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecificsWithNavigationId(navigation_id);
  if (!specifics) {
    return;
  }

  PasswordReuseDialogInteraction* const dialog_interaction =
      specifics->mutable_gaia_password_reuse_event()
          ->mutable_dialog_interaction();
  dialog_interaction->set_interaction_result(interaction_result);

  user_event_service->RecordUserEvent(std::move(specifics));
}

std::u16string ChromePasswordProtectionService::GetWarningDetailText(
    ReusedPasswordAccountType password_type) const {
  DCHECK(password_type.account_type() ==
         ReusedPasswordAccountType::SAVED_PASSWORD);
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED);
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
      NOTREACHED_IN_MIGRATION();
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
    if (request_ios->web_state() == web_state) {
      return true;
    }
  }
  return false;
}

void ChromePasswordProtectionService::RemoveWarningRequestsByWebState(
    web::WebState* web_state) {
  for (auto it = warning_requests_.begin(); it != warning_requests_.end();) {
    safe_browsing::PasswordProtectionRequestIOS* request_ios =
        static_cast<safe_browsing::PasswordProtectionRequestIOS*>(it->get());
    if (request_ios->web_state() == web_state) {
      it = warning_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

void ChromePasswordProtectionService::FillUserPopulation(
    const GURL& main_frame_url,
    LoginReputationClientRequest* request_proto) {
  *request_proto->mutable_population() = GetUserPopulationForProfile(profile_);

  safe_browsing::VerdictCacheManager* cache_manager =
      VerdictCacheManagerFactory::GetForProfile(profile_);
  ChromeUserPopulation::PageLoadToken token =
      cache_manager->GetPageLoadToken(main_frame_url);
  // It's possible that the token is not found because real time URL check is
  // not performed for this navigation. Create a new page load token in this
  // case.
  if (!token.has_token_value()) {
    token = cache_manager->CreatePageLoadToken(main_frame_url);
  }
  request_proto->mutable_population()->mutable_page_load_tokens()->Add()->Swap(
      &token);
}

password_manager::PasswordStoreInterface*
ChromePasswordProtectionService::GetStoreForReusedCredential(
    const password_manager::MatchingReusedCredential& reused_credential) {
  if (!profile_) {
    return nullptr;
  }
  return reused_credential.in_store ==
                 password_manager::PasswordForm::Store::kAccountStore
             ? GetAccountPasswordStore()
             : GetProfilePasswordStore();
}

password_manager::PasswordStoreInterface*
ChromePasswordProtectionService::GetProfilePasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  return IOSChromeProfilePasswordStoreFactory::GetForProfile(
             profile_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStoreInterface*
ChromePasswordProtectionService::GetAccountPasswordStore() const {
  return IOSChromeAccountPasswordStoreFactory::GetForProfile(
             profile_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

PrefService* ChromePasswordProtectionService::GetPrefs() const {
  return profile_->GetPrefs();
}

bool ChromePasswordProtectionService::IsSafeBrowsingEnabled() {
  return ::safe_browsing::IsSafeBrowsingEnabled(*GetPrefs());
}
