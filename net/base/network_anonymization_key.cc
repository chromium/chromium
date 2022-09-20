// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/base/network_anonymization_key.h"
#include "base/feature_list.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

NetworkAnonymizationKey::NetworkAnonymizationKey(
    const SchemefulSite& top_frame_site,
    const absl::optional<SchemefulSite>& frame_site,
    const absl::optional<bool> is_cross_site,
    const absl::optional<base::UnguessableToken> nonce)
    : top_frame_site_(top_frame_site),
      frame_site_(!IsFrameSiteEnabled() ? absl::nullopt : frame_site),
      is_cross_site_(IsCrossSiteFlagSchemeEnabled() ? is_cross_site
                                                    : absl::nullopt),
      nonce_(nonce) {}

NetworkAnonymizationKey NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
    const net::NetworkIsolationKey& network_isolation_key) {
  // If NIK is a double key, NAK must also be a double key.
  DCHECK(NetworkIsolationKey::IsFrameSiteEnabled() ||
         (!NetworkIsolationKey::IsFrameSiteEnabled() &&
          !NetworkAnonymizationKey::IsFrameSiteEnabled()));

  // We cannot create a valid NetworkAnonymizationKey from a NetworkIsolationKey
  // that is not fully populated.
  if (!network_isolation_key.IsFullyPopulated()) {
    return NetworkAnonymizationKey();
  }

  absl::optional<SchemefulSite> nak_frame_site =
      NetworkAnonymizationKey::IsFrameSiteEnabled()
          ? network_isolation_key.GetFrameSite()
          : absl::nullopt;

  // If we are unable to determine the value of `is_cross_site` from the
  // NetworkIsolationKey, we default the value to `nullopt`. Otherwise we
  // calculate what the value will be. If the NetworkAnonymizationKey is being
  // constructed in a scheme where the is cross site value is not used this
  // value will be overridden in the constructor and set to `nullopt`.
  absl::optional<bool> nak_is_cross_site = absl::nullopt;
  if (NetworkAnonymizationKey::IsCrossSiteFlagSchemeEnabled()) {
    SiteForCookies site_for_cookies =
        net::SiteForCookies(network_isolation_key.GetTopFrameSite().value());
    nak_is_cross_site = !site_for_cookies.IsFirstParty(
        network_isolation_key.GetFrameSite()->GetURL());
  }

  return NetworkAnonymizationKey(
      network_isolation_key.GetTopFrameSite().value(), nak_frame_site,
      nak_is_cross_site, network_isolation_key.GetNonce());
}

NetworkAnonymizationKey::NetworkAnonymizationKey() = default;

NetworkAnonymizationKey::NetworkAnonymizationKey(
    const NetworkAnonymizationKey& network_anonymization_key) = default;

NetworkAnonymizationKey::NetworkAnonymizationKey(
    NetworkAnonymizationKey&& network_anonymization_key) = default;

NetworkAnonymizationKey::~NetworkAnonymizationKey() = default;

NetworkAnonymizationKey& NetworkAnonymizationKey::operator=(
    const NetworkAnonymizationKey& network_anonymization_key) = default;

NetworkAnonymizationKey& NetworkAnonymizationKey::operator=(
    NetworkAnonymizationKey&& network_anonymization_key) = default;

std::string NetworkAnonymizationKey::ToDebugString() const {
  std::string str = GetSiteDebugString(top_frame_site_);
  str += " " + GetSiteDebugString(frame_site_);
  std::string cross_site_str =
      IsCrossSiteFlagSchemeEnabled()
          ? (GetIsCrossSite() ? " cross_site" : " same_site")
          : "";
  str += cross_site_str;

  // Currently, if the NAK has a nonce it will be marked transient. For debug
  // purposes we will print the value but if called via
  // `NetworkAnonymizationKey::ToString` we will have already returned "".
  if (nonce_.has_value()) {
    str += " (with nonce " + nonce_->ToString() + ")";
  }

  return str;
}

bool NetworkAnonymizationKey::IsEmpty() const {
  return !top_frame_site_.has_value();
}

bool NetworkAnonymizationKey::IsFullyPopulated() const {
  return top_frame_site_.has_value() &&
         (!IsFrameSiteEnabled() || frame_site_.has_value()) &&
         (!IsCrossSiteFlagSchemeEnabled() || is_cross_site_.has_value());
}

bool NetworkAnonymizationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;

  return top_frame_site_->opaque() ||
         (IsFrameSiteEnabled() && frame_site_->opaque()) || nonce_.has_value();
}

bool NetworkAnonymizationKey::GetIsCrossSite() const {
  DCHECK(IsCrossSiteFlagSchemeEnabled() && is_cross_site_.has_value());
  return is_cross_site_.value();
}

const absl::optional<SchemefulSite>& NetworkAnonymizationKey::GetFrameSite()
    const {
  // Frame site will be empty if double-keying is enabled.
  CHECK(NetworkAnonymizationKey::IsFrameSiteEnabled());
  return frame_site_;
}

bool NetworkAnonymizationKey::IsFrameSiteEnabled() {
  return !base::FeatureList::IsEnabled(
             net::features::kEnableDoubleKeyNetworkAnonymizationKey) &&
         !base::FeatureList::IsEnabled(
             net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
}

bool NetworkAnonymizationKey::IsDoubleKeySchemeEnabled() {
  // There's no reason both of these will be enabled simultaneously but if
  // someone manually enables both flags, double key with cross site flag scheme
  // should take precedence.
  return base::FeatureList::IsEnabled(
             net::features::kEnableDoubleKeyNetworkAnonymizationKey) &&
         !base::FeatureList::IsEnabled(
             net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
}

bool NetworkAnonymizationKey::IsCrossSiteFlagSchemeEnabled() {
  return base::FeatureList::IsEnabled(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
}

std::string NetworkAnonymizationKey::GetSiteDebugString(
    const absl::optional<SchemefulSite>& site) const {
  return site ? site->GetDebugString() : "null";
}

}  // namespace net
