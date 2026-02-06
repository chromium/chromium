// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_test_util.h"

#import "base/rand_util.h"
#import "components/webauthn/ios/passkey_request_params.h"

namespace webauthn {

std::vector<uint8_t> AsByteVector(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

sync_pb::WebauthnCredentialSpecifics GetTestPasskey(
    const std::string& credential_id) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_rp_id(kRpId);
  passkey.set_credential_id(credential_id);
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_user_id(base::RandBytesAsString(16));
  passkey.set_user_name(base::RandBytesAsString(16));
  passkey.set_user_display_name(base::RandBytesAsString(16));
  return passkey;
}

PasskeyRequestParams BuildPasskeyRequestParams(
    device::UserVerificationRequirement user_verification,
    std::string request_id,
    std::string frame_id) {
  IOSPasskeyClient::RequestInfo request_info(frame_id, request_id);
  device::PublicKeyCredentialRpEntity rp_entity(kRpId);
  std::vector<uint8_t> challenge;
  PasskeyRequestParams::RequestType request_type =
      PasskeyRequestParams::RequestType::kModal;
  PasskeyExtensionData extension_data;
  return PasskeyRequestParams(std::move(request_info), std::move(rp_entity),
                              std::move(challenge), user_verification,
                              request_type, std::move(extension_data));
}

RegistrationRequestParams BuildRegistrationRequestParams(
    const std::vector<device::PublicKeyCredentialDescriptor>&
        exclude_credentials,
    device::UserVerificationRequirement user_verification,
    std::string request_id,
    std::string frame_id) {
  device::PublicKeyCredentialUserEntity user_entity;
  return RegistrationRequestParams(
      BuildPasskeyRequestParams(user_verification, request_id, frame_id),
      std::move(user_entity), exclude_credentials);
}

AssertionRequestParams BuildAssertionRequestParams(
    const std::vector<device::PublicKeyCredentialDescriptor>& allow_credentials,
    device::UserVerificationRequirement user_verification,
    std::string request_id,
    std::string frame_id) {
  return AssertionRequestParams(
      BuildPasskeyRequestParams(user_verification, request_id, frame_id),
      allow_credentials);
}

}  // namespace webauthn
