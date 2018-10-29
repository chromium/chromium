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
class COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorSupportedOptions {
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

  AuthenticatorSupportedOptions();
  AuthenticatorSupportedOptions(AuthenticatorSupportedOptions&& other);
  AuthenticatorSupportedOptions& operator=(
      AuthenticatorSupportedOptions&& other);
  ~AuthenticatorSupportedOptions();

  AuthenticatorSupportedOptions& SetIsPlatformDevice(bool is_platform_device);
  AuthenticatorSupportedOptions& SetSupportsResidentKey(
      bool supports_resident_key);
  AuthenticatorSupportedOptions& SetUserVerificationAvailability(
      UserVerificationAvailability user_verification_required);
  AuthenticatorSupportedOptions& SetUserPresenceRequired(
      bool user_presence_required);
  AuthenticatorSupportedOptions& SetClientPinAvailability(
      ClientPinAvailability client_pin_availability);

  bool is_platform_device() const { return is_platform_device_; }
  bool supports_resident_key() const { return supports_resident_key_; }
  UserVerificationAvailability user_verification_availability() const {
    return user_verification_availability_;
  }
  bool user_presence_required() const { return user_presence_required_; }
  ClientPinAvailability client_pin_availability() const {
    return client_pin_availability_;
  }

 private:
  // Indicates that the device is attached to the client and therefore can't be
  // removed and used on another client.
  bool is_platform_device_ = false;
  // Indicates that the device is capable of storing keys on the device itself
  // and therefore can satisfy the authenticatorGetAssertion request with
  // allowList parameter not specified or empty.
  bool supports_resident_key_ = false;
  // Indicates whether the device is capable of verifying the user on its own.
  UserVerificationAvailability user_verification_availability_ =
      UserVerificationAvailability::kNotSupported;
  bool user_presence_required_ = true;
  // Represents whether client pin in set and stored in device. Set as null
  // optional if client pin capability is not supported by the authenticator.
  ClientPinAvailability client_pin_availability_ =
      ClientPinAvailability::kNotSupported;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorSupportedOptions);
};

COMPONENT_EXPORT(DEVICE_FIDO)
cbor::Value ConvertToCBOR(const AuthenticatorSupportedOptions& options);

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_
