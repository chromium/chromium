// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARAMS_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARAMS_H_

#import <set>
#import <vector>

#import "components/webauthn/core/browser/passkey_model.h"
#import "device/fido/public_key_credential_descriptor.h"
#import "device/fido/public_key_credential_rp_entity.h"
#import "device/fido/public_key_credential_user_entity.h"

namespace webauthn {

// Utility class containing common parameters used by passkey assertion and
// registration requests.
class PasskeyRequestParams {
 public:
  PasskeyRequestParams();
  PasskeyRequestParams(const std::string& frame_id,
                       const std::string& request_id,
                       device::PublicKeyCredentialRpEntity rp_entity,
                       std::vector<uint8_t> challenge,
                       device::UserVerificationRequirement user_verification);
  PasskeyRequestParams(PasskeyRequestParams&& other);
  ~PasskeyRequestParams();

  // Returns the web::WebFrame's identifier.
  const std::string& FrameId() const;

  // Returns the request id associated with a PublicKeyCredential promise.
  const std::string& RequestId() const;

  // Return the challenge for this passkey request.
  const std::vector<uint8_t> Challenge() const;

  // Returns the relying party identifier.
  const std::string& RpId() const;

  // Returns whether user verification should be performed.
  bool ShouldPerformUserVerification(
      bool is_biometric_authentication_enabled) const;

 private:
  // ID associated with the web::WebFrame the request originally came from.
  const std::string frame_id_;
  // The request ID associated with a PublicKeyCredential promise.
  const std::string request_id_;
  // The relying party entity, including name and ID.
  const device::PublicKeyCredentialRpEntity rp_entity_;
  // The passkey request's cryptographic challenge.
  const std::vector<uint8_t> challenge_;
  // The passkey request's user verification preference.
  const device::UserVerificationRequirement user_verification_;
};

// Utility class containing parameters used by passkey assertion requests.
class AssertionRequestParams : public PasskeyRequestParams {
 public:
  AssertionRequestParams(
      PasskeyRequestParams request_params,
      std::vector<device::PublicKeyCredentialDescriptor> allow_credentials);
  AssertionRequestParams(AssertionRequestParams&& other);
  ~AssertionRequestParams();

  // Returns the credential ids contained in `allow_credentials_`.
  std::set<std::string> GetAllowCredentialIds() const;

 private:
  // List of credentials allowed to fulfill the request. If empty, all
  // credentials are allowed to fulfill the request.
  const std::vector<device::PublicKeyCredentialDescriptor> allow_credentials_;
};

// Utility class containing parameters used by passkey registration requests.
class RegistrationRequestParams : public PasskeyRequestParams {
 public:
  RegistrationRequestParams(
      PasskeyRequestParams request_params,
      device::PublicKeyCredentialUserEntity user_entity,
      std::vector<device::PublicKeyCredentialDescriptor> exclude_credentials);
  RegistrationRequestParams(RegistrationRequestParams&& other);
  ~RegistrationRequestParams();

  // Returns the credential ids contained in `exclude_credentials_`.
  std::set<std::string> GetExcludeCredentialIds() const;

  // Converts `user_entity_` to PasskeyModel::UserEntity.
  PasskeyModel::UserEntity UserEntity() const;

 private:
  // The user entity, including name and ID.
  const device::PublicKeyCredentialUserEntity user_entity_;
  // List of credentials which prevent passkey creation. If a credential from
  // this list is already present in a credential provider for the same RP ID,
  // then another passkey for the same RP ID can not be added to the same
  // credential provider.
  const std::vector<device::PublicKeyCredentialDescriptor> exclude_credentials_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARAMS_H_
