// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include <functional>
#include <iterator>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/static_cookie_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
namespace {

using SamePartyCookieContextType = net::SamePartyContext::Type;

bool IsExplicitSetting(const ContentSettingPatternSource& setting) {
  return !setting.primary_pattern.MatchesAllHosts() ||
         !setting.secondary_pattern.MatchesAllHosts();
}

}  // namespace

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

void CookieSettings::GetSettingForLegacyCookieAccess(
    const std::string& cookie_domain,
    ContentSetting* setting) const {
  DCHECK(setting);

  // Default to match what was registered in the ContentSettingsRegistry.
  *setting = CONTENT_SETTING_BLOCK;

  if (settings_for_legacy_cookie_access_.empty())
    return;

  // If there are no domain-specific settings, return early to avoid the cost of
  // constructing a GURL to match against.
  if (base::ranges::all_of(settings_for_legacy_cookie_access_,
                           [](const ContentSettingPatternSource& entry) {
                             return entry.primary_pattern.MatchesAllHosts();
                           })) {
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
    const net::SiteForCookies& site_for_cookies) const {
  return base::Contains(secure_origin_cookies_allowed_schemes_,
                        site_for_cookies.scheme()) &&
         url.SchemeIsCryptographic();
}

bool CookieSettings::IsCookieAccessible(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin) const {
  return IsHypotheticalCookieAllowed(
      GetCookieSettingWithMetadata(
          url,
          GetFirstPartyURL(site_for_cookies,
                           base::OptionalOrNullptr(top_frame_origin)),
          IsThirdPartyRequest(url, site_for_cookies)),
      cookie.IsSameParty(), cookie.IsPartitioned(), /*record_metrics=*/true);
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
    SamePartyCookieContextType same_party_cookie_context_type) const {
  // PrivacySetting should be kStateDisallowed iff no cookies should ever
  // be sent on this request. E.g.:
  //
  // * if cookie settings block cookies on this site or for this URL; or
  //
  // * if cookie settings block 3P cookies, the context is cross-party, and
  // content settings blocks the 1P from using cookies; or
  //
  // * if cookie settings block 3P cookies, and the context is same-party, but
  // SameParty cookies aren't considered 1P.
  //
  // PrivacySetting should be kPartitionedStateAllowedOnly iff the request is
  // cross-party, cookie settings block 3P cookies, and content settings allows
  // the 1P to use cookies.
  //
  // Otherwise, the PrivacySetting should be kStateAllowed.
  //
  // We don't record metrics here, since this isn't actually accessing a cookie.
  CookieSettingWithMetadata metadata = GetCookieSettingWithMetadata(
      url, site_for_cookies, base::OptionalOrNullptr(top_frame_origin));
  if (IsHypotheticalCookieAllowed(metadata,
                                  same_party_cookie_context_type ==
                                      SamePartyCookieContextType::kSameParty,
                                  /*is_partitioned*/ false,
                                  /*record_metrics=*/false)) {
    return net::NetworkDelegate::PrivacySetting::kStateAllowed;
  }
  return metadata.blocked_by_third_party_setting ==
                 CookieSettings::ThirdPartyCookieBlockingSetting::
                     kPartitionedThirdPartyStateAllowedOnly
             ? net::NetworkDelegate::PrivacySetting::
                   kPartitionedStateAllowedOnly
             : net::NetworkDelegate::PrivacySetting::kStateDisallowed;
}

CookieSettings::CookieSettingWithMetadata
CookieSettings::GetCookieSettingWithMetadata(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request) const {
  if (ShouldAlwaysAllowCookies(url, first_party_url)) {
    return {
        /*cookie_setting=*/CONTENT_SETTING_ALLOW,
        /*blocked_by_third_party_setting=*/
        CookieSettings::ThirdPartyCookieBlockingSetting::
            kThirdPartyStateAllowed,
    };
  }

  // Default to allowing cookies.
  ContentSetting cookie_setting = CONTENT_SETTING_ALLOW;
  CookieSettings::ThirdPartyCookieBlockingSetting
      blocked_by_third_party_setting = CookieSettings::
          ThirdPartyCookieBlockingSetting::kThirdPartyStateAllowed;
  if (block_third_party_cookies_ && is_third_party_request &&
      !base::Contains(third_party_cookies_allowed_schemes_,
                      first_party_url.scheme())) {
    blocked_by_third_party_setting = CookieSettings::
        ThirdPartyCookieBlockingSetting::kThirdPartyStateDisallowed;
  }
  {
    // `content_settings_` is sorted in order of precedence, so we use the first
    // matching rule we find.
    const auto& entry = base::ranges::find_if(
        content_settings_, [&](const ContentSettingPatternSource& entry) {
          // The primary pattern is for the request URL; the secondary pattern
          // is for the first-party URL (which is the top-frame origin [if
          // available] or the site-for-cookies).
          return entry.primary_pattern.Matches(url) &&
                 entry.secondary_pattern.Matches(first_party_url);
        });
    if (entry != content_settings_.end()) {
      cookie_setting = entry->GetContentSetting();
      // Site-specific settings and global blocks override the "block
      // third-party cookies" setting.
      // Note: global settings are implemented as a catch-all (*, *) pattern.
      if (IsExplicitSetting(*entry) || cookie_setting == CONTENT_SETTING_BLOCK)
        // TODO(dylancutler): Consider adding an enum variant for this case.
        blocked_by_third_party_setting = CookieSettings::
            ThirdPartyCookieBlockingSetting::kThirdPartyStateAllowed;
    }
  }

  if (blocked_by_third_party_setting ==
      CookieSettings::ThirdPartyCookieBlockingSetting::
          kThirdPartyStateDisallowed) {
    // If a valid entry exists that matches both our first party and request url
    // this indicates a Storage Access API grant that may unblock storage access
    // despite third party cookies being blocked.
    // ContentSettingsType::STORAGE_ACCESS stores grants in the following
    // manner:
    // Primary Pattern:   Embedded site requiring third party storage access
    // Secondary Pattern: Top-Level site hosting embedded content
    // Value:             CONTENT_SETTING_[ALLOW/BLOCK] indicating grant
    //                    status
    const auto& entry = base::ranges::find_if(
        storage_access_grants_, [&](const ContentSettingPatternSource& entry) {
          return !entry.IsExpired() && entry.primary_pattern.Matches(url) &&
                 entry.secondary_pattern.Matches(first_party_url);
        });
    if (entry != storage_access_grants_.end()) {
      // We'll only utilize the SAA grant if our value is set to
      // CONTENT_SETTING_ALLOW as other values would indicate the user
      // rejected a prompt to allow access.
      if (entry->GetContentSetting() == CONTENT_SETTING_ALLOW) {
        blocked_by_third_party_setting = CookieSettings::
            ThirdPartyCookieBlockingSetting::kThirdPartyStateAllowed;
        FireStorageAccessHistogram(net::cookie_util::StorageAccessResult::
                                       ACCESS_ALLOWED_STORAGE_ACCESS_GRANT);
      }
    } else {
      // If the third-party cookie blocking setting is enabled, we check if the
      // user has any content settings for the first-party URL as the primary
      // pattern. If cookies are allowed for the first-party URL then we allow
      // partitioned cross-site cookies.
      const auto& first_party_entry = base::ranges::find_if(
          content_settings_, [&](const ContentSettingPatternSource& entry) {
            return entry.primary_pattern.Matches(first_party_url) &&
                   entry.secondary_pattern.Matches(first_party_url);
          });
      if (first_party_entry == content_settings_.end() ||
          first_party_entry->GetContentSetting() == CONTENT_SETTING_ALLOW) {
        blocked_by_third_party_setting =
            CookieSettings::ThirdPartyCookieBlockingSetting::
                kPartitionedThirdPartyStateAllowedOnly;
      }
    }
  } else {
    // Cookies aren't blocked solely due to the third-party-cookie blocking
    // setting, but they still may be blocked due to a global default. So we
    // have to check what the setting is here.
    FireStorageAccessHistogram(
        cookie_setting == CONTENT_SETTING_BLOCK
            ? net::cookie_util::StorageAccessResult::ACCESS_BLOCKED
            : net::cookie_util::StorageAccessResult::ACCESS_ALLOWED);
  }

  if (blocked_by_third_party_setting !=
      CookieSettings::ThirdPartyCookieBlockingSetting::
          kThirdPartyStateAllowed) {
    cookie_setting = CONTENT_SETTING_BLOCK;
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED);
  }

  return {cookie_setting, blocked_by_third_party_setting};
}

