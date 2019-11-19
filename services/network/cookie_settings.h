// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COOKIE_SETTINGS_H_
#define SERVICES_NETWORK_COOKIE_SETTINGS_H_

#include "base/component_export.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "services/network/session_cleanup_cookie_store.h"

class GURL;

namespace network {

// Handles cookie access and deletion logic for the network service.
class COMPONENT_EXPORT(NETWORK_SERVICE) CookieSettings
    : public content_settings::CookieSettingsBase {
 public:
  CookieSettings();
  ~CookieSettings() override;

  void set_content_settings(const ContentSettingsForOneType& content_settings) {
    content_settings_ = content_settings;
  }

  void set_block_third_party_cookies(bool block_third_party_cookies) {
    block_third_party_cookies_ = block_third_party_cookies;
  }

  void set_secure_origin_cookies_allowed_schemes(
      const std::vector<std::string>& secure_origin_cookies_allowed_schemes) {
    secure_origin_cookies_allowed_schemes_.clear();
    secure_origin_cookies_allowed_schemes_.insert(
        secure_origin_cookies_allowed_schemes.begin(),
        secure_origin_cookies_allowed_schemes.end());
  }

  void set_matching_scheme_cookies_allowed_schemes(
      const std::vector<std::string>& matching_scheme_cookies_allowed_schemes) {
    matching_scheme_cookies_allowed_schemes_.clear();
    matching_scheme_cookies_allowed_schemes_.insert(
        matching_scheme_cookies_allowed_schemes.begin(),
        matching_scheme_cookies_allowed_schemes.end());
  }

  void set_third_party_cookies_allowed_schemes(
      const std::vector<std::string>& third_party_cookies_allowed_schemes) {
    third_party_cookies_allowed_schemes_.clear();
    third_party_cookies_allowed_schemes_.insert(
        third_party_cookies_allowed_schemes.begin(),
        third_party_cookies_allowed_schemes.end());
  }

  void set_content_settings_for_legacy_cookie_access(
      const ContentSettingsForOneType& settings) {
    settings_for_legacy_cookie_access_ = settings;
  }

  // Returns a predicate that takes the domain of a cookie and a bool whether
  // the cookie is secure and returns true if the cookie should be deleted on
  // exit.
  SessionCleanupCookieStore::DeleteCookiePredicate
  CreateDeleteCookieOnExitPredicate() const;

  // content_settings::CookieSettingsBase:
  void GetSettingForLegacyCookieAccess(const std::string& cookie_domain,
                                       ContentSetting* setting) const override;
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const GURL& site_for_cookies) const override;

 private:
  // Returns whether third-party cookie blocking should be bypassed (i.e. always
  // allow the cookie regardless of cookie content settings and third-party
  // cookie blocking settings.
  // This just checks the scheme of the |url| and |site_for_cookies|:
  //  - Allow cookies if the |site_for_cookies| is a chrome:// scheme URL, and
  //    the |url| has a secure scheme.
  //  - Allow cookies if the |site_for_cookies| and the |url| match in scheme
  //    and both have the Chrome extensions scheme.
  bool ShouldAlwaysAllowCookies(const GURL& url,
                                const GURL& first_party_url) const;

  // content_settings::CookieSettingsBase:
  void GetCookieSettingInternal(const GURL& url,
                                const GURL& first_party_url,
                                bool is_third_party_request,
                                content_settings::SettingSource* source,
                                ContentSetting* cookie_setting) const override;

  // Returns true if at least one content settings is session only.
  bool HasSessionOnlyOrigins() const;

  ContentSettingsForOneType content_settings_;
  bool block_third_party_cookies_ = false;
  std::set<std::string> secure_origin_cookies_allowed_schemes_;
  std::set<std::string> matching_scheme_cookies_allowed_schemes_;
  std::set<std::string> third_party_cookies_allowed_schemes_;
  ContentSettingsForOneType settings_for_legacy_cookie_access_;

  DISALLOW_COPY_AND_ASSIGN(CookieSettings);
};

}  // namespace network

#endif  // SERVICES_NETWORK_COOKIE_SETTINGS_H_
