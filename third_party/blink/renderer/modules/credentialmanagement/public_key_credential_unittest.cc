// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authenticator_selection_criteria.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity_js_on.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {
namespace {

using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::IsNull;
using testing::Matcher;
using testing::Pointee;
using testing::Pointwise;
using testing::Property;
using WTF::String;

#define SUBTEST(F)                                          \
  {                                                         \
    SCOPED_TRACE(testing::Message() << "<-- invoked here"); \
    F;                                                      \
  }

constexpr char kTestB64URL[] = "dGVzdA";    // 'test' base64-url encoded
constexpr char kTest2B64URL[] = "VEVTVDI";  // 'TEST2' base64-url encoded
constexpr char kInvalidB64URL[] = "invalid base64url string";

// Fields to instantiate PublicKeyCredentialOptionsJSON (non-exhaustive). All
// required fields are default-initialized.
struct CredentialDescriptorValues {
  std::string id = kTestB64URL;
  std::string type = "public-key";
};

// Fields to instantiate AuthenticationExtensionsClientInputsJSON.
struct ExtensionsClientInputsValues {
  std::optional<std::string> appid;
  std::optional<std::string> appid_exclude;
  struct CredProtect {
    std::string credential_protection_policy;
    bool enforce_credential_protection_policy;
  };
  std::optional<CredProtect> cred_protect;
  std::optional<bool> cred_props;
  std::optional<std::string> cred_blob;
  std::optional<bool> get_cred_blob;
  using PRFValues = std::pair<std::string, std::string>;
  std::optional<PRFValues> prf_eval;
  std::optional<std::vector<std::pair<std::string, PRFValues>>>
      prf_eval_by_credential;
};

AuthenticationExtensionsClientInputsJSON* MakeExtensionsInputsJSON(
    const ExtensionsClientInputsValues& in) {
  auto* extensions = AuthenticationExtensionsClientInputsJSON::Create();
  if (in.appid) {
    extensions->setAppid(String(*in.appid));
  }
  if (in.appid_exclude) {
    extensions->setAppidExclude(String(*in.appid_exclude));
  }
  if (in.cred_protect) {
    extensions->setCredentialProtectionPolicy(
        String(in.cred_protect->credential_protection_policy));
    extensions->setEnforceCredentialProtectionPolicy(
        in.cred_protect->enforce_credential_protection_policy);
  }
  if (in.cred_props) {
    extensions->setCredProps(*in.cred_props);
  }
  if (in.cred_blob) {
    extensions->setCredBlob(String(*in.cred_blob));
  }
  if (in.get_cred_blob) {
    extensions->setGetCredBlob(*in.get_cred_blob);
  }
  if (in.prf_eval) {
    auto* prf_inputs = AuthenticationExtensionsPRFInputsJSON::Create();
    auto* prf_values = AuthenticationExtensionsPRFValuesJSON::Create();
    prf_values->setFirst(String(in.prf_eval->first));
    prf_values->setSecond(String(in.prf_eval->second));
    prf_inputs->setEval(prf_values);
    extensions->setPrf(prf_inputs);
  }
  if (in.prf_eval_by_credential) {
    HeapVector<std::pair<String, Member<AuthenticationExtensionsPRFValuesJSON>>>
        prf_values_by_cred;
    for (const auto& cred_and_values : *in.prf_eval_by_credential) {
      auto* prf_values = AuthenticationExtensionsPRFValuesJSON::Create();
      prf_values->setFirst(String(cred_and_values.second.first));
      prf_values->setSecond(String(cred_and_values.second.second));
      prf_values_by_cred.emplace_back(String(cred_and_values.first),
                                      prf_values);
    }
    auto* prf_inputs = AuthenticationExtensionsPRFInputsJSON::Create();
    prf_inputs->setEvalByCredential(prf_values_by_cred);
    extensions->setPrf(prf_inputs);
  }
  return extensions;
}

// Fields to instantiate PublicKeyCredentialCreationOptionsJSON.
struct CredentialCreationOptionsValues {
  std::string rp_id = "example.com";
  std::string user_id = kTestB64URL;
  std::string user_name = "example user";
  std::string pub_key_cred_params_type = "public-key";
  int pub_key_cred_params_alg = -7;
  std::string challenge = kTestB64URL;
  std::optional<uint32_t> timeout;
  std::vector<CredentialDescriptorValues> exclude_credentials;
  struct AuthSelection {
    std::optional<std::string> attachment;
    std::optional<std::string> resident_key;
    bool require_resident_key = false;
    std::string user_verification = "preferred";
  };
  std::optional<AuthSelection> authenticator_selection;
  std::vector<std::string> hints;
  std::string attestation = "none";
  std::optional<ExtensionsClientInputsValues> extensions;
};

PublicKeyCredentialCreationOptionsJSON* MakeCreationOptionsJSON(
    const CredentialCreationOptionsValues& in) {
  auto* json = PublicKeyCredentialCreationOptionsJSON::Create();
  json->setChallenge(String(in.challenge));
  auto* rp = PublicKeyCredentialRpEntity::Create();
  rp->setId(String(in.rp_id));
  json->setRp(rp);
  auto* user = PublicKeyCredentialUserEntityJSON::Create();
  user->setId(String(in.user_id));
  user->setName(String(in.user_name));
  json->setUser(user);
  // Test only supports single pubKeyCredParams.
  auto* pub_key_cred_params = PublicKeyCredentialParameters::Create();
  pub_key_cred_params->setType(String(in.pub_key_cred_params_type));
  pub_key_cred_params->setAlg(in.pub_key_cred_params_alg);
  json->setPubKeyCredParams(
      VectorOf<PublicKeyCredentialParameters>({pub_key_cred_params}));
  if (in.timeout) {
    json->setTimeout(*in.timeout);
  }
  VectorOf<PublicKeyCredentialDescriptorJSON> exclude_credentials;
  for (const CredentialDescriptorValues& desc : in.exclude_credentials) {
    auto* desc_json = PublicKeyCredentialDescriptorJSON::Create();
    desc_json->setId(String(desc.id));
    desc_json->setType(String(desc.type));
    exclude_credentials.push_back(desc_json);
  }
  json->setExcludeCredentials(std::move(exclude_credentials));
  if (in.authenticator_selection) {
    auto* authenticator_selection = AuthenticatorSelectionCriteria::Create();
    if (in.authenticator_selection->attachment) {
      authenticator_selection->setAuthenticatorAttachment(
          String(*in.authenticator_selection->attachment));
    }
    if (in.authenticator_selection->resident_key) {
      authenticator_selection->setResidentKey(
          String(*in.authenticator_selection->resident_key));
    }
    authenticator_selection->setRequireResidentKey(
        in.authenticator_selection->require_resident_key);
    authenticator_selection->setUserVerification(
        String(in.authenticator_selection->user_verification));
    json->setAuthenticatorSelection(authenticator_selection);
  }
  VectorOf<String> hints;
  for (const std::string& hint : in.hints) {
    hints.push_back(String(hint));
  }
  json->setHints(hints);
  json->setAttestation(String(in.attestation));
  if (in.extensions) {
    json->setExtensions(MakeExtensionsInputsJSON(*in.extensions));
  }
  return json;
}

// Fields to instantiate PublicKeyCredentialRequestOptionsJSON.
struct CredentialRequestOptionsValues {
  std::string challenge = kTestB64URL;
  std::optional<uint32_t> timeout;
  std::optional<std::string> rp_id;
  std::vector<CredentialDescriptorValues> allow_credentials;
  std::string user_verification = "preferred";
  std::vector<std::string> hints;
  std::optional<ExtensionsClientInputsValues> extensions;
};

PublicKeyCredentialRequestOptionsJSON* MakeRequestOptionsJSON(
    const CredentialRequestOptionsValues& in) {
  auto* json = PublicKeyCredentialRequestOptionsJSON::Create();
  json->setChallenge(String(in.challenge));
  if (in.timeout) {
    json->setTimeout(*in.timeout);
  }
  if (in.rp_id) {
    json->setRpId(String(*in.rp_id));
  }
  VectorOf<PublicKeyCredentialDescriptorJSON> allow_credentials;
  for (const CredentialDescriptorValues& desc : in.allow_credentials) {
    auto* desc_json = PublicKeyCredentialDescriptorJSON::Create();
    desc_json->setId(String(desc.id));
    desc_json->setType(String(desc.type));
    allow_credentials.emplace_back(desc_json);
  }
  json->setAllowCredentials(std::move(allow_credentials));
  json->setUserVerification(String(in.user_verification));
  VectorOf<String> hints;
  for (const std::string& hint : in.hints) {
    hints.push_back(String(hint));
  }
  json->setHints(hints);
  if (in.extensions) {
    json->setExtensions(MakeExtensionsInputsJSON(*in.extensions));
  }
  return json;
}

// Matches a blink WTF::String and a std::string for byte equality.
MATCHER_P(StrEq, str, "") {
  return ExplainMatchResult(Eq(String(str)), arg, result_listener);
}

// Matches a pair of (WTF::String, std::string) for equality (used with
// `testing::Pointwise`).
MATCHER(StrEq, "") {
  const String& s1 = std::get<0>(arg);
  const std::string& s2 = std::get<1>(arg);
  return ExplainMatchResult(Eq(String(s2)), s1, result_listener);
}

// Matches the underlying `T` pointee of a `blink::Member<T>`.
template <typename T>
Matcher<Member<T>> MemberField(Matcher<T*> matcher) {
  return Property("Get", &Member<T>::Get, matcher);
}

// Performs WebAuthn Base64URL encoding, which is always unpadded.
WTF::String Base64URLEncode(DOMArrayPiece buffer) {
  // WTF::Base64URLEncode always pads, so we strip trailing '='.
  String encoded = WTF::Base64URLEncode(buffer.ByteSpan());
  unsigned padding_start = encoded.length();
  for (; padding_start > 0; --padding_start) {
    if (encoded[padding_start - 1] != '=') {
      break;
    }
  }
  encoded.Truncate(padding_start);
  return encoded;
}

// Matches the Base64URL-encoding of the byte contents of a DOMArrayPiece.
MATCHER_P(Base64URL, matcher, "") {
  String encoded = Base64URLEncode(arg);
  return ExplainMatchResult(matcher, encoded, result_listener);
}

// Matches a pair of `Member<PublicKeyCredentialDescriptor>` and
// `CredentialDescriptorValues`. (Use with `testing::Pointwise`).
MATCHER(CredentialDescriptorsEq, "") {
  const Member<PublicKeyCredentialDescriptor>& desc = std::get<0>(arg);
  const CredentialDescriptorValues& values = std::get<1>(arg);
  return ExplainMatchResult(
      MemberField<PublicKeyCredentialDescriptor>(
          AllOf(Property("id", &PublicKeyCredentialDescriptor::id,
                         Base64URL(StrEq(values.id))),
                Property("type", &PublicKeyCredentialDescriptor::type,
                         StrEq(values.type)))),
      desc, result_listener);
}

// Matches `PublicKeyCredentialParameters`.
MATCHER_P2(PubKeyCredParamsEq, type, alg, "") {
  return arg->type() == String(type) && arg->alg() == alg;
}

// Matches `AuthenticationExtensionsPRFValues`.
MATCHER_P(PRFValuesEq, values, "") {
  return ExplainMatchResult(
      AllOf(Property(&AuthenticationExtensionsPRFValues::first,
                     Base64URL(StrEq(values.first))),
            Property(&AuthenticationExtensionsPRFValues::second,
                     Base64URL(StrEq(values.second)))),
      arg, result_listener);
}

// Matches `AuthenticationExtensionsPRFInputs::evalByCredential()`.
MATCHER(PRFCredIdAndValuesEq, "") {
  const std::pair<String, Member<AuthenticationExtensionsPRFValues>>& actual =
      std::get<0>(arg);
  const std::pair<std::string, ExtensionsClientInputsValues::PRFValues>&
      expected = std::get<1>(arg);
  return ExplainMatchResult(
      AllOf(Field(&std::pair<String,
                             Member<AuthenticationExtensionsPRFValues>>::first,
                  StrEq(expected.first)),
            Field(&std::pair<String,
                             Member<AuthenticationExtensionsPRFValues>>::second,
                  MemberField<AuthenticationExtensionsPRFValues>(
                      PRFValuesEq(expected.second)))),
      actual, result_listener);
}

// Tests `AuthenticationExtensionsClientInputs` and
// `ExtensionsClientInputsValues` for equality. Invoke with SUBTEST().
void ExpectExtensionsMatch(
    const AuthenticationExtensionsClientInputs& extensions,
    const ExtensionsClientInputsValues& values) {
  if (values.appid) {
    EXPECT_THAT(extensions.appid(), StrEq(*values.appid));
  } else {
    EXPECT_FALSE(extensions.hasAppid());
  }
  if (values.appid_exclude) {
    EXPECT_THAT(extensions.appidExclude(), StrEq(*values.appid_exclude));
  } else {
    EXPECT_FALSE(extensions.hasAppidExclude());
  }
  if (values.cred_protect) {
    EXPECT_THAT(extensions.credentialProtectionPolicy(),
                StrEq(values.cred_protect->credential_protection_policy));
    EXPECT_EQ(extensions.enforceCredentialProtectionPolicy(),
              values.cred_protect->enforce_credential_protection_policy);
  } else {
    EXPECT_FALSE(extensions.hasCredentialProtectionPolicy());
    EXPECT_FALSE(
        extensions.enforceCredentialProtectionPolicy());  // defaults to 'false'
  }
  if (values.cred_props) {
    EXPECT_EQ(extensions.credProps(), *values.cred_props);
  } else {
    EXPECT_FALSE(extensions.credProps());  // defaults to 'false'
  }
  if (values.cred_blob) {
    EXPECT_THAT(extensions.credBlob(), Base64URL(StrEq(*values.cred_blob)));
  } else {
    EXPECT_FALSE(extensions.hasCredBlob());
  }
  if (values.get_cred_blob) {
    EXPECT_EQ(extensions.getCredBlob(), *values.get_cred_blob);
  } else {
    EXPECT_FALSE(extensions.hasGetCredBlob());
  }
  if (values.prf_eval) {
    EXPECT_THAT(extensions.prf()->eval(),
                MemberField<AuthenticationExtensionsPRFValues>(
                    PRFValuesEq(*values.prf_eval)));
  } else {
    EXPECT_TRUE(!extensions.hasPrf() || !extensions.prf()->hasEval());
  }
  if (values.prf_eval_by_credential) {
    EXPECT_THAT(
        extensions.prf()->evalByCredential(),
        Pointwise(PRFCredIdAndValuesEq(), *values.prf_eval_by_credential));
  } else {
    EXPECT_TRUE(!extensions.hasPrf() ||
                !extensions.prf()->hasEvalByCredential());
  }
  EXPECT_EQ(extensions.hasPrf(),
            values.prf_eval || values.prf_eval_by_credential);
}

// Tests `PublicKeyCredentialCreationOptions` and `CreationOptionsValues` for
// equality. Invoke with SUBTEST().
void ExpectCreationOptionsMatches(
    const PublicKeyCredentialCreationOptions& options,
    const CredentialCreationOptionsValues& values) {
  EXPECT_THAT(options.rp()->id(), StrEq(values.rp_id));
  EXPECT_THAT(options.user(),
              AllOf(Property("id", &PublicKeyCredentialUserEntity::id,
                             Base64URL(StrEq(values.user_id))),
                    Property("name", &PublicKeyCredentialUserEntity::name,
                             StrEq(values.user_name))));
  EXPECT_THAT(options.pubKeyCredParams(),
              testing::ElementsAre(MemberField<PublicKeyCredentialParameters>(
                  PubKeyCredParamsEq(values.pub_key_cred_params_type,
                                     values.pub_key_cred_params_alg))));
  EXPECT_THAT(options.challenge(), Base64URL(StrEq(values.challenge)));
  if (values.timeout.has_value()) {
    EXPECT_EQ(options.timeout(), *values.timeout);
  } else {
    EXPECT_FALSE(options.hasTimeout());
  }
  EXPECT_THAT(options.excludeCredentials(),
              Pointwise(CredentialDescriptorsEq(), values.exclude_credentials));
  if (values.authenticator_selection) {
    if (values.authenticator_selection->attachment) {
      EXPECT_THAT(options.authenticatorSelection()->authenticatorAttachment(),
                  StrEq(*values.authenticator_selection->attachment));
    } else {
      EXPECT_FALSE(
          options.authenticatorSelection()->hasAuthenticatorAttachment());
    }
    if (values.authenticator_selection->resident_key) {
      EXPECT_THAT(options.authenticatorSelection()->residentKey(),
                  StrEq(*values.authenticator_selection->resident_key));
    } else {
      EXPECT_FALSE(options.authenticatorSelection()->hasResidentKey());
    }
    EXPECT_EQ(options.authenticatorSelection()->requireResidentKey(),
              values.authenticator_selection->require_resident_key);
    EXPECT_THAT(options.authenticatorSelection()->userVerification(),
                StrEq(values.authenticator_selection->user_verification));
  } else {
    EXPECT_FALSE(options.hasAuthenticatorSelection());
  }
  EXPECT_THAT(options.hints(), Pointwise(StrEq(), values.hints));
  EXPECT_THAT(options.attestation(), StrEq(values.attestation));
  if (values.extensions.has_value()) {
    EXPECT_TRUE(options.hasExtensions());
    SUBTEST(ExpectExtensionsMatch(*options.extensions(), *values.extensions));
  } else {
    EXPECT_FALSE(options.hasExtensions());
  }
}

// Tests `PublicKeyCredentialRequestOptions` and `RequestOptionsValues` for
// equality. Invoke with SUBTEST().
void ExpectRequestOptionsMatches(
    const PublicKeyCredentialRequestOptions& options,
    const CredentialRequestOptionsValues& values) {
  EXPECT_THAT(options.challenge(), Base64URL(StrEq(values.challenge)));
  if (values.timeout.has_value()) {
    EXPECT_EQ(options.timeout(), *values.timeout);
  } else {
    EXPECT_FALSE(options.hasTimeout());
  }
  if (values.rp_id) {
    EXPECT_THAT(options.rpId(), StrEq(*values.rp_id));
  } else {
    EXPECT_FALSE(options.hasRpId());
  }
  EXPECT_THAT(options.allowCredentials(),
              Pointwise(CredentialDescriptorsEq(), values.allow_credentials));
  EXPECT_THAT(options.userVerification(), StrEq(values.user_verification));
  EXPECT_THAT(options.hints(), Pointwise(StrEq(), values.hints));
  if (values.extensions.has_value()) {
    EXPECT_TRUE(options.hasExtensions());
    SUBTEST(ExpectExtensionsMatch(*options.extensions(), *values.extensions));
  } else {
    EXPECT_FALSE(options.hasExtensions());
  }
}

// Test parseCreationOptionsFromJSON with minimal fields.
TEST(PublicKeyCredentialTest, ParseCreationOptionsFromJSON) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
  CredentialCreationOptionsValues options_values{};
  PublicKeyCredentialCreationOptionsJSON* json =
      MakeCreationOptionsJSON(options_values);
  DummyExceptionStateForTesting exception_state;
  const PublicKeyCredentialCreationOptions* options =
      PublicKeyCredential::parseCreationOptionsFromJSON(script_state, json,
                                                        exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_NE(options, nullptr);
  SUBTEST(ExpectCreationOptionsMatches(*options, options_values))
}

// Test parseRequestOptionsFromJSON with minimal fields.
TEST(PublicKeyCredentialTest, ParseRequestOptions) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
  CredentialRequestOptionsValues options_values{};
  PublicKeyCredentialRequestOptionsJSON* json =
      MakeRequestOptionsJSON(options_values);
  DummyExceptionStateForTesting exception_state;
  const PublicKeyCredentialRequestOptions* options =
      PublicKeyCredential::parseRequestOptionsFromJSON(script_state, json,
                                                       exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_NE(options, nullptr);
  SUBTEST(ExpectRequestOptionsMatches(*options, options_values));
}

// Test parseCreationOptionsFromJSON with all fields.
TEST(PublicKeyCredentialTest, ParseCreationOptionsFromJSON_Full) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
  CredentialCreationOptionsValues options_values{
      .timeout = 123,
      .exclude_credentials = {{.id = kTestB64URL}, {.id = kTest2B64URL}},
      .authenticator_selection =
          CredentialCreationOptionsValues::AuthSelection{
              .attachment = "cross-platform",
              .resident_key = "required",
              .require_resident_key = true,
              .user_verification = "required"},
      .hints = {"security-key"},
      .attestation = "required",
  };
  PublicKeyCredentialCreationOptionsJSON* json =
      MakeCreationOptionsJSON(options_values);
  DummyExceptionStateForTesting exception_state;
  const PublicKeyCredentialCreationOptions* options =
      PublicKeyCredential::parseCreationOptionsFromJSON(script_state, json,
                                                        exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_NE(options, nullptr);
  SUBTEST(ExpectCreationOptionsMatches(*options, options_values));
}

// Test parseRequestOptionsFromJSON with all fields.
TEST(PublicKeyCredentialTest, ParseRequestOptionsFromJSON_Full) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
  CredentialRequestOptionsValues options_values{
      .timeout = 123,
      .rp_id = "example.com",
      .allow_credentials = {{.id = kTestB64URL}, {.id = kTest2B64URL}},
      .user_verification = "required",
      .hints = {"security-key"},
  };
  PublicKeyCredentialRequestOptionsJSON* json =
      MakeRequestOptionsJSON(options_values);
  DummyExceptionStateForTesting exception_state;
  const PublicKeyCredentialRequestOptions* options =
      PublicKeyCredential::parseRequestOptionsFromJSON(script_state, json,
                                                       exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_NE(options, nullptr);
  SUBTEST(ExpectRequestOptionsMatches(*options, options_values));
}

// PublicKeyCredentialCreationOptions extensions should convert as expected.
TEST(PublicKeyCredentialTest, ParseCreationOptionsFromJSON_WithExtensions) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
  static const ExtensionsClientInputsValues::PRFValues kTestPRFValues{
      kTestB64URL, kTest2B64URL};
  static const ExtensionsClientInputsValues kTestCases[] = {
      {.appid_exclude = "https://example.com/appid.json"},
      {.cred_protect =
           ExtensionsClientInputsValues::CredProtect{"Level One", true}},
      {.cred_props = true},
      {.cred_blob = kTestB64URL},
      {.prf_eval = kTestPRFValues},
  };
  for (const auto& ext : kTestCases) {
    CredentialCreationOptionsValues options_values{.extensions = ext};
    PublicKeyCredentialCreationOptionsJSON* json =
        MakeCreationOptionsJSON(options_values);
    DummyExceptionStateForTesting exception_state;
    const PublicKeyCredentialCreationOptions* options =
        PublicKeyCredential::parseCreationOptionsFromJSON(script_state, json,
                                                          exception_state);
    EXPECT_FALSE(exception_state.HadException());
    ASSERT_NE(options, nullptr);
    SUBTEST(ExpectCreationOptionsMatches(*options, options_values))
  }
}

