// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_request_params.h"

#import "components/webauthn/core/browser/gpm_user_verification_policy.h"

namespace webauthn {

namespace {

// Returns a set of credential ids from a vector of credential descriptors.
std::set<std::string> GetIdsFromDescriptors(
    const std::vector<device::PublicKeyCredentialDescriptor>& descriptors) {
  std::set<std::string> descriptor_ids;
  std::transform(descriptors.begin(), descriptors.end(),
                 std::inserter(descriptor_ids, descriptor_ids.begin()),
                 [](const device::PublicKeyCredentialDescriptor& desc) {
                   return std::string(desc.id.begin(), desc.id.end());
                 });
  return descriptor_ids;
}

}  // namespace

PasskeyRequestParams::PasskeyRequestParams()
    : user_verification_(device::UserVerificationRequirement::kPreferred) {}

PasskeyRequestParams::PasskeyRequestParams(
    const std::string& frame_id,
    const std::string& request_id,
    device::PublicKeyCredentialRpEntity rp_entity,
    std::vector<uint8_t> challenge,
    device::UserVerificationRequirement user_verification)
    : frame_id_(frame_id),
      request_id_(request_id),
      rp_entity_(std::move(rp_entity)),
      challenge_(std::move(challenge)),
      user_verification_(user_verification) {}

PasskeyRequestParams::PasskeyRequestParams(PasskeyRequestParams&& other) =
    default;

PasskeyRequestParams::~PasskeyRequestParams() = default;

const std::string& PasskeyRequestParams::FrameId() const {
  return frame_id_;
}

const std::string& PasskeyRequestParams::RequestId() const {
  return request_id_;
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

AssertionRequestParams::AssertionRequestParams(
    PasskeyRequestParams request_params,
    std::vector<device::PublicKeyCredentialDescriptor> allow_credentials)
    : PasskeyRequestParams(std::move(request_params)),
      allow_credentials_(std::move(allow_credentials)) {}

AssertionRequestParams::AssertionRequestParams(AssertionRequestParams&& other) =
    default;

std::set<std::string> AssertionRequestParams::GetAllowCredentialIds() const {
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

std::set<std::string> RegistrationRequestParams::GetExcludeCredentialIds()
    const {
  return GetIdsFromDescriptors(exclude_credentials_);
}

PasskeyModel::UserEntity RegistrationRequestParams::UserEntity() const {
  return PasskeyModel::UserEntity(user_entity_.id,
                                  user_entity_.name.value_or(""),
                                  user_entity_.display_name.value_or(""));
}

RegistrationRequestParams::~RegistrationRequestParams() = default;

}  // namespace webauthn
