// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_FEATURES_H_
#define NET_CERT_INTERNAL_TRUST_STORE_FEATURES_H_

#include "base/feature_list.h"
#include "net/base/net_export.h"

namespace net {

// Returns true when platform TrustStore implementations should enforce
// constraints encoded into X.509 certificate trust anchors.
// When disabled, platform TrustStore implementations will not enforce anchor
// constraints (other than expiry).
// Has no effect if using a platform CertVerifyProc implementation.
// TODO(https://crbug.com/1406103): remove this a few milestones after the
// trust anchor constraints enforcement has been launched on all relevant
// platforms.
// Should only be called after base::Features have been resolved. Note that
// using ScopedFeatureList to override this won't work properly in unittests,
// use ScopedLocalAnchorConstraintsEnforcementForTesting instead. Using
// ScopedFeatureList in browser_tests is fine.
// It is safe to call this function on any thread.
NET_EXPORT bool IsLocalAnchorConstraintsEnforcementEnabled();

// Override the feature flag. Don't call this without consulting
// net/cert/OWNERS.
// It is safe to call this function on any thread.
NET_EXPORT void SetLocalAnchorConstraintsEnforcementEnabled(bool enabled);

// Temporarily change the SetLocalAnchorConstraintsEnforcementEnabled value,
// resetting to the original value when destructed.
class NET_EXPORT ScopedLocalAnchorConstraintsEnforcementForTesting {
 public:
  explicit ScopedLocalAnchorConstraintsEnforcementForTesting(bool enabled)
      : previous_value_(IsLocalAnchorConstraintsEnforcementEnabled()) {
    SetLocalAnchorConstraintsEnforcementEnabled(enabled);
  }

  ~ScopedLocalAnchorConstraintsEnforcementForTesting() {
    SetLocalAnchorConstraintsEnforcementEnabled(previous_value_);
  }

 private:
  const bool previous_value_;
};

namespace features {

// Most code should not check this feature flag directly, instead use
// IsLocalAnchorConstraintsEnforcementEnabled().
NET_EXPORT BASE_DECLARE_FEATURE(kEnforceLocalAnchorConstraints);

}  // namespace features

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_FEATURES_H_
