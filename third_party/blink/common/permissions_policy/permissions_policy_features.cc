// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"

#include <stdint.h>

#include <string>

#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/strings/string_util.h"
#include "third_party/blink/common/permissions_policy/permissions_policy_features_generated.h"
#include "third_party/blink/common/permissions_policy/permissions_policy_features_internal.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "url/origin.h"

// This file contains static code that is combined with templated code of
// permissions_policy_features.cc.tmpl.

namespace blink {
namespace {
// Return true if we should use EnabledForNone as the default for "unload"
// feature. This is special logic for https://crbug.com/1432116
// `bucket` is cast to a uint8_t, so there should be no more than 256 possible
// buckets.
// If `origin` is an opaque origin, its precursor host will be used.
bool ShouldUnloadBeNone(const url::Origin& origin, int percent, int bucket) {
  if (percent == 100) {
    return true;
  }
  if (percent == 0) {
    return false;
  }
  // For opaque origins we hash them by their precursor host to avoid placing
  // them all in the same bucket.
  const std::string& host =
      origin.opaque() ? origin.GetTupleOrPrecursorTupleIfOpaque().host()
                      : origin.host();
  // Hash the host then hash that with the bucket. Without this (by simply
  // adding the bucket afterwards), a user in bucket `hash` is identical to a
  // user in buckets `hash+1`, `hash+2`, ..., `hash+percent-1`. With this, no
  // buckets get identical behaviour.
  const uint8_t hash[2] = {static_cast<uint8_t>(base::PersistentHash(host)),
                           static_cast<uint8_t>(bucket)};
  const int hash_bucket = base::PersistentHash(hash) % 100;
  return hash_bucket < percent;
}

}  // namespace

const PermissionsPolicyFeatureList& GetPermissionsPolicyFeatureList(
    const url::Origin& origin) {
  // Respect enterprise policy.
  if (!base::CommandLine::InitializedForCurrentProcess() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePermissionPolicyUnloadDefaultEnabled)) {
    return GetPermissionsPolicyFeatureListUnloadAll();
  }

  // Consider the finch flags and params.
  if (!base::FeatureList::IsEnabled(features::kDeprecateUnload) ||
      !UnloadDeprecationAllowedForOrigin(origin)) {
    return GetPermissionsPolicyFeatureListUnloadAll();
  }
  if (ShouldUnloadBeNone(origin, features::kDeprecateUnloadPercent.Get(),
                         features::kDeprecateUnloadBucket.Get())) {
    // If the flag is on and the rollout % is high enough, disable unload by
    // default.
    return GetPermissionsPolicyFeatureListUnloadNone();
  } else {
    return GetPermissionsPolicyFeatureListUnloadAll();
  }
}

void UpdatePermissionsPolicyFeatureListForTesting() {
  UpdatePermissionsPolicyFeatureListFlagDefaults(
      GetPermissionsPolicyFeatureListUnloadAll());
  UpdatePermissionsPolicyFeatureListFlagDefaults(
      GetPermissionsPolicyFeatureListUnloadNone());
}

}  // namespace blink
