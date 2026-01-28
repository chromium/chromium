// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_request_params.h"

#import "components/webauthn/core/browser/gpm_user_verification_policy.h"

namespace webauthn {

namespace {

// Returns a set of credential ids from a vector of credential descriptors.
std::set<std::vector<uint8_t>> GetIdsFromDescriptors(
    const std::vector<device::PublicKeyCredentialDescriptor>& descriptors) {
  std::set<std::vector<uint8_t>> descriptor_ids;
  std::transform(descriptors.begin(), descriptors.end(),
                 std::inserter(descriptor_ids, descriptor_ids.begin()),
                 [](const device::PublicKeyCredentialDescriptor& desc) {
                   return desc.id;
                 });
  return descriptor_ids;
}

// Builds a new ExtensionInputData.
// Uses the PRFInputData if prf_eval is not std::nullopt.
passkey_model_utils::ExtensionInputData BuildExtensionInputData(
    const std::optional<passkey_model_utils::PRFInputData>& prf_eval) {
  return prf_eval.has_value()
             ? passkey_model_utils::ExtensionInputData(*prf_eval)
             : passkey_model_utils::ExtensionInputData();
}

}  // namespace

PasskeyExtensionData::PasskeyExtensionData() = default;
PasskeyExtensionData::PasskeyExtensionData(const PasskeyExtensionData& other) =
    default;
PasskeyExtensionData::PasskeyExtensionData(PasskeyExtensionData&& other) =
    default;
PasskeyExtensionData::~PasskeyExtensionData() = default;

PasskeyRequestParams::PasskeyRequestParams(
    IOSPasskeyClient::RequestInfo request_info,
    device::PublicKeyCredentialRpEntity rp_entity,
    std::vector<uint8_t> challenge,
    device::UserVerificationRequirement user_verification,
    enum RequestType request_type,
    PasskeyExtensionData extension_data)
    : request_info_(std::move(request_info)),
      rp_entity_(std::move(rp_entity)),
      challenge_(std::move(challenge)),
      user_verification_(user_verification),
      request_type_(request_type),
      extension_data_(std::move(extension_data)) {}

PasskeyRequestParams::PasskeyRequestParams(PasskeyRequestParams&& other) =
    default;

PasskeyRequestParams::~PasskeyRequestParams() = default;

const IOSPasskeyClient::RequestInfo& PasskeyRequestParams::RequestInfo() const {
  return request_info_;
}

const std::string& PasskeyRequestParams::FrameId() const {
  return request_info_.frame_id;
}

const std::string& PasskeyRequestParams::RequestId() const {
  return request_info_.request_id;
}

const std::vector<uint8_t> PasskeyRequestParams::Challenge() const {
  return challenge_;
}

const std::string& PasskeyRequestParams::RpId() const {
  return rp_entity_.id;
}

bool PasskeyRequestParams::ShouldPerformUserVerification(
    bool is_biometric_authentication_enabled) const {
  return GpmWillDoUserVerification(user_verification_,
                                   is_biometric_authentication_enabled);
}

PasskeyRequestParams::RequestType PasskeyRequestParams::Type() const {
  return request_type_;
}

passkey_model_utils::ExtensionInputData
PasskeyRequestParams::ExtensionInputForCreation() const {
  return BuildExtensionInputData(extension_data_.prf_eval);
}

passkey_model_utils::ExtensionInputData
PasskeyRequestParams::ExtensionInputForCredential(
    std::vector<uint8_t> credential_id) const {
  auto it = extension_data_.prf_eval_by_credential.find(credential_id);
  auto itEnd = extension_data_.prf_eval_by_credential.end();
  return BuildExtensionInputData((it != itEnd) ? it->second
                                               : extension_data_.prf_eval);
}

AssertionRequestParams::AssertionRequestParams(
    PasskeyRequestParams request_params,
    std::vector<device::PublicKeyCredentialDescriptor> allow_credentials)
    : PasskeyRequestParams(std::move(request_params)),
      allow_credentials_(std::move(allow_credentials)) {}

AssertionRequestParams::AssertionRequestParams(AssertionRequestParams&& other) =
    default;

std::set<std::vector<uint8_t>> AssertionRequestParams::GetAllowCredentialIds()
    const {
  return GetIdsFromDescriptors(allow_credentials_);
}

AssertionRequestParams::~AssertionRequestParams() = default;

RegistrationRequestParams::RegistrationRequestParams(
    PasskeyRequestParams request_params,
    device::PublicKeyCredentialUserEntity user_entity,
    std::vector<device::PublicKeyCredentialDescriptor> exclude_credentials)
    : PasskeyRequestParams(std::move(request_params)),
      user_entity_(std::move(user_entity)),
      exclude_credentials_(std::move(exclude_credentials)) {}

RegistrationRequestParams::RegistrationRequestParams(
    RegistrationRequestParams&& other) = default;

std::set<std::vector<uint8_t>>
RegistrationRequestParams::GetExcludeCredentialIds() const {
  return GetIdsFromDescriptors(exclude_credentials_);
}

PasskeyModel::UserEntity RegistrationRequestParams::UserEntity() const {
  return PasskeyModel::UserEntity(user_entity_.id,
                                  user_entity_.name.value_or(""),
                                  user_entity_.display_name.value_or(""));
}

RegistrationRequestParams::~RegistrationRequestParams() = default;

}  // namespace webauthn
