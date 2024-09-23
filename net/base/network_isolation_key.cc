// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_isolation_key.h"

#include <cstddef>
#include <optional>
#include <string>

#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "schemeful_site.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace net {

namespace {

std::string GetSiteDebugString(const std::optional<SchemefulSite>& site) {
  return site ? site->GetDebugString() : "null";
}

}  // namespace

NetworkIsolationKey::NetworkIsolationKey(
    const SchemefulSite& top_frame_site,
    const SchemefulSite& frame_site,
    const std::optional<base::UnguessableToken>& nonce)
    : NetworkIsolationKey(SchemefulSite(top_frame_site),
                          SchemefulSite(frame_site),
                          std::optional<base::UnguessableToken>(nonce)) {}

NetworkIsolationKey::NetworkIsolationKey(
    SchemefulSite&& top_frame_site,
    SchemefulSite&& frame_site,
    std::optional<base::UnguessableToken>&& nonce)
    : top_frame_site_(std::move(top_frame_site)),
      frame_site_(std::make_optional(std::move(frame_site))),
      nonce_(std::move(nonce)) {
  DCHECK(!nonce_ || !nonce_->is_empty());
}

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

NetworkIsolationKey NetworkIsolationKey::CreateTransientForTesting() {
  SchemefulSite site_with_opaque_origin;
  return NetworkIsolationKey(site_with_opaque_origin, site_with_opaque_origin);
}

NetworkIsolationKey NetworkIsolationKey::CreateWithNewFrameSite(
    const SchemefulSite& new_frame_site) const {
  if (!top_frame_site_)
    return NetworkIsolationKey();
  return NetworkIsolationKey(top_frame_site_.value(), new_frame_site, nonce_);
}

std::optional<std::string> NetworkIsolationKey::ToCacheKeyString() const {
  if (IsTransient())
    return std::nullopt;

  return top_frame_site_->Serialize() + " " + frame_site_->Serialize();
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
  if (!top_frame_site_.has_value()) {
    return false;
  }
  if (!frame_site_.has_value()) {
    return false;
  }
  return true;
}

bool NetworkIsolationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;
  return IsOpaque();
}

bool NetworkIsolationKey::IsEmpty() const {
  return !top_frame_site_.has_value() && !frame_site_.has_value();
}

bool NetworkIsolationKey::IsOpaque() const {
  if (top_frame_site_->opaque()) {
    return true;
  }
  if (frame_site_->opaque()) {
    return true;
  }
  if (nonce_.has_value()) {
    return true;
  }
  return false;
}

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const NetworkIsolationKey& nik) {
  os << nik.ToDebugString();
  return os;
}

}  // namespace net
