// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#import "components/safe_browsing/ios/password_protection/password_protection_service.h"

class ChromeBrowserState;
class PrefService;

namespace safe_browsing {

class ChromePasswordProtectionService : public PasswordProtectionService {
 public:
  explicit ChromePasswordProtectionService(ChromeBrowserState* browser_state);
  ~ChromePasswordProtectionService() override;

  void ShowModalWarning(PasswordProtectionRequest* request,
                        LoginReputationClientResponse::VerdictType verdict_type,
                        const std::string& verdict_token,
                        ReusedPasswordAccountType password_type) override;

  void MaybeReportPasswordReuseDetected(PasswordProtectionRequest* request,
                                        const std::string& username,
                                        PasswordType password_type,
                                        bool is_phishing_url) override;

  void ReportPasswordChanged() override;

  void FillReferrerChain(const GURL& event_url,
                         SessionID event_tab_id,  // SessionID::InvalidValue()
                                                  // if tab not available.
                         LoginReputationClientRequest::Frame* frame) override;

  void SanitizeReferrerChain(ReferrerChain* referrer_chain) override;

  void PersistPhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) override;

  void RemovePhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) override;

  RequestOutcome GetPingNotSentReason(
      LoginReputationClientRequest::TriggerType trigger_type,
      const GURL& url,
      ReusedPasswordAccountType password_type) override;

  void RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      bool all_history,
      const history::URLRows& deleted_rows) override;

  bool UserClickedThroughSBInterstitial(
      PasswordProtectionRequest* request) override;

  PasswordProtectionTrigger GetPasswordProtectionWarningTriggerPref(
      ReusedPasswordAccountType password_type) const override;

  LoginReputationClientRequest::UrlDisplayExperiment GetUrlDisplayExperiment()
      const override;

  const policy::BrowserPolicyConnector* GetBrowserPolicyConnector()
      const override;

  AccountInfo GetAccountInfo() const override;

  AccountInfo GetSignedInNonSyncAccount(
      const std::string& username) const override;

  LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
  GetSyncAccountType() const override;

  bool CanShowInterstitial(ReusedPasswordAccountType password_type,
                           const GURL& main_frame_url) override;

  bool IsURLAllowlistedForPasswordEntry(const GURL& url) const override;

  bool IsInPasswordAlertMode(ReusedPasswordAccountType password_type) override;

  bool CanSendSamplePing() override;

  bool IsPingingEnabled(LoginReputationClientRequest::TriggerType trigger_type,
                        ReusedPasswordAccountType password_type) override;

  bool IsIncognito() override;

  bool IsExtendedReporting() override;

  bool IsEnhancedProtection() override;

  bool IsUserMBBOptedIn() override;

  bool IsHistorySyncEnabled() override;

  bool IsPrimaryAccountSyncing() const override;

  bool IsPrimaryAccountSignedIn() const override;

  bool IsPrimaryAccountGmail() const override;

  bool IsOtherGaiaAccountGmail(const std::string& username) const override;

  bool IsInExcludedCountry() override;

  // PasswordProtectionService override.
  void MaybeLogPasswordReuseLookupEvent(
      web::WebState* web_state,
      RequestOutcome outcome,
      PasswordType password_type,
      const LoginReputationClientResponse* response) override;

 protected:
  ChromeBrowserState* browser_state_;

 private:
  // Gets prefs associated with |browser_state_|.
  PrefService* GetPrefs();

  // Returns whether |browser_state_| has safe browsing service enabled.
  bool IsSafeBrowsingEnabled();
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
