// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_change_quota_tracker.h"

#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/singleton.h"
#include "components/sync/engine/cycle/commit_quota.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/origin.h"

namespace webauthn {

PasskeyChangeQuotaTracker::~PasskeyChangeQuotaTracker() = default;

// static
PasskeyChangeQuotaTracker* PasskeyChangeQuotaTracker::GetInstance() {
  return base::Singleton<PasskeyChangeQuotaTracker>::get();
}

void PasskeyChangeQuotaTracker::TrackChange(const url::Origin& origin) {
  // Do not limit requests to localhost, which would hurt developers.
  if (net::HostStringIsLocalhost(origin.host())) {
    return;
  }
  GetOrAllocateQuota(origin).ConsumeToken();
}

bool PasskeyChangeQuotaTracker::CanMakeChange(const url::Origin& origin) {
  return GetOrAllocateQuota(origin).HasTokensAvailable();
}

PasskeyChangeQuotaTracker::PasskeyChangeQuotaTracker() = default;

syncer::CommitQuota& PasskeyChangeQuotaTracker::GetOrAllocateQuota(
    const url::Origin& origin) {
  std::string relying_party =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  auto rp_quota_it = quota_per_rp_.find(relying_party);
  if (rp_quota_it == quota_per_rp_.end()) {
    bool did_insert;
    std::tie(rp_quota_it, did_insert) = quota_per_rp_.try_emplace(
        std::move(relying_party), kMaxTokensPerRP, kRefillInterval);
    CHECK(did_insert);
  }
  return rp_quota_it->second;
}

}  // namespace webauthn
