// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#import <map>
#import <string>
#import <vector>

#import "base/gtest_prod_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/password_manager/core/browser/insecure_credentials_helper.h"
#import "components/password_manager/core/browser/password_reuse_detector.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/safe_browsing/ios/browser/password_protection/password_protection_service.h"
#import "components/sync/protocol/gaia_password_reuse.pb.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class GURL;
class PrefService;
class SafeBrowsingService;

namespace history {
class HistoryService;
}

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

namespace safe_browsing {
class PasswordProtectionRequest;
class SafeBrowsingMetricsCollector;
}  // namespace safe_browsing

namespace web {
class WebState;
}  // namespace web

class ChromePasswordProtectionService
    : public safe_browsing::PasswordProtectionService,
      public KeyedService {
 public:
  using ChangePhishedCredentialsCallback = base::RepeatingCallback<void(
      password_manager::PasswordStoreInterface*,
      const password_manager::MatchingReusedCredential&)>;
  ChromePasswordProtectionService(
      SafeBrowsingService* sb_service,
      ProfileIOS* profile,
      history::HistoryService* history_service,
      safe_browsing::SafeBrowsingMetricsCollector*
          safe_browsing_metrics_collector,
      ChangePhishedCredentialsCallback add_phished_credentials =
          base::BindRepeating(&password_manager::AddPhishedCredentials),
      ChangePhishedCredentialsCallback remove_phished_credentials =
          base::BindRepeating(&password_manager::RemovePhishedCredentials));
  ~ChromePasswordProtectionService() override;

  // PasswordProtectionServiceBase:
  void RequestFinished(
      safe_browsing::PasswordProtectionRequest* request,
      safe_browsing::RequestOutcome outcome,
      std::unique_ptr<safe_browsing::LoginReputationClientResponse> response)
      override;

  void ShowModalWarning(
      safe_browsing::PasswordProtectionRequest* request,
      safe_browsing::LoginReputationClientResponse::VerdictType verdict_type,
      const std::string& verdict_token,
      safe_browsing::ReusedPasswordAccountType password_type) override;

  // Stores `verdict` in the cache based on its `trigger_type`, `url`,
  // reused `password_type`, `verdict` and `receive_time`.
  void CacheVerdict(
      const GURL& url,
      safe_browsing::LoginReputationClientRequest::TriggerType trigger_type,
      safe_browsing::ReusedPasswordAccountType password_type,
      const safe_browsing::LoginReputationClientResponse& verdict,
      const base::Time& receive_time) override;

  // Looks up the cached verdict response. If verdict is not available or is
  // expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on any thread.
  safe_browsing::LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const GURL& url,
      safe_browsing::LoginReputationClientRequest::TriggerType trigger_type,
      safe_browsing::ReusedPasswordAccountType password_type,
      safe_browsing::LoginReputationClientResponse* out_response) override;

  // Returns the number of saved verdicts for the given `trigger_type`.
  int GetStoredVerdictCount(
      safe_browsing::LoginReputationClientRequest::TriggerType trigger_type)
      override;

  void MaybeReportPasswordReuseDetected(
      const GURL& main_frame_url,
      const std::string& username,
      safe_browsing::PasswordType password_type,
      bool is_phishing_url,
      bool warning_shown) override;

  void ReportPasswordChanged() override;

  void FillReferrerChain(
      const GURL& event_url,
      SessionID event_tab_id,  // SessionID::InvalidValue()
                               // if tab not available.
      safe_browsing::LoginReputationClientRequest::Frame* frame) override;

  void SanitizeReferrerChain(
      safe_browsing::ReferrerChain* referrer_chain) override;

  void PersistPhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) override;

  void RemovePhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) override;

  safe_browsing::RequestOutcome GetPingNotSentReason(
      safe_browsing::LoginReputationClientRequest::TriggerType trigger_type,
      const GURL& url,
      safe_browsing::ReusedPasswordAccountType password_type) override;

  void RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      bool all_history,
      const history::URLRows& deleted_rows) override;

  bool UserClickedThroughSBInterstitial(
      safe_browsing::PasswordProtectionRequest* request) override;

  safe_browsing::PasswordProtectionTrigger
  GetPasswordProtectionWarningTriggerPref(
      safe_browsing::ReusedPasswordAccountType password_type) const override;

  safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment
  GetUrlDisplayExperiment() const override;

  AccountInfo GetAccountInfo() const override;

  safe_browsing::ChromeUserPopulation::UserPopulation GetUserPopulationPref()
      const override;

  AccountInfo GetAccountInfoForUsername(
      const std::string& username) const override;

  safe_browsing::LoginReputationClientRequest::PasswordReuseEvent::
      SyncAccountType
      GetSyncAccountType() const override;

  bool CanShowInterstitial(
      safe_browsing::ReusedPasswordAccountType password_type,
      const GURL& main_frame_url) override;

  bool IsURLAllowlistedForPasswordEntry(const GURL& url) const override;

  bool IsInPasswordAlertMode(
      safe_browsing::ReusedPasswordAccountType password_type) override;

  bool CanSendSamplePing() override;

  bool IsPingingEnabled(
      safe_browsing::LoginReputationClientRequest::TriggerType trigger_type,
      safe_browsing::ReusedPasswordAccountType password_type) override;

  bool IsIncognito() override;

  bool IsExtendedReporting() override;

  bool IsPrimaryAccountSyncingHistory() const override;

  bool IsPrimaryAccountSignedIn() const override;

  bool IsAccountGmail(const std::string& username) const override;

  bool IsInExcludedCountry() override;

  // PasswordProtectionService override.
  void MaybeStartProtectedPasswordEntryRequest(
      web::WebState* web_state,
      const GURL& main_frame_url,
      const std::string& username,
      safe_browsing::PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists,
      safe_browsing::PasswordProtectionService::ShowWarningCallback
          show_warning_callback) override;

  // PasswordProtectionService override.
  void MaybeLogPasswordReuseLookupEvent(
      web::WebState* web_state,
      safe_browsing::RequestOutcome outcome,
      safe_browsing::PasswordType password_type,
      const safe_browsing::LoginReputationClientResponse* response) override;

  // PasswordProtectionService override.
  void MaybeLogPasswordReuseDetectedEvent(web::WebState* web_state) override;

  // Records a Chrome Sync event with the result of the user's interaction with
  // the warning dialog.
  void MaybeLogPasswordReuseDialogInteraction(
      int64_t navigation_id,
      sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction::
          InteractionResult interaction_result);

  // Gets the detailed warning text that should show in the modal warning
  // dialog.
  std::u16string GetWarningDetailText(
      safe_browsing::ReusedPasswordAccountType password_type) const;

  // Creates, starts, and tracks a new request.
  void StartRequest(
      web::WebState* web_state,
      const GURL& main_frame_url,
      const std::string& username,
      safe_browsing::PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      safe_browsing::LoginReputationClientRequest::TriggerType trigger_type,
      bool password_field_exists,
      safe_browsing::PasswordProtectionService::ShowWarningCallback
          show_warning_callback);

  // Called when user interacts with password protection UIs.
  void OnUserAction(web::WebState* web_state,
                    safe_browsing::ReusedPasswordAccountType password_type,
                    safe_browsing::WarningAction action);

 protected:
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifySendsPingForAboutBlank);

  void FillUserPopulation(
      const GURL& main_frame_url,
      safe_browsing::LoginReputationClientRequest* request_proto) override;

 private:
  // Returns true if the `web_state` is already showing a warning dialog.
  bool IsModalWarningShowingInWebState(web::WebState* web_state);
  // Removes all warning requests for `web_state`.
  void RemoveWarningRequestsByWebState(web::WebState* web_state);

  password_manager::PasswordStoreInterface* GetStoreForReusedCredential(
      const password_manager::MatchingReusedCredential& reused_credential);

  // Returns the profile PasswordStore associated with this instance.
  password_manager::PasswordStoreInterface* GetProfilePasswordStore() const;

  // Returns the GAIA-account-scoped PasswordStore associated with this
  // instance. The account password store contains passwords stored in the
  // account and is accessible only when the user is signed in and non syncing.
  password_manager::PasswordStoreInterface* GetAccountPasswordStore() const;

  // Gets prefs associated with `profile_`.
  PrefService* GetPrefs() const;

  // Returns whether `profile_` has safe browsing service enabled.
  bool IsSafeBrowsingEnabled();

  // Lookup for a callback for showing a warning for a given request.
  std::map<safe_browsing::PasswordProtectionRequest*,
           safe_browsing::PasswordProtectionService::ShowWarningCallback>
      show_warning_callbacks_;

  raw_ptr<ProfileIOS> profile_;

  // Calls `password_manager::AddPhishedCredentials`. Used to facilitate
  // testing.
  ChangePhishedCredentialsCallback add_phished_credentials_;

  // Calls `password_manager::RemovePhishedCredentials`. Used to facilitate
  // testing.
  ChangePhishedCredentialsCallback remove_phished_credentials_;

  base::WeakPtrFactory<ChromePasswordProtectionService> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_H_
