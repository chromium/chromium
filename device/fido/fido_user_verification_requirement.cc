// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_user_verification_requirement.h"

namespace device {

const char kUserVerificationRequired[] = "required";
const char kUserVerificationPreferred[] = "preferred";
const char kUserVerificationDiscouraged[] = "discouraged";

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
