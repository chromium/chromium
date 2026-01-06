// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_request_parser.h"

#import "device/fido/public/fido_constants.h"
#import "testing/platform_test.h"

using PasskeyRequestParserTest = PlatformTest;

namespace webauthn {

namespace {

// An empty string for testing purposes.
constexpr std::string kEmpty = "";

// A base 64 encoded string.
constexpr std::string kBase64 = "TGVlcm95IEplbmtpbnM=";

// A malformed base 64 encoded string.
constexpr std::string kNotBase64 = "NOT_BASE_64!";

// Request ID associated with deferred promises.
constexpr std::string kRequestId = "requestId";

// Frame ID for handle* events.
constexpr std::string kFrameId = "frameId";

// Common parameters of "handleGetRequest" and "handleCreateRequest" events.
constexpr std::string kRequest = "request";
constexpr std::string kRpEntity = "rpEntity";

// Parameters exclusive to the "handleCreateRequest" event.
constexpr std::string kUserEntity = "userEntity";
constexpr std::string kExcludeCredentials = "excludeCredentials";

// Parameter exclusive to the "handleGetRequest" event.
constexpr std::string kAllowCredentials = "allowCredentials";

// Member of the "request" dictionary.
constexpr std::string kChallenge = "challenge";

// Common members of the "rpEntity" and "userEntity" dictionaries.
constexpr std::string kId = "id";

// Member of the credential descriptors array.
constexpr char kType[] = "type";

base::Value::Dict BuildRequestInfoDict(const std::string* frame_id,
                                       const std::string* request_id) {
  base::Value::Dict request_info_dict;

  if (frame_id) {
    request_info_dict.Set(kFrameId, *frame_id);
  }

  if (request_id) {
    request_info_dict.Set(kRequestId, *request_id);
  }

  return request_info_dict;
}

IOSPasskeyClient::RequestInfo ValidRequestInfo() {
  return {kFrameId, kRequestId};
}

base::Value::Dict BuildRequestParamsDict(
    bool has_request_dict,
    const std::string* challenge,
    bool has_rp_entity_dict,
    const std::string* rp_id,
    bool has_user_entity_dict,
    const std::string* user_id,
    const std::string* credential_list_name,
    const std::string* credential_type,
    const std::string* credential_id) {
  base::Value::Dict request_params_dict;

  // Common request parameters dictionary.
  if (has_request_dict) {
    base::Value::Dict request_dict;

    if (challenge) {
      request_dict.Set(kChallenge, *challenge);
    }

    request_params_dict.Set(kRequest, std::move(request_dict));
  }

  // Relying party entity dictionary.
  if (has_rp_entity_dict) {
    base::Value::Dict rp_entity_dict;

    if (rp_id) {
      rp_entity_dict.Set(kId, *rp_id);
    }

    request_params_dict.Set(kRpEntity, std::move(rp_entity_dict));
  }

  // User entity dictionary.
  if (has_user_entity_dict) {
    base::Value::Dict user_entity_dict;

    if (user_id) {
      user_entity_dict.Set(kId, *user_id);
    }

    request_params_dict.Set(kUserEntity, std::move(user_entity_dict));
  }

  if (credential_list_name) {
    base::Value::Dict credential_dict;

    if (credential_type) {
      credential_dict.Set(kType, *credential_type);
    }

    if (credential_id) {
      credential_dict.Set(kId, *credential_id);
    }

    request_params_dict.Set(
        *credential_list_name,
        base::Value::List().Append(std::move(credential_dict)));
  }

  return request_params_dict;
}

base::Value::Dict BuildRequestParamsDictForRequest(
    bool has_request_dict,
    const std::string* challenge) {
  return BuildRequestParamsDict(has_request_dict, challenge,
                                /*has_rp_entity_dict=*/false, nullptr,
                                /*has_user_entity_dict=*/false, nullptr,
                                nullptr, nullptr, nullptr);
}

base::Value::Dict BuildRequestParamsDictForRp(bool has_rp_entity_dict,
                                              const std::string* rp_id) {
  return BuildRequestParamsDict(
      /*has_request_dict=*/true, &kBase64, has_rp_entity_dict, rp_id,
      /*has_user_entity_dict=*/false, nullptr, nullptr, nullptr, nullptr);
}

base::Value::Dict BuildRequestParamsDictForUser(bool has_user_entity_dict,
                                                const std::string* user_id) {
  return BuildRequestParamsDict(/*has_request_dict=*/true, &kBase64,
                                /*has_rp_entity_dict=*/true, &kBase64,
                                has_user_entity_dict, user_id, nullptr, nullptr,
                                nullptr);
}

base::Value::Dict BuildRequestParamsDictForCredentials(
    const std::string* credential_list_name,
    const std::string* credential_type,
    const std::string* credential_id) {
  return BuildRequestParamsDict(/*has_request_dict=*/true, &kBase64,
                                /*has_rp_entity_dict=*/true, &kBase64,
                                /*has_user_entity_dict=*/true, &kBase64,
                                credential_list_name, credential_type,
                                credential_id);
}

// Verifies that we get the desired RequestInfo parsing error.
void VerifyRequestInfoError(const base::Value::Dict& dict,
                            PasskeysParsingError error) {
  auto request_info = BuildRequestInfo(dict);
  ASSERT_FALSE(request_info.has_value());
  ASSERT_EQ(request_info.error(), error);
}

// Verifies that we get the desired parsing error on assertion requests.
void VerifyAssertionRequestParamsError(const base::Value::Dict& dict,
                                       PasskeysParsingError error) {
  auto assertion_request_params =
      BuildAssertionRequestParams(ValidRequestInfo(), dict);
  ASSERT_FALSE(assertion_request_params.has_value());
  ASSERT_EQ(assertion_request_params.error(), error);
}

// Verifies that we get the desired parsing error on registration requests.
void VerifyRegistrationRequestParamsError(const base::Value::Dict& dict,
                                          PasskeysParsingError error) {
  auto registration_request_params =
      BuildRegistrationRequestParams(ValidRequestInfo(), dict);
  ASSERT_FALSE(registration_request_params.has_value());
  ASSERT_EQ(registration_request_params.error(), error);
}

// Verifies that we get the desired parsing error on both assertion and
// registration requests.
void VerifyRequestParamsError(const base::Value::Dict& dict,
                              PasskeysParsingError error) {
  VerifyAssertionRequestParamsError(dict, error);
  VerifyRegistrationRequestParamsError(dict, error);
}

// Verifies that we get the desired credential ID parsing error on both
// assertion and registration requests.
void VerifyRequestCredentialIdError(const std::string* credential_id,
                                    PasskeysParsingError error) {
  const std::string public_key = device::kPublicKey;
  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForCredentials(&kExcludeCredentials, &public_key,
                                           credential_id),
      error);

  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForCredentials(&kAllowCredentials, &public_key,
                                           credential_id),
      error);
}

}  // namespace

