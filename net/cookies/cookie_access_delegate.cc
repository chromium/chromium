// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_access_delegate.h"

#include <set>

#include "base/callback.h"
#include "base/stl_util.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/first_party_set_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

CookieAccessDelegate::CookieAccessDelegate() = default;

CookieAccessDelegate::~CookieAccessDelegate() = default;

bool CookieAccessDelegate::ShouldTreatUrlAsTrustworthy(const GURL& url) const {
  return false;
}

// static
void CookieAccessDelegate::CreateCookiePartitionKey(
    const CookieAccessDelegate* delegate,
    const NetworkIsolationKey& network_isolation_key,
    base::OnceCallback<void(absl::optional<net::CookiePartitionKey>)>
        callback) {
  if (!delegate) {
    std::move(callback).Run(CookiePartitionKey::FromNetworkIsolationKey(
        network_isolation_key, nullptr));
    return;
  }

  absl::optional<SchemefulSite> top_frame_site =
      network_isolation_key.GetTopFrameSite();
  if (!top_frame_site) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  delegate->FindFirstPartySetOwner(
      top_frame_site.value(),
      base::BindOnce(
          [](const NetworkIsolationKey& network_isolation_key,
             base::OnceCallback<void(absl::optional<net::CookiePartitionKey>)>
                 callback,
             absl::optional<net::SchemefulSite> first_party_set_owner) {
            std::move(callback).Run(CookiePartitionKey::FromNetworkIsolationKey(
                network_isolation_key,
                base::OptionalOrNullptr(first_party_set_owner)));
          },
          network_isolation_key, std::move(callback)));
}

// static
void CookieAccessDelegate::FirstPartySetifyPartitionKey(
    const CookieAccessDelegate* delegate,
    const CookiePartitionKey& cookie_partition_key,
    base::OnceCallback<void(absl::optional<CookiePartitionKey>)> callback) {
  if (!delegate) {
    std::move(callback).Run(cookie_partition_key);
    return;
  }
  delegate->FindFirstPartySetOwner(
      cookie_partition_key.site(),
      base::BindOnce(
          [](const CookiePartitionKey& cookie_partition_key,
             base::OnceCallback<void(absl::optional<CookiePartitionKey>)>
                 callback,
             absl::optional<SchemefulSite> first_party_set_owner) {
            if (!first_party_set_owner) {
              std::move(callback).Run(cookie_partition_key);
              return;
            }
            std::move(callback).Run(CookiePartitionKey::FromWire(
                first_party_set_owner.value(), cookie_partition_key.nonce()));
          },
          cookie_partition_key, std::move(callback)));
}

}  // namespace net
