// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_user_verification_requirement.h"

namespace device {

const char kUserVerificationRequired[] = "required";
const char kUserVerificationPreferred[] = "preferred";
const char kUserVerificationDiscouraged[] = "discouraged";

std::optional<UserVerificationRequirement> ConvertToUserVerificationRequirement(
    std::string_view user_verification_requirement) {
  if (user_verification_requirement == kUserVerificationRequired) {
    return UserVerificationRequirement::kRequired;
  } else if (user_verification_requirement == kUserVerificationPreferred) {
    return UserVerificationRequirement::kPreferred;
  } else if (user_verification_requirement == kUserVerificationDiscouraged) {
    return UserVerificationRequirement::kDiscouraged;
  } else {
    return std::nullopt;
  }
}

std::string_view ToString(
    UserVerificationRequirement user_verification_requirement) {
  switch (user_verification_requirement) {
    case UserVerificationRequirement::kRequired:
      return kUserVerificationRequired;
    case UserVerificationRequirement::kPreferred:
      return kUserVerificationPreferred;
    case UserVerificationRequirement::kDiscouraged:
      return kUserVerificationDiscouraged;
  }
}

}  // namespace device
