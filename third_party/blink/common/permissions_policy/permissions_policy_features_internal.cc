// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/permissions_policy/permissions_policy_features_internal.h"

#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

using HostSet = std::unordered_set<std::string>;

const HostSet UnloadDeprecationAllowedHosts() {
  auto hosts =
      base::SplitString(features::kDeprecateUnloadAllowlist.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return HostSet(hosts.begin(), hosts.end());
}

bool UnloadDeprecationAllowedForOrigin(const url::Origin& origin,
                                       const HostSet& hosts) {
  if (hosts.empty()) {
    return true;
  }
  return hosts.contains(origin.host());
}

bool UnloadDeprecationAllowedForOrigin(const url::Origin& origin) {
  static const base::NoDestructor<HostSet> hosts(
      UnloadDeprecationAllowedHosts());
  return UnloadDeprecationAllowedForOrigin(origin, *hosts);
}

}  // namespace blink