// Tests that an error is returned on a missing frame id.
TEST_F(PasskeyRequestParserTest, MissingFrameId) {
  VerifyRequestInfoError(BuildRequestInfoDict(nullptr, nullptr),
                         PasskeysParsingError::kMissingFrameId);
}

// Tests that an error is returned on an empty frame id.
TEST_F(PasskeyRequestParserTest, EmptyFrameId) {
  VerifyRequestInfoError(BuildRequestInfoDict(&kEmpty, nullptr),
                         PasskeysParsingError::kEmptyFrameId);
}

// Tests that an error is returned on a missing request id.
TEST_F(PasskeyRequestParserTest, MissingRequestId) {
  VerifyRequestInfoError(BuildRequestInfoDict(&kFrameId, nullptr),
                         PasskeysParsingError::kMissingRequestId);
}

// Tests that an error is returned on an empty request id.
TEST_F(PasskeyRequestParserTest, EmptyRequestId) {
  VerifyRequestInfoError(BuildRequestInfoDict(&kFrameId, &kEmpty),
                         PasskeysParsingError::kEmptyRequestId);
}

// Tests that an error is returned on a missing request.
TEST_F(PasskeyRequestParserTest, MissingRequest) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRequest(/*has_request_dict=*/false, nullptr),
      PasskeysParsingError::kMissingRequest);
}

