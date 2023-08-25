// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include <functional>
#include <iterator>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/types/optional_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "net/base/network_delegate.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/static_cookie_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace {

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

// Check whether the allowed cookie should add `WARN_THIRD_PARTY_PHASEOUT`
// reason. `block_third_party_cookies` should be the global setting of whether
// or not third party cookies is blocked.
bool ShouldWarnThirdPartyCookiePhaseout(const bool block_third_party_cookies,
                                        const bool is_third_party_request,
                                        const bool is_cookie_partitioned,
                                        const bool is_explicit_setting) {
  return !block_third_party_cookies && is_third_party_request &&
         !is_cookie_partitioned && !is_explicit_setting;
}

// Check whether the blocked cookie should add `EXCLUDE_THIRD_PARTY_PHASEOUT`
// reason. `block_third_party_cookies` should be the global setting of whether
// or not third party cookies is blocked.
bool ShouldExcludeThirdPartyCookiePhaseout(const bool block_third_party_cookies,
                                           const bool is_third_party_request,
                                           const bool is_cookie_partitioned,
                                           const bool is_explicit_setting) {
  return block_third_party_cookies && is_third_party_request &&
         !is_cookie_partitioned && !is_explicit_setting;
}

}  // namespace

// static
bool CookieSettings::IsCookieAllowed(const net::CanonicalCookie& cookie,
                                     const CookieSettingWithMetadata& setting) {
  return IsAllowed(setting.cookie_setting()) ||
         (cookie.IsPartitioned() && setting.IsPartitionedStateAllowed());
}

// static
net::NetworkDelegate::PrivacySetting CookieSettings::PrivacySetting(
    const CookieSettingWithMetadata& setting) {
  if (IsAllowed(setting.cookie_setting())) {
    return net::NetworkDelegate::PrivacySetting::kStateAllowed;
  }

  if (setting.IsPartitionedStateAllowed()) {
    return net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly;
  }

  return net::NetworkDelegate::PrivacySetting::kStateDisallowed;
}

CookieSettings::CookieSettings() {
  // Allow cookies by default until we receive CookieSettings.
  set_content_settings({});
}

CookieSettings::~CookieSettings() = default;

DeleteCookiePredicate CookieSettings::CreateDeleteCookieOnExitPredicate()
    const {
  if (!HasSessionOnlyOrigins())
    return DeleteCookiePredicate();
  return base::BindRepeating(&CookieSettings::ShouldDeleteCookieOnExit,
                             base::Unretained(this),
                             std::cref(content_settings_));
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
    net::CookieSettingOverrides overrides,
    net::CookieInclusionStatus* cookie_inclusion_status) const {
  const CookieSettingWithMetadata setting_with_metadata =
      GetCookieSettingWithMetadata(url, site_for_cookies,
                                   base::OptionalToPtr(top_frame_origin),
                                   overrides);
  bool allowed = IsCookieAllowed(cookie, setting_with_metadata);
  bool is_third_party_request = IsThirdPartyRequest(url, site_for_cookies);
  if (cookie_inclusion_status) {
    if (allowed) {
      if (ShouldWarnThirdPartyCookiePhaseout(
              block_third_party_cookies_, is_third_party_request,
              cookie.IsPartitioned(),
              setting_with_metadata.is_explicit_setting())) {
        cookie_inclusion_status->AddWarningReason(
            net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT);
      }
    } else {
      if (ShouldExcludeThirdPartyCookiePhaseout(
              net::cookie_util::IsForceThirdPartyCookieBlockingEnabled(),
              is_third_party_request, cookie.IsPartitioned(),
              setting_with_metadata.is_explicit_setting())) {
        cookie_inclusion_status->AddExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT);
      } else {
        cookie_inclusion_status->AddExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
      }
    }
  }
  return allowed;
}

