// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/permissions_policy/permissions_policy_features_internal.h"

#include <stdint.h>

#include <string>

#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "third_party/blink/public/common/features.h"
#include "url/scheme_host_port.h"

namespace blink {

using HostSet = std::unordered_set<std::string>;

const HostSet UnloadDeprecationAllowedHosts() {
  auto hosts =
      base::SplitString(features::kDeprecateUnloadAllowlist.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return HostSet(hosts.begin(), hosts.end());
}

// Return true if we should use EnabledForNone as the default for "unload"
// feature. This is special logic for https://crbug.com/1432116
// `bucket` is cast to a uint8_t, so there should be no more than 256 possible
// buckets.
bool IsIncludedInGradualRollout(const std::string& host,
                                int percent,
                                int bucket) {
  if (percent == 100) {
    return true;
  }
  if (percent == 0) {
    return false;
  }
  // Hash the host then hash that with the bucket. Without this (by simply
  // adding the bucket afterwards), a user in bucket `hash` is identical to a
  // user in buckets `hash+1`, `hash+2`, ..., `hash+percent-1`. With this, no
  // buckets get identical behaviour.
  const uint8_t hash[2] = {static_cast<uint8_t>(base::PersistentHash(host)),
                           static_cast<uint8_t>(bucket)};
  const int hash_bucket = base::PersistentHash(hash) % 100;
  return hash_bucket < percent;
}

bool UnloadDeprecationAllowedForHost(const std::string& host,
                                     const HostSet& hosts) {
  if (hosts.empty()) {
    return true;
  }
  return hosts.contains(host);
}

bool UnloadDeprecationAllowedForOrigin(const url::Origin& origin) {
  // For opaque origins we want their behaviour to be consistent with their
  // precursor. If the origin is opaque and has no precursor, we will use "",
  // there's not much else we can do in this case.
  const url::SchemeHostPort& shp = origin.GetTupleOrPrecursorTupleIfOpaque();
  // Only disable unload on http(s):// pages, not chrome:// etc.
  // TODO(https://crbug.com/1495734): Remove this when all internal unload usage
  // has been removed.
  if (shp.scheme() != "http" && shp.scheme() != "https") {
    return false;
  }

  if (base::FeatureList::IsEnabled(features::kDeprecateUnloadByAllowList)) {
    static const base::NoDestructor<HostSet> hosts(
        UnloadDeprecationAllowedHosts());
    if (!UnloadDeprecationAllowedForHost(shp.host(), *hosts)) {
      return false;
    }
  }

  return IsIncludedInGradualRollout(shp.host(),
                                    features::kDeprecateUnloadPercent.Get(),
                                    features::kDeprecateUnloadBucket.Get());
}

}  // namespace blink
