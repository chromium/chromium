// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_isolation_key.h"

#include <cstddef>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/network_isolation_partition.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "schemeful_site.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace net {

namespace {

std::string GetNetworkIsolationPartitionStringForCacheKey(
    NetworkIsolationPartition network_isolation_partition) {
  return base::NumberToString(
      base::strict_cast<int32_t>(network_isolation_partition));
}

}  // namespace

NetworkIsolationKey::NetworkIsolationKey(
    const SchemefulSite& top_frame_site,
    const SchemefulSite& frame_site,
    const std::optional<base::UnguessableToken>& nonce,
    NetworkIsolationPartition network_isolation_partition)
    : NetworkIsolationKey(SchemefulSite(top_frame_site),
                          SchemefulSite(frame_site),
                          std::optional<base::UnguessableToken>(nonce),
                          network_isolation_partition) {}

NetworkIsolationKey::NetworkIsolationKey(
    SchemefulSite&& top_frame_site,
    SchemefulSite&& frame_site,
    std::optional<base::UnguessableToken>&& nonce,
    NetworkIsolationPartition network_isolation_partition)
    : NetworkIsolationKey(
          base::MakeRefCounted<Data>(std::move(top_frame_site),
                                     std::move(frame_site),
                                     std::move(nonce),
                                     network_isolation_partition)) {}

NetworkIsolationKey::NetworkIsolationKey()
    : NetworkIsolationKey(Data::GetEmptyData()) {}

NetworkIsolationKey::NetworkIsolationKey(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey::NetworkIsolationKey(
    NetworkIsolationKey&& network_isolation_key) = default;

NetworkIsolationKey::NetworkIsolationKey(const scoped_refptr<const Data>& data)
    : data_(data) {
  CHECK(data_);
  CHECK(!data_->nonce() || !data_->nonce()->is_empty());
}

NetworkIsolationKey::~NetworkIsolationKey() = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    NetworkIsolationKey&& network_isolation_key) = default;

NetworkIsolationKey NetworkIsolationKey::CreateTransientForTesting() {
  SchemefulSite site_with_opaque_origin;
  return NetworkIsolationKey(site_with_opaque_origin, site_with_opaque_origin);
}

NetworkIsolationKey NetworkIsolationKey::CreateWithNewFrameSite(
    const SchemefulSite& new_frame_site) const {
  if (data_->is_empty()) {
    return NetworkIsolationKey();
  }
  return NetworkIsolationKey(data_->top_frame_site().value(), new_frame_site,
                             data_->nonce(),
                             data_->network_isolation_partition());
}

std::optional<std::string> NetworkIsolationKey::ToCacheKeyString() const {
  if (IsTransient())
    return std::nullopt;

  std::string network_isolation_partition_string =
      GetNetworkIsolationPartition() == NetworkIsolationPartition::kGeneral
          ? ""
          : " " + GetNetworkIsolationPartitionStringForCacheKey(
                      GetNetworkIsolationPartition());
  return GetTopFrameSite()->Serialize() + " " + GetFrameSite()->Serialize() +
         network_isolation_partition_string;
}

std::string NetworkIsolationKey::ToDebugString() const {
  if (data_->is_empty()) {
    return "null null";
  }
  // The space-separated serialization of |top_frame_site_| and
  // |frame_site_|.
  std::string return_string = GetTopFrameSite()->GetDebugString();
  return_string += " " + GetFrameSite()->GetDebugString();

  if (GetNonce().has_value()) {
    return_string += " (with nonce " + GetNonce()->ToString() + ")";
  }

  if (GetNetworkIsolationPartition() != NetworkIsolationPartition::kGeneral) {
    return_string +=
        " (" +
        NetworkIsolationPartitionToDebugString(GetNetworkIsolationPartition()) +
        ")";
  }

  return return_string;
}

bool NetworkIsolationKey::IsFullyPopulated() const {
  return !IsEmpty();
}

bool NetworkIsolationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;
  return IsOpaque();
}

bool NetworkIsolationKey::IsEmpty() const {
  return data_->is_empty();
}

bool NetworkIsolationKey::IsOpaque() const {
  if (GetTopFrameSite()->opaque()) {
    return true;
  }
  if (GetFrameSite()->opaque()) {
    return true;
  }
  if (GetNonce().has_value()) {
    return true;
  }
  return false;
}

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const NetworkIsolationKey& nik) {
  os << nik.ToDebugString();
  return os;
}

// static
scoped_refptr<NetworkIsolationKey::Data>
NetworkIsolationKey::Data::GetEmptyData() {
  static base::NoDestructor<scoped_refptr<NetworkIsolationKey::Data>>
      empty_data(base::MakeRefCounted<Data>(base::PassKey<Data>()));
  return *empty_data;
}

NetworkIsolationKey::Data::Data(base::PassKey<Data>)
    : network_isolation_partition_(NetworkIsolationPartition::kGeneral) {
  CHECK(is_empty());
}

NetworkIsolationKey::Data::Data(
    SchemefulSite&& top_frame_site,
    SchemefulSite&& frame_site,
    std::optional<base::UnguessableToken>&& nonce,
    NetworkIsolationPartition network_isolation_partition)
    : top_frame_site_(std::move(top_frame_site)),
      frame_site_(std::move(frame_site)),
      nonce_(std::move(nonce)),
      network_isolation_partition_(network_isolation_partition) {
  CHECK(top_frame_site_.has_value());
  CHECK(frame_site_.has_value());
  CHECK(!is_empty());
}

NetworkIsolationKey::Data::~Data() = default;

}  // namespace net
