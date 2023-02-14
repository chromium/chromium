// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_supported_options.h"

#include <utility>

#include "device/fido/fido_constants.h"

namespace device {

AuthenticatorSupportedOptions::AuthenticatorSupportedOptions() = default;
AuthenticatorSupportedOptions::AuthenticatorSupportedOptions(
    const AuthenticatorSupportedOptions& other) = default;
AuthenticatorSupportedOptions& AuthenticatorSupportedOptions::operator=(
    const AuthenticatorSupportedOptions& other) = default;
AuthenticatorSupportedOptions::~AuthenticatorSupportedOptions() = default;

cbor::Value AsCBOR(const AuthenticatorSupportedOptions& options) {
  cbor::Value::MapValue option_map;
  option_map.emplace(kResidentKeyMapKey, options.supports_resident_key);
  option_map.emplace(kUserPresenceMapKey, options.supports_user_presence);
  option_map.emplace(kPlatformDeviceMapKey,
                     options.is_platform_device ==
                         AuthenticatorSupportedOptions::PlatformDevice::kYes);

  using UvAvailability =
      AuthenticatorSupportedOptions::UserVerificationAvailability;

  switch (options.user_verification_availability) {
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

  switch (options.client_pin_availability) {
    case ClientPinAvailability::kSupportedAndPinSet:
      option_map.emplace(kClientPinMapKey, true);
      break;
    case ClientPinAvailability::kSupportedButPinNotSet:
      option_map.emplace(kClientPinMapKey, false);
      break;
    case ClientPinAvailability::kNotSupported:
      break;
  }

  if (options.supports_credential_management) {
    option_map.emplace(kCredentialManagementMapKey, true);
  }
  if (options.supports_credential_management_preview) {
    option_map.emplace(kCredentialManagementPreviewMapKey, true);
  }

  using BioEnrollmentAvailability =
      AuthenticatorSupportedOptions::BioEnrollmentAvailability;

  switch (options.bio_enrollment_availability) {
    case BioEnrollmentAvailability::kSupportedAndProvisioned:
      option_map.emplace(kBioEnrollmentMapKey, true);
      break;
    case BioEnrollmentAvailability::kSupportedButUnprovisioned:
      option_map.emplace(kBioEnrollmentMapKey, false);
      break;
    case BioEnrollmentAvailability::kNotSupported:
      break;
  }

  switch (options.bio_enrollment_availability_preview) {
    case BioEnrollmentAvailability::kSupportedAndProvisioned:
      option_map.emplace(kBioEnrollmentPreviewMapKey, true);
      break;
    case BioEnrollmentAvailability::kSupportedButUnprovisioned:
      option_map.emplace(kBioEnrollmentPreviewMapKey, false);
      break;
    case BioEnrollmentAvailability::kNotSupported:
      break;
  }

  if (options.supports_pin_uv_auth_token) {
    option_map.emplace(kPinUvTokenMapKey, true);
  }

  if (options.default_cred_protect != CredProtect::kUVOptional) {
    option_map.emplace(kDefaultCredProtectKey,
                       static_cast<int64_t>(options.default_cred_protect));
  }

  if (options.enterprise_attestation) {
    option_map.emplace(kEnterpriseAttestationKey, true);
  }

  if (options.large_blob_type == LargeBlobSupportType::kKey) {
    option_map.emplace(kLargeBlobsKey, true);
  }

  if (options.always_uv) {
    option_map.emplace(kAlwaysUvKey, true);
  }

  if (options.make_cred_uv_not_required) {
    option_map.emplace(kMakeCredUvNotRqdKey, true);
  }

  if (options.supports_min_pin_length_extension) {
    option_map.emplace(kExtensionMinPINLength, true);
  }

  return cbor::Value(std::move(option_map));
}

}  // namespace device
