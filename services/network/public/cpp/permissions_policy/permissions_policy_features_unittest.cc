// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy_features.h"

#include "services/network/public/cpp/permissions_policy/permissions_policy_features_generated.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(PermissionsPolicyFeaturesTest,
     IsPermissionsPolicyFeatureGuardedByIsolatedContext) {
  // Guarded by IsolatedContext
  EXPECT_TRUE(IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "all-screens-capture"));
  EXPECT_TRUE(
      IsPermissionsPolicyFeatureGuardedByIsolatedContext("controlled-frame"));
  EXPECT_TRUE(
      IsPermissionsPolicyFeatureGuardedByIsolatedContext("direct-sockets"));
  EXPECT_TRUE(IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "direct-sockets-private"));
  EXPECT_TRUE(IsPermissionsPolicyFeatureGuardedByIsolatedContext("smart-card"));
  EXPECT_TRUE(IsPermissionsPolicyFeatureGuardedByIsolatedContext("sub-apps"));
  EXPECT_TRUE(
      IsPermissionsPolicyFeatureGuardedByIsolatedContext("usb-unrestricted"));
  EXPECT_TRUE(
      IsPermissionsPolicyFeatureGuardedByIsolatedContext("web-printing"));

  // Not guarded by IsolatedContext
  EXPECT_FALSE(
      IsPermissionsPolicyFeatureGuardedByIsolatedContext("accelerometer"));
  EXPECT_FALSE(IsPermissionsPolicyFeatureGuardedByIsolatedContext("autoplay"));
  EXPECT_FALSE(IsPermissionsPolicyFeatureGuardedByIsolatedContext("camera"));
  EXPECT_FALSE(
      IsPermissionsPolicyFeatureGuardedByIsolatedContext("geolocation"));
  EXPECT_FALSE(
      IsPermissionsPolicyFeatureGuardedByIsolatedContext("microphone"));
  EXPECT_FALSE(IsPermissionsPolicyFeatureGuardedByIsolatedContext("usb"));
  EXPECT_FALSE(
      IsPermissionsPolicyFeatureGuardedByIsolatedContext("unknown-feature"));
}

}  // namespace network
