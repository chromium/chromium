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

namespace {
CookiePartitionKey CreateCookiePartitionKeyFromFirstPartySetOwner(
    const CookiePartitionKey& cookie_partition_key,
    absl::optional<SchemefulSite> first_party_set_owner) {
  if (!first_party_set_owner) {
    return cookie_partition_key;
  }
  return CookiePartitionKey::FromWire(first_party_set_owner.value(),
                                      cookie_partition_key.nonce());
}
}  // namespace

CookieAccessDelegate::CookieAccessDelegate() = default;

CookieAccessDelegate::~CookieAccessDelegate() = default;

bool CookieAccessDelegate::ShouldTreatUrlAsTrustworthy(const GURL& url) const {
  return false;
}

// static
absl::optional<CookiePartitionKey>
CookieAccessDelegate::FirstPartySetifyPartitionKey(
    const CookieAccessDelegate* delegate,
    const CookiePartitionKey& cookie_partition_key,
    base::OnceCallback<void(CookiePartitionKey)> callback) {
  // FirstPartySetify doesn't need to transform partition keys with a nonce,
  // since those partitions are only available to a single fenced/anonymous
  // iframe.
  if (!delegate || cookie_partition_key.nonce()) {
    return cookie_partition_key;
  }

  absl::optional<absl::optional<SchemefulSite>> maybe_owner =
      delegate->FindFirstPartySetOwner(
          cookie_partition_key.site(),
          base::BindOnce(&CreateCookiePartitionKeyFromFirstPartySetOwner,
                         cookie_partition_key)
              .Then(std::move(callback)));
  if (maybe_owner.has_value())
    return CreateCookiePartitionKeyFromFirstPartySetOwner(cookie_partition_key,
                                                          maybe_owner.value());

  return absl::nullopt;
}

}  // namespace net
