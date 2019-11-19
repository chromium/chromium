// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include <functional>

#include "base/bind.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "net/cookies/cookie_util.h"

namespace network {
namespace {
bool IsDefaultSetting(const ContentSettingPatternSource& setting) {
  return setting.primary_pattern.MatchesAllHosts() &&
         setting.secondary_pattern.MatchesAllHosts();
}
}  // namespace

CookieSettings::CookieSettings() {}
CookieSettings::~CookieSettings() {}

SessionCleanupCookieStore::DeleteCookiePredicate
CookieSettings::CreateDeleteCookieOnExitPredicate() const {
  if (!HasSessionOnlyOrigins())
    return SessionCleanupCookieStore::DeleteCookiePredicate();
  return base::BindRepeating(&CookieSettings::ShouldDeleteCookieOnExit,
                             base::Unretained(this),
                             std::cref(content_settings_));
}

void CookieSettings::GetSettingForLegacyCookieAccess(
    const std::string& cookie_domain,
    ContentSetting* setting) const {
  DCHECK(setting);

  // Default to match what was registered in the ContentSettingsRegistry.
  *setting = net::cookie_util::IsSameSiteByDefaultCookiesEnabled()
                 ? CONTENT_SETTING_BLOCK
                 : CONTENT_SETTING_ALLOW;

  if (settings_for_legacy_cookie_access_.empty())
    return;

  // If there are no domain-specific settings, return early to avoid the cost of
  // constructing a GURL to match against.
  bool has_non_wildcard_setting = false;
  for (const auto& entry : settings_for_legacy_cookie_access_) {
    if (!entry.primary_pattern.MatchesAllHosts()) {
      has_non_wildcard_setting = true;
      break;
    }
  }
  if (!has_non_wildcard_setting) {
    // Take the first entry because we know all entries match any host.
    *setting = settings_for_legacy_cookie_access_[0].GetContentSetting();
    DCHECK(IsValidSettingForLegacyAccess(*setting));
    return;
  }

  // The content setting patterns are treated as domains, not URLs, so the
  // scheme is irrelevant (so we can just arbitrarily pass false).
  GURL cookie_domain_url = net::cookie_util::CookieOriginToURL(
      cookie_domain, false /* secure scheme */);

  for (const auto& entry : settings_for_legacy_cookie_access_) {
    // TODO(crbug.com/1015611): This should ignore scheme and port, but
    // currently takes them into account. It says in the policy description that
    // specifying a scheme or port in the pattern may lead to undefined
    // behavior, but this is not ideal.
    if (entry.primary_pattern.Matches(cookie_domain_url)) {
      *setting = entry.GetContentSetting();
      DCHECK(IsValidSettingForLegacyAccess(*setting));
      return;
    }
  }
}

bool CookieSettings::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const GURL& site_for_cookies) const {
  return base::Contains(secure_origin_cookies_allowed_schemes_,
                        site_for_cookies.scheme()) &&
         url.SchemeIsCryptographic();
}

bool CookieSettings::ShouldAlwaysAllowCookies(
    const GURL& url,
    const GURL& first_party_url) const {
  if (base::Contains(secure_origin_cookies_allowed_schemes_,
                     first_party_url.scheme()) &&
      url.SchemeIsCryptographic()) {
    return true;
  }

  if (base::Contains(matching_scheme_cookies_allowed_schemes_, url.scheme()) &&
      url.SchemeIs(first_party_url.scheme_piece())) {
    return true;
  }

  return false;
}

void CookieSettings::GetCookieSettingInternal(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    content_settings::SettingSource* source,
    ContentSetting* cookie_setting) const {
  DCHECK(cookie_setting);
  if (ShouldAlwaysAllowCookies(url, first_party_url)) {
    *cookie_setting = CONTENT_SETTING_ALLOW;
    return;
  }

  // Default to allowing cookies.
  *cookie_setting = CONTENT_SETTING_ALLOW;
  bool block_third = block_third_party_cookies_ &&
                     !base::Contains(third_party_cookies_allowed_schemes_,
                                     first_party_url.scheme());
  for (const auto& entry : content_settings_) {
    if (entry.primary_pattern.Matches(url) &&
        entry.secondary_pattern.Matches(first_party_url)) {
      *cookie_setting = entry.GetContentSetting();
      // Only continue to block third party cookies if there is not an explicit
      // exception.
      if (!IsDefaultSetting(entry))
        block_third = false;
      break;
    }
  }

  if (block_third && is_third_party_request)
    *cookie_setting = CONTENT_SETTING_BLOCK;
}

bool CookieSettings::HasSessionOnlyOrigins() const {
  for (const auto& entry : content_settings_) {
    if (entry.GetContentSetting() == CONTENT_SETTING_SESSION_ONLY)
      return true;
  }
  return false;
}

}  // namespace network
