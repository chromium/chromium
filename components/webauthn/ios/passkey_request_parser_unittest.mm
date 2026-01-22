// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_request_parser.h"

#import "base/base64url.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "device/fido/public/fido_constants.h"
#import "testing/platform_test.h"

using PasskeyRequestParserTest = PlatformTest;

namespace webauthn {

namespace {

// An empty string for testing purposes.
constexpr std::string kEmpty = "";

// A base 64 encoded URL string.
constexpr std::string kBase64url = "TGVlcm95IEplbmtpbnM=";
constexpr std::string kBase64url_2 = "U2FsdCBCYWU=";

// A malformed base 64 url encoded string.
constexpr std::string kNotBase64url = "NOT_BASE_64_URL!";

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
constexpr std::string kIsConditional = "isConditional";

// Common members of the "rpEntity" and "userEntity" dictionaries.
constexpr std::string kId = "id";

// Member of the credential descriptors array.
constexpr char kType[] = "type";

// JSON formatted extension input data.
constexpr char kExtensions[] = "extensions";

// Creates a base 64 encoded string larger than the maximum PRF input size.
std::string BuildLargeBase64String() {
  const std::string pattern64 = "eHh4";  // Base 64 encoding of "xxx".
  std::string tooLarge64;
  tooLarge64.reserve(pattern64.length() * device::kMaxPRFInputSize);
  for (size_t i = 0; i < device::kMaxPRFInputSize; ++i) {
    tooLarge64 += pattern64;
  }
  return tooLarge64;
}

// Encodes a byte vector to base 64 URL encoded string.
std::string Base64UrlEncode(base::span<const uint8_t> input) {
  std::string output;
  base::Base64UrlEncode(input, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &output);
  return output;
}

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
    bool has_conditional,
    bool has_rp_entity_dict,
    const std::string* rp_id,
    bool has_user_entity_dict,
    const std::string* user_id,
    const std::string* credential_list_name,
    const std::string* credential_type,
    const std::string* credential_id,
    bool has_extensions,
    const std::string* prf_input1,
    const std::string* prf_input2,
    const std::string* per_credential_id,
    const std::string* per_credential_prf_input1,
    const std::string* per_credential_prf_input2) {
  base::Value::Dict request_params_dict;

  // Common request parameters dictionary.
  if (has_request_dict) {
    base::Value::Dict request_dict;

    if (challenge) {
      request_dict.Set(kChallenge, *challenge);
    }

    if (has_conditional) {
      request_dict.Set(kIsConditional, false);
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

  if (has_extensions) {
    base::Value::Dict extensions_dict;

    base::Value::Dict prf_dict;

    if (prf_input1 || prf_input2) {
      base::Value::Dict prf_eval;

      if (prf_input1) {
        prf_eval.Set(device::kExtensionPRFFirst, *prf_input1);
      }
      if (prf_input2) {
        prf_eval.Set(device::kExtensionPRFSecond, *prf_input2);
      }

      prf_dict.Set(device::kExtensionPRFEval, std::move(prf_eval));
    }

    if (per_credential_id) {
      base::Value::Dict prf_eval_by_credential;

      base::Value::Dict prf_eval;
      if (per_credential_prf_input1 || per_credential_prf_input2) {
        if (per_credential_prf_input1) {
          prf_eval.Set(device::kExtensionPRFFirst, *per_credential_prf_input1);
        }
        if (per_credential_prf_input2) {
          prf_eval.Set(device::kExtensionPRFSecond, *per_credential_prf_input2);
        }
      }

      prf_eval_by_credential.Set(*per_credential_id, std::move(prf_eval));

      prf_dict.Set(device::kExtensionPRFEvalByCredential,
                   std::move(prf_eval_by_credential));
    }

    extensions_dict.Set(device::kExtensionPRF, std::move(prf_dict));

    request_params_dict.Set(kExtensions, std::move(extensions_dict));
  }

  return request_params_dict;
}

base::Value::Dict BuildRequestParamsDictNoExtensions(
    bool has_request_dict,
    const std::string* challenge,
    bool has_conditional,
    bool has_rp_entity_dict,
    const std::string* rp_id,
    bool has_user_entity_dict,
    const std::string* user_id,
    const std::string* credential_list_name,
    const std::string* credential_type,
    const std::string* credential_id) {
  return BuildRequestParamsDict(has_request_dict, challenge, has_conditional,
                                has_rp_entity_dict, rp_id, has_user_entity_dict,
                                user_id, credential_list_name, credential_type,
                                credential_id, /*has_extensions=*/true, nullptr,
                                nullptr, nullptr, nullptr, nullptr);
}

base::Value::Dict BuildRequestParamsDictForRequest(bool has_request_dict,
                                                   const std::string* challenge,
                                                   bool has_conditional) {
  return BuildRequestParamsDictNoExtensions(
      has_request_dict, challenge, has_conditional,
      /*has_rp_entity_dict=*/false, nullptr, /*has_user_entity_dict=*/false,
      nullptr, nullptr, nullptr, nullptr);
}

base::Value::Dict BuildRequestParamsDictForRp(bool has_rp_entity_dict,
                                              const std::string* rp_id) {
  return BuildRequestParamsDictNoExtensions(
      /*has_request_dict=*/true, &kBase64url, /*has_conditional=*/true,
      has_rp_entity_dict, rp_id, /*has_user_entity_dict=*/false, nullptr,
      nullptr, nullptr, nullptr);
}

base::Value::Dict BuildRequestParamsDictForUser(bool has_user_entity_dict,
                                                const std::string* user_id) {
  return BuildRequestParamsDictNoExtensions(
      /*has_request_dict=*/true, &kBase64url, /*has_conditional=*/true,
      /*has_rp_entity_dict=*/true, &kBase64url, has_user_entity_dict, user_id,
      nullptr, nullptr, nullptr);
}

base::Value::Dict BuildRequestParamsDictForCredentials(
    const std::string* credential_list_name,
    const std::string* credential_type,
    const std::string* credential_id) {
  return BuildRequestParamsDictNoExtensions(
      /*has_request_dict=*/true, &kBase64url, /*has_conditional=*/true,
      /*has_rp_entity_dict=*/true, &kBase64url, /*has_user_entity_dict=*/true,
      &kBase64url, credential_list_name, credential_type, credential_id);
}

base::Value::Dict BuildRequestParamsDictForExtensions(
    bool has_extensions,
    bool for_creation,
    const std::string* prf_input1,
    const std::string* prf_input2,
    const std::string* per_credential_id,
    const std::string* per_credential_prf_input1,
    const std::string* per_credential_prf_input2) {
  const std::string public_key = device::kPublicKey;
  return BuildRequestParamsDict(
      /*has_request_dict=*/true, &kBase64url, /*has_conditional=*/true,
      /*has_rp_entity_dict=*/true, &kBase64url, /*has_user_entity_dict=*/true,
      &kBase64url, for_creation ? &kExcludeCredentials : &kAllowCredentials,
      &public_key, &kBase64url, has_extensions, prf_input1, prf_input2,
      per_credential_id, per_credential_prf_input1, per_credential_prf_input2);
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
      BuildRequestParamsDictForRequest(/*has_request_dict=*/false, nullptr,
                                       /*has_conditional=*/true),
      PasskeysParsingError::kMissingRequest);
}

// Tests that an error is returned on a missing challenge.
TEST_F(PasskeyRequestParserTest, MissingChallenge) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRequest(/*has_request_dict=*/true, nullptr,
                                       /*has_conditional=*/true),
      PasskeysParsingError::kMissingChallenge);
}

