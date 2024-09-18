// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/base/network_anonymization_key.h"

#include <atomic>
#include <optional>

#include "base/feature_list.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"

namespace net {

namespace {

// True if network state partitioning should be enabled regardless of feature
// settings.
bool g_partition_by_default = false;

// True if NAK::IsPartitioningEnabled has been called, and the value of
// `g_partition_by_default` cannot be changed.
constinit std::atomic<bool> g_partition_by_default_locked = false;

}  // namespace

NetworkAnonymizationKey::NetworkAnonymizationKey(
    const SchemefulSite& top_frame_site,
    bool is_cross_site,
    std::optional<base::UnguessableToken> nonce)
    : top_frame_site_(top_frame_site),
      is_cross_site_(is_cross_site),
      nonce_(nonce) {
  DCHECK(top_frame_site_.has_value());
}

NetworkAnonymizationKey NetworkAnonymizationKey::CreateFromFrameSite(
    const SchemefulSite& top_frame_site,
    const SchemefulSite& frame_site,
    std::optional<base::UnguessableToken> nonce) {
  bool is_cross_site = top_frame_site != frame_site;
  return NetworkAnonymizationKey(top_frame_site, is_cross_site, nonce);
}

NetworkAnonymizationKey NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
    const net::NetworkIsolationKey& network_isolation_key) {
  // We cannot create a valid NetworkAnonymizationKey from a NetworkIsolationKey
  // that is not fully populated.
  if (!network_isolation_key.IsFullyPopulated()) {
    return NetworkAnonymizationKey();
  }

  return CreateFromFrameSite(
      network_isolation_key.GetTopFrameSite().value(),
      network_isolation_key
          .GetFrameSiteForNetworkAnonymizationKey(
              NetworkIsolationKey::NetworkAnonymizationKeyPassKey())
          .value(),
      network_isolation_key.GetNonce());
}

NetworkAnonymizationKey::NetworkAnonymizationKey()
    : top_frame_site_(std::nullopt),
      is_cross_site_(false),
      nonce_(std::nullopt) {}

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
  return NetworkAnonymizationKey(site_with_opaque_origin, false);
}

std::string NetworkAnonymizationKey::ToDebugString() const {
  if (!IsFullyPopulated()) {
    return "null";
  }

  std::string str = GetSiteDebugString(top_frame_site_);
  str += IsCrossSite() ? " cross_site" : " same_site";

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
  return top_frame_site_.has_value();
}

bool NetworkAnonymizationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;

  return top_frame_site_->opaque() || nonce_.has_value();
}

bool NetworkAnonymizationKey::ToValue(base::Value* out_value) const {
  if (IsEmpty()) {
    *out_value = base::Value(base::Value::Type::LIST);
    return true;
  }

  if (IsTransient())
    return false;

  std::optional<std::string> top_frame_value =
      SerializeSiteWithNonce(*top_frame_site_);
  if (!top_frame_value)
    return false;
  base::Value::List list;
  list.Append(std::move(top_frame_value).value());

  list.Append(IsCrossSite());

  *out_value = base::Value(std::move(list));
  return true;
}

bool NetworkAnonymizationKey::FromValue(
    const base::Value& value,
    NetworkAnonymizationKey* network_anonymization_key) {
  if (!value.is_list()) {
    return false;
  }

  const base::Value::List& list = value.GetList();
  if (list.empty()) {
    *network_anonymization_key = NetworkAnonymizationKey();
    return true;
  }

  // Check the format.
  if (list.size() != 2 || !list[0].is_string() || !list[1].is_bool()) {
    return false;
  }

  // Check top_level_site is valid for any key scheme
  std::optional<SchemefulSite> top_frame_site =
      SchemefulSite::DeserializeWithNonce(
          base::PassKey<NetworkAnonymizationKey>(), list[0].GetString());
  if (!top_frame_site) {
    return false;
  }

  bool is_cross_site = list[1].GetBool();

  *network_anonymization_key =
      NetworkAnonymizationKey(top_frame_site.value(), is_cross_site);
  return true;
}

std::string NetworkAnonymizationKey::GetSiteDebugString(
    const std::optional<SchemefulSite>& site) const {
  return site ? site->GetDebugString() : "null";
}

std::optional<std::string> NetworkAnonymizationKey::SerializeSiteWithNonce(
    const SchemefulSite& site) {
  return *(const_cast<SchemefulSite&>(site).SerializeWithNonce(
      base::PassKey<NetworkAnonymizationKey>()));
}

// static
bool NetworkAnonymizationKey::IsPartitioningEnabled() {
  g_partition_by_default_locked.store(true, std::memory_order_relaxed);
  return g_partition_by_default ||
         base::FeatureList::IsEnabled(
             features::kPartitionConnectionsByNetworkIsolationKey);
}

// static
void NetworkAnonymizationKey::PartitionByDefault() {
  DCHECK(!g_partition_by_default_locked.load(std::memory_order_relaxed));
  // Only set the global if none of the relevant features are overridden.
  if (!base::FeatureList::GetInstance()->IsFeatureOverridden(
          "PartitionConnectionsByNetworkIsolationKey")) {
    g_partition_by_default = true;
  }
}

// static
void NetworkAnonymizationKey::ClearGlobalsForTesting() {
  g_partition_by_default = false;
  g_partition_by_default_locked.store(false);
}

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const NetworkAnonymizationKey& nak) {
  os << nak.ToDebugString();
  return os;
}

}  // namespace net
