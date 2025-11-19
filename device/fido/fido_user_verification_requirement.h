// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_USER_VERIFICATION_REQUIREMENT_H_
#define DEVICE_FIDO_FIDO_USER_VERIFICATION_REQUIREMENT_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "device/fido/fido_types.h"

namespace device {

COMPONENT_EXPORT(DEVICE_FIDO) extern const char kUserVerificationRequired[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kUserVerificationPreferred[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kUserVerificationDiscouraged[];

// Converts the input string to a UserVerificationRequirement.
// Returns std::nullopt if the input string doesn't match any valid
// user verification requirement as defined in:
// https://w3c.github.io/webauthn/#enumdef-userverificationrequirement
COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<UserVerificationRequirement> ConvertToUserVerificationRequirement(
    std::string_view user_verification_requirement);

COMPONENT_EXPORT(DEVICE_FIDO)
std::string_view ToString(
    UserVerificationRequirement user_verification_requirement);

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_USER_VERIFICATION_REQUIREMENT_H_
