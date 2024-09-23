// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include <functional>
#include <iterator>
#include <memory>
#include <optional>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/to_string.h"
#include "base/types/optional_ref.h"
#include "base/types/optional_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "net/base/network_delegate.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/static_cookie_policy.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "services/network/public/cpp/features.h"
#include "services/network/tpcd/metadata/manager.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace {

bool ShouldApply3pcdRelatedReasons(const net::CanonicalCookie& cookie) {
  return cookie.SameSite() == net::CookieSameSite::NO_RESTRICTION &&
         !cookie.IsPartitioned();
}

bool IsValidType(ContentSettingsType type) {
  // ContentSettingsType::TPCD_METADATA_GRANTS settings are managed by the
  // `network::tpcd::metadata::Manager` and are considered valid ContentSettings
  // for CookieSettings.
  if (type == ContentSettingsType::TPCD_METADATA_GRANTS) {
    return true;
  }
  return CookieSettings::GetContentSettingsTypes().contains(type);
}

net::CookieInclusionStatus::ExemptionReason GetExemptionReason(
    CookieSettings::ThirdPartyCookieAllowMechanism allow_mechanism) {
  using AllowMechanism = CookieSettings::ThirdPartyCookieAllowMechanism;
  using ExemptionReason = net::CookieInclusionStatus::ExemptionReason;
  switch (allow_mechanism) {
    case AllowMechanism::kAllowByExplicitSetting:
    case AllowMechanism::kAllowByTrackingProtectionException:
      return ExemptionReason::kUserSetting;
    case AllowMechanism::kAllowBy3PCDHeuristics:
      return ExemptionReason::k3PCDHeuristics;
    case AllowMechanism::kAllowBy3PCDMetadataSourceUnspecified:
    case AllowMechanism::kAllowBy3PCDMetadataSourceTest:
    case AllowMechanism::kAllowBy3PCDMetadataSource1pDt:
    case AllowMechanism::kAllowBy3PCDMetadataSource3pDt:
    case AllowMechanism::kAllowBy3PCDMetadataSourceDogFood:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCriticalSector:
    case AllowMechanism::kAllowBy3PCDMetadataSourceCuj:
    case AllowMechanism::kAllowBy3PCDMetadataSourceGovEduTld:
      return ExemptionReason::k3PCDMetadata;
    case AllowMechanism::kAllowBy3PCD:
      return ExemptionReason::k3PCDDeprecationTrial;
    case AllowMechanism::kAllowByTopLevel3PCD:
      return ExemptionReason::kTopLevel3PCDDeprecationTrial;
    case AllowMechanism::kAllowByGlobalSetting:
    case AllowMechanism::kAllowByEnterprisePolicyCookieAllowedForUrls:
      return ExemptionReason::kEnterprisePolicy;
    case AllowMechanism::kAllowByStorageAccess:
      return ExemptionReason::kStorageAccess;
    case AllowMechanism::kAllowByTopLevelStorageAccess:
      return ExemptionReason::kTopLevelStorageAccess;
    case AllowMechanism::kNone:
      return ExemptionReason::kNone;
    case AllowMechanism::kAllowByScheme:
      return ExemptionReason::kScheme;
  }
}

bool IsOriginOpaqueHttpOrHttps(const url::Origin* top_frame_origin) {
  if (!top_frame_origin) {
    return false;
  }
  if (!top_frame_origin->opaque()) {
    return false;
  }
  const GURL url =
      top_frame_origin->GetTupleOrPrecursorTupleIfOpaque().GetURL();
  return url.SchemeIsHTTPOrHTTPS();
}

}  // namespace

// static
bool CookieSettings::IsCookieAllowed(const net::CanonicalCookie& cookie,
                                     const CookieSettingWithMetadata& setting) {
  return IsAllowed(setting.cookie_setting()) ||
         (cookie.IsPartitioned() && setting.allow_partitioned_cookies());
}

// static
net::NetworkDelegate::PrivacySetting CookieSettings::PrivacySetting(
    const CookieSettingWithMetadata& setting) {
  if (IsAllowed(setting.cookie_setting())) {
    return net::NetworkDelegate::PrivacySetting::kStateAllowed;
  }

  if (setting.allow_partitioned_cookies()) {
    return net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly;
  }

  return net::NetworkDelegate::PrivacySetting::kStateDisallowed;
}

CookieSettings::CookieSettings() {
  // Initialize content_settings_ until we receive data.
  for (auto type : GetContentSettingsTypes()) {
    set_content_settings(type, {});
  }
}

CookieSettings::~CookieSettings() = default;