// Tests that an error is returned on an empty challenge.
TEST_F(PasskeyRequestParserTest, EmptyChallenge) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRequest(/*has_request_dict=*/true, &kEmpty,
                                       /*has_conditional=*/true),
      PasskeysParsingError::kEmptyChallenge);
}

// Tests that an error is returned on a malformed challenge.
TEST_F(PasskeyRequestParserTest, MalformedChallenge) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRequest(
          /*has_request_dict=*/true, &kNotBase64url, /*has_conditional=*/true),
      PasskeysParsingError::kMalformedChallenge);
}

// Tests that an error is returned on a missing conditional setting.
TEST_F(PasskeyRequestParserTest, MissingConditional) {
  VerifyRequestParamsError(
      BuildRequestParamsDictForRequest(
          /*has_request_dict=*/true, &kBase64url, /*has_conditional=*/false),
      PasskeysParsingError::kMissingConditional);
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
      BuildRequestParamsDictForUser(/*has_user_entity_dict=*/true,
                                    &kNotBase64url),
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
  VerifyRequestCredentialIdError(&kNotBase64url,
                                 PasskeysParsingError::kMalformedCredentialId);
}

TEST_F(PasskeyRequestParserTest, MissingExtensions) {
  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/false, /*for_creation=*/false, nullptr, nullptr,
          nullptr, nullptr, nullptr),
      PasskeysParsingError::kMissingExtensions);

  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/false, /*for_creation=*/true, nullptr, nullptr,
          nullptr, nullptr, nullptr),
      PasskeysParsingError::kMissingExtensions);
}

