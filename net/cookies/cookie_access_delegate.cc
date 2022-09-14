// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_access_delegate.h"

#include <set>

#include "base/callback.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {
CookiePartitionKey CreateCookiePartitionKeyFromFirstPartySetEntry(
    const CookiePartitionKey& cookie_partition_key,
    base::flat_map<net::SchemefulSite, FirstPartySetEntry> entries) {
  if (entries.empty()) {
    return cookie_partition_key;
  }
  return CookiePartitionKey::FromWire(entries.begin()->second.primary(),
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

  absl::optional<base::flat_map<net::SchemefulSite, FirstPartySetEntry>>
      maybe_entries = delegate->FindFirstPartySetEntries(
          {cookie_partition_key.site()},
          base::BindOnce(&CreateCookiePartitionKeyFromFirstPartySetEntry,
                         cookie_partition_key)
              .Then(std::move(callback)));
  if (maybe_entries.has_value())
    return CreateCookiePartitionKeyFromFirstPartySetEntry(
        cookie_partition_key, maybe_entries.value());

  return absl::nullopt;
}

}  // namespace net
