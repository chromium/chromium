// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_SELECTION_CRITERIA_H_
#define DEVICE_FIDO_AUTHENTICATOR_SELECTION_CRITERIA_H_

#include "base/component_export.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_types.h"

namespace device {

// Represents authenticator properties the relying party can specify to restrict
// the type of authenticator used in creating credentials.
//
// https://w3c.github.io/webauthn/#authenticatorSelection
class COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorSelectionCriteria {
 public:
  AuthenticatorSelectionCriteria();
  AuthenticatorSelectionCriteria(
      AuthenticatorAttachment authenticator_attachment,
      ResidentKeyRequirement resident_key,
      UserVerificationRequirement user_verification_requirement);
  AuthenticatorSelectionCriteria(const AuthenticatorSelectionCriteria& other);
  AuthenticatorSelectionCriteria(AuthenticatorSelectionCriteria&& other);
  AuthenticatorSelectionCriteria& operator=(
      const AuthenticatorSelectionCriteria& other);
  AuthenticatorSelectionCriteria& operator=(
      AuthenticatorSelectionCriteria&& other);
  bool operator==(const AuthenticatorSelectionCriteria& other) const;
  ~AuthenticatorSelectionCriteria();

  AuthenticatorAttachment authenticator_attachment =
      AuthenticatorAttachment::kAny;
  ResidentKeyRequirement resident_key = ResidentKeyRequirement::kDiscouraged;
  UserVerificationRequirement user_verification_requirement =
      UserVerificationRequirement::kPreferred;
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_SELECTION_CRITERIA_H_
