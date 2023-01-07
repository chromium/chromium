// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>

#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
                                         const SchemefulSite& frame_site,
                                         const base::UnguessableToken* nonce)
    : NetworkIsolationKey(SchemefulSite(top_frame_site),
                          SchemefulSite(frame_site),
                          nonce) {}

NetworkIsolationKey::NetworkIsolationKey(SchemefulSite&& top_frame_site,
                                         SchemefulSite&& frame_site,
                                         const base::UnguessableToken* nonce)
    : top_frame_site_(std::move(top_frame_site)),
      frame_site_(IsFrameSiteEnabled()
                      ? absl::make_optional(std::move(frame_site))
                      : absl::nullopt),
      nonce_(nonce ? absl::make_optional(*nonce) : absl::nullopt) {
  DCHECK(!nonce || !nonce->is_empty());
}

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

NetworkIsolationKey NetworkIsolationKey::CreateWithNewFrameSite(
    const SchemefulSite& new_frame_site) const {
  if (!top_frame_site_)
    return NetworkIsolationKey();
  NetworkIsolationKey key(top_frame_site_.value(), new_frame_site);
  key.nonce_ = nonce_;
  return key;
}

absl::optional<std::string> NetworkIsolationKey::ToCacheKeyString() const {
  if (IsTransient())
    return absl::nullopt;

  std::string frame_site_str =
      " " + (IsFrameSiteEnabled() ? frame_site_->Serialize()
                                  : top_frame_site_->Serialize());
  return top_frame_site_->Serialize() + frame_site_str;
}

std::string NetworkIsolationKey::ToDebugString() const {
  // The space-separated serialization of |top_frame_site_| and
  // |frame_site_|.
  std::string return_string = GetSiteDebugString(top_frame_site_);
  return_string += " " + GetSiteDebugString(frame_site_);

  if (nonce_.has_value()) {
    return_string += " (with nonce " + nonce_->ToString() + ")";
  }

  return return_string;
}

bool NetworkIsolationKey::IsFullyPopulated() const {
  return top_frame_site_.has_value() &&
         (!IsFrameSiteEnabled() || frame_site_.has_value());
}

bool NetworkIsolationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;
  return IsOpaque();
}

bool NetworkIsolationKey::ToValue(base::Value* out_value) const {
  if (IsEmpty()) {
    *out_value = base::Value(base::Value::Type::LIST);
    return true;
  }

  if (IsTransient())
    return false;

  // NetworkIsolationKeys with nonces are now always transient, so serializing
  // with nonces isn't strictly needed, but it's used for backwards
  // compatibility, Origin::Deserialize() is not compatible with
  // SerializeWithNonce().
  absl::optional<std::string> top_frame_value =
      SerializeSiteWithNonce(*top_frame_site_);
  if (!top_frame_value)
    return false;
  base::Value::List list;
  list.Append(std::move(top_frame_value).value());

  absl::optional<std::string> frame_value =
      IsFrameSiteEnabled() ? SerializeSiteWithNonce(*frame_site_)
                           : absl::nullopt;

  if (frame_value.has_value()) {
    // If there is a frame value, append it.
    list.Append(std::move(frame_value).value());
  } else if (IsFrameSiteEnabled()) {
    // If there is supposed to be a frame value but there isn't return false.
    return false;
  }

  // List will have size 1 when frame site is disabled.
  *out_value = base::Value(std::move(list));
  return true;
}

bool NetworkIsolationKey::FromValue(
    const base::Value& value,
    NetworkIsolationKey* network_isolation_key) {
  if (!value.is_list())
    return false;

  const base::Value::List& list = value.GetList();
  if (list.empty()) {
    *network_isolation_key = NetworkIsolationKey();
    return true;
  }

  // When frame site is enabled list must be of size 2 and both values must be
  // strings.

  // When frame site is disabled for double key `list` can be either be of size
  // 2 or of size 1. For backwards compatibility, frame site is allowed to be of
  // size 2 when frame site is disabled because a previous expirement set frame
  // site equal to a copy of top frame site rather than setting it empty.
  if (IsFrameSiteEnabled()) {
    if (list.size() != 2 || !list[0].is_string() || !list[1].is_string()) {
      return false;
    }
  } else {
    if (list.size() < 0 || list.size() > 2 || !list[0].is_string()) {
      return false;
    }
  }

  absl::optional<SchemefulSite> top_frame_site =
      SchemefulSite::DeserializeWithNonce(list[0].GetString());
  // Opaque origins are currently never serialized to disk, but they used to be.
  if (!top_frame_site || top_frame_site->opaque())
    return false;

  if (list.size() == 1) {
    // The value of the frame_site parameter doesn't matter because the
    // constructor will default it to nullopt.
    *network_isolation_key =
        NetworkIsolationKey(std::move(*top_frame_site), net::SchemefulSite());
    return true;
  }

  absl::optional<SchemefulSite> frame_site =
      SchemefulSite::DeserializeWithNonce(list[1].GetString());
  // Opaque origins are currently never serialized to disk, but they used to be.
  if (!frame_site || frame_site->opaque())
    return false;

  *network_isolation_key =
      NetworkIsolationKey(std::move(*top_frame_site), std::move(*frame_site));
  return true;
}

const absl::optional<SchemefulSite>& NetworkIsolationKey::GetFrameSite() const {
  // Frame site will be empty if double-keying is enabled.
  CHECK(NetworkIsolationKey::IsFrameSiteEnabled());
  return frame_site_;
}

bool NetworkIsolationKey::IsEmpty() const {
  return !top_frame_site_.has_value() && !frame_site_.has_value();
}

bool NetworkIsolationKey::IsFrameSiteEnabled() {
  return !base::FeatureList::IsEnabled(
      net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
}

bool NetworkIsolationKey::IsOpaque() const {
  return top_frame_site_->opaque() ||
         (IsFrameSiteEnabled() && frame_site_->opaque()) || nonce_.has_value();
}

absl::optional<std::string> NetworkIsolationKey::SerializeSiteWithNonce(
    const SchemefulSite& site) {
  return *(const_cast<SchemefulSite&>(site).SerializeWithNonce());
}

}  // namespace net
