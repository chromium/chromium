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
absl::optional<CookiePartitionKey>
CookieAccessDelegate::CreateCookiePartitionKey(
    const CookieAccessDelegate* delegate,
    const NetworkIsolationKey& network_isolation_key) {
  absl::optional<SchemefulSite> fps_owner_site = absl::nullopt;
  if (delegate) {
    absl::optional<SchemefulSite> top_frame_site =
        network_isolation_key.GetTopFrameSite();
    if (!top_frame_site)
      return absl::nullopt;
    fps_owner_site = delegate->FindFirstPartySetOwner(top_frame_site.value());
  }
  return CookiePartitionKey::FromNetworkIsolationKey(
      network_isolation_key, base::OptionalOrNullptr(fps_owner_site));
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
  absl::optional<SchemefulSite> fps_owner_site =
      delegate->FindFirstPartySetOwner(cookie_partition_key.site());
  if (!fps_owner_site) {
    std::move(callback).Run(cookie_partition_key);
    return;
  }
  std::move(callback).Run(CookiePartitionKey::FromWire(
      fps_owner_site.value(), cookie_partition_key.nonce()));
}

}  // namespace net
