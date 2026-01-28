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

// Event keys and types.
constexpr char kEvent[] = "event";
constexpr char kHandleGetRequest[] = "handleGetRequest";
constexpr char kHandleCreateRequest[] = "handleCreateRequest";
constexpr char kLogGetRequest[] = "logGetRequest";
constexpr char kLogCreateRequest[] = "logCreateRequest";
constexpr char kLogGetResolved[] = "logGetResolved";
constexpr char kLogCreateResolved[] = "logCreateResolved";

// Parameter keys for logging.
constexpr char kRpId[] = "rpId";
constexpr char kCredentialId[] = "credentialId";
constexpr char kIsGpm[] = "isGpm";

// Test values.
constexpr char kOtherKey[] = "other";
constexpr char kValue[] = "value";
constexpr char kUnknownEvent[] = "unknownEventString";
constexpr char kExampleRpId[] = "example.com";
constexpr char kExampleCredId[] = "cred123";

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

base::DictValue BuildRequestInfoDict(const std::string* frame_id,
                                     const std::string* request_id) {
  base::DictValue request_info_dict;

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

// Convenience structure to have default values for tested request parameters.
struct TestRequestParams {
  // Request parameters.
  bool has_request_dict = true;
  std::optional<std::string> challenge = kBase64url;
  bool has_conditional = true;

  // Relying Party Entity parameters.
  bool has_rp_entity_dict = true;
  std::optional<std::string> rp_id = kBase64url;

  // User Entity parameters.
  bool has_user_entity_dict = true;
  std::optional<std::string> user_id = kBase64url;

  // Credential list (allow or exclude) parameters.
  std::optional<std::string> credential_list_name;
  std::optional<std::string> credential_type = device::kPublicKey;
  std::optional<std::string> credential_id = kBase64url;

  // Extension parameters.
  bool has_extensions = true;
  std::optional<std::string> prf_input1;
  std::optional<std::string> prf_input2;
  std::optional<std::string> per_credential_id;
  std::optional<std::string> per_credential_prf_input1;
  std::optional<std::string> per_credential_prf_input2;

  // Sets up the object so that per credential PRF can be used.
  void EnablePerCredentialPRF() {
    credential_list_name = kAllowCredentials;
    per_credential_id = kBase64url;
  }

  // Builds a dictionary based on the structure's parameters.
  base::DictValue BuildDict() const {
    base::DictValue request_params_dict;

    // Common request parameters dictionary.
    if (has_request_dict) {
      base::DictValue request_dict;

      if (challenge.has_value()) {
        request_dict.Set(kChallenge, *challenge);
      }

      if (has_conditional) {
        request_dict.Set(kIsConditional, false);
      }

      request_params_dict.Set(kRequest, std::move(request_dict));
    }

    // Relying party entity dictionary.
    if (has_rp_entity_dict) {
      base::DictValue rp_entity_dict;

      if (rp_id.has_value()) {
        rp_entity_dict.Set(kId, *rp_id);
      }

      request_params_dict.Set(kRpEntity, std::move(rp_entity_dict));
    }

    // User entity dictionary.
    if (has_user_entity_dict) {
      base::DictValue user_entity_dict;

      if (user_id.has_value()) {
        user_entity_dict.Set(kId, *user_id);
      }

      request_params_dict.Set(kUserEntity, std::move(user_entity_dict));
    }

    if (credential_list_name.has_value()) {
      base::DictValue credential_dict;

      if (credential_type.has_value()) {
        credential_dict.Set(kType, *credential_type);
      }

      if (credential_id.has_value()) {
        credential_dict.Set(kId, *credential_id);
      }

      request_params_dict.Set(
          *credential_list_name,
          base::ListValue().Append(std::move(credential_dict)));
    }

    if (has_extensions) {
      base::DictValue extensions_dict;

      base::DictValue prf_dict;

      bool has_prf_input = prf_input1.has_value() || prf_input2.has_value();
      if (has_prf_input) {
        base::DictValue prf_eval;

        if (prf_input1.has_value()) {
          prf_eval.Set(device::kExtensionPRFFirst, *prf_input1);
        }
        if (prf_input2.has_value()) {
          prf_eval.Set(device::kExtensionPRFSecond, *prf_input2);
        }

        prf_dict.Set(device::kExtensionPRFEval, std::move(prf_eval));
      }

      bool has_prf_per_credential = per_credential_id.has_value();
      if (has_prf_per_credential) {
        base::DictValue prf_eval_by_credential;

        base::DictValue prf_eval;
        if (per_credential_prf_input1.has_value() ||
            per_credential_prf_input2.has_value()) {
          if (per_credential_prf_input1.has_value()) {
            prf_eval.Set(device::kExtensionPRFFirst,
                         *per_credential_prf_input1);
          }
          if (per_credential_prf_input2.has_value()) {
            prf_eval.Set(device::kExtensionPRFSecond,
                         *per_credential_prf_input2);
          }
        }

        prf_eval_by_credential.Set(*per_credential_id, std::move(prf_eval));

        prf_dict.Set(device::kExtensionPRFEvalByCredential,
                     std::move(prf_eval_by_credential));
      }

      if (has_prf_input || has_prf_per_credential) {
        extensions_dict.Set(device::kExtensionPRF, std::move(prf_dict));
      }

      request_params_dict.Set(kExtensions, std::move(extensions_dict));
    }

    return request_params_dict;
  }

  // Verifies that this structure's parameters produces the desired parsing
  // error on assertion requests.
  void VerifyAssertionError(PasskeysParsingError error) {
    auto assertion_request_params =
        BuildAssertionRequestParams(ValidRequestInfo(), BuildDict());
    ASSERT_FALSE(assertion_request_params.has_value());
    ASSERT_EQ(assertion_request_params.error(), error);
  }

  // Verifies that this structure's parameters produces the desired parsing
  // error on registration requests.
  void VerifyRegistrationError(PasskeysParsingError error) {
    auto registration_request_params =
        BuildRegistrationRequestParams(ValidRequestInfo(), BuildDict());
    ASSERT_FALSE(registration_request_params.has_value());
    ASSERT_EQ(registration_request_params.error(), error);
  }

  // Verifies that this structure's parameters produces the desired parsing
  // error on both assertion and registration requests.
  void VerifyError(PasskeysParsingError error) {
    VerifyAssertionError(error);
    VerifyRegistrationError(error);
  }

  // Verifies that this structure's parameters produces the desired credential
  // list related parsing error on both assertion and registration requests.
  void VerifyCredentialListError(PasskeysParsingError error) {
    credential_list_name = kExcludeCredentials;
    VerifyRegistrationError(error);

    credential_list_name = kAllowCredentials;
    VerifyAssertionError(error);
  }
};

// Verifies that the dictionary produces the desired RequestInfo parsing error.
void VerifyRequestInfoError(const base::DictValue& dict,
                            PasskeysParsingError error) {
  auto request_info = BuildRequestInfo(dict);
  ASSERT_FALSE(request_info.has_value());
  ASSERT_EQ(request_info.error(), error);
}

// Helper function for parser tests that don't depend on the GPM check result
// or expect a false result.
bool IsGpmPasskey(const std::string&, const std::string&) {
  return false;
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
  TestRequestParams params;
  params.has_request_dict = false;

  params.VerifyError(PasskeysParsingError::kMissingRequest);
}

// Tests that an error is returned on a missing challenge.
TEST_F(PasskeyRequestParserTest, MissingChallenge) {
  TestRequestParams params;
  params.challenge = std::nullopt;

  params.VerifyError(PasskeysParsingError::kMissingChallenge);
}

// Tests that an error is returned on an empty challenge.
TEST_F(PasskeyRequestParserTest, EmptyChallenge) {
  TestRequestParams params;
  params.challenge = kEmpty;

  params.VerifyError(PasskeysParsingError::kEmptyChallenge);
}

// Tests that an error is returned on a malformed challenge.
TEST_F(PasskeyRequestParserTest, MalformedChallenge) {
  TestRequestParams params;
  params.challenge = kNotBase64url;

  params.VerifyError(PasskeysParsingError::kMalformedChallenge);
}

// Tests that an error is returned on a missing conditional setting.
TEST_F(PasskeyRequestParserTest, MissingConditional) {
  TestRequestParams params;
  params.has_conditional = false;

  params.VerifyError(PasskeysParsingError::kMissingConditional);
}

// Tests that an error is returned on a missing rp entity.
TEST_F(PasskeyRequestParserTest, MissingRpEntity) {
  TestRequestParams params;
  params.has_rp_entity_dict = false;

  params.VerifyError(PasskeysParsingError::kMissingRpEntity);
}

// Tests that an error is returned on a missing rp id.
TEST_F(PasskeyRequestParserTest, MissingRpId) {
  TestRequestParams params;
  params.rp_id = std::nullopt;

  params.VerifyError(PasskeysParsingError::kMissingRpId);
}

// Tests that an error is returned on an empty rp id.
TEST_F(PasskeyRequestParserTest, EmptyRpId) {
  TestRequestParams params;
  params.rp_id = kEmpty;

  params.VerifyError(PasskeysParsingError::kEmptyRpId);
}

// Tests that an error is returned on a missing user entity.
TEST_F(PasskeyRequestParserTest, MissingUserEntity) {
  TestRequestParams params;
  params.has_user_entity_dict = false;

  params.VerifyRegistrationError(PasskeysParsingError::kMissingUserEntity);
}

// Tests that an error is returned on a missing user id.
TEST_F(PasskeyRequestParserTest, MissingUserId) {
  TestRequestParams params;
  params.user_id = std::nullopt;

  params.VerifyRegistrationError(PasskeysParsingError::kMissingUserId);
}

// Tests that an error is returned on an empty user id.
TEST_F(PasskeyRequestParserTest, EmptyUserId) {
  TestRequestParams params;
  params.user_id = kEmpty;

  params.VerifyRegistrationError(PasskeysParsingError::kEmptyUserId);
}

// Tests that an error is returned on a malformed user id.
TEST_F(PasskeyRequestParserTest, MalformedUserId) {
  TestRequestParams params;
  params.user_id = kNotBase64url;

  params.VerifyRegistrationError(PasskeysParsingError::kMalformedUserId);
}

// Tests that an error is returned on a missing credential type.
TEST_F(PasskeyRequestParserTest, MissingCredentialType) {
  TestRequestParams params;
  params.credential_type = std::nullopt;

  params.VerifyCredentialListError(
      PasskeysParsingError::kMissingCredentialType);
}

// Tests that an error is returned on a missing credential id.
TEST_F(PasskeyRequestParserTest, MissingCredentialId) {
  TestRequestParams params;
  params.credential_id = std::nullopt;

  params.VerifyCredentialListError(PasskeysParsingError::kMissingCredentialId);
}

// Tests that an error is returned on an empty credential id.
TEST_F(PasskeyRequestParserTest, EmptyCredentialId) {
  TestRequestParams params;
  params.credential_id = kEmpty;

  params.VerifyCredentialListError(PasskeysParsingError::kEmptyCredentialId);
}

// Tests that an error is returned on a malformed credential id.
TEST_F(PasskeyRequestParserTest, MalformedCredentialId) {
  TestRequestParams params;
  params.credential_id = kNotBase64url;

  params.VerifyCredentialListError(
      PasskeysParsingError::kMalformedCredentialId);
}

TEST_F(PasskeyRequestParserTest, MissingExtensions) {
  TestRequestParams params;
  params.has_extensions = false;

  params.VerifyError(PasskeysParsingError::kMissingExtensions);
}

TEST_F(PasskeyRequestParserTest, EvalByCredentialOnCreate) {
  TestRequestParams params;
  params.per_credential_id = kBase64url;

  params.VerifyRegistrationError(
      PasskeysParsingError::kEvalByCredentialOnCreate);
}

TEST_F(PasskeyRequestParserTest, MissingEvalByCredential) {
  TestRequestParams params;
  params.per_credential_id = kEmpty;

  params.VerifyAssertionError(PasskeysParsingError::kMissingEvalByCredential);
}

TEST_F(PasskeyRequestParserTest, MalformedEvalByCredential) {
  TestRequestParams params;
  params.per_credential_id = kNotBase64url;

  params.VerifyAssertionError(PasskeysParsingError::kMalformedEvalByCredential);
}

TEST_F(PasskeyRequestParserTest, EvalByCredentialNotAllowed) {
  TestRequestParams params;
  params.credential_list_name = kAllowCredentials;
  params.per_credential_id = kBase64url_2;

  params.VerifyAssertionError(
      PasskeysParsingError::kEvalByCredentialNotAllowed);
}

TEST_F(PasskeyRequestParserTest, MissingFirstPRFInput) {
  // Verify both assertion and registration for missing first PRF input.
  {
    TestRequestParams params;
    params.prf_input2 = kBase64url;

    params.VerifyError(PasskeysParsingError::kMissingFirstPRFInput);
  }

  // Verify assertion only for missing per credential first PRF input (per
  // credential not allowed on registration requests).
  {
    TestRequestParams params;
    params.EnablePerCredentialPRF();
    params.per_credential_prf_input2 = kBase64url;

    params.VerifyAssertionError(PasskeysParsingError::kMissingFirstPRFInput);
  }
}

TEST_F(PasskeyRequestParserTest, MalformedFirstPRFInput) {
  // Verify both assertion and registration for malformed first PRF input.
  {
    TestRequestParams params;
    params.prf_input1 = kNotBase64url;

    params.VerifyError(PasskeysParsingError::kMalformedFirstPRFInput);
  }

  // Verify assertion only for malformed per credential first PRF input (per
  // credential not allowed on registration requests).
  {
    TestRequestParams params;
    params.EnablePerCredentialPRF();
    params.per_credential_prf_input1 = kNotBase64url;

    params.VerifyAssertionError(PasskeysParsingError::kMalformedFirstPRFInput);
  }
}

TEST_F(PasskeyRequestParserTest, MalformedSecondPRFInput) {
  // Verify both assertion and registration for malformed second PRF input.
  {
    TestRequestParams params;
    params.prf_input1 = kBase64url;
    params.prf_input2 = kNotBase64url;

    params.VerifyError(PasskeysParsingError::kMalformedSecondPRFInput);
  }

  // Verify assertion only for malformed per credential second PRF input (per
  // credential not allowed on registration requests).
  {
    TestRequestParams params;
    params.EnablePerCredentialPRF();
    params.per_credential_prf_input1 = kBase64url;
    params.per_credential_prf_input2 = kNotBase64url;

    params.VerifyAssertionError(PasskeysParsingError::kMalformedSecondPRFInput);
  }
}

TEST_F(PasskeyRequestParserTest, PRFInputTooLarge) {
  const std::string tooLarge64(BuildLargeBase64String());

  // Verify both assertion and registration for large first PRF input.
  {
    TestRequestParams params;
    params.prf_input1 = tooLarge64;

    params.VerifyError(PasskeysParsingError::kPRFInputTooLarge);
  }

  // Verify both assertion and registration for large second PRF input.
  {
    TestRequestParams params;
    params.prf_input1 = kBase64url;
    params.prf_input2 = tooLarge64;

    params.VerifyError(PasskeysParsingError::kPRFInputTooLarge);
  }

  // Verify assertion only for large per credential first PRF input (per
  // credential not allowed on registration requests).
  {
    TestRequestParams params;
    params.EnablePerCredentialPRF();
    params.per_credential_prf_input1 = tooLarge64;

    params.VerifyAssertionError(PasskeysParsingError::kPRFInputTooLarge);
  }

  // Verify assertion only for large per credential second PRF input (per
  // credential not allowed on registration requests).
  {
    TestRequestParams params;
    params.EnablePerCredentialPRF();
    params.per_credential_prf_input1 = kBase64url;
    params.per_credential_prf_input2 = tooLarge64;

    params.VerifyAssertionError(PasskeysParsingError::kPRFInputTooLarge);
  }
}

TEST_F(PasskeyRequestParserTest, NoError) {
  TestRequestParams params;
  base::DictValue dict = params.BuildDict();

  auto assertion_request_params =
      BuildAssertionRequestParams(ValidRequestInfo(), dict);
  ASSERT_TRUE(assertion_request_params.has_value());

  auto registration_request_params =
      BuildRegistrationRequestParams(ValidRequestInfo(), dict);
  ASSERT_TRUE(registration_request_params.has_value());
}

TEST_F(PasskeyRequestParserTest, ToAuthenticationExtensionsClientOutputsJSON) {
  passkey_model_utils::ExtensionOutputData extension_output_data;

  // Test case 1: Empty prf_result.
  extension_output_data.prf_result = {};
  base::DictValue dict =
      ToAuthenticationExtensionsClientOutputsJSON(extension_output_data);
  EXPECT_TRUE(dict.empty());

  // Test case 2: prf_result size 32.
  std::vector<uint8_t> prf_result_32(32, 0xAA);
  extension_output_data.prf_result = prf_result_32;
  dict = ToAuthenticationExtensionsClientOutputsJSON(extension_output_data);
  EXPECT_FALSE(dict.empty());
  const base::DictValue* prf_dict = dict.FindDict(device::kExtensionPRF);
  ASSERT_TRUE(prf_dict);
  EXPECT_TRUE(prf_dict->FindBool(device::kExtensionPRFEnabled).value_or(false));
  const base::DictValue* results =
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

// Tests that ParsePasskeyScriptEvent handles invalid or missing event data.
TEST_F(PasskeyRequestParserTest, ParseInvalidEventData) {
  // Test case 1: Empty dictionary.
  base::DictValue dict;
  EXPECT_FALSE(ParsePasskeyScriptEvent(dict, &IsGpmPasskey).has_value());

  // Test case 2: Missing "event" key.
  dict.Set(kOtherKey, kValue);
  EXPECT_FALSE(ParsePasskeyScriptEvent(dict, &IsGpmPasskey).has_value());

  // Test case 3: Unknown event string.
  dict.Set(kEvent, kUnknownEvent);
  EXPECT_FALSE(ParsePasskeyScriptEvent(dict, &IsGpmPasskey).has_value());
}

// Tests parsing of simple PasskeyScriptEvent types without additional
// parameters.
TEST_F(PasskeyRequestParserTest, ParseSimpleEventTypes) {
  // Test case 1: Handle Get.
  {
    base::DictValue dict;
    dict.Set(kEvent, kHandleGetRequest);
    auto result = ParsePasskeyScriptEvent(dict, &IsGpmPasskey);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, PasskeyScriptEvent::kHandleGetRequest);
  }

  // Test case 2: Handle Create.
  {
    base::DictValue dict;
    dict.Set(kEvent, kHandleCreateRequest);
    auto result = ParsePasskeyScriptEvent(dict, &IsGpmPasskey);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, PasskeyScriptEvent::kHandleCreateRequest);
  }

  // Test case 3: Log Get.
  {
    base::DictValue dict;
    dict.Set(kEvent, kLogGetRequest);
    auto result = ParsePasskeyScriptEvent(dict, &IsGpmPasskey);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, PasskeyScriptEvent::kLogGetRequest);
  }

  // Test case 4: Log Create.
  {
    base::DictValue dict;
    dict.Set(kEvent, kLogCreateRequest);
    auto result = ParsePasskeyScriptEvent(dict, &IsGpmPasskey);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, PasskeyScriptEvent::kLogCreateRequest);
  }
}

