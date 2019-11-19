// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_
#define DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/cbor/values.h"

namespace device {

// Represents CTAP device properties and capabilities received as a response to
// AuthenticatorGetInfo command.
struct COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorSupportedOptions {
 public:
  enum class UserVerificationAvailability {
    // e.g. Authenticator with finger print sensor and user's fingerprint is
    // registered to the device.
    kSupportedAndConfigured,
    // e.g. Authenticator with fingerprint sensor without user's fingerprint
    // registered.
    kSupportedButNotConfigured,
    kNotSupported
  };

  enum class ClientPinAvailability {
    kSupportedAndPinSet,
    kSupportedButPinNotSet,
    kNotSupported,
  };

  enum class BioEnrollmentAvailability {
    kSupportedAndProvisioned,
    kSupportedButUnprovisioned,
    kNotSupported
  };

  AuthenticatorSupportedOptions();
  AuthenticatorSupportedOptions(const AuthenticatorSupportedOptions& other);
  AuthenticatorSupportedOptions& operator=(
      const AuthenticatorSupportedOptions& other);
  ~AuthenticatorSupportedOptions();

  // Indicates that the authenticator is attached to the client and therefore
  // can't be removed and used on another client.
  bool is_platform_device = false;
  // Indicates that the authenticator is capable of storing keys on the
  // authenticator itself
  // and therefore can satisfy the authenticatorGetAssertion request with
  // allowList parameter not specified or empty.
  bool supports_resident_key = false;
  // Indicates whether the authenticator is capable of verifying the user on its
  // own.
  UserVerificationAvailability user_verification_availability =
      UserVerificationAvailability::kNotSupported;
  // supports_user_presence indicates whether the authenticator can assert user
  // presence. E.g. a touch for a USB device, or being placed in the reader
  // field for an NFC device.
  bool supports_user_presence = true;
  // Indicates whether the authenticator supports the CTAP2
  // authenticatorCredentialManagement command.
  bool supports_credential_management = false;
  // Indicates whether the authenticator supports the vendor-specific preview of
  // the CTAP2 authenticatorCredentialManagement command.
  bool supports_credential_management_preview = false;
  // Indicates whether the authenticator supports the CTAP 2.1
  // authenticatorBioEnrollment command.
  BioEnrollmentAvailability bio_enrollment_availability =
      BioEnrollmentAvailability::kNotSupported;
  // Indicates whether the authenticator supports the CTAP 2.1 vendor-specific
  // authenticatorBioEnrollment command.
  BioEnrollmentAvailability bio_enrollment_availability_preview =
      BioEnrollmentAvailability::kNotSupported;
  // supports_cred_protect is true if the authenticator supports the
  // `credProtect` extension. See CTAP2 draft for details.
  bool supports_cred_protect = false;
  // Represents whether client pin is set and stored in authenticator. Set as
  // null optional if client pin capability is not supported by the
  // authenticator.
  ClientPinAvailability client_pin_availability =
      ClientPinAvailability::kNotSupported;
};

COMPONENT_EXPORT(DEVICE_FIDO)
cbor::Value AsCBOR(const AuthenticatorSupportedOptions& options);

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_