TEST_F(PasskeyRequestParserTest, EvalByCredentialOnCreate) {
  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/true, &kBase64url,
          &kBase64url, &kBase64url, &kBase64url, &kBase64url),
      PasskeysParsingError::kEvalByCredentialOnCreate);
}

TEST_F(PasskeyRequestParserTest, MissingEvalByCredential) {
  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, &kBase64url,
          &kBase64url, &kEmpty, nullptr, nullptr),
      PasskeysParsingError::kMissingEvalByCredential);
}

TEST_F(PasskeyRequestParserTest, MalformedEvalByCredential) {
  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, nullptr, nullptr,
          &kNotBase64url, &kBase64url, nullptr),
      PasskeysParsingError::kMalformedEvalByCredential);
}

TEST_F(PasskeyRequestParserTest, EvalByCredentialNotAllowed) {
  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, &kBase64url,
          &kBase64url, &kBase64url_2, &kBase64url, &kBase64url),
      PasskeysParsingError::kEvalByCredentialNotAllowed);
}

TEST_F(PasskeyRequestParserTest, MissingFirstPRFInput) {
  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, nullptr, &kBase64url,
          nullptr, nullptr, nullptr),
      PasskeysParsingError::kMissingFirstPRFInput);

  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, nullptr, nullptr,
          &kBase64url, nullptr, &kBase64url),
      PasskeysParsingError::kMissingFirstPRFInput);

  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/true, nullptr, &kBase64url,
          nullptr, nullptr, nullptr),
      PasskeysParsingError::kMissingFirstPRFInput);
}

TEST_F(PasskeyRequestParserTest, MalformedFirstPRFInput) {
  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, &kNotBase64url,
          nullptr, nullptr, nullptr, nullptr),
      PasskeysParsingError::kMalformedFirstPRFInput);

  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, nullptr, nullptr,
          &kBase64url, &kNotBase64url, nullptr),
      PasskeysParsingError::kMalformedFirstPRFInput);

  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/true, &kNotBase64url,
          nullptr, nullptr, nullptr, nullptr),
      PasskeysParsingError::kMalformedFirstPRFInput);
}

TEST_F(PasskeyRequestParserTest, MalformedSecondPRFInput) {
  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, &kBase64url,
          &kNotBase64url, nullptr, nullptr, nullptr),
      PasskeysParsingError::kMalformedSecondPRFInput);

  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, nullptr, nullptr,
          &kBase64url, &kBase64url, &kNotBase64url),
      PasskeysParsingError::kMalformedSecondPRFInput);

  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/true, &kBase64url,
          &kNotBase64url, nullptr, nullptr, nullptr),
      PasskeysParsingError::kMalformedSecondPRFInput);
}