// PublicKeyCredentialRequestOptions extensions should convert as expected.
TEST(PublicKeyCredentialTest, ParseRequestOptionsFromJSON_WithExtensions) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
  static const ExtensionsClientInputsValues::PRFValues kTestPRFValues{
      kTestB64URL, kTest2B64URL};
  static const ExtensionsClientInputsValues kTestCases[] = {
      {.appid = "https://example.com/appid.json"},
      {.get_cred_blob = true},
      {.prf_eval_by_credential = {{std::make_pair("ABEiMw", kTestPRFValues)}}},
  };
  for (const auto& ext : kTestCases) {
    CredentialRequestOptionsValues options_values{.extensions = ext};
    PublicKeyCredentialRequestOptionsJSON* json =
        MakeRequestOptionsJSON(options_values);
    DummyExceptionStateForTesting exception_state;
    const PublicKeyCredentialRequestOptions* options =
        PublicKeyCredential::parseRequestOptionsFromJSON(script_state, json,
                                                         exception_state);
    EXPECT_FALSE(exception_state.HadException());
    ASSERT_NE(options, nullptr);
    SUBTEST(ExpectRequestOptionsMatches(*options, options_values));
  }
}

// Parsing PublicKeyCredentialCreationOptionsJSON with invalid base64url data
// should yield meaningful error messages.
TEST(PublicKeyCredentialTest, ParseCreationOptionsFromJSON_InvalidBase64URL) {
  test::TaskEnvironment task_environment;
  static const struct {
    CredentialCreationOptionsValues in;
    std::string expected_message;
  } kTestCases[] = {
      {{.user_id = kInvalidB64URL},
       "'user.id' contains invalid base64url data"},
      {{.challenge = kInvalidB64URL},
       "'challenge' contains invalid base64url data"},
      {{.exclude_credentials = {{.id = kInvalidB64URL}}},
       "'excludeCredentials' contains PublicKeyCredentialDescriptorJSON with "
       "invalid base64url data in 'id'"},
  };
  for (const auto& t : kTestCases) {
    SCOPED_TRACE(testing::Message() << t.expected_message);
    PublicKeyCredentialCreationOptionsJSON* json =
        MakeCreationOptionsJSON(t.in);
    DummyPageHolder holder;
    ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
    DummyExceptionStateForTesting exception_state;
    const PublicKeyCredentialCreationOptions* options =
        PublicKeyCredential::parseCreationOptionsFromJSON(script_state, json,
                                                          exception_state);

    EXPECT_EQ(options, nullptr);
    ASSERT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Message().Utf8(), t.expected_message);
  }
}

