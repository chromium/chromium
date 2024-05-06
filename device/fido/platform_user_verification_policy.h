// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PLATFORM_USER_VERIFICATION_POLICY_H_
#define DEVICE_FIDO_PLATFORM_USER_VERIFICATION_POLICY_H_

#include "base/component_export.h"
#include "device/fido/fido_types.h"

namespace device::fido {

// Returns whether the platform norm is to do user verification for the given
// requirement. The platform norm is taken from the native platform
// authenticator, e.g. Windows Hello on Windows or iCloud Keychain on macOS.
COMPONENT_EXPORT(DEVICE_FIDO)
bool PlatformWillDoUserVerification(UserVerificationRequirement requirement);

}  // namespace device::fido

#endif  // DEVICE_FIDO_PLATFORM_USER_VERIFICATION_POLICY_H_