void CookieSettings::set_content_settings(
    ContentSettingsType type,
    const ContentSettingsForOneType& settings) {
  CHECK_NE(type, ContentSettingsType::TPCD_METADATA_GRANTS)
      << "TPCD Metadata exceptions are managed by the "
         "`network::tpcd::metadata::Manager`.";
  CHECK(IsValidType(type)) << static_cast<int>(type);

  content_settings_[type] =
      content_settings::HostIndexedContentSettings::Create(settings);

  if (type == ContentSettingsType::COOKIES ||
      type == ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL) {
    // Cookies and the top-level origin trial for 3PCD use allow-by-default
    // settings, so ensure their default is set appropriately.
    if (settings.empty() ||
        settings.back().primary_pattern != ContentSettingsPattern::Wildcard() ||
        settings.back().secondary_pattern !=
            ContentSettingsPattern::Wildcard()) {
      auto& index = content_settings_[type].emplace_back(
          content_settings::ProviderType::kDefaultProvider, false);
      index.SetValue(ContentSettingsPattern::Wildcard(),
                     ContentSettingsPattern::Wildcard(),
                     base::Value(CONTENT_SETTING_ALLOW), /*metadata=*/{});
    }
  }
}

DeleteCookiePredicate CookieSettings::CreateDeleteCookieOnExitPredicate()
    const {
  if (!HasSessionOnlyOrigins()) {
    return DeleteCookiePredicate();
  }
  ContentSettingsForOneType settings;
  // TODO(b/316530672): Ideally, clear on exit would work with the index
  // directly to benefit from faster lookup times instead of iterating over
  // a vector of content settings.
  for (const auto& index :
       GetHostIndexedContentSettings(ContentSettingsType::COOKIES)) {
    for (const auto& entry : index) {
      settings.emplace_back(entry.first.primary_pattern,
                            entry.first.secondary_pattern,
                            entry.second.value.Clone(), index.source(),
                            *index.off_the_record(), entry.second.metadata);
    }
  }

  return base::BindRepeating(&CookieSettings::ShouldDeleteCookieOnExit,
                             base::Unretained(this), std::move(settings));
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
    const std::optional<url::Origin>& top_frame_origin,
    const net::FirstPartySetMetadata& first_party_set_metadata,
    net::CookieSettingOverrides overrides,
    net::CookieInclusionStatus* cookie_inclusion_status) const {
  const CookieSettingWithMetadata setting_with_metadata =
      GetCookieSettingWithMetadata(url, site_for_cookies,
                                   base::OptionalToPtr(top_frame_origin),
                                   overrides);
  bool allowed = IsCookieAllowed(cookie, setting_with_metadata);
  if (cookie_inclusion_status) {
    AugmentInclusionStatus(cookie, base::OptionalToPtr(top_frame_origin),
                           setting_with_metadata, first_party_set_metadata,
                           *cookie_inclusion_status);
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
    const std::optional<url::Origin>& top_frame_origin,
    net::CookieSettingOverrides overrides) const {
  return PrivacySetting(GetCookieSettingWithMetadata(
      url, site_for_cookies, base::OptionalToPtr(top_frame_origin), overrides));
}

CookieSettings::CookieSettingWithMetadata
CookieSettings::GetCookieSettingWithMetadata(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin,
    net::CookieSettingOverrides overrides) const {
  return GetCookieSettingInternal(
      url, site_for_cookies,
      FirstPartyURLForMetadata(site_for_cookies, top_frame_origin), overrides,
      nullptr);
}

// static
GURL CookieSettings::FirstPartyURLForMetadata(
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin) {
  return IsOriginOpaqueHttpOrHttps(top_frame_origin)
             ? top_frame_origin->GetTupleOrPrecursorTupleIfOpaque().GetURL()
             : GetFirstPartyURL(site_for_cookies, top_frame_origin);
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
  // Add `WARN_THIRD_PARTY_PHASEOUT` `WarningReason` for allowed cookies
  // that meets the conditions and add the `ExclusionReason` for cookies
  // that ought to be blocked.
  for (net::CookieWithAccessResult& cookie : maybe_included_cookies) {
    AugmentInclusionStatus(cookie.cookie, top_frame_origin,
                           setting_with_metadata, first_party_set_metadata,
                           cookie.access_result.status);
  }
  for (net::CookieWithAccessResult& cookie : excluded_cookies) {
    AugmentInclusionStatus(cookie.cookie, top_frame_origin,
                           setting_with_metadata, first_party_set_metadata,
                           cookie.access_result.status);
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

  return IsAllowed(setting_with_metadata.cookie_setting()) ||
         !maybe_included_cookies.empty();
}

bool CookieSettings::HasSessionOnlyOrigins() const {
  for (const auto& index :
       GetHostIndexedContentSettings(ContentSettingsType::COOKIES)) {
    for (const auto& entry : index) {
      if (content_settings::ValueToContentSetting(entry.second.value) ==
          CONTENT_SETTING_SESSION_ONLY) {
        return true;
      }
    }
  }
  return false;
}

const std::vector<content_settings::HostIndexedContentSettings>&
CookieSettings::GetHostIndexedContentSettings(ContentSettingsType type) const {
  CHECK(IsValidType(type)) << static_cast<int>(type);
  return content_settings_.at(type);
}

ContentSetting CookieSettings::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    content_settings::SettingInfo* info) const {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "ContentSettings.GetContentSetting.Network.Duration");

  if (content_type == ContentSettingsType::TPCD_METADATA_GRANTS) {
    if (tpcd_metadata_manager_) {
      return tpcd_metadata_manager_->GetContentSetting(primary_url,
                                                       secondary_url, info);
    }
  } else {
    for (const auto& index : GetHostIndexedContentSettings(content_type)) {
      const content_settings::RuleEntry* result =
          index.Find(primary_url, secondary_url);
      if (result) {
        if (info) {
          info->SetAttributes(*result);
          info->source = content_settings::GetSettingSourceFromProviderType(
              index.source());
        }
        return content_settings::ValueToContentSetting(result->second.value);
      }
    }
  }

  if (info) {
    info->primary_pattern = ContentSettingsPattern::Wildcard();
    info->secondary_pattern = ContentSettingsPattern::Wildcard();
    info->metadata = {};
  }
  return CONTENT_SETTING_BLOCK;
}