// Parsing PublicKeyCredentialRequestOptionsJSON with invalid base64url data
// should yield meaningful error messages.
TEST(PublicKeyCredentialTest, ParseRequestOptionsFromJSON_InvalidBase64URL) {
  test::TaskEnvironment task_environment;
  static const struct {
    CredentialRequestOptionsValues in;
    std::string expected_message;
  } kTestCases[] = {
      {{.challenge = kInvalidB64URL},
       "'challenge' contains invalid base64url data"},
      {{.allow_credentials = {{.id = kInvalidB64URL}}},
       "'allowCredentials' contains PublicKeyCredentialDescriptorJSON with "
       "invalid base64url data in 'id'"},
  };
  for (const auto& t : kTestCases) {
    SCOPED_TRACE(testing::Message() << t.expected_message);
    PublicKeyCredentialRequestOptionsJSON* json = MakeRequestOptionsJSON(t.in);
    DummyPageHolder holder;
    ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
    DummyExceptionStateForTesting exception_state;
    const PublicKeyCredentialRequestOptions* options =
        PublicKeyCredential::parseRequestOptionsFromJSON(script_state, json,
                                                         exception_state);

    EXPECT_EQ(options, nullptr);
    ASSERT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Message().Utf8(), t.expected_message);
  }
}

}  // namespace
}  // namespace blink
