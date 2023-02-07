// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/base/network_anonymization_key.h"
#include "base/feature_list.h"
#include "base/unguessable_token.h"
#include "base/values.h"
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
      is_cross_site_(IsCrossSiteFlagSchemeEnabled() ? is_cross_site
                                                    : absl::nullopt),
      nonce_(nonce) {
  DCHECK(top_frame_site_.has_value());
  // If `is_cross_site` is enabled but the value is not populated, and we have
  // the information to calculate it, do calculate it.
  if (IsCrossSiteFlagSchemeEnabled() && !is_cross_site_.has_value() &&
      frame_site.has_value()) {
    SiteForCookies site_for_cookies =
        net::SiteForCookies(top_frame_site_.value());
    is_cross_site_ =
        !site_for_cookies.IsFirstParty(frame_site.value().GetURL());
  }
  if (IsCrossSiteFlagSchemeEnabled()) {
    // If `frame_site_` is populated, `is_cross_site_` must be as well.
    DCHECK(is_cross_site_.has_value());
  }
}

NetworkAnonymizationKey NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
    const net::NetworkIsolationKey& network_isolation_key) {
  // If NIK is double-keyed, a 2.5-keyed NAK cannot be constructed from it.
  DCHECK(NetworkIsolationKey::IsFrameSiteEnabled() ||
         IsDoubleKeySchemeEnabled());

  // We cannot create a valid NetworkAnonymizationKey from a NetworkIsolationKey
  // that is not fully populated.
  if (!network_isolation_key.IsFullyPopulated()) {
    return NetworkAnonymizationKey();
  }

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
      network_isolation_key.GetTopFrameSite().value(), absl::nullopt,
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

NetworkAnonymizationKey NetworkAnonymizationKey::CreateTransient() {
  SchemefulSite site_with_opaque_origin;
  return NetworkAnonymizationKey(site_with_opaque_origin,
                                 site_with_opaque_origin, false);
}

std::string NetworkAnonymizationKey::ToDebugString() const {
  std::string str = GetSiteDebugString(top_frame_site_);
  std::string cross_site_str =
      IsCrossSiteFlagSchemeEnabled()
          ? (!GetIsCrossSite().has_value() ? " with empty is_cross_site value"
             : GetIsCrossSite().value()    ? " cross_site"
                                           : " same_site")
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
         (!IsCrossSiteFlagSchemeEnabled() || is_cross_site_.has_value());
}

bool NetworkAnonymizationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;

  return top_frame_site_->opaque() || nonce_.has_value();
}

absl::optional<bool> NetworkAnonymizationKey::GetIsCrossSite() const {
  DCHECK(IsCrossSiteFlagSchemeEnabled());
  return is_cross_site_;
}

bool NetworkAnonymizationKey::IsDoubleKeySchemeEnabled() {
  return !base::FeatureList::IsEnabled(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
}

bool NetworkAnonymizationKey::IsCrossSiteFlagSchemeEnabled() {
  return base::FeatureList::IsEnabled(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
}

bool NetworkAnonymizationKey::ToValue(base::Value* out_value) const {
  if (IsEmpty()) {
    *out_value = base::Value(base::Value::Type::LIST);
    return true;
  }

  if (IsTransient())
    return false;

  absl::optional<std::string> top_frame_value =
      SerializeSiteWithNonce(*top_frame_site_);
  if (!top_frame_value)
    return false;
  base::Value::List list;
  list.Append(std::move(top_frame_value).value());

  // Append frame site for tripe key scheme or is_cross_site flag for double key
  // with cross site flag scheme.
  if (IsCrossSiteFlagSchemeEnabled()) {
    const absl::optional<bool> is_cross_site = GetIsCrossSite();
    if (is_cross_site.has_value()) {
      list.Append(is_cross_site.value());
    }
  }

  *out_value = base::Value(std::move(list));
  return true;
}

bool NetworkAnonymizationKey::FromValue(
    const base::Value& value,
    NetworkAnonymizationKey* network_anonymization_key) {
  if (!value.is_list())
    return false;

  const base::Value::List& list = value.GetList();
  if (list.empty()) {
    *network_anonymization_key = NetworkAnonymizationKey();
    return true;
  }

  // Check top_level_site is valid for any key scheme
  if (list.size() < 1 || !list[0].is_string()) {
    return false;
  }
  absl::optional<SchemefulSite> top_frame_site =
      SchemefulSite::DeserializeWithNonce(list[0].GetString());
  if (!top_frame_site) {
    return false;
  }

  absl::optional<SchemefulSite> frame_site = absl::nullopt;
  absl::optional<bool> is_cross_site = absl::nullopt;

  // If double key scheme is enabled `list` must be of length 1. list[0] will be
  // top_frame_site.
  if (IsDoubleKeySchemeEnabled()) {
    if (list.size() != 1) {
      return false;
    }
  } else /* if (IsCrossSiteFlagSchemeEnabled()) */ {
    // If double key + is cross site scheme is enabled `list` must be of
    // length 2. list[0] will be top_frame_site and list[1] will be
    // is_cross_site.
    if (list.size() != 2 || !list[1].is_bool()) {
      return false;
    }
    is_cross_site = list[1].GetBool();
  }

  *network_anonymization_key =
      NetworkAnonymizationKey(std::move(top_frame_site.value()),
                              std::move(frame_site), std::move(is_cross_site));
  return true;
}

std::string NetworkAnonymizationKey::GetSiteDebugString(
    const absl::optional<SchemefulSite>& site) const {
  return site ? site->GetDebugString() : "null";
}

absl::optional<std::string> NetworkAnonymizationKey::SerializeSiteWithNonce(
    const SchemefulSite& site) {
  return *(const_cast<SchemefulSite&>(site).SerializeWithNonce());
}

}  // namespace net
