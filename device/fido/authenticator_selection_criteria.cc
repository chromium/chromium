// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_selection_criteria.h"

namespace device {

AuthenticatorSelectionCriteria::AuthenticatorSelectionCriteria() = default;

AuthenticatorSelectionCriteria::AuthenticatorSelectionCriteria(
    AuthenticatorAttachment authenticator_attachment,
    ResidentKeyRequirement resident_key,
    UserVerificationRequirement user_verification_requirement)
    : authenticator_attachment(authenticator_attachment),
      resident_key(resident_key),
      user_verification_requirement(user_verification_requirement) {}

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
  return authenticator_attachment == other.authenticator_attachment &&
         resident_key == other.resident_key &&
         user_verification_requirement == other.user_verification_requirement;
}

AuthenticatorSelectionCriteria::~AuthenticatorSelectionCriteria() = default;

}  // namespace device
