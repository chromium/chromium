// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include <functional>
#include <iterator>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
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

using SamePartyCookieContextType = net::SamePartyContext::Type;

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
  return IsHypotheticalCookieAllowed(
      GetCookieSettingWithMetadata(
          url,
          GetFirstPartyURL(site_for_cookies,
                           base::OptionalToPtr(top_frame_origin)),
          IsThirdPartyRequest(url, site_for_cookies), overrides,
          QueryReason::kCookies),
      cookie.IsSameParty(), cookie.IsPartitioned());
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
    SamePartyCookieContextType same_party_cookie_context_type,
    net::CookieSettingOverrides overrides) const {
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
  CookieSettingWithMetadata metadata = GetCookieSettingWithMetadata(
      url, site_for_cookies, base::OptionalToPtr(top_frame_origin), overrides,
      QueryReason::kCookies);
  if (IsHypotheticalCookieAllowed(metadata,
                                  same_party_cookie_context_type ==
                                      SamePartyCookieContextType::kSameParty,
                                  /*is_partitioned*/ false)) {
    return net::NetworkDelegate::PrivacySetting::kStateAllowed;
  }

  // No unpartitioned cookie should be sent on this request. The only other
  // options are to block all cookies, or allow just partitioned cookies.

  switch (metadata.third_party_blocking_outcome) {
    case ThirdPartyBlockingOutcome::kIrrelevant:
      [[fallthrough]];
    case ThirdPartyBlockingOutcome::kAllStateDisallowed:
      return net::NetworkDelegate::PrivacySetting::kStateDisallowed;

    case ThirdPartyBlockingOutcome::kPartitionedStateAllowed:
      return net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly;
    case ThirdPartyBlockingOutcome::kForceAllowed:
      return net::NetworkDelegate::PrivacySetting::kStateAllowed;
  }
}

CookieSettings::ThirdPartyBlockingOutcome
CookieSettings::GetThirdPartyBlockingScope(const GURL& first_party_url) const {
  // If cookies are allowed for the first-party URL then we allow
  // partitioned cross-site cookies.
  if (const ContentSettingPatternSource* match = FindMatchingSetting(
          first_party_url, first_party_url, content_settings_);
      !match || match->GetContentSetting() == CONTENT_SETTING_ALLOW) {
    return ThirdPartyBlockingOutcome::kPartitionedStateAllowed;
  }
  return ThirdPartyBlockingOutcome::kAllStateDisallowed;
}

CookieSettings::CookieSettingWithMetadata
CookieSettings::GetCookieSettingWithMetadata(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    net::CookieSettingOverrides overrides,
    QueryReason query_reason) const {
  if (ShouldAlwaysAllowCookies(url, first_party_url)) {
    return {
        /*cookie_setting=*/CONTENT_SETTING_ALLOW,
        /*third_party_blocking_outcome=*/
        ThirdPartyBlockingOutcome::kIrrelevant,
    };
  }

  // Default to allowing cookies.
  ContentSetting cookie_setting = CONTENT_SETTING_ALLOW;
  ThirdPartyBlockingOutcome third_party_blocking_outcome =
      ThirdPartyBlockingOutcome::kIrrelevant;

  bool found_explicit_setting = false;
  if (const ContentSettingPatternSource* match =
          FindMatchingSetting(url, first_party_url, content_settings_);
      match) {
    cookie_setting = match->GetContentSetting();
    found_explicit_setting = IsExplicitSetting(*match);
  }

  bool allowed_by_storage_access_grant = false;
  bool allowed_by_override = false;
  if (cookie_setting != CONTENT_SETTING_BLOCK && !found_explicit_setting) {
    // Check for should block third party.
    if (block_third_party_cookies_ && is_third_party_request &&
        !base::Contains(third_party_cookies_allowed_schemes_,
                        first_party_url.scheme())) {
      // See if a Storage Access permission grant can unblock.
      if (const ContentSettingPatternSource* match =
              FindMatchingSetting(url, first_party_url, storage_access_grants_);
          ShouldConsiderStorageAccessGrants(query_reason) && match &&
          match->GetContentSetting() == CONTENT_SETTING_ALLOW) {
        allowed_by_storage_access_grant = true;
      } else if (overrides.Has(
                     net::CookieSettingOverride::kForceThirdPartyByUser)) {
        cookie_setting = CONTENT_SETTING_ALLOW;
        third_party_blocking_outcome = ThirdPartyBlockingOutcome::kForceAllowed;
        allowed_by_override = true;
      } else {
        cookie_setting = CONTENT_SETTING_BLOCK;
        third_party_blocking_outcome =
            GetThirdPartyBlockingScope(first_party_url);
      }
    }
  }

  if (cookie_setting == CONTENT_SETTING_BLOCK) {
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED);
  } else if (allowed_by_storage_access_grant) {
    FireStorageAccessHistogram(net::cookie_util::StorageAccessResult::
                                   ACCESS_ALLOWED_STORAGE_ACCESS_GRANT);

  } else if (allowed_by_override) {
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_FORCED);
  } else {
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_ALLOWED);
  }

  return {cookie_setting, third_party_blocking_outcome};
}