// Tests that an error is returned on a missing challenge.
TEST_F(PasskeyRequestParserTest, MissingChallenge) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRequest(/*has_request_dict=*/true, nullptr),
      PasskeysParsingError::kMissingChallenge);
}

// Tests that an error is returned on an empty challenge.
TEST_F(PasskeyRequestParserTest, EmptyChallenge) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRequest(/*has_request_dict=*/true, &kEmpty),
      PasskeysParsingError::kEmptyChallenge);
}

// Tests that an error is returned on a malformed challenge.
TEST_F(PasskeyRequestParserTest, MalformedChallenge) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRequest(/*has_request_dict=*/true, &kNotBase64),
      PasskeysParsingError::kMalformedChallenge);
}

// Tests that an error is returned on a missing rp entity.
TEST_F(PasskeyRequestParserTest, MissingRpEntity) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRp(/*has_rp_entity_dict=*/false, nullptr),
      PasskeysParsingError::kMissingRpEntity);
}

// Tests that an error is returned on a missing rp id.
TEST_F(PasskeyRequestParserTest, MissingRpId) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRp(/*has_rp_entity_dict=*/true, nullptr),
      PasskeysParsingError::kMissingRpId);
}

// Tests that an error is returned on an empty rp id.
TEST_F(PasskeyRequestParserTest, EmptyRpId) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRp(/*has_rp_entity_dict=*/true, &kEmpty),
      PasskeysParsingError::kEmptyRpId);
}

// Tests that an error is returned on a missing user entity.
TEST_F(PasskeyRequestParserTest, MissingUserEntity) {
  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForUser(/*has_user_entity_dict=*/false, nullptr),
      PasskeysParsingError::kMissingUserEntity);
}

// Tests that an error is returned on a missing user id.
TEST_F(PasskeyRequestParserTest, MissingUserId) {
  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForUser(/*has_user_entity_dict=*/true, nullptr),
      PasskeysParsingError::kMissingUserId);
}

// Tests that an error is returned on an empty user id.
TEST_F(PasskeyRequestParserTest, EmptyUserId) {
  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForUser(/*has_user_entity_dict=*/true, &kEmpty),
      PasskeysParsingError::kEmptyUserId);
}

// Tests that an error is returned on a malformed user id.
TEST_F(PasskeyRequestParserTest, MalformedUserId) {
  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForUser(/*has_user_entity_dict=*/true, &kNotBase64),
      PasskeysParsingError::kMalformedUserId);
}

// Tests that an error is returned on a missing credential type.
TEST_F(PasskeyRequestParserTest, MissingCredentialType) {
  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForCredentials(&kExcludeCredentials, nullptr,
                                           nullptr),
      PasskeysParsingError::kMissingCredentialType);

  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForCredentials(&kAllowCredentials, nullptr,
                                           nullptr),
      PasskeysParsingError::kMissingCredentialType);
}

// Tests that an error is returned on a missing credential id.
TEST_F(PasskeyRequestParserTest, MissingCredentialId) {
  VerifyRequestCredentialIdError(nullptr,
                                 PasskeysParsingError::kMissingCredentialId);
}

// Tests that an error is returned on an empty credential id.
TEST_F(PasskeyRequestParserTest, EmptyCredentialId) {
  VerifyRequestCredentialIdError(&kEmpty,
                                 PasskeysParsingError::kEmptyCredentialId);
}

// Tests that an error is returned on a malformed credential id.
TEST_F(PasskeyRequestParserTest, MalformedCredentialId) {
  VerifyRequestCredentialIdError(&kNotBase64,
                                 PasskeysParsingError::kMalformedCredentialId);
}

TEST_F(PasskeyRequestParserTest, NoError) {
  auto assertion_request_params = BuildAssertionRequestParams(
      ValidRequestInfo(),
      BuildRequestParamsDictForRp(/*has_rp_entity_dict=*/true, &kBase64));
  ASSERT_TRUE(assertion_request_params.has_value());

  auto registration_request_params = BuildRegistrationRequestParams(
      ValidRequestInfo(),
      BuildRequestParamsDictForUser(/*has_user_entity_dict=*/true, &kBase64));
  ASSERT_TRUE(registration_request_params.has_value());
}

}  // namespace webauthn
