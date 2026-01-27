// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/agent_cluster_key.h"

#include <utility>

namespace blink {

AgentClusterKey::AgentClusterKey(const AgentClusterKey& other) = default;

AgentClusterKey::~AgentClusterKey() = default;

// Static
AgentClusterKey AgentClusterKey::CreateSiteKeyed(const KURL& site_url) {
  return AgentClusterKey(site_url);
}

// Static
AgentClusterKey AgentClusterKey::CreateOriginKeyed(
    scoped_refptr<const SecurityOrigin> origin) {
  return AgentClusterKey(
      OriginKey{.origin = std::move(origin), .isolation_key = std::nullopt});
}

// static
AgentClusterKey AgentClusterKey::CreateUniversalFileAgent() {
  return AgentClusterKey(File());
}

AgentClusterKey::CrossOriginIsolationKey::CrossOriginIsolationKey(
    scoped_refptr<const SecurityOrigin> common_origin,
    mojom::blink::CrossOriginIsolationMode mode)
    : common_origin(std::move(common_origin)), mode(mode) {}

AgentClusterKey::CrossOriginIsolationKey::CrossOriginIsolationKey(
    const CrossOriginIsolationKey& other) = default;

AgentClusterKey::CrossOriginIsolationKey::~CrossOriginIsolationKey() = default;

bool AgentClusterKey::CrossOriginIsolationKey::operator==(
    const CrossOriginIsolationKey& b) const {
  if (!common_origin->IsSameOriginWith(b.common_origin.get())) {
    return false;
  }

  return mode == b.mode;
}

// Static
AgentClusterKey AgentClusterKey::CreateWithCrossOriginIsolationKey(
    scoped_refptr<const SecurityOrigin> origin,
    const CrossOriginIsolationKey& isolation_key) {
  return AgentClusterKey(
      OriginKey{.origin = std::move(origin), .isolation_key = isolation_key});
}

// Static
AgentClusterKey AgentClusterKey::CreateEmpty(PassKey) {
  return AgentClusterKey(Empty());
}

// Static
AgentClusterKey AgentClusterKey::CreateDeleted(PassKey) {
  return AgentClusterKey(Deleted());
}

bool AgentClusterKey::IsSiteKeyed() const {
  return std::holds_alternative<KURL>(key_);
}

bool AgentClusterKey::IsOriginKeyed() const {
  return std::holds_alternative<OriginKey>(key_);
}

bool AgentClusterKey::IsUniversalFileAgent() const {
  return std::holds_alternative<File>(key_);
}

const AgentClusterKey::CrossOriginIsolationKey*
AgentClusterKey::GetCrossOriginIsolationKey() const {
  if (!IsOriginKeyed()) {
    return nullptr;
  }

  const auto& origin_key = std::get<OriginKey>(key_);

  if (!origin_key.isolation_key.has_value()) {
    return nullptr;
  }

  return &(origin_key.isolation_key.value());
}

bool AgentClusterKey::operator==(const AgentClusterKey& b) const = default;

bool AgentClusterKey::File::operator==(const AgentClusterKey::File& b) const {
  return true;
}

bool AgentClusterKey::Empty::operator==(const AgentClusterKey::Empty& b) const {
  return true;
}

bool AgentClusterKey::Deleted::operator==(
    const AgentClusterKey::Deleted& b) const {
  return true;
}

bool AgentClusterKey::OriginKey::operator==(
    const AgentClusterKey::OriginKey& b) const {
  if (!origin->IsSameOriginWith(b.origin.get())) {
    return false;
  }
  return isolation_key == b.isolation_key;
}

AgentClusterKey::AgentClusterKey(const KURL& site_url) : key_(site_url) {}

AgentClusterKey::AgentClusterKey(const OriginKey& origin_key)
    : key_(origin_key) {}

AgentClusterKey::AgentClusterKey(Empty empty) : key_(empty) {}
AgentClusterKey::AgentClusterKey(Deleted deleted) : key_(deleted) {}
AgentClusterKey::AgentClusterKey(File file) : key_(file) {}

}  // namespace blink
