// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/payments/payment_response.h"

#include <memory>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_complete.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_validation_errors.h"
#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"
#include "third_party/blink/renderer/modules/payments/payment_address.h"
#include "third_party/blink/renderer/modules/payments/payment_state_resolver.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class MockPaymentStateResolver final
    : public GarbageCollected<MockPaymentStateResolver>,
      public PaymentStateResolver {
 public:
  MockPaymentStateResolver() {
    ON_CALL(*this, Complete(testing::_, testing::_, testing::_))
        .WillByDefault(testing::ReturnPointee(&dummy_promise_));
  }

  MockPaymentStateResolver(const MockPaymentStateResolver&) = delete;
  MockPaymentStateResolver& operator=(const MockPaymentStateResolver&) = delete;

  ~MockPaymentStateResolver() override = default;

  MOCK_METHOD3(Complete,
               ScriptPromise<IDLUndefined>(ScriptState*,
                                           PaymentComplete result,
                                           ExceptionState&));
  MOCK_METHOD3(
      Retry,
      ScriptPromise<IDLUndefined>(ScriptState*,
                                  const PaymentValidationErrors* errorFields,
                                  ExceptionState&));

  void Trace(Visitor* visitor) const override {
    visitor->Trace(dummy_promise_);
  }

 private:
  ScriptPromise<IDLUndefined> dummy_promise_;
};

TEST(PaymentResponseTest, DataCopiedOver) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      BuildPaymentResponseForTest();
  input->method_name = "foo";
  input->stringified_details = "{\"transactionId\": 123}";
  input->shipping_option = "standardShippingOption";
  input->payer->name = "Jon Doe";
  input->payer->email = "abc@gmail.com";
  input->payer->phone = "0123";
  MockPaymentStateResolver* complete_callback =
      MakeGarbageCollected<MockPaymentStateResolver>();

  PaymentResponse* output = MakeGarbageCollected<PaymentResponse>(
      scope.GetScriptState(), std::move(input), nullptr, complete_callback,
      "id");

  EXPECT_EQ("foo", output->methodName());
  EXPECT_EQ("standardShippingOption", output->shippingOption());
  EXPECT_EQ("Jon Doe", output->payerName());
  EXPECT_EQ("abc@gmail.com", output->payerEmail());
  EXPECT_EQ("0123", output->payerPhone());
  EXPECT_EQ("id", output->requestId());

  ScriptValue details = output->details(scope.GetScriptState());

  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_TRUE(details.V8Value()->IsObject());

  ScriptValue transaction_id(
      scope.GetIsolate(),
      details.V8Value()
          .As<v8::Object>()
          ->Get(scope.GetContext(),
                V8String(scope.GetIsolate(), "transactionId"))
          .ToLocalChecked());

  ASSERT_TRUE(transaction_id.V8Value()->IsNumber());
  EXPECT_EQ(123, transaction_id.V8Value().As<v8::Number>()->Value());
}

MATCHER_P(ArrayBufferEqualTo, other_buffer, "equal to") {
  if (arg->ByteLength() != std::size(other_buffer)) {
    return false;
  }

  uint8_t* data = (uint8_t*)arg->Data();
  return std::equal(data, data + arg->ByteLength(), std::begin(other_buffer));
}

// Calls getClientExtensionResults on the given public_key_credential.
static v8::Local<v8::Object> GetClientExtensionResults(
    V8TestingScope& scope,
    v8::Local<v8::Object> public_key_credential) {
  v8::Local<v8::Function> get_client_extension_results_method =
      public_key_credential.As<v8::Object>()
          ->Get(scope.GetContext(),
                V8String(scope.GetIsolate(), "getClientExtensionResults"))
          .ToLocalChecked()
          .As<v8::Function>();
  return get_client_extension_results_method
      ->Call(scope.GetContext(), public_key_credential,
             /*argc=*/0,
             /*argv=*/nullptr)
      .ToLocalChecked()
      .As<v8::Object>();
}

// Gets a v8 object property of array_buffer type.
static v8::Local<v8::ArrayBuffer> GetArrayBuffer(V8TestingScope& scope,
                                                 v8::Local<v8::Object>& object,
                                                 const char* property_key) {
  return object
      ->Get(scope.GetContext(), V8String(scope.GetIsolate(), property_key))
      .ToLocalChecked()
      .As<v8::ArrayBuffer>();
}

TEST(PaymentResponseTest, PaymentResponseDetailsContainsSpcExtensionsPRF) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      BuildPaymentResponseForTest();
  input->get_assertion_authenticator_response =
      blink::mojom::blink::GetAssertionAuthenticatorResponse::New();
  input->get_assertion_authenticator_response->info =
      blink::mojom::blink::CommonCredentialInfo::New();
  input->get_assertion_authenticator_response->info->id = "rpid";
  input->get_assertion_authenticator_response->extensions =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  input->get_assertion_authenticator_response->extensions->echo_prf = true;
  input->get_assertion_authenticator_response->extensions->prf_results =
      mojom::blink::PRFValues::New(
          /*id=*/std::nullopt,
          /*first=*/WTF::Vector<uint8_t>{1, 2, 3},
          /*second=*/WTF::Vector<uint8_t>{4, 5, 6});
  MockPaymentStateResolver* complete_callback =
      MakeGarbageCollected<MockPaymentStateResolver>();

  PaymentResponse* output = MakeGarbageCollected<PaymentResponse>(
      scope.GetScriptState(), std::move(input), /*shipping_address=*/nullptr,
      complete_callback, "request_id");

  v8::Local<v8::Object> details =
      output->details(scope.GetScriptState()).V8Value().As<v8::Object>();
  v8::Local<v8::Object> prf =
      GetClientExtensionResults(scope, details)
          ->Get(scope.GetContext(), V8String(scope.GetIsolate(), "prf"))
          .ToLocalChecked()
          .As<v8::Object>();
  v8::Local<v8::Object> results =
      prf->Get(scope.GetContext(), V8String(scope.GetIsolate(), "results"))
          .ToLocalChecked()
          .As<v8::Object>();
  EXPECT_THAT(GetArrayBuffer(scope, results, "first"),
              ArrayBufferEqualTo(WTF::Vector{1, 2, 3}));
  EXPECT_THAT(GetArrayBuffer(scope, results, "second"),
              ArrayBufferEqualTo(WTF::Vector{4, 5, 6}));
}

