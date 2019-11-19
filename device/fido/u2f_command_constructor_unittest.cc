// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_command_constructor.h"

#include <utility>

#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

CtapMakeCredentialRequest ConstructMakeCredentialRequest() {
  PublicKeyCredentialRpEntity rp("acme.com");
  rp.name = "acme.com";

  PublicKeyCredentialUserEntity user(
      fido_parsing_utils::Materialize(test_data::kUserId));
  user.name = "johnpsmith@example.com";
  user.display_name = "John P. Smith";
  user.icon_url = GURL("https://pics.acme.com/00/p/aBjjjpqPb.png");

  return CtapMakeCredentialRequest(
      test_data::kClientDataJson, std::move(rp), std::move(user),
      PublicKeyCredentialParams(PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>(1))));
}

CtapGetAssertionRequest ConstructGetAssertionRequest() {
  return CtapGetAssertionRequest("acme.com", test_data::kClientDataJson);
}

}  // namespace

TEST(U2fCommandConstructorTest, TestCreateU2fRegisterCommand) {
  const auto register_command_without_individual_attestation =
      ConstructU2fRegisterCommand(test_data::kApplicationParameter,
                                  test_data::kChallengeParameter,
                                  false /* is_individual_attestation */);

  EXPECT_THAT(register_command_without_individual_attestation,
              ::testing::ElementsAreArray(test_data::kU2fRegisterCommandApdu));

  const auto register_command_with_individual_attestation =
      ConstructU2fRegisterCommand(test_data::kApplicationParameter,
                                  test_data::kChallengeParameter,
                                  true /* is_individual_attestation */);

  EXPECT_THAT(register_command_with_individual_attestation,
              ::testing::ElementsAreArray(
                  test_data::kU2fRegisterCommandApduWithIndividualAttestation));
}

TEST(U2fCommandConstructorTest, TestConvertCtapMakeCredentialToU2fRegister) {
  const auto make_credential_param = ConstructMakeCredentialRequest();

  EXPECT_TRUE(IsConvertibleToU2fRegisterCommand(make_credential_param));

  const auto u2f_register_command =
      ConvertToU2fRegisterCommand(make_credential_param);
  ASSERT_TRUE(u2f_register_command);
  EXPECT_THAT(*u2f_register_command,
              ::testing::ElementsAreArray(test_data::kU2fRegisterCommandApdu));
}

TEST(U2fCommandConstructorTest, TestU2fRegisterCredentialAlgorithmRequirement) {
  PublicKeyCredentialRpEntity rp("acme.com");
  rp.name = "acme.com";

  PublicKeyCredentialUserEntity user(
      fido_parsing_utils::Materialize(test_data::kUserId));
  user.name = "johnpsmith@example.com";
  user.display_name = "John P. Smith";
  user.icon_url = GURL("https://pics.acme.com/00/p/aBjjjpqPb.png");

  CtapMakeCredentialRequest make_credential_param(
      test_data::kClientDataJson, std::move(rp), std::move(user),
      PublicKeyCredentialParams({{CredentialType::kPublicKey, -257}}));

  EXPECT_FALSE(IsConvertibleToU2fRegisterCommand(make_credential_param));
}

TEST(U2fCommandConstructorTest, TestU2fRegisterUserVerificationRequirement) {
  auto make_credential_param = ConstructMakeCredentialRequest();
  make_credential_param.user_verification =
      UserVerificationRequirement::kRequired;

  EXPECT_FALSE(IsConvertibleToU2fRegisterCommand(make_credential_param));
}

TEST(U2fCommandConstructorTest, TestU2fRegisterResidentKeyRequirement) {
  auto make_credential_param = ConstructMakeCredentialRequest();
  make_credential_param.resident_key_required = true;

  EXPECT_FALSE(IsConvertibleToU2fRegisterCommand(make_credential_param));
}

TEST(U2fCommandConstructorTest, TestCreateSignApduCommand) {
  const auto& encoded_sign = ConstructU2fSignCommand(
      test_data::kApplicationParameter, test_data::kChallengeParameter,
      test_data::kU2fSignKeyHandle);
  ASSERT_TRUE(encoded_sign);
  EXPECT_THAT(*encoded_sign,
              ::testing::ElementsAreArray(test_data::kU2fSignCommandApdu));
}

TEST(U2fCommandConstructorTest, TestConvertCtapGetAssertionToU2fSignRequest) {
  auto get_assertion_req = ConstructGetAssertionRequest();
  std::vector<PublicKeyCredentialDescriptor> allowed_list;
  allowed_list.push_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)));
  get_assertion_req.allow_list = std::move(allowed_list);

  const auto u2f_sign_command = ConvertToU2fSignCommand(
      get_assertion_req, ApplicationParameterType::kPrimary,
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle));

  EXPECT_TRUE(IsConvertibleToU2fSignCommand(get_assertion_req));
  ASSERT_TRUE(u2f_sign_command);
  EXPECT_THAT(*u2f_sign_command,
              ::testing::ElementsAreArray(test_data::kU2fSignCommandApdu));
}

TEST(U2fCommandConstructorTest, TestU2fSignAllowListRequirement) {
  auto get_assertion_req = ConstructGetAssertionRequest();
  EXPECT_FALSE(IsConvertibleToU2fSignCommand(get_assertion_req));
}

TEST(U2fCommandConstructorTest, TestU2fSignUserVerificationRequirement) {
  auto get_assertion_req = ConstructGetAssertionRequest();
  std::vector<PublicKeyCredentialDescriptor> allowed_list;
  allowed_list.push_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)));
  get_assertion_req.allow_list = std::move(allowed_list);
  get_assertion_req.user_verification = UserVerificationRequirement::kRequired;

  EXPECT_FALSE(IsConvertibleToU2fSignCommand(get_assertion_req));
}

TEST(U2fCommandConstructorTest, TestCreateSignWithIncorrectKeyHandle) {
  std::array<uint8_t, kU2fApplicationParamLength> application_parameter = {
      0x01};
  std::array<uint8_t, kU2fChallengeParamLength> challenge_parameter = {0x02};
  std::vector<uint8_t> key_handle(kMaxKeyHandleLength, 0xff);

  const auto valid_sign_command = ConstructU2fSignCommand(
      application_parameter, challenge_parameter, key_handle);
  ASSERT_TRUE(valid_sign_command);

  key_handle.push_back(0xff);
  const auto invalid_sign_command = ConstructU2fSignCommand(
      application_parameter, challenge_parameter, key_handle);
  ASSERT_FALSE(invalid_sign_command);
}

}  // namespace device
