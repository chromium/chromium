// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_URLS_H_
#define GOOGLE_APIS_GAIA_GAIA_URLS_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/singleton.h"
#include "url/gurl.h"
#include "url/origin.h"

// A singleton that provides all the URLs that are used for connecting to GAIA.
//
// Please update InitializeFromConfig() when adding new URLs.
class COMPONENT_EXPORT(GOOGLE_APIS) GaiaUrls {
 public:
  static GaiaUrls* GetInstance();

  // Public for testing, otherwise use singleton above.
  GaiaUrls();
  ~GaiaUrls();

  GaiaUrls(const GaiaUrls&) = delete;
  GaiaUrls& operator=(const GaiaUrls&) = delete;

  // Simplifies unit tests that depend on `GaiaUrls` singleton. Set to `nullptr`
  // to reset.
  static void SetInstanceForTesting(GaiaUrls* gaia_urls);

  // The URLs for different calls in the Google Accounts programmatic login API.
  const GURL& google_url() const;
  const GURL& secure_google_url() const;
  const url::Origin& gaia_origin() const;
  GURL gaia_url() const;
  const GURL& embedded_setup_chromeos_url(unsigned version) const;
  const GURL& embedded_setup_chromeos_kid_signup_url() const;
  const GURL& embedded_setup_chromeos_kid_signin_url() const;
  const GURL& embedded_setup_windows_url() const;
  const GURL& embedded_reauth_chromeos_url() const;
  const GURL& signin_chrome_sync_dice() const;
  const GURL& signin_chrome_sync_keys_retrieval_url() const;
  const GURL& signin_chrome_sync_keys_recoverability_degraded_url() const;
  const GURL& service_logout_url() const;
  const GURL& merge_session_url() const;
  const GURL& oauth_multilogin_url() const;
  const GURL& oauth_user_info_url() const;
  const GURL& oauth1_login_url() const;
  const GURL& embedded_signin_url() const;
  const GURL& add_account_url() const;
  const GURL& reauth_url() const;
  const GURL& account_capabilities_url() const;

  const std::string& oauth2_chrome_client_id() const;
  const std::string& oauth2_chrome_client_secret() const;
  const GURL& oauth2_token_url() const;
  const GURL& oauth2_issue_token_url() const;
  const GURL& oauth2_token_info_url() const;
  const GURL& oauth2_revoke_url() const;
  const GURL& reauth_api_url() const;

  // The base URL for communicating with the google api server.
  const GURL& google_apis_origin_url() const;

  GURL ListAccountsURLWithSource(const std::string& source);
  GURL LogOutURLWithSource(const std::string& source);
  GURL GetCheckConnectionInfoURLWithSource(const std::string& source);

 private:
  friend struct base::DefaultSingletonTraits<GaiaUrls>;

  void InitializeDefault();
  void InitializeFromConfig();

  GURL google_url_;
  GURL secure_google_url_;
  url::Origin gaia_origin_;

  GURL lso_origin_url_;
  GURL google_apis_origin_url_;
  GURL oauth_account_manager_origin_url_;
  GURL account_capabilities_origin_url_;

  GURL embedded_setup_chromeos_url_v2_;
  GURL embedded_setup_chromeos_kid_signup_url_;
  GURL embedded_setup_chromeos_kid_signin_url_;
  GURL embedded_setup_windows_url_;
  GURL embedded_reauth_chromeos_url_;
  GURL signin_chrome_sync_dice_;
  GURL signin_chrome_sync_keys_retrieval_url_;
  GURL signin_chrome_sync_keys_recoverability_degraded_url_;
  GURL service_logout_url_;
  GURL continue_url_for_logout_;
  GURL merge_session_url_;
  GURL oauth_multilogin_url_;
  GURL oauth_user_info_url_;
  GURL oauth1_login_url_;
  GURL list_accounts_url_;
  GURL embedded_signin_url_;
  GURL add_account_url_;
  GURL reauth_url_;
  GURL account_capabilities_url_;
  GURL get_check_connection_info_url_;

  std::string oauth2_chrome_client_id_;
  std::string oauth2_chrome_client_secret_;

  GURL oauth2_token_url_;
  GURL oauth2_issue_token_url_;
  GURL oauth2_token_info_url_;
  GURL oauth2_revoke_url_;

  GURL reauth_api_url_;
};

#endif  // GOOGLE_APIS_GAIA_GAIA_URLS_H_
