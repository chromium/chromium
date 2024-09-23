// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/agent_cluster_key.h"

#include "base/notreached.h"

namespace content {

AgentClusterKey::CrossOriginIsolationKey::CrossOriginIsolationKey(
    const url::Origin& common_coi_origin,
    CrossOriginIsolationMode cross_origin_isolation_mode)
    : common_coi_origin(common_coi_origin),
      cross_origin_isolation_mode(cross_origin_isolation_mode) {}

AgentClusterKey::CrossOriginIsolationKey::CrossOriginIsolationKey(
    const CrossOriginIsolationKey& other) = default;

AgentClusterKey::CrossOriginIsolationKey::~CrossOriginIsolationKey() = default;

bool AgentClusterKey::CrossOriginIsolationKey::operator==(
    const CrossOriginIsolationKey& b) const = default;
bool AgentClusterKey::CrossOriginIsolationKey::operator!=(
    const CrossOriginIsolationKey& b) const = default;

// static
AgentClusterKey AgentClusterKey::CreateSiteKeyed(const GURL& site_url) {
  return AgentClusterKey(site_url, std::nullopt);
}

// static
AgentClusterKey AgentClusterKey::CreateOriginKeyed(const url::Origin& origin) {
  return AgentClusterKey(origin, std::nullopt);
}

// static
AgentClusterKey AgentClusterKey::CreateWithCrossOriginIsolationKey(
    const url::Origin& origin,
    const CrossOriginIsolationKey& isolation_key) {
  return AgentClusterKey(origin, isolation_key);
}

AgentClusterKey::AgentClusterKey(const AgentClusterKey& other) = default;

AgentClusterKey::~AgentClusterKey() = default;

bool AgentClusterKey::IsSiteKeyed() const {
  return absl::holds_alternative<GURL>(key_);
}

bool AgentClusterKey::IsOriginKeyed() const {
  return absl::holds_alternative<url::Origin>(key_);
}

const GURL& AgentClusterKey::GetSite() const {
  CHECK(IsSiteKeyed());
  return absl::get<GURL>(key_);
}

const url::Origin& AgentClusterKey::GetOrigin() const {
  CHECK(IsOriginKeyed());
  return absl::get<url::Origin>(key_);
}

const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
AgentClusterKey::GetCrossOriginIsolationKey() const {
  return isolation_key_;
}

bool AgentClusterKey::operator==(const AgentClusterKey& b) const = default;
bool AgentClusterKey::operator!=(const AgentClusterKey& b) const = default;

bool AgentClusterKey::operator<(const AgentClusterKey& b) const {
  if (GetCrossOriginIsolationKey().has_value() !=
      b.GetCrossOriginIsolationKey().has_value()) {
    return !GetCrossOriginIsolationKey().has_value();
  }

  if (GetCrossOriginIsolationKey().has_value() &&
      GetCrossOriginIsolationKey() != b.GetCrossOriginIsolationKey()) {
    if (GetCrossOriginIsolationKey()->cross_origin_isolation_mode !=
        b.GetCrossOriginIsolationKey()->cross_origin_isolation_mode) {
      return GetCrossOriginIsolationKey()->cross_origin_isolation_mode !=
             CrossOriginIsolationMode::kConcrete;
    }
    return GetCrossOriginIsolationKey()->common_coi_origin <
           b.GetCrossOriginIsolationKey()->common_coi_origin;
  }

  if (IsOriginKeyed() != b.IsOriginKeyed()) {
    return !IsOriginKeyed();
  }

  if (IsOriginKeyed()) {
    return GetOrigin() < b.GetOrigin();
  }

  return GetSite() < b.GetSite();
}

AgentClusterKey::AgentClusterKey(
    const absl::variant<GURL, url::Origin>& key,
    const std::optional<CrossOriginIsolationKey>& isolation_key)
    : key_(key), isolation_key_(isolation_key) {}

std::ostream& operator<<(std::ostream& out,
                         const AgentClusterKey& agent_cluster_key) {
  out << "{";
  if (agent_cluster_key.IsSiteKeyed()) {
    out << "site_: ";
    out << agent_cluster_key.GetSite();
  } else {
    out << "origin_: ";
    out << agent_cluster_key.GetOrigin();
  }

  if (agent_cluster_key.GetCrossOriginIsolationKey().has_value()) {
    out << ", cross_origin_isolation_key_: {common_coi_origin: ";
    out << agent_cluster_key.GetCrossOriginIsolationKey()->common_coi_origin;
    out << ", cross_origin_isolation_mode: ";
    if (agent_cluster_key.GetCrossOriginIsolationKey()
            ->cross_origin_isolation_mode ==
        CrossOriginIsolationMode::kConcrete) {
      out << "concrete";
    } else {
      out << "logical";
    }
    out << "}";
  }

  out << "}";

  return out;
}

}  // namespace content
