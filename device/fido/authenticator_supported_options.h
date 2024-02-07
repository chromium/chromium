// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_
#define DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_

#include <optional>

#include "base/component_export.h"
#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"

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

  enum class PlatformDevice {
    kNo,
    kYes,
    // kBoth authenticators may forward requests to both types of device.
    kBoth,
  };

  AuthenticatorSupportedOptions();
  AuthenticatorSupportedOptions(const AuthenticatorSupportedOptions& other);
  AuthenticatorSupportedOptions& operator=(
      const AuthenticatorSupportedOptions& other);
  ~AuthenticatorSupportedOptions();

  // Indicates that the authenticator is attached to the client and therefore
  // can't be removed and used on another client. If `kBoth` then the
  // authenticator handles both types of requests (e.g. Windows Hello).
  PlatformDevice is_platform_device = PlatformDevice::kNo;
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
  // default_cred_protect specifies the default credProtect level applied by
  // this authenticator.
  CredProtect default_cred_protect = CredProtect::kUVOptional;
  // Represents whether client pin is set and stored in authenticator. Set as
  // null optional if client pin capability is not supported by the
  // authenticator.
  ClientPinAvailability client_pin_availability =
      ClientPinAvailability::kNotSupported;
  // Indicates whether the authenticator supports CTAP 2.1 pinUvAuthToken for
  // establishing user verification via client PIN or a built-in sensor.
  bool supports_pin_uv_auth_token = false;
  // True iff enterprise attestation is supported and enabled. (In CTAP2 this is
  // a tri-state, but the state that represents "administratively disabled" is
  // uninteresting to Chromium because we do not support the administrative
  // operation to configure it. Thus this member reduces to a boolean.)
  bool enterprise_attestation = false;
  // Whether the authenticator supports large blobs, and, if so, the method of
  // that support.
  std::optional<LargeBlobSupportType> large_blob_type;
  // Indicates whether user verification must be used for make credential, final
  // (i.e. not pre-flight) get assertion requests, and writing large blobs. An
  // |always_uv| value of true will make uv=0 get assertion requests return
  // invalid signatures, which is okay for pre-flighting.
  bool always_uv = false;
  // If true, indicates that the authenticator permits creation of non-resident
  // credentials without UV.
  bool make_cred_uv_not_required = false;
  // If true, indicates that the authenticator supports the minPinLength
  // extension.
  bool supports_min_pin_length_extension = false;
  // If true, indicates that the authenticator supports the hmac_secret
  // extension.
  bool supports_hmac_secret = false;
  // If true, indicates that the authenticator supports the PRF extension. This
  // will be preferred to the hmac-secret extension if supported.
  bool supports_prf = false;
  // max_cred_blob_length is the longest credBlob value that this authenticator
  // can store. A value of `nullopt` indicates no support for credBlob.
  std::optional<uint16_t> max_cred_blob_length;
};

COMPONENT_EXPORT(DEVICE_FIDO)
cbor::Value AsCBOR(const AuthenticatorSupportedOptions& options);

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_
