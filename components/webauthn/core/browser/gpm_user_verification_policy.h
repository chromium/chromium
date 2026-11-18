// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_GPM_USER_VERIFICATION_POLICY_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_GPM_USER_VERIFICATION_POLICY_H_

#include <string_view>

namespace device {
enum class UserVerificationRequirement;
}

namespace webauthn {

// Returns whether Google Password Manager policy is to do user verification for
// the given requirement.
bool GpmWillDoUserVerification(device::UserVerificationRequirement requirement,
                               bool platform_has_biometrics);

// Converts `user_verification` to a `device::UserVerificationRequirement`.
// Falls back to `kPreferred`, as per the WebAuthn spec, if `user_verification`
// isn't a valid user verification requirement string.
device::UserVerificationRequirement UserVerificationRequirementFromString(
    std::string_view user_verification);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_GPM_USER_VERIFICATION_POLICY_H_
