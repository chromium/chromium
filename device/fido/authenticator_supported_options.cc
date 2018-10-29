// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_supported_options.h"

#include <utility>

#include "device/fido/fido_constants.h"

namespace device {

AuthenticatorSupportedOptions::AuthenticatorSupportedOptions() = default;

AuthenticatorSupportedOptions::AuthenticatorSupportedOptions(
    AuthenticatorSupportedOptions&& other) = default;

AuthenticatorSupportedOptions& AuthenticatorSupportedOptions::operator=(
    AuthenticatorSupportedOptions&& other) = default;

AuthenticatorSupportedOptions::~AuthenticatorSupportedOptions() = default;

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetSupportsResidentKey(
    bool supports_resident_key) {
  supports_resident_key_ = supports_resident_key;
  return *this;
}

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetUserVerificationAvailability(
    UserVerificationAvailability user_verification_availability) {
  user_verification_availability_ = user_verification_availability;
  return *this;
}

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetUserPresenceRequired(
    bool user_presence_required) {
  user_presence_required_ = user_presence_required;
  return *this;
}

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetClientPinAvailability(
    ClientPinAvailability client_pin_availability) {
  client_pin_availability_ = client_pin_availability;
  return *this;
}

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetIsPlatformDevice(bool is_platform_device) {
  is_platform_device_ = is_platform_device;
  return *this;
}

cbor::Value ConvertToCBOR(const AuthenticatorSupportedOptions& options) {
  cbor::Value::MapValue option_map;
  option_map.emplace(kResidentKeyMapKey, options.supports_resident_key());
  option_map.emplace(kUserPresenceMapKey, options.user_presence_required());
  option_map.emplace(kPlatformDeviceMapKey, options.is_platform_device());

  using UvAvailability =
      AuthenticatorSupportedOptions::UserVerificationAvailability;

  switch (options.user_verification_availability()) {
    case UvAvailability::kSupportedAndConfigured:
      option_map.emplace(kUserVerificationMapKey, true);
      break;
    case UvAvailability::kSupportedButNotConfigured:
      option_map.emplace(kUserVerificationMapKey, false);
      break;
    case UvAvailability::kNotSupported:
      break;
  }

  using ClientPinAvailability =
      AuthenticatorSupportedOptions::ClientPinAvailability;

  switch (options.client_pin_availability()) {
    case ClientPinAvailability::kSupportedAndPinSet:
      option_map.emplace(kClientPinMapKey, true);
      break;
    case ClientPinAvailability::kSupportedButPinNotSet:
      option_map.emplace(kClientPinMapKey, false);
      break;
    case ClientPinAvailability::kNotSupported:
      break;
  }

  return cbor::Value(std::move(option_map));
}

}  // namespace device
