// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include <functional>
#include <iterator>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/types/optional_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/static_cookie_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
namespace {

bool IsExplicitSetting(const ContentSettingPatternSource& setting) {
  return !setting.primary_pattern.MatchesAllHosts() ||
         !setting.secondary_pattern.MatchesAllHosts();
}

const ContentSettingPatternSource* FindMatchingSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    const ContentSettingsForOneType& settings) {
  // We assume `settings` is sorted in order of precedence, so we use the first
  // matching rule we find.
  const auto& entry = base::ranges::find_if(
      settings, [&](const ContentSettingPatternSource& entry) {
        // The primary pattern is for the request URL; the secondary pattern
        // is for the first-party URL (which is the top-frame origin [if
        // available] or the site-for-cookies).
        return !entry.IsExpired() &&
               entry.primary_pattern.Matches(primary_url) &&
               entry.secondary_pattern.Matches(secondary_url);
      });
  return entry == settings.end() ? nullptr : &*entry;
}

}  // namespace

CookieSettings::CookieSettingWithMetadata::CookieSettingWithMetadata(
    ContentSetting cookie_setting,
    absl::optional<ThirdPartyBlockingScope> third_party_blocking_scope)
    : cookie_setting_(cookie_setting),
      third_party_blocking_scope_(third_party_blocking_scope) {
  DCHECK(!third_party_blocking_scope_.has_value() ||
         !IsAllowed(cookie_setting_));
}

bool CookieSettings::CookieSettingWithMetadata::
    BlockedByThirdPartyCookieBlocking() const {
  return !IsAllowed(cookie_setting_) && third_party_blocking_scope_.has_value();
}

bool CookieSettings::CookieSettingWithMetadata::IsCookieAllowed(
    const net::CanonicalCookie& cookie) const {
  return IsAllowed(cookie_setting_) ||
         (cookie.IsPartitioned() && IsPartitionedStateAllowed());
}

net::NetworkDelegate::PrivacySetting
CookieSettings::CookieSettingWithMetadata::PrivacySetting() const {
  if (IsAllowed(cookie_setting_)) {
    return net::NetworkDelegate::PrivacySetting::kStateAllowed;
  }

  if (IsPartitionedStateAllowed()) {
    return net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly;
  }

  return net::NetworkDelegate::PrivacySetting::kStateDisallowed;
}

bool CookieSettings::CookieSettingWithMetadata::IsPartitionedStateAllowed()
    const {
  return IsAllowed(cookie_setting_) ||
         third_party_blocking_scope_ ==
             ThirdPartyBlockingScope::kUnpartitionedOnly;
}

CookieSettings::CookieSettings() = default;

CookieSettings::~CookieSettings() = default;

DeleteCookiePredicate CookieSettings::CreateDeleteCookieOnExitPredicate()
    const {
  if (!HasSessionOnlyOrigins())
    return DeleteCookiePredicate();
  return base::BindRepeating(&CookieSettings::ShouldDeleteCookieOnExit,
                             base::Unretained(this),
                             std::cref(content_settings_));
}

ContentSetting CookieSettings::GetSettingForLegacyCookieAccess(
    const std::string& cookie_domain) const {
  // Default to match what was registered in the ContentSettingsRegistry.
  ContentSetting setting = CONTENT_SETTING_BLOCK;

  if (settings_for_legacy_cookie_access_.empty())
    return setting;

  // If there are no domain-specific settings, return early to avoid the cost of
  // constructing a GURL to match against.
  if (base::ranges::all_of(settings_for_legacy_cookie_access_,
                           [](const ContentSettingPatternSource& entry) {
                             return entry.primary_pattern.MatchesAllHosts();
                           })) {
    // Take the first entry because we know all entries match any host.
    setting = settings_for_legacy_cookie_access_[0].GetContentSetting();
    DCHECK(IsValidSettingForLegacyAccess(setting));
    return setting;
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
      DCHECK(IsValidSettingForLegacyAccess(entry.GetContentSetting()));
      return entry.GetContentSetting();
    }
  }

  return setting;
}