TEST_F(PasskeyRequestParserTest, PRFInputTooLarge) {
  const std::string tooLarge64(BuildLargeBase64String());

  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/true, &tooLarge64,
          &kBase64url, nullptr, nullptr, nullptr),
      PasskeysParsingError::kPRFInputTooLarge);

  VerifyRegistrationRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/true, &kBase64url,
          &tooLarge64, nullptr, nullptr, nullptr),
      PasskeysParsingError::kPRFInputTooLarge);

  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, &tooLarge64,
          &kBase64url, &kBase64url, &kBase64url, &kBase64url),
      PasskeysParsingError::kPRFInputTooLarge);

  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, &kBase64url,
          &tooLarge64, &kBase64url, &kBase64url, &kBase64url),
      PasskeysParsingError::kPRFInputTooLarge);

  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, &kBase64url,
          &kBase64url, &kBase64url, &tooLarge64, &kBase64url),
      PasskeysParsingError::kPRFInputTooLarge);

  VerifyAssertionRequestParamsError(
      BuildRequestParamsDictForExtensions(
          /*has_extensions=*/true, /*for_creation=*/false, &kBase64url,
          &kBase64url, &kBase64url, &kBase64url, &tooLarge64),
      PasskeysParsingError::kPRFInputTooLarge);
}

TEST_F(PasskeyRequestParserTest, NoError) {
  auto assertion_request_params = BuildAssertionRequestParams(
      ValidRequestInfo(),
      BuildRequestParamsDictForRp(/*has_rp_entity_dict=*/true, &kBase64url));
  ASSERT_TRUE(assertion_request_params.has_value());

  auto registration_request_params = BuildRegistrationRequestParams(
      ValidRequestInfo(), BuildRequestParamsDictForUser(
                              /*has_user_entity_dict=*/true, &kBase64url));
  ASSERT_TRUE(registration_request_params.has_value());
}

TEST_F(PasskeyRequestParserTest, ToAuthenticationExtensionsClientOutputsJSON) {
  passkey_model_utils::ExtensionOutputData extension_output_data;

  // Test case 1: Empty prf_result.
  extension_output_data.prf_result = {};
  base::Value::Dict dict =
      ToAuthenticationExtensionsClientOutputsJSON(extension_output_data);
  EXPECT_TRUE(dict.empty());

  // Test case 2: prf_result size 32.
  std::vector<uint8_t> prf_result_32(32, 0xAA);
  extension_output_data.prf_result = prf_result_32;
  dict = ToAuthenticationExtensionsClientOutputsJSON(extension_output_data);
  EXPECT_FALSE(dict.empty());
  const base::Value::Dict* prf_dict = dict.FindDict(device::kExtensionPRF);
  ASSERT_TRUE(prf_dict);
  EXPECT_TRUE(prf_dict->FindBool(device::kExtensionPRFEnabled).value_or(false));
  const base::Value::Dict* results =
      prf_dict->FindDict(device::kExtensionPRFResults);
  ASSERT_TRUE(results);
  const std::string* first = results->FindString(device::kExtensionPRFFirst);
  ASSERT_TRUE(first);
  EXPECT_EQ(*first, Base64UrlEncode(prf_result_32));
  EXPECT_FALSE(results->FindString(device::kExtensionPRFSecond));

  // Test case 3: prf_result size 64.
  std::vector<uint8_t> prf_result_64(64, 0xBB);
  extension_output_data.prf_result = prf_result_64;
  dict = ToAuthenticationExtensionsClientOutputsJSON(extension_output_data);
  EXPECT_FALSE(dict.empty());
  prf_dict = dict.FindDict(device::kExtensionPRF);
  ASSERT_TRUE(prf_dict);
  EXPECT_TRUE(prf_dict->FindBool(device::kExtensionPRFEnabled).value_or(false));
  results = prf_dict->FindDict(device::kExtensionPRFResults);
  ASSERT_TRUE(results);
  first = results->FindString(device::kExtensionPRFFirst);
  const std::string* second = results->FindString(device::kExtensionPRFSecond);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_EQ(*first, Base64UrlEncode(base::span(prf_result_64).first(32u)));
  EXPECT_EQ(*second, Base64UrlEncode(base::span(prf_result_64).subspan(32u)));
}

}  // namespace webauthn