// Tests ParsePasskeyScriptEvent logic for "logGetResolved" based on GPM status.
TEST_F(PasskeyRequestParserTest, ParseLogGetResolvedEvent) {
  base::DictValue dict;
  dict.Set(kEvent, kLogGetResolved);

  // Test case 1: Missing required parameters (rpId, credentialId).
  EXPECT_FALSE(ParsePasskeyScriptEvent(dict, &IsGpmPasskey).has_value());

  dict.Set(kRpId, kExampleRpId);
  dict.Set(kCredentialId, kExampleCredId);

  // Test case 2: Lambda returns TRUE (Credential found in GPM).
  auto result_gpm = ParsePasskeyScriptEvent(
      dict, [](const std::string& rp, const std::string& id) {
        EXPECT_EQ(rp, kExampleRpId);
        EXPECT_EQ(id, kExampleCredId);
        return true;
      });
  ASSERT_TRUE(result_gpm.has_value());
  EXPECT_EQ(*result_gpm, PasskeyScriptEvent::kLogGetResolvedGpm);

  // Test case 3: Lambda returns FALSE (Credential NOT found).
  auto result_non_gpm = ParsePasskeyScriptEvent(
      dict, [](const std::string&, const std::string&) { return false; });
  ASSERT_TRUE(result_non_gpm.has_value());
  EXPECT_EQ(*result_non_gpm, PasskeyScriptEvent::kLogGetResolvedNonGpm);
}

// Tests ParsePasskeyScriptEvent logic for "logCreateResolved" based on the
// "isGpm" flag.
TEST_F(PasskeyRequestParserTest, ParseEventLogCreateResolved) {
  base::DictValue dict;
  dict.Set(kEvent, kLogCreateResolved);

  // Test case 1: Missing "isGpm" parameter.
  EXPECT_FALSE(ParsePasskeyScriptEvent(dict, &IsGpmPasskey).has_value());

  // Test case 2: isGpm = true.
  dict.Set(kIsGpm, true);
  auto result_gpm = ParsePasskeyScriptEvent(
      dict, [](const std::string&, const std::string&) { return false; });
  ASSERT_TRUE(result_gpm.has_value());
  EXPECT_EQ(*result_gpm, PasskeyScriptEvent::kLogCreateResolvedGpm);

  // Test case 3: isGpm = false.
  dict.Set(kIsGpm, false);
  auto result_non_gpm = ParsePasskeyScriptEvent(
      dict, [](const std::string&, const std::string&) { return false; });
  ASSERT_TRUE(result_non_gpm.has_value());
  EXPECT_EQ(*result_non_gpm, PasskeyScriptEvent::kLogCreateResolvedNonGpm);
}

}  // namespace webauthn
