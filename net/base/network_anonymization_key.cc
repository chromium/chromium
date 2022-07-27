// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/base/network_anonymization_key.h"
#include "base/feature_list.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
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

NetworkAnonymizationKey::NetworkAnonymizationKey() = default;

NetworkAnonymizationKey::NetworkAnonymizationKey(
    const NetworkAnonymizationKey& network_anonymization_key) = default;

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
