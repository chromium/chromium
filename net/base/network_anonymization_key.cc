// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/base/network_anonymization_key.h"

#include <atomic>
#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/network_isolation_partition.h"
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

NetworkAnonymizationKey::NetworkAnonymizationKey()
    : data_(Data::GetEmptyData()) {}

NetworkAnonymizationKey::NetworkAnonymizationKey(
    const NetworkAnonymizationKey& network_anonymization_key) = default;

NetworkAnonymizationKey::NetworkAnonymizationKey(
    NetworkAnonymizationKey&& network_anonymization_key) = default;

NetworkAnonymizationKey& NetworkAnonymizationKey::operator=(
    const NetworkAnonymizationKey& network_anonymization_key) = default;

NetworkAnonymizationKey& NetworkAnonymizationKey::operator=(
    NetworkAnonymizationKey&& network_anonymization_key) = default;

NetworkAnonymizationKey::NetworkAnonymizationKey(
    const std::optional<SchemefulSite>& top_frame_site,
    bool is_cross_site,
    std::optional<base::UnguessableToken> nonce,
    NetworkIsolationPartition network_isolation_partition)
    : data_(base::MakeRefCounted<Data>(top_frame_site,
                                       is_cross_site,
                                       std::move(nonce),
                                       network_isolation_partition)) {}

NetworkAnonymizationKey::~NetworkAnonymizationKey() = default;

NetworkAnonymizationKey NetworkAnonymizationKey::CreateFromFrameSite(
    const SchemefulSite& top_frame_site,
    const SchemefulSite& frame_site,
    std::optional<base::UnguessableToken> nonce,
    NetworkIsolationPartition network_isolation_partition) {
  bool is_cross_site = top_frame_site != frame_site;
  return NetworkAnonymizationKey(top_frame_site, is_cross_site, nonce,
                                 network_isolation_partition);
}

NetworkAnonymizationKey NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
    const net::NetworkIsolationKey& network_isolation_key) {
  return NetworkAnonymizationKey(
      network_isolation_key.GetTopFrameSite(),
      network_isolation_key.GetTopFrameSite() !=
          network_isolation_key.GetFrameSiteForNetworkAnonymizationKey(
              base::PassKey<NetworkAnonymizationKey>()),
      network_isolation_key.GetNonce(),
      network_isolation_key.GetNetworkIsolationPartition());
}

NetworkAnonymizationKey NetworkAnonymizationKey::CreateTransient() {
  SchemefulSite site_with_opaque_origin;
  return NetworkAnonymizationKey(site_with_opaque_origin, false);
}

std::string NetworkAnonymizationKey::ToDebugString() const {
  if (!IsFullyPopulated()) {
    return "null";
  }

  std::string str = GetSiteDebugString(GetTopFrameSite());
  str += IsCrossSite() ? " cross_site" : " same_site";

  // Currently, if the NAK has a nonce it will be marked transient. For debug
  // purposes we will print the value but if called via
  // `NetworkAnonymizationKey::ToString` we will have already returned "".
  if (GetNonce().has_value()) {
    str += " (with nonce " + GetNonce()->ToString() + ")";
  }

  if (network_isolation_partition() != NetworkIsolationPartition::kGeneral) {
    str +=
        " (" +
        NetworkIsolationPartitionToDebugString(network_isolation_partition()) +
        ")";
  }

  return str;
}

bool NetworkAnonymizationKey::IsEmpty() const {
  return data_->is_empty();
}

bool NetworkAnonymizationKey::IsFullyPopulated() const {
  return !IsEmpty();
}

bool NetworkAnonymizationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;

  return GetTopFrameSite()->opaque() || GetNonce().has_value();
}

bool NetworkAnonymizationKey::ToValue(base::Value* out_value) const {
  if (IsEmpty()) {
    *out_value = base::Value(base::Value::Type::LIST);
    return true;
  }

  if (IsTransient())
    return false;

  std::optional<std::string> top_frame_value =
      SerializeSiteWithNonce(*GetTopFrameSite());
  if (!top_frame_value)
    return false;
  base::Value::List list;
  list.Append(std::move(top_frame_value).value());

  list.Append(IsCrossSite());

  list.Append(base::strict_cast<int32_t>(network_isolation_partition()));

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
  // While migrating to using NetworkIsolationPartition, continue supporting
  // values of length 2 for a few months.
  // TODO(abigailkatcoff): Stop support for lists of length 2 after a few
  // months.
  if (list.size() < 2 || list.size() > 3 || !list[0].is_string() ||
      !list[1].is_bool()) {
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

  NetworkIsolationPartition network_isolation_partition =
      NetworkIsolationPartition::kGeneral;
  if (list.size() == 3) {
    if (!list[2].is_int() ||
        list[2].GetInt() >
            base::strict_cast<int32_t>(NetworkIsolationPartition::kMaxValue) ||
        list[2].GetInt() < 0) {
      return false;
    }
    network_isolation_partition =
        static_cast<NetworkIsolationPartition>(list[2].GetInt());
  }

  *network_anonymization_key = NetworkAnonymizationKey(
      top_frame_site.value(), is_cross_site, /*nonce=*/std::nullopt,
      network_isolation_partition);
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

// static
scoped_refptr<NetworkAnonymizationKey::Data>
NetworkAnonymizationKey::Data::GetEmptyData() {
  static base::NoDestructor<scoped_refptr<NetworkAnonymizationKey::Data>>
      empty_data(base::MakeRefCounted<Data>(base::PassKey<Data>()));
  return *empty_data;
}

NetworkAnonymizationKey::Data::Data(base::PassKey<Data>)
    : is_cross_site_(false),
      network_isolation_partition_(NetworkIsolationPartition::kGeneral) {
  CHECK(is_empty());
}

NetworkAnonymizationKey::Data::Data(
    const std::optional<SchemefulSite>& top_frame_site,
    bool is_cross_site,
    std::optional<base::UnguessableToken> nonce,
    NetworkIsolationPartition network_isolation_partition)
    : top_frame_site_(top_frame_site),
      is_cross_site_(is_cross_site),
      nonce_(std::move(nonce)),
      network_isolation_partition_(network_isolation_partition) {}

NetworkAnonymizationKey::Data::~Data() = default;

}  // namespace net
