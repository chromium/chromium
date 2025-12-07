// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_GPM_USER_VERIFICATION_POLICY_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_GPM_USER_VERIFICATION_POLICY_H_

namespace device {
enum class UserVerificationRequirement;
}

namespace webauthn {

// Returns whether Google Password Manager policy is to do user verification for
// the given requirement.
bool GpmWillDoUserVerification(device::UserVerificationRequirement requirement,
                               bool platform_has_biometrics);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_GPM_USER_VERIFICATION_POLICY_H_
