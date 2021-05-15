// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/values.h"
#include "net/base/network_isolation_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace net {

namespace {

std::string GetSiteDebugString(const absl::optional<SchemefulSite>& site) {
  return site ? site->GetDebugString() : "null";
}

}  // namespace

NetworkIsolationKey::NetworkIsolationKey(const SchemefulSite& top_frame_site,
                                         const SchemefulSite& frame_site)
    : NetworkIsolationKey(SchemefulSite(top_frame_site),
                          SchemefulSite(frame_site),
                          false /* opaque_and_non_transient */) {}

NetworkIsolationKey::NetworkIsolationKey(SchemefulSite&& top_frame_site,
                                         SchemefulSite&& frame_site)
    : NetworkIsolationKey(std::move(top_frame_site),
                          std::move(frame_site),
                          false /* opaque_and_non_transient */) {}

NetworkIsolationKey::NetworkIsolationKey(const url::Origin& top_frame_origin,
                                         const url::Origin& frame_origin)
    : NetworkIsolationKey(SchemefulSite(top_frame_origin),
                          SchemefulSite(frame_origin)) {}

NetworkIsolationKey::NetworkIsolationKey() = default;

NetworkIsolationKey::NetworkIsolationKey(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey::NetworkIsolationKey(
    NetworkIsolationKey&& network_isolation_key) = default;

NetworkIsolationKey::~NetworkIsolationKey() = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    NetworkIsolationKey&& network_isolation_key) = default;

NetworkIsolationKey NetworkIsolationKey::CreateTransient() {
  SchemefulSite site_with_opaque_origin;
  return NetworkIsolationKey(site_with_opaque_origin, site_with_opaque_origin);
}

NetworkIsolationKey NetworkIsolationKey::CreateOpaqueAndNonTransient() {
  SchemefulSite site_with_opaque_origin;
  return NetworkIsolationKey(SchemefulSite(site_with_opaque_origin),
                             SchemefulSite(site_with_opaque_origin),
                             true /* opaque_and_non_transient */);
}

NetworkIsolationKey NetworkIsolationKey::CreateWithNewFrameSite(
    const SchemefulSite& new_frame_site) const {
  if (!top_frame_site_)
    return NetworkIsolationKey();
  NetworkIsolationKey key(top_frame_site_.value(), new_frame_site);
  key.opaque_and_non_transient_ = opaque_and_non_transient_;
  return key;
}

NetworkIsolationKey NetworkIsolationKey::CreateWithNewFrameOrigin(
    const url::Origin& new_frame_origin) const {
  return CreateWithNewFrameSite(SchemefulSite(new_frame_origin));
}

std::string NetworkIsolationKey::ToString() const {
  if (IsTransient())
    return "";

  if (IsOpaque()) {
    // This key is opaque but not transient.

    absl::optional<std::string> serialized_top_frame_site =
        SerializeSiteWithNonce(top_frame_site_.value());
    absl::optional<std::string> serialized_frame_site =
        SerializeSiteWithNonce(frame_site_.value());

    // SerializeSiteWithNonce() can't fail for valid origins, and only valid
    // origins can be opaque.
    DCHECK(serialized_top_frame_site);
    DCHECK(serialized_frame_site);

    return "opaque non-transient " + *serialized_top_frame_site + " " +
           *serialized_frame_site;
  }

  return top_frame_site_->Serialize() + " " + frame_site_->Serialize();
}

std::string NetworkIsolationKey::ToDebugString() const {
  // The space-separated serialization of |top_frame_site_| and
  // |frame_site_|.
  std::string return_string = GetSiteDebugString(top_frame_site_);
  return_string += " " + GetSiteDebugString(frame_site_);

  if (IsFullyPopulated() && IsOpaque() && opaque_and_non_transient_) {
    return_string += " non-transient";
  }

  return return_string;
}

bool NetworkIsolationKey::IsFullyPopulated() const {
  return top_frame_site_.has_value() && frame_site_.has_value();
}

bool NetworkIsolationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;
  if (opaque_and_non_transient_) {
    DCHECK(IsOpaque());
    return false;
  }
  return IsOpaque();
}

bool NetworkIsolationKey::ToValue(base::Value* out_value) const {
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
  *out_value = base::Value(base::Value::Type::LIST);
  out_value->Append(std::move(*top_frame_value));

  absl::optional<std::string> frame_value =
      SerializeSiteWithNonce(*frame_site_);
  if (!frame_value)
    return false;
  out_value->Append(std::move(*frame_value));

  return true;
}

bool NetworkIsolationKey::FromValue(
    const base::Value& value,
    NetworkIsolationKey* network_isolation_key) {
  if (!value.is_list())
    return false;

  base::Value::ConstListView list = value.GetList();
  if (list.empty()) {
    *network_isolation_key = NetworkIsolationKey();
    return true;
  }

  if (list.size() != 2 || !list[0].is_string() || !list[1].is_string())
    return false;

  absl::optional<SchemefulSite> top_frame_site =
      SchemefulSite::DeserializeWithNonce(list[0].GetString());
  if (!top_frame_site)
    return false;

  // An opaque origin key will only be serialized into a base::Value if
  // |opaque_and_non_transient_| is set. Therefore if either origin is opaque,
  // |opaque_and_non_transient_| must be true.
  bool opaque_and_non_transient = top_frame_site->opaque();

  absl::optional<SchemefulSite> frame_site =
      SchemefulSite::DeserializeWithNonce(list[1].GetString());
  if (!frame_site)
    return false;

  opaque_and_non_transient |= frame_site->opaque();

  *network_isolation_key =
      NetworkIsolationKey(std::move(*top_frame_site), std::move(*frame_site));
  network_isolation_key->opaque_and_non_transient_ = opaque_and_non_transient;
  return true;
}

bool NetworkIsolationKey::IsEmpty() const {
  return !top_frame_site_.has_value() && !frame_site_.has_value();
}

NetworkIsolationKey::NetworkIsolationKey(SchemefulSite&& top_frame_site,
                                         SchemefulSite&& frame_site,
                                         bool opaque_and_non_transient)
    : opaque_and_non_transient_(opaque_and_non_transient),
      top_frame_site_(std::move(top_frame_site)),
      frame_site_(std::move(frame_site)) {
  DCHECK(!opaque_and_non_transient || top_frame_site_->opaque());
  DCHECK(!opaque_and_non_transient || frame_site.opaque());
}

bool NetworkIsolationKey::IsOpaque() const {
  return top_frame_site_->opaque() || frame_site_->opaque();
}

absl::optional<std::string> NetworkIsolationKey::SerializeSiteWithNonce(
    const SchemefulSite& site) {
  return *(const_cast<SchemefulSite&>(site).SerializeWithNonce());
}

}  // namespace net
