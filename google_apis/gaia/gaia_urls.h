// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_URLS_H_
#define GOOGLE_APIS_GAIA_GAIA_URLS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "url/gurl.h"

// A signleton that provides all the URLs that are used for connecting to GAIA.
class GaiaUrls {
 public:
  static GaiaUrls* GetInstance();

  // The URLs for different calls in the Google Accounts programmatic login API.
  const GURL& google_url() const;
  const GURL& secure_google_url() const;
  const GURL& gaia_url() const;
  const GURL& captcha_base_url() const;
  const GURL& client_login_url() const;
  const GURL& service_login_url() const;
  const GURL& embedded_setup_chromeos_url(unsigned version) const;
  const GURL& embedded_setup_windows_url() const;
  const GURL& signin_chrome_sync_dice() const;
  const GURL& signin_chrome_sync_keys_url() const;
  const GURL& service_login_auth_url() const;
  const GURL& service_logout_url() const;
  const GURL& get_user_info_url() const;
  const GURL& token_auth_url() const;
  const GURL& merge_session_url() const;
  const GURL& get_oauth_token_url() const;
  const GURL& oauth_get_access_token_url() const;
  const GURL& oauth_multilogin_url() const;
  const GURL& oauth_wrap_bridge_url() const;
  const GURL& oauth_user_info_url() const;
  const GURL& oauth_revoke_token_url() const;
  const GURL& oauth1_login_url() const;
  const GURL& embedded_signin_url() const;
  const GURL& add_account_url() const;

  const std::string& oauth2_chrome_client_id() const;
  const std::string& oauth2_chrome_client_secret() const;
  const GURL& oauth2_auth_url() const;
  const GURL& oauth2_token_url() const;
  const GURL& oauth2_issue_token_url() const;
  const GURL& oauth2_token_info_url() const;
  const GURL& oauth2_revoke_url() const;
  const GURL& reauth_api_url() const;

  const GURL& gaia_login_form_realm() const;

  GURL ListAccountsURLWithSource(const std::string& source);
  GURL LogOutURLWithSource(const std::string& source);
  GURL GetCheckConnectionInfoURLWithSource(const std::string& source);

 private:
  GaiaUrls();
  ~GaiaUrls();

  friend struct base::DefaultSingletonTraits<GaiaUrls>;

  GURL google_url_;
  GURL secure_google_url_;
  GURL gaia_url_;
  GURL captcha_base_url_;

  GURL client_login_url_;
  GURL service_login_url_;
  GURL embedded_setup_chromeos_url_v2_;
  GURL embedded_setup_windows_url_;
  GURL signin_chrome_sync_dice_;
  GURL signin_chrome_sync_keys_url_;
  GURL service_login_auth_url_;
  GURL service_logout_url_;
  GURL continue_url_for_logout_;
  GURL get_user_info_url_;
  GURL token_auth_url_;
  GURL merge_session_url_;
  GURL get_oauth_token_url_;
  GURL oauth_get_access_token_url_;
  GURL oauth_wrap_bridge_url_;
  GURL oauth_multilogin_url_;
  GURL oauth_user_info_url_;
  GURL oauth_revoke_token_url_;
  GURL oauth1_login_url_;
  GURL list_accounts_url_;
  GURL embedded_signin_url_;
  GURL add_account_url_;
  GURL get_check_connection_info_url_;

  std::string oauth2_chrome_client_id_;
  std::string oauth2_chrome_client_secret_;

  GURL oauth2_auth_url_;
  GURL oauth2_token_url_;
  GURL oauth2_issue_token_url_;
  GURL oauth2_token_info_url_;
  GURL oauth2_revoke_url_;

  GURL reauth_api_url_;

  GURL gaia_login_form_realm_;

  DISALLOW_COPY_AND_ASSIGN(GaiaUrls);
};

#endif  // GOOGLE_APIS_GAIA_GAIA_URLS_H_