TEST(PaymentResponseTest,
     PaymentResponseDetailsWithUnexpectedJSONFormatString) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      BuildPaymentResponseForTest();
  input->stringified_details = "transactionId";
  MockPaymentStateResolver* complete_callback =
      MakeGarbageCollected<MockPaymentStateResolver>();
  PaymentResponse* output = MakeGarbageCollected<PaymentResponse>(
      scope.GetScriptState(), std::move(input), nullptr, complete_callback,
      "id");

  ScriptValue details = output->details(scope.GetScriptState());
  ASSERT_TRUE(details.V8Value()->IsObject());

  String stringified_details = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          details.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  EXPECT_EQ("{}", stringified_details);
}

TEST(PaymentResponseTest, PaymentResponseDetailsRetrunsTheSameObject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      BuildPaymentResponseForTest();
  input->method_name = "foo";
  input->stringified_details = "{\"transactionId\": 123}";
  MockPaymentStateResolver* complete_callback =
      MakeGarbageCollected<MockPaymentStateResolver>();
  PaymentResponse* output = MakeGarbageCollected<PaymentResponse>(
      scope.GetScriptState(), std::move(input), nullptr, complete_callback,
      "id");
  EXPECT_EQ(output->details(scope.GetScriptState()),
            output->details(scope.GetScriptState()));
}

TEST(PaymentResponseTest, CompleteCalledWithSuccess) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      BuildPaymentResponseForTest();
  input->method_name = "foo";
  input->stringified_details = "{\"transactionId\": 123}";
  MockPaymentStateResolver* complete_callback =
      MakeGarbageCollected<MockPaymentStateResolver>();
  PaymentResponse* output = MakeGarbageCollected<PaymentResponse>(
      scope.GetScriptState(), std::move(input), nullptr, complete_callback,
      "id");

  EXPECT_CALL(*complete_callback,
              Complete(scope.GetScriptState(), PaymentStateResolver::kSuccess,
                       testing::_));

  output->complete(scope.GetScriptState(),
                   V8PaymentComplete(V8PaymentComplete::Enum::kSuccess),
                   scope.GetExceptionState());
}

TEST(PaymentResponseTest, CompleteCalledWithFailure) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      BuildPaymentResponseForTest();
  input->method_name = "foo";
  input->stringified_details = "{\"transactionId\": 123}";
  MockPaymentStateResolver* complete_callback =
      MakeGarbageCollected<MockPaymentStateResolver>();
  PaymentResponse* output = MakeGarbageCollected<PaymentResponse>(
      scope.GetScriptState(), std::move(input), nullptr, complete_callback,
      "id");

  EXPECT_CALL(*complete_callback,
              Complete(scope.GetScriptState(), PaymentStateResolver::kFail,
                       testing::_));

  output->complete(scope.GetScriptState(),
                   V8PaymentComplete(V8PaymentComplete::Enum::kFail),
                   scope.GetExceptionState());
}

TEST(PaymentResponseTest, JSONSerializerTest) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      BuildPaymentResponseForTest();
  input->method_name = "foo";
  input->stringified_details = "{\"transactionId\": 123}";
  input->shipping_option = "standardShippingOption";
  input->payer->email = "abc@gmail.com";
  input->payer->phone = "0123";
  input->payer->name = "Jon Doe";
  input->shipping_address = payments::mojom::blink::PaymentAddress::New();
  input->shipping_address->country = "US";
  input->shipping_address->address_line.push_back("340 Main St");
  input->shipping_address->address_line.push_back("BIN1");
  input->shipping_address->address_line.push_back("First floor");
  PaymentAddress* address =
      MakeGarbageCollected<PaymentAddress>(std::move(input->shipping_address));

  PaymentResponse* output = MakeGarbageCollected<PaymentResponse>(
      scope.GetScriptState(), std::move(input), address,
      MakeGarbageCollected<MockPaymentStateResolver>(), "id");
  ScriptValue json_object = output->toJSONForBinding(scope.GetScriptState());
  EXPECT_TRUE(json_object.IsObject());

  String json_string = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);
  String expected =
      "{\"requestId\":\"id\",\"methodName\":\"foo\",\"details\":{"
      "\"transactionId\":123},"
      "\"shippingAddress\":{\"country\":\"US\",\"addressLine\":[\"340 Main "
      "St\","
      "\"BIN1\",\"First "
      "floor\"],\"region\":\"\",\"city\":\"\",\"dependentLocality\":"
      "\"\",\"postalCode\":\"\",\"sortingCode\":\"\","
      "\"organization\":\"\",\"recipient\":\"\",\"phone\":\"\"},"
      "\"shippingOption\":"
      "\"standardShippingOption\",\"payerName\":\"Jon Doe\","
      "\"payerEmail\":\"abc@gmail.com\",\"payerPhone\":\"0123\"}";
  EXPECT_EQ(expected, json_string);
}

}  // namespace
}  // namespace blink