// Returns whether third-party cookie blocking should be bypassed (i.e. always
// allow the cookie regardless of cookie content settings and third-party
// cookie blocking settings.
// This just checks the scheme of the |url| and |site_for_cookies|:
//  - Allow cookies if the |site_for_cookies| is a chrome:// scheme URL, and
//    the |url| has a secure scheme.
//  - Allow cookies if the |site_for_cookies| and the |url| match in scheme
//    and both have the Chrome extensions scheme.
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
  return PrivacySetting(GetCookieSettingWithMetadata(
      url, site_for_cookies, base::OptionalToPtr(top_frame_origin), overrides));
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
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin,
    net::CookieSettingOverrides overrides) const {
  return GetCookieSettingInternal(
      url, GetFirstPartyURL(site_for_cookies, top_frame_origin),
      IsThirdPartyRequest(url, site_for_cookies), overrides, nullptr);
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
  bool is_any_allowed = false;
  if (IsAllowed(setting_with_metadata.cookie_setting())) {
    is_any_allowed = true;
  }

  bool is_third_party_request = IsThirdPartyRequest(url, site_for_cookies);
  // Add `WARN_THIRD_PARTY_PHASEOUT` `WarningReason` for allowed cookies
  // that meets the conditions and add the `ExclusionReason` for cookies
  // that ought to be blocked.
  for (net::CookieWithAccessResult& cookie : maybe_included_cookies) {
    if (IsCookieAllowed(cookie.cookie, setting_with_metadata)) {
      is_any_allowed = true;

      if (ShouldWarnThirdPartyCookiePhaseout(
              block_third_party_cookies_, is_third_party_request,
              cookie.cookie.IsPartitioned(),
              setting_with_metadata.is_explicit_setting())) {
        cookie.access_result.status.AddWarningReason(
            net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT);
      }
    } else {
      // Use a different exclusion reason when the 3pc is blocked by browser.
      if (ShouldExcludeThirdPartyCookiePhaseout(
              net::cookie_util::IsForceThirdPartyCookieBlockingEnabled(),
              is_third_party_request, cookie.cookie.IsPartitioned(),
              setting_with_metadata.is_explicit_setting())) {
        cookie.access_result.status.AddExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT);
      } else {
        // User has a explicit setting to block 3pc.
        cookie.access_result.status.AddExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
      }
      if (setting_with_metadata.BlockedByThirdPartyCookieBlocking() &&
          first_party_set_metadata.AreSitesInSameFirstPartySet()) {
        cookie.access_result.status.AddExclusionReason(
            net::CookieInclusionStatus::
                EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET);
      }
    }
  }
  for (net::CookieWithAccessResult& cookie : excluded_cookies) {
    if (!IsCookieAllowed(cookie.cookie, setting_with_metadata)) {
      // Use a different exclusion reason when the 3pc is blocked by browser.
      if (ShouldExcludeThirdPartyCookiePhaseout(
              net::cookie_util::IsForceThirdPartyCookieBlockingEnabled(),
              is_third_party_request, cookie.cookie.IsPartitioned(),
              setting_with_metadata.is_explicit_setting())) {
        cookie.access_result.status.AddExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT);
      } else {
        // User has a explicit setting to block 3pc.
        cookie.access_result.status.AddExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
      }
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

const ContentSettingsForOneType& CookieSettings::GetContentSettings(
    ContentSettingsType type) const {
  switch (type) {
    case ContentSettingsType::COOKIES:
      return content_settings_;
    case ContentSettingsType::STORAGE_ACCESS:
      return storage_access_grants_;
    case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      return top_level_storage_access_grants_;
    case ContentSettingsType::LEGACY_COOKIE_ACCESS:
      return settings_for_legacy_cookie_access_;
    case ContentSettingsType::TPCD_SUPPORT:
      return settings_for_3pcd_;
    case ContentSettingsType::TPCD_METADATA_GRANTS:
      return settings_for_3pcd_metadata_grants_;
    default:
      // Only implements types that are actually used by CookieSettings since
      // settings need to be copied to the network service.
      NOTREACHED_NORETURN() << static_cast<int>(type);
  }
}

ContentSetting CookieSettings::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    content_settings::SettingInfo* info) const {
  const ContentSettingPatternSource* result = FindMatchingSetting(
      primary_url, secondary_url, GetContentSettings(content_type));

  if (!result) {
    if (info) {
      info->primary_pattern = ContentSettingsPattern::Wildcard();
      info->secondary_pattern = ContentSettingsPattern::Wildcard();
    }
    return CONTENT_SETTING_BLOCK;
  }

  if (info) {
    info->primary_pattern = result->primary_pattern;
    info->secondary_pattern = result->secondary_pattern;
    info->metadata = result->metadata;
  }
  return result->GetContentSetting();
}

bool CookieSettings::IsThirdPartyCookiesAllowedScheme(
    const std::string& scheme) const {
  return base::Contains(third_party_cookies_allowed_schemes_, scheme);
}

bool CookieSettings::ShouldBlockThirdPartyCookies() const {
  return block_third_party_cookies_;
}

bool CookieSettings::IsStorageAccessApiEnabled() const {
  // The network service relies on the browser process passing
  // storage_access_grants_ correctly.
  return true;
}

}  // namespace network
