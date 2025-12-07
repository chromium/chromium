// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/secure_payment_confirmation_helper.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_credential_instrument.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_entity_logo.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_secure_payment_confirmation_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

static const char PUBLIC_KEY_CREDENTIAL_TYPE_STRING[] = "public-key";

static const uint8_t kPrfInputData[] = {1, 2, 3, 4, 5, 6};

constexpr char kPngImageDataUrl[] =
    "data:image/png;base64,"
    "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVQYlWNk+M/"
    "wn4GBgYGJAQoAHhgCAh6X4CYAAAAASUVORK5CYII=";

Vector<uint8_t> CreateVector(base::span<const uint8_t> buffer) {
  Vector<uint8_t> vector;
  vector.AppendSpan(buffer);
  return vector;
}

static V8UnionArrayBufferOrArrayBufferView* ArrayBufferOrView(
    const uint8_t* data,
    size_t size) {
  DOMArrayBuffer* dom_array =
      DOMArrayBuffer::Create(UNSAFE_TODO(base::span(data, size)));
  return MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(dom_array);
}

static AuthenticationExtensionsPRFInputs* CreatePrfInputs(
    v8::Isolate* isolate) {
  AuthenticationExtensionsPRFValues* prf_values =
      AuthenticationExtensionsPRFValues::Create(isolate);
  prf_values->setFirst(ArrayBufferOrView(kPrfInputData, sizeof(kPrfInputData)));
  AuthenticationExtensionsPRFInputs* prf_inputs =
      AuthenticationExtensionsPRFInputs::Create(isolate);
  prf_inputs->setEval(prf_values);
  return prf_inputs;
}

// Matches a PublicKeyCredentialParameters with the given type and algorithm.
testing::Matcher<
    const mojo::InlinedStructPtr<mojom::blink::PublicKeyCredentialParameters>>
EqPublicKeyCredentialParameters(mojom::blink::PublicKeyCredentialType type,
                                int32_t algorithm_identifier) {
  return testing::Pointee(testing::AllOf(
      testing::Field("type",
                     &blink::mojom::blink::PublicKeyCredentialParameters::type,
                     type),
      testing::Field("algorithm_identifier",
                     &blink::mojom::blink::PublicKeyCredentialParameters::
                         algorithm_identifier,
                     algorithm_identifier)));
}

}  // namespace

// Test that parsing a valid SecurePaymentConfirmationRequest succeeds and
// correctly copies the fields to the mojo output.
TEST(SecurePaymentConfirmationHelperTest, Parse_Success) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  ::payments::mojom::blink::SecurePaymentConfirmationRequestPtr parsed_request =
      SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
          script_value, *scope.GetExecutionContext(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(parsed_request);

  ASSERT_EQ(parsed_request->credential_ids.size(), 1u);
  EXPECT_EQ(parsed_request->credential_ids[0],
            CreateVector(kSecurePaymentConfirmationCredentialId));
  EXPECT_EQ(parsed_request->challenge,
            CreateVector(kSecurePaymentConfirmationChallenge));
  EXPECT_EQ(parsed_request->instrument->display_name, "My Card");
  EXPECT_EQ(parsed_request->instrument->icon.GetString(),
            "https://bank.example/icon.png");
  EXPECT_EQ(parsed_request->payee_name, "Merchant Shop");
  EXPECT_EQ(parsed_request->rp_id, "bank.example");
  EXPECT_TRUE(parsed_request->extensions.is_null());
}

