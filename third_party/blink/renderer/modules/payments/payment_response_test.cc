// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_response.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/modules/payments/payment_address.h"
#include "third_party/blink/renderer/modules/payments/payment_state_resolver.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/modules/payments/payment_validation_errors.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {

class MockPaymentStateResolver final
    : public GarbageCollected<MockPaymentStateResolver>,
      public PaymentStateResolver {
  USING_GARBAGE_COLLECTED_MIXIN(MockPaymentStateResolver);

 public:
  MockPaymentStateResolver() {
    ON_CALL(*this, Complete(testing::_, testing::_))
        .WillByDefault(testing::ReturnPointee(&dummy_promise_));
  }

  ~MockPaymentStateResolver() override = default;

  MOCK_METHOD2(Complete, ScriptPromise(ScriptState*, PaymentComplete result));
  MOCK_METHOD2(Retry,
               ScriptPromise(ScriptState*,
                             const PaymentValidationErrors* errorFields));

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(dummy_promise_);
  }

 private:
  ScriptPromise dummy_promise_;

  DISALLOW_COPY_AND_ASSIGN(MockPaymentStateResolver);
};

TEST(PaymentResponseTest, DataCopiedOver) {
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

TEST(PaymentResponseTest,
     PaymentResponseDetailsWithUnexpectedJSONFormatString) {
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
      v8::JSON::Stringify(scope.GetContext(),
                          details.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  EXPECT_EQ("{}", stringified_details);
}

TEST(PaymentResponseTest, PaymentResponseDetailsRetrunsTheSameObject) {
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
              Complete(scope.GetScriptState(), PaymentStateResolver::kSuccess));

  output->complete(scope.GetScriptState(), "success");
}

TEST(PaymentResponseTest, CompleteCalledWithFailure) {
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
              Complete(scope.GetScriptState(), PaymentStateResolver::kFail));

  output->complete(scope.GetScriptState(), "fail");
}

TEST(PaymentResponseTest, JSONSerializerTest) {
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
