// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#include <vector>

#include "base/strings/string16.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#import "components/safe_browsing/ios/password_protection/password_protection_service.h"
#include "components/sync/protocol/gaia_password_reuse.pb.h"

class ChromeBrowserState;
class GURL;
class PrefService;

namespace password_manager {
class PasswordStore;
}

namespace web {
class WebState;
}

namespace safe_browsing {

class ChromePasswordProtectionService : public PasswordProtectionService,
                                        public KeyedService {
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

  // Records a Chrome Sync event that sync password reuse was detected.
  void MaybeLogPasswordReuseDetectedEvent(web::WebState* web_state);

  // Records a Chrome Sync event with the result of the user's interaction with
  // the warning dialog.
  void MaybeLogPasswordReuseDialogInteraction(
      int64_t navigation_id,
      sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction::
          InteractionResult interaction_result);

  // Gets the detailed warning text that should show in the modal warning
  // dialog. |placeholder_offsets| are the start points/indices of the
  // placeholders that are passed into the resource string. It is only set for
  // saved passwords.
  base::string16 GetWarningDetailText(
      ReusedPasswordAccountType password_type,
      std::vector<size_t>* placeholder_offsets) const;

  // Gets the warning text for saved password reuse warnings.
  // |placeholder_offsets| are the start points/indices of the placeholders that
  // are passed into the resource string.
  base::string16 GetWarningDetailTextForSavedPasswords(
      std::vector<size_t>* placeholder_offsets) const;

  // Gets the warning text of the saved password reuse warnings that tells the
  // user to check their saved passwords. |placeholder_offsets| are the start
  // points/indices of the placeholders that are passed into the resource
  // string.
  base::string16 GetWarningDetailTextToCheckSavedPasswords(
      std::vector<size_t>* placeholder_offsets) const;

  // Get placeholders for the warning detail text for saved password reuse
  // warnings.
  std::vector<base::string16> GetPlaceholdersForSavedPasswordWarningText()
      const;

 private:
  password_manager::PasswordStore* GetStoreForReusedCredential(
      const password_manager::MatchingReusedCredential& reused_credential);

  // Returns the profile PasswordStore associated with this instance.
  password_manager::PasswordStore* GetProfilePasswordStore() const;

  // Returns the GAIA-account-scoped PasswordStore associated with this
  // instance. The account password store contains passwords stored in the
  // account and is accessible only when the user is signed in and non syncing.
  password_manager::PasswordStore* GetAccountPasswordStore() const;

  // Gets prefs associated with |browser_state_|.
  PrefService* GetPrefs();

  // Returns whether |browser_state_| has safe browsing service enabled.
  bool IsSafeBrowsingEnabled();

  ChromeBrowserState* browser_state_;
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