// Test that optional fields are correctly copied to the mojo output.
TEST(SecurePaymentConfirmationHelperTest, Parse_OptionalFields) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);
  request->instrument()->setDetails("instrument details");
  request->setPayeeOrigin("https://merchant.example");
  request->setTimeout(5 * 60 * 1000);  // 5 minutes

  PaymentEntityLogo* logo1 = PaymentEntityLogo::Create(scope.GetIsolate());
  logo1->setUrl("https://entity1.example/icon.png");
  logo1->setLabel("Label 1");
  PaymentEntityLogo* logo2 = PaymentEntityLogo::Create(scope.GetIsolate());
  logo2->setUrl(kPngImageDataUrl);
  logo2->setLabel("Label 2");
  request->setPaymentEntitiesLogos({logo1, logo2});

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  ::payments::mojom::blink::SecurePaymentConfirmationRequestPtr parsed_request =
      SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
          script_value, *scope.GetExecutionContext(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(parsed_request);

  // Instrument details is behind a default-disabled flag; however, when set
  // directly as above, they will still be present and testable here.
  EXPECT_EQ(parsed_request->instrument->details, "instrument details");
  EXPECT_EQ(parsed_request->payee_origin->ToString(),
            "https://merchant.example");
  EXPECT_EQ(parsed_request->timeout, base::Minutes(5));

  // This field is behind a default-disabled flag, however when set directly
  // into the request as above it will still be present and we can test that the
  // mojo parsing works correctly.
  EXPECT_EQ(parsed_request->payment_entities_logos.size(), 2u);
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// credentialIds field throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyIdCredentialIds) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  HeapVector<Member<V8UnionArrayBufferOrArrayBufferView>> emptyCredentialIds;
  request->setCredentialIds(emptyCredentialIds);

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kRangeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty ID inside
// the credentialIds field throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyId) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  // This credentialIds array contains one valid and one empty ID. The empty one
  // should cause an exception to be thrown.
  HeapVector<Member<V8UnionArrayBufferOrArrayBufferView>> credentialIds;
  credentialIds.push_back(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(kSecurePaymentConfirmationCredentialId)));
  const size_t num_elements = 0;
  const size_t byte_length = 0;
  credentialIds.push_back(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::CreateOrNull(num_elements, byte_length)));
  ASSERT_NE(credentialIds[1], nullptr);  // Make sure the return was non-null.
  request->setCredentialIds(credentialIds);

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kRangeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty challenge
// throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyChallenge) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  const size_t num_elements = 0;
  const size_t byte_length = 0;
  request->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::CreateOrNull(num_elements, byte_length)));
  ASSERT_NE(request->challenge(),
            nullptr);  // Make sure the return was non-null.

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty instrument
// displayName throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyInstrumentDisplayName) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->instrument()->setDisplayName("");

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// instrument icon throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyInstrumentIcon) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->instrument()->setIcon("");

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with an invalid
// instrument icon URL throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_InvalidInstrumentIcon) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->instrument()->setIcon("thisisnotaurl");

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with a detail string
// that is present but empty throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyInstrumentDetails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->instrument()->setDetails("");

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with a detail string
// that is longer than 4K throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_TooLargeInstrumentDetails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->instrument()->setDetails(String::FromUTF8(std::string(4097, '.')));

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with an invalid RP
// domain throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_InvalidRpId) {
  test::TaskEnvironment task_environment;
  const String invalid_cases[] = {
      "",
      "domains cannot have spaces.example",
      "https://bank.example",
      "username:password@bank.example",
      "bank.example/has/a/path",
      "139.56.146.66",
      "9d68:ea08:fc14:d8be:344c:60a0:c4db:e478",
  };
  for (const String& rp_id : invalid_cases) {
    V8TestingScope scope;
    SecurePaymentConfirmationRequest* request =
        CreateSecurePaymentConfirmationRequest(scope);

    request->setRpId(rp_id);

    ScriptValue script_value(scope.GetIsolate(),
                             ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                                 scope.GetScriptState(), request));
    SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
        script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException())
        << "RpId " << rp_id << " did not throw";
    EXPECT_EQ(ESErrorType::kTypeError,
              scope.GetExceptionState().CodeAs<ESErrorType>());
  }
}

