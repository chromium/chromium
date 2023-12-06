// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/secure_payment_confirmation_helper.h"

#include "base/time/time.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_credential_instrument.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_secure_payment_confirmation_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

static const uint8_t kPrfInputData[] = {1, 2, 3, 4, 5, 6};

WTF::Vector<uint8_t> CreateVector(const uint8_t* buffer,
                                  const unsigned length) {
  WTF::Vector<uint8_t> vector;
  vector.Append(buffer, length);
  return vector;
}

static V8UnionArrayBufferOrArrayBufferView* ArrayBufferOrView(
    const uint8_t* data,
    size_t size) {
  DOMArrayBuffer* dom_array = DOMArrayBuffer::Create(data, size);
  return MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      std::move(dom_array));
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

}  // namespace

// Test that parsing a valid SecurePaymentConfirmationRequest succeeds and
// correctly copies the fields to the mojo output.
TEST(SecurePaymentConfirmationHelperTest, Parse_Success) {
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
            CreateVector(kSecurePaymentConfirmationCredentialId,
                         std::size(kSecurePaymentConfirmationCredentialId)));
  EXPECT_EQ(parsed_request->challenge,
            CreateVector(kSecurePaymentConfirmationChallenge,
                         std::size(kSecurePaymentConfirmationChallenge)));
  EXPECT_EQ(parsed_request->instrument->display_name, "My Card");
  EXPECT_EQ(parsed_request->instrument->icon.GetString(),
            "https://bank.example/icon.png");
  EXPECT_EQ(parsed_request->payee_name, "Merchant Shop");
  EXPECT_EQ(parsed_request->rp_id, "bank.example");
  EXPECT_TRUE(parsed_request->extensions.is_null());
}

// Test that optional fields are correctly copied to the mojo output.
TEST(SecurePaymentConfirmationHelperTest, Parse_OptionalFields) {
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  request->setPayeeOrigin("https://merchant.example");
  request->setTimeout(5 * 60 * 1000);  // 5 minutes

  ScriptValue script_value(scope.GetIsolate(),
                           ToV8Traits<SecurePaymentConfirmationRequest>::ToV8(
                               scope.GetScriptState(), request));
  ::payments::mojom::blink::SecurePaymentConfirmationRequestPtr parsed_request =
      SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
          script_value, *scope.GetExecutionContext(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(parsed_request);

  EXPECT_EQ(parsed_request->payee_origin->ToString(),
            "https://merchant.example");
  EXPECT_EQ(parsed_request->timeout, base::Minutes(5));
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// credentialIds field throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyIdCredentialIds) {
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
  V8TestingScope scope;
  SecurePaymentConfirmationRequest* request =
      CreateSecurePaymentConfirmationRequest(scope);

  // This credentialIds array contains one valid and one empty ID. The empty one
  // should cause an exception to be thrown.
  HeapVector<Member<V8UnionArrayBufferOrArrayBufferView>> credentialIds;
  credentialIds.push_back(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(
              kSecurePaymentConfirmationCredentialId,
              std::size(kSecurePaymentConfirmationCredentialId))));
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

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// displayName throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyDisplayName) {
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
// icon throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_EmptyIcon) {
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

// Test that parsing a SecurePaymentConfirmationRequest with an invalid icon URL
// throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_InvalidIcon) {
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

// Test that parsing a SecurePaymentConfirmationRequest with an invalid RP
// domain throws.
TEST(SecurePaymentConfirmationHelperTest, Parse_InvalidRpId) {
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
  WTF::Vector<uint8_t> prf_expected =
      CreateVector(kPrfInputData, sizeof(kPrfInputData));
  ASSERT_EQ(parsed_request->extensions->prf_inputs[0]->first, prf_expected);
}

}  // namespace blink
