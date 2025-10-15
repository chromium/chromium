// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/agent_cluster_key.h"

#include <variant>

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

// static
AgentClusterKey AgentClusterKey::CreateSiteKeyed(const GURL& site_url,
                                                 const OACStatus& oac_status) {
  CHECK(oac_status != AgentClusterKey::OACStatus::kOriginKeyedByHeader &&
        oac_status != AgentClusterKey::OACStatus::kOriginKeyedByDefault);
  return AgentClusterKey(site_url, std::nullopt, oac_status);
}

// static
AgentClusterKey AgentClusterKey::CreateOriginKeyed(
    const url::Origin& origin,
    const OACStatus& oac_status) {
  // Note: while one might expect that the |oac_status| in this case would be
  // kOriginKeyed*, this is not necessarily true. The browser might want to
  // assign origin-keyed agent clusters in some cases, even when the document
  // did not request OAC and kOriginKeyedProcessesByDefault is not enabled. This
  // does not happen in practice currently, but should happen when we convert
  // the following cases to always create origin-keyed AgentClusterKeys:
  //   - origin-isolated sandboxed data iframes
  //   - legacy kStrictOriginIsolation mode.
  return AgentClusterKey(origin, std::nullopt, oac_status);
}

// static
AgentClusterKey AgentClusterKey::CreateWithCrossOriginIsolationKey(
    const url::Origin& origin,
    const CrossOriginIsolationKey& isolation_key,
    const OACStatus& oac_status) {
  // Note: cross-origin isolated contexts are always origin-keyed per spec,
  // regardless of the OAC header. So the |oac_status| passed to this function
  // is not necessarily kOriginKeyed*.
  return AgentClusterKey(origin, isolation_key, oac_status);
}

AgentClusterKey::AgentClusterKey() : key_(GURL()) {}

AgentClusterKey::AgentClusterKey(const AgentClusterKey& other) = default;

AgentClusterKey::~AgentClusterKey() = default;

bool AgentClusterKey::IsSiteKeyed() const {
  return std::holds_alternative<GURL>(key_);
}

bool AgentClusterKey::IsOriginKeyed() const {
  return std::holds_alternative<url::Origin>(key_);
}

const GURL& AgentClusterKey::GetSite() const {
  CHECK(IsSiteKeyed());
  return std::get<GURL>(key_);
}

const url::Origin& AgentClusterKey::GetOrigin() const {
  CHECK(IsOriginKeyed());
  return std::get<url::Origin>(key_);
}

GURL AgentClusterKey::GetURL() const {
  if (IsSiteKeyed()) {
    return GetSite();
  }
  return GetOrigin().GetURL();
}

const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
AgentClusterKey::GetCrossOriginIsolationKey() const {
  return isolation_key_;
}

bool AgentClusterKey::IsCrossOriginIsolated() const {
  if (!isolation_key_.has_value()) {
    return false;
  }
  return isolation_key_->cross_origin_isolation_mode ==
         CrossOriginIsolationMode::kConcrete;
}

bool AgentClusterKey::operator==(const AgentClusterKey& b) const {
  if (GetCrossOriginIsolationKey() != b.GetCrossOriginIsolationKey()) {
    return false;
  }

  if (key_ != b.key_) {
    return false;
  }

  // |oac_status_| is intentionally omitted from the comparison operator. See
  // the member description for more details.
  return true;
}

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

  // |oac_status_| is intentionally omitted from the comparison operator. See
  // the member description for more details.
  return GetSite() < b.GetSite();
}

AgentClusterKey::AgentClusterKey(
    const std::variant<GURL, url::Origin>& key,
    const std::optional<CrossOriginIsolationKey>& isolation_key,
    const OACStatus& oac_status)
    : key_(key), isolation_key_(isolation_key), oac_status_(oac_status) {}

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
