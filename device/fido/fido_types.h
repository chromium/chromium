// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_TYPES_H_
#define DEVICE_FIDO_FIDO_TYPES_H_

// The definitions below are for mojo-mappable types that need to be
// transferred from Blink. Types that have mojo equivalents are better placed
// in fido_constants.h.

namespace device {

enum class ProtocolVersion {
  kCtap2,
  kU2f,
  kUnknown,
};

enum class CredentialType { kPublicKey };

// Authenticator attachment constraint passed on from the relying party as a
// parameter for AuthenticatorSelectionCriteria. |kAny| is equivalent to the
// (optional) attachment field not being present.
// https://w3c.github.io/webauthn/#attachment
enum class AuthenticatorAttachment {
  kAny,
  kPlatform,
  kCrossPlatform,
};

// User verification constraint passed on from the relying party as a parameter
// for AuthenticatorSelectionCriteria and for CtapGetAssertion request.
// https://w3c.github.io/webauthn/#enumdef-userverificationrequirement
enum class UserVerificationRequirement {
  kRequired,
  kPreferred,
  kDiscouraged,
};

// https://w3c.github.io/webauthn/#attestation-convey
enum class AttestationConveyancePreference : uint8_t {
  kNone,
  kIndirect,
  kDirect,
  // Non-standard value for individual attestation that we hope to end up in
  // the standard eventually.
  kEnterprise,
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_TYPES_H_
