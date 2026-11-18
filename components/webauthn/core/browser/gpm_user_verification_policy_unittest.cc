// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/gpm_user_verification_policy.h"

#include "device/fido/public/fido_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {

namespace {

using device::UserVerificationRequirement;

// Tests that valid user verification requirement strings are converted to the
// matching enum value.
TEST(GpmUserVerificationPolicyTest, UserVerificationRequirementFromString) {
  EXPECT_EQ(UserVerificationRequirementFromString("required"),
            UserVerificationRequirement::kRequired);
  EXPECT_EQ(UserVerificationRequirementFromString("preferred"),
            UserVerificationRequirement::kPreferred);
  EXPECT_EQ(UserVerificationRequirementFromString("discouraged"),
            UserVerificationRequirement::kDiscouraged);
}

// Tests that invalid user verification requirement strings fall back to
// `kPreferred`.
TEST(GpmUserVerificationPolicyTest,
     UserVerificationRequirementFromInvalidString) {
  EXPECT_EQ(UserVerificationRequirementFromString(""),
            UserVerificationRequirement::kPreferred);
  EXPECT_EQ(UserVerificationRequirementFromString("invalid"),
            UserVerificationRequirement::kPreferred);
  EXPECT_EQ(UserVerificationRequirementFromString("Required"),
            UserVerificationRequirement::kPreferred);
  EXPECT_EQ(UserVerificationRequirementFromString("discouraged "),
            UserVerificationRequirement::kPreferred);
}

}  // namespace

}  // namespace webauthn
