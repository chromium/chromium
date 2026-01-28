// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARAMS_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARAMS_H_

#import <set>
#import <vector>

#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "components/webauthn/ios/ios_passkey_client.h"
#import "device/fido/public/public_key_credential_descriptor.h"
#import "device/fido/public/public_key_credential_rp_entity.h"
#import "device/fido/public/public_key_credential_user_entity.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace webauthn {

struct PasskeyExtensionData {
  PasskeyExtensionData();
  PasskeyExtensionData(const PasskeyExtensionData& other);
  PasskeyExtensionData(PasskeyExtensionData&& other);
  ~PasskeyExtensionData();

  // Main PRF input data.
  std::optional<passkey_model_utils::PRFInputData> prf_eval;
  // Per credential input data.
  // The map's keys (std::vector<uint8_t>) are credential IDs.
  absl::flat_hash_map<std::vector<uint8_t>,
                      std::optional<passkey_model_utils::PRFInputData>>
      prf_eval_by_credential;
};

// Utility class containing common parameters used by passkey assertion and
// registration requests.
class PasskeyRequestParams {
 public:
  // LINT.IfChange
  // Whether the request in modal, conditional get or conditional create.
  enum class RequestType {
    // Unknown (due to bad request).
    kUnknown = 0,
    // Modal (non conditional) request.
    kModal = 1,
    // Conditional assertion request.
    kConditionalGet = 2,
    // Conditional registration request.
    kConditionalCreate = 3,
  };
  // LINT.ThenChange(//components/webauthn/ios/resources/passkey_controller.ts)

  PasskeyRequestParams(IOSPasskeyClient::RequestInfo request_info,
                       device::PublicKeyCredentialRpEntity rp_entity,
                       std::vector<uint8_t> challenge,
                       device::UserVerificationRequirement user_verification,
                       RequestType request_type,
                       PasskeyExtensionData extension_data);
  PasskeyRequestParams(PasskeyRequestParams&& other);
  ~PasskeyRequestParams();

  // Returns the request information.
  const IOSPasskeyClient::RequestInfo& RequestInfo() const;

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

  // Returns the request type (modal / conditional).
  PasskeyRequestParams::RequestType Type() const;

  // Returns the extensions input data for passkey creation.
  passkey_model_utils::ExtensionInputData ExtensionInputForCreation() const;

  // Returns the extensions input data for the selected passkey's credential ID
  // for passkey assertion.
  passkey_model_utils::ExtensionInputData ExtensionInputForCredential(
      std::vector<uint8_t> credential_id) const;

 private:
  // The request information (frame id, request id).
  const IOSPasskeyClient::RequestInfo request_info_;
  // The relying party entity, including name and ID.
  const device::PublicKeyCredentialRpEntity rp_entity_;
  // The passkey request's cryptographic challenge.
  const std::vector<uint8_t> challenge_;
  // The passkey request's user verification preference.
  const device::UserVerificationRequirement user_verification_;
  // Type of request (modal / conditional).
  const RequestType request_type_ = RequestType::kUnknown;
  // Extension data, including the PRF extension.
  const PasskeyExtensionData extension_data_;
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
  std::set<std::vector<uint8_t>> GetAllowCredentialIds() const;

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
  std::set<std::vector<uint8_t>> GetExcludeCredentialIds() const;

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