bool CookieSettings::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  return base::Contains(secure_origin_cookies_allowed_schemes_,
                        site_for_cookies.scheme()) &&
         url.SchemeIsCryptographic();
}

bool CookieSettings::IsCookieAccessible(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    net::CookieSettingOverrides overrides) const {
  return GetCookieSettingWithMetadata(
             url,
             GetFirstPartyURL(site_for_cookies,
                              base::OptionalToPtr(top_frame_origin)),
             IsThirdPartyRequest(url, site_for_cookies), overrides)
      .IsCookieAllowed(cookie);
}

bool CookieSettings::ShouldAlwaysAllowCookies(
    const GURL& url,
    const GURL& first_party_url) const {
  return (base::Contains(secure_origin_cookies_allowed_schemes_,
                         first_party_url.scheme()) &&
          url.SchemeIsCryptographic()) ||
         (base::Contains(matching_scheme_cookies_allowed_schemes_,
                         url.scheme()) &&
          url.SchemeIs(first_party_url.scheme_piece()));
}

net::NetworkDelegate::PrivacySetting CookieSettings::IsPrivacyModeEnabled(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    net::CookieSettingOverrides overrides) const {
  return GetCookieSettingWithMetadata(url, site_for_cookies,
                                      base::OptionalToPtr(top_frame_origin),
                                      overrides)
      .PrivacySetting();
}

CookieSettings::ThirdPartyBlockingScope
CookieSettings::GetThirdPartyBlockingScope(const GURL& first_party_url) const {
  // If cookies are allowed for the first-party URL then we allow
  // partitioned cross-site cookies.
  if (const ContentSettingPatternSource* match = FindMatchingSetting(
          first_party_url, first_party_url, content_settings_);
      !match || match->GetContentSetting() == CONTENT_SETTING_ALLOW) {
    return ThirdPartyBlockingScope::kUnpartitionedOnly;
  }
  return ThirdPartyBlockingScope::kUnpartitionedAndPartitioned;
}

CookieSettings::CookieSettingWithMetadata
CookieSettings::GetCookieSettingWithMetadata(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    net::CookieSettingOverrides overrides) const {
  if (ShouldAlwaysAllowCookies(url, first_party_url)) {
    return {/*cookie_setting=*/CONTENT_SETTING_ALLOW,
            /*third_party_blocking_scope=*/absl::nullopt};
  }

  // Default to allowing cookies.
  ContentSetting cookie_setting = CONTENT_SETTING_ALLOW;
  net::cookie_util::StorageAccessResult storage_access_result =
      net::cookie_util::StorageAccessResult::ACCESS_ALLOWED;
  absl::optional<ThirdPartyBlockingScope> third_party_blocking_scope;

  bool found_explicit_setting = false;
  if (const ContentSettingPatternSource* match =
          FindMatchingSetting(url, first_party_url, content_settings_);
      match) {
    cookie_setting = match->GetContentSetting();
    found_explicit_setting = IsExplicitSetting(*match);
    if (cookie_setting == CONTENT_SETTING_BLOCK) {
      storage_access_result =
          net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
    }
  }

  if (cookie_setting != CONTENT_SETTING_BLOCK && !found_explicit_setting &&
      block_third_party_cookies_ && is_third_party_request &&
      !base::Contains(third_party_cookies_allowed_schemes_,
                      first_party_url.scheme())) {
    // Cookie access will be blocked by the third-party-cookie-blocking
    // setting, unless there's an applicable override.
    //
    // We optimistically search for an applicable override before changing the
    // setting to `CONTENT_SETTING_BLOCK` so as not to accidentally change the
    // setting from `CONTENT_SETTING_SESSION_ONLY` to `CONTENT_SETTING_ALLOW` or
    // vice versa.
    if (ShouldConsiderStorageAccessGrants(overrides) &&
        IsAllowedByStorageAccessGrant(url, first_party_url)) {
      storage_access_result = net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT;
    } else if (ShouldConsiderTopLevelStorageAccessGrants(overrides) &&
               IsAllowedByTopLevelStorageAccessGrant(url, first_party_url)) {
      storage_access_result = net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT;
    } else if (overrides.Has(
                   net::CookieSettingOverride::kForceThirdPartyByUser)) {
      storage_access_result =
          net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_FORCED;
    } else {
      cookie_setting = CONTENT_SETTING_BLOCK;
      storage_access_result =
          net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
      third_party_blocking_scope = GetThirdPartyBlockingScope(first_party_url);
    }
  }

  FireStorageAccessHistogram(storage_access_result);

  return {cookie_setting, third_party_blocking_scope};
}