CookieSettings::CookieSettingWithMetadata
CookieSettings::GetCookieSettingWithMetadata(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin,
    net::CookieSettingOverrides overrides,
    QueryReason query_reason) const {
  return GetCookieSettingWithMetadata(
      url, GetFirstPartyURL(site_for_cookies, top_frame_origin),
      IsThirdPartyRequest(url, site_for_cookies), overrides, query_reason);
}

ContentSetting CookieSettings::GetCookieSettingInternal(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    net::CookieSettingOverrides overrides,
    content_settings::SettingSource* source,
    QueryReason query_reason) const {
  return GetCookieSettingWithMetadata(url, first_party_url,
                                      is_third_party_request, overrides,
                                      query_reason)
      .cookie_setting;
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
                                   overrides, QueryReason::kCookies);

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
      if (IsThirdPartyCookieBlockedInSamePartySites(
              setting_with_metadata.third_party_blocking_outcome,
              first_party_set_metadata)) {
        cookie.access_result.status.AddExclusionReason(
            net::CookieInclusionStatus::
                EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET);
      }
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
    const CookieSettingWithMetadata& setting_with_metadata,
    const net::CookieWithAccessResult& cookie) const {
  return IsHypotheticalCookieAllowed(
      setting_with_metadata,
      cookie.cookie.IsSameParty() &&
          !cookie.access_result.status.HasExclusionReason(
              net::CookieInclusionStatus::
                  EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT),
      cookie.cookie.IsPartitioned());
}

bool CookieSettings::IsAllowedSamePartyCookie(
    bool is_same_party,
    ThirdPartyBlockingOutcome third_party_blocking_outcome) const {
  bool blocked_by_3p_but_same_party =
      is_same_party &&
      third_party_blocking_outcome != ThirdPartyBlockingOutcome::kIrrelevant;

  return sameparty_cookies_considered_first_party_ &&
         blocked_by_3p_but_same_party;
}

// static
bool CookieSettings::IsAllowedPartitionedCookie(
    bool is_partitioned,
    ThirdPartyBlockingOutcome third_party_blocking_outcome) {
  return is_partitioned &&
         third_party_blocking_outcome ==
             ThirdPartyBlockingOutcome::kPartitionedStateAllowed;
}

// static
bool CookieSettings::IsThirdPartyCookieBlockedInSamePartySites(
    ThirdPartyBlockingOutcome third_party_blocking_outcome,
    const net::FirstPartySetMetadata& first_party_set_metadata) {
  // If partitioned state is allowed only, it means the cookie was excluded due
  // to the third-party cookie blocking setting.
  if (third_party_blocking_outcome !=
      ThirdPartyBlockingOutcome::kPartitionedStateAllowed)
    return false;
  return first_party_set_metadata.AreSitesInSameFirstPartySet();
}

bool CookieSettings::IsHypotheticalCookieAllowed(
    const CookieSettingWithMetadata& setting_with_metadata,
    bool is_same_party,
    bool is_partitioned) const {
  DCHECK(!is_partitioned || !is_same_party);
  return IsAllowed(setting_with_metadata.cookie_setting) ||
         IsAllowedSamePartyCookie(
             is_same_party,
             setting_with_metadata.third_party_blocking_outcome) ||
         IsAllowedPartitionedCookie(
             is_partitioned,
             setting_with_metadata.third_party_blocking_outcome);
}

bool CookieSettings::HasSessionOnlyOrigins() const {
  return base::ranges::any_of(content_settings_, [](const auto& entry) {
    return entry.GetContentSetting() == CONTENT_SETTING_SESSION_ONLY;
  });
}

}  // namespace network
