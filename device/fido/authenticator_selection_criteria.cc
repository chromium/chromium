// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_selection_criteria.h"

namespace device {

AuthenticatorSelectionCriteria::AuthenticatorSelectionCriteria() = default;

AuthenticatorSelectionCriteria::AuthenticatorSelectionCriteria(
    AuthenticatorAttachment authenticator_attachment,
    bool require_resident_key,
    UserVerificationRequirement user_verification_requirement)
    : authenticator_attachment_(authenticator_attachment),
      require_resident_key_(require_resident_key),
      user_verification_requirement_(user_verification_requirement) {}

AuthenticatorSelectionCriteria::AuthenticatorSelectionCriteria(
    AuthenticatorSelectionCriteria&& other) = default;

AuthenticatorSelectionCriteria::AuthenticatorSelectionCriteria(
    const AuthenticatorSelectionCriteria& other) = default;

AuthenticatorSelectionCriteria& AuthenticatorSelectionCriteria::operator=(
    AuthenticatorSelectionCriteria&& other) = default;

AuthenticatorSelectionCriteria& AuthenticatorSelectionCriteria::operator=(
    const AuthenticatorSelectionCriteria& other) = default;

bool AuthenticatorSelectionCriteria::operator==(
    const AuthenticatorSelectionCriteria& other) const {
  return authenticator_attachment_ == other.authenticator_attachment_ &&
         require_resident_key_ == other.require_resident_key_ &&
         user_verification_requirement_ == other.user_verification_requirement_;
}

AuthenticatorSelectionCriteria::~AuthenticatorSelectionCriteria() = default;

}  // namespace device