bool CookieSettings::IsThirdPartyCookiesAllowedScheme(
    const std::string& scheme) const {
  return base::Contains(third_party_cookies_allowed_schemes_, scheme);
}

bool CookieSettings::ShouldBlockThirdPartyCookies() const {
  return block_third_party_cookies_;
}

bool CookieSettings::IsThirdPartyPhaseoutEnabled() const {
  return net::cookie_util::IsForceThirdPartyCookieBlockingEnabled() ||
         tracking_protection_enabled_for_3pcd_;
}

bool CookieSettings::MitigationsEnabledFor3pcd() const {
  return mitigations_enabled_for_3pcd_;
}

void CookieSettings::AugmentInclusionStatus(
    const net::CanonicalCookie& cookie,
    const url::Origin* top_frame_origin,
    const CookieSettings::CookieSettingWithMetadata& setting_with_metadata,
    const net::FirstPartySetMetadata& first_party_set_metadata,
    net::CookieInclusionStatus& out_status) const {
  bool affected_by_3pcd_origin_trial =
      top_frame_origin &&
      IsBlockedByTopLevel3pcdOriginTrial(top_frame_origin->GetURL());

  if (IsCookieAllowed(cookie, setting_with_metadata)) {
    if (!setting_with_metadata.is_third_party_request() ||
        !ShouldApply3pcdRelatedReasons(cookie)) {
      return;
    }
    if (ShouldBlockThirdPartyCookies() || affected_by_3pcd_origin_trial) {
      out_status.MaybeSetExemptionReason(GetExemptionReason(
          setting_with_metadata.third_party_cookie_allow_mechanism()));
      return;
    }
    if (!setting_with_metadata.is_explicit_setting()) {
      // The cookie should be allowed by default to have this warning
      // reason.
      out_status.AddWarningReason(
          net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT);
    }
    return;
  }

  // The cookie is blocked.

  if (setting_with_metadata.is_third_party_request() &&
      setting_with_metadata.allow_partitioned_cookies()) {
    if (IsThirdPartyPhaseoutEnabled() &&
        !setting_with_metadata.is_explicit_setting()) {
      // This cookie is blocked due to 3PCD.
      if (!ShouldApply3pcdRelatedReasons(cookie)) {
        return;
      }
      out_status.AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT);

      if (first_party_set_metadata.AreSitesInSameFirstPartySet()) {
        out_status.AddExclusionReason(
            net::CookieInclusionStatus::
                EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET);
      }
      return;
    }
    if (affected_by_3pcd_origin_trial &&
        ShouldApply3pcdRelatedReasons(cookie)) {
      // This cookie is blocked by the Origin Trial for 3PCD.
      out_status.AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT);

      return;
    }
  }

  // The cookie is blocked, but not by 3PCD.
  out_status.AddExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
}

bool CookieSettings::IsStorageAccessHeadersEnabled(
    const GURL& url,
    base::optional_ref<const url::Origin> top_frame_origin) const {
  if (base::FeatureList::IsEnabled(network::features::kStorageAccessHeaders)) {
    return true;
  }
  return top_frame_origin &&
         base::FeatureList::IsEnabled(
             network::features::kStorageAccessHeadersTrial) &&
         GetContentSetting(
             url, top_frame_origin->GetURL(),
             ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
             /*info=*/nullptr) == CONTENT_SETTING_ALLOW;
}

}  // namespace network