// Test that parsing a SecurePaymentConfirmationRequest with neither a payeeName
// or payeeOrigin throws.
TEST(SecurePaymentConfirmationHelperTest,
     Parse_MissingPayeeNameAndPayeeOrigin) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope,
                                             /*include_payee_name=*/false);

  // Validate that the helper method did not include the two fields.
  ASSERT_FALSE(request->hasPayeeName());
  ASSERT_FALSE(request->hasPayeeOrigin());

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty payeeName
// throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyPayeeName) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->setPayeeName("");

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// payeeOrigin throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyPayeeOrigin) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->setPayeeOrigin("");

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with an invalid
// payeeOrigin URL throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_InvalidPayeeOrigin) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->setPayeeOrigin("thisisnotaurl");

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with a non-https
// payeeOrigin URL throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_NotHttpsPayeeOrigin) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->setPayeeOrigin("http://merchant.example");

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that extensions are converted while parsing a
// SecurePaymentConfirmationRequest.
TEST(SecurePaymentConfirmationHelperTest, Parse_Extensions) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);
  AuthenticationExtensionsClientInputs* extensions =
      AuthenticationExtensionsClientInputs::Create(scope.GetIsolate());
  extensions->setPrf(CreatePrfInputs(scope.GetIsolate()));
  request->setExtensions(extensions);
  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));

  ::payments::mojom::blink::SecurePaymentConfirmationRequestPtr parsed_request =
      SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
          script_value, *scope.GetExecutionContext(),
          scope.GetExceptionState());

  ASSERT_FALSE(parsed_request->extensions.is_null());
  Vector<uint8_t> prf_expected = CreateVector(kPrfInputData);
  ASSERT_EQ(parsed_request->extensions->prf_inputs[0]->first, prf_expected);
}

// Test that parsing a SecurePaymentConfirmationRequest with a PaymentEntityLogo
// entry that has an empty url throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyPaymentEntityLogoUrl) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  PaymentEntityLogo* logo = PaymentEntityLogo::Create(scope.GetIsolate());
  logo->setUrl("");
  logo->setLabel("Label");
  request->setPaymentEntitiesLogos({logo});

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with a PaymentEntityLogo
// entry that has an invalid url throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_InvalidPaymentEntityLogoUrl) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  PaymentEntityLogo* logo = PaymentEntityLogo::Create(scope.GetIsolate());
  logo->setUrl("thisisnotaurl");
  logo->setLabel("Label");
  request->setPaymentEntitiesLogos({logo});

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with a PaymentEntityLogo
// entry that has a url with a disallowed protocol throws.
TEST(SecurePaymentConfirmationHelperTest,
     Parse_DisallowedProtocolPaymentEntityLogoUrl) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  PaymentEntityLogo* logo = PaymentEntityLogo::Create(scope.GetIsolate());
  logo->setUrl("blob://blob.foo.com/logo.png");
  logo->setLabel("Label");
  request->setPaymentEntitiesLogos({logo});

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest with a PaymentEntityLogo
// entry that has an empty label throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyPaymentEntityLogoLabel) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  PaymentEntityLogo* logo = PaymentEntityLogo::Create(scope.GetIsolate());
  logo->setUrl("https://entity.example/icon.png");
  logo->setLabel("");
  request->setPaymentEntitiesLogos({logo});

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
      script_value, *scope.GetExecutionContext(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that parsing a SecurePaymentConfirmationRequest converts the browser
// bound public key credential parameters.
TEST(SecurePaymentConfirmationHelperTest, Parse_BrowserBroundPubKeyCredParams) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);
  auto request_cred_params =
      HeapVector<Member<PublicKeyCredentialParameters>>();
  PublicKeyCredentialParameters* cred_param_1 =
      PublicKeyCredentialParameters::Create(scope.GetIsolate());
  cred_param_1->setType(PUBLIC_KEY_CREDENTIAL_TYPE_STRING);
  // See https://www.iana.org/assignments/cose/cose.xhtml for algorithm
  // codes.
  cred_param_1->setAlg(-9); /* -9 is "Unassigned" */
  request_cred_params.push_back(std::move(cred_param_1));
  request->setBrowserBoundPubKeyCredParams(std::move(request_cred_params));

  // browserBoundPubKeyCredParams() are behind the
  // SecurePaymentConfirmationBrowserBoundKeys runtime features flag. This test
  // needs the flag's status at "test" or greater.
  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  ::payments::mojom::blink::SecurePaymentConfirmationRequestPtr parsed_request =
      SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
          script_value, *scope.GetExecutionContext(), ASSERT_NO_EXCEPTION);

  EXPECT_THAT(parsed_request->browser_bound_pub_key_cred_params,
              testing::ElementsAre(EqPublicKeyCredentialParameters(
                  blink::mojom::PublicKeyCredentialType::PUBLIC_KEY, -9)));
}

}  // namespace blink