CookieSettings::CookieSettingWithMetadata
CookieSettings::GetCookieSettingWithMetadata(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin,
    net::CookieSettingOverrides overrides) const {
  return GetCookieSettingWithMetadata(
      url, GetFirstPartyURL(site_for_cookies, top_frame_origin),
      IsThirdPartyRequest(url, site_for_cookies), overrides);
}

ContentSetting CookieSettings::GetCookieSettingInternal(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    net::CookieSettingOverrides overrides,
    content_settings::SettingSource* source) const {
  return GetCookieSettingWithMetadata(url, first_party_url,
                                      is_third_party_request, overrides)
      .cookie_setting();
}

bool CookieSettings::AnnotateAndMoveUserBlockedCookies(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin,
    const net::FirstPartySetMetadata& first_party_set_metadata,
    net::CookieSettingOverrides overrides,
    net::CookieAccessResultList& maybe_included_cookies,
    net::CookieAccessResultList& excluded_cookies) const {
  const CookieSettingWithMetadata setting_with_metadata =
      GetCookieSettingWithMetadata(url, site_for_cookies, top_frame_origin,
                                   overrides);

  if (IsAllowed(setting_with_metadata.cookie_setting())) {
    return true;
  }

  // Add the `EXCLUDE_USER_PREFERENCES` `ExclusionReason` for cookies that ought
  // to be blocked, and find any cookies that should still be allowed.
  bool is_any_allowed = false;
  for (net::CookieWithAccessResult& cookie : maybe_included_cookies) {
    if (setting_with_metadata.IsCookieAllowed(cookie.cookie)) {
      is_any_allowed = true;
    } else {
      cookie.access_result.status.AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
      if (setting_with_metadata.BlockedByThirdPartyCookieBlocking() &&
          first_party_set_metadata.AreSitesInSameFirstPartySet()) {
        cookie.access_result.status.AddExclusionReason(
            net::CookieInclusionStatus::
                EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET);
      }
    }
  }
  for (net::CookieWithAccessResult& cookie : excluded_cookies) {
    if (!setting_with_metadata.IsCookieAllowed(cookie.cookie)) {
      cookie.access_result.status.AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
    }
  }
  const auto to_be_moved = base::ranges::stable_partition(
      maybe_included_cookies, [](const net::CookieWithAccessResult& cookie) {
        return cookie.access_result.status.IsInclude();
      });
  excluded_cookies.insert(
      excluded_cookies.end(), std::make_move_iterator(to_be_moved),
      std::make_move_iterator(maybe_included_cookies.end()));
  maybe_included_cookies.erase(to_be_moved, maybe_included_cookies.end());

  net::cookie_util::DCheckIncludedAndExcludedCookieLists(maybe_included_cookies,
                                                         excluded_cookies);

  return is_any_allowed;
}

bool CookieSettings::HasSessionOnlyOrigins() const {
  return base::ranges::any_of(content_settings_, [](const auto& entry) {
    return entry.GetContentSetting() == CONTENT_SETTING_SESSION_ONLY;
  });
}

bool CookieSettings::IsAllowedByStorageAccessGrant(
    const GURL& url,
    const GURL& first_party_url) const {
  const ContentSettingPatternSource* match =
      FindMatchingSetting(url, first_party_url, storage_access_grants_);
  return match && match->GetContentSetting() == CONTENT_SETTING_ALLOW;
}

bool CookieSettings::IsAllowedByTopLevelStorageAccessGrant(
    const GURL& url,
    const GURL& first_party_url) const {
  const ContentSettingPatternSource* match = FindMatchingSetting(
      url, first_party_url, top_level_storage_access_grants_);
  return match && match->GetContentSetting() == CONTENT_SETTING_ALLOW;
}

}  // namespace network