CookieSettings::CookieSettingWithMetadata
CookieSettings::GetCookieSettingWithMetadata(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin) const {
  return GetCookieSettingWithMetadata(
      url, GetFirstPartyURL(site_for_cookies, top_frame_origin),
      IsThirdPartyRequest(url, site_for_cookies));
}

ContentSetting CookieSettings::GetCookieSettingInternal(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    content_settings::SettingSource* source) const {
  return GetCookieSettingWithMetadata(url, first_party_url,
                                      is_third_party_request)
      .cookie_setting;
}

bool CookieSettings::AnnotateAndMoveUserBlockedCookies(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin,
    net::CookieAccessResultList& maybe_included_cookies,
    net::CookieAccessResultList& excluded_cookies) const {
  const CookieSettings::CookieSettingWithMetadata setting_with_metadata =
      GetCookieSettingWithMetadata(url, site_for_cookies, top_frame_origin);

  if (IsAllowed(setting_with_metadata.cookie_setting))
    return true;

  // Add the `EXCLUDE_USER_PREFERENCES` `ExclusionReason` for cookies that ought
  // to be blocked, and find any cookies that should still be allowed.
  bool is_any_allowed = false;
  for (net::CookieWithAccessResult& cookie : maybe_included_cookies) {
    if (IsCookieAllowed(setting_with_metadata, cookie)) {
      is_any_allowed = true;
    } else {
      cookie.access_result.status.AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
    }
  }
  for (net::CookieWithAccessResult& cookie : excluded_cookies) {
    if (!IsCookieAllowed(setting_with_metadata, cookie)) {
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

bool CookieSettings::IsCookieAllowed(
    const CookieSettings::CookieSettingWithMetadata& setting_with_metadata,
    const net::CookieWithAccessResult& cookie) const {
  return IsHypotheticalCookieAllowed(
      setting_with_metadata,
      cookie.cookie.IsSameParty() &&
          !cookie.access_result.status.HasExclusionReason(
              net::CookieInclusionStatus::
                  EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT),
      cookie.cookie.IsPartitioned(),
      /*record_metrics=*/true);
}

bool CookieSettings::IsHypotheticalCookieAllowed(
    const CookieSettings::CookieSettingWithMetadata& setting_with_metadata,
    bool is_same_party,
    bool is_partitioned,
    bool record_metrics) const {
  if (IsAllowed(setting_with_metadata.cookie_setting))
    return true;

  bool blocked_by_3p_but_same_party =
      setting_with_metadata.blocked_by_third_party_setting !=
          CookieSettings::ThirdPartyCookieBlockingSetting::
              kThirdPartyStateAllowed &&
      is_same_party;
  if (record_metrics && blocked_by_3p_but_same_party) {
    UMA_HISTOGRAM_BOOLEAN(
        "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting",
        !sameparty_cookies_considered_first_party_);
  }
  bool blocked = !(blocked_by_3p_but_same_party &&
                   sameparty_cookies_considered_first_party_);
  DCHECK(!is_partitioned || !is_same_party);
  if (blocked && is_partitioned &&
      setting_with_metadata.blocked_by_third_party_setting ==
          CookieSettings::ThirdPartyCookieBlockingSetting::
              kPartitionedThirdPartyStateAllowedOnly) {
    return true;
  }

  return !blocked;
}

bool CookieSettings::HasSessionOnlyOrigins() const {
  return base::ranges::any_of(content_settings_, [](const auto& entry) {
    return entry.GetContentSetting() == CONTENT_SETTING_SESSION_ONLY;
  });
}

}  // namespace network
