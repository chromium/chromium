// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_features.h"

#include <atomic>

#include "base/no_destructor.h"

namespace net {

namespace {
std::atomic_bool* GetLocalAnchorConstraintsEnforcementFlag() {
  static std::atomic_bool flag = base::FeatureList::IsEnabled(
      net::features::kEnforceLocalAnchorConstraints);
  return &flag;
}

}  // namespace

bool IsLocalAnchorConstraintsEnforcementEnabled() {
  return GetLocalAnchorConstraintsEnforcementFlag()->load();
}

void SetLocalAnchorConstraintsEnforcementEnabled(bool enabled) {
  GetLocalAnchorConstraintsEnforcementFlag()->store(enabled);
}

namespace features {

BASE_FEATURE(kEnforceLocalAnchorConstraints,
             "EnforceLocalAnchorConstraints",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

}  // namespace net
