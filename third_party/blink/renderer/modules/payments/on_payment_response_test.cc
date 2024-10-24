// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::OnPaymentResponse().

#include <utility>

#include "base/memory/raw_ptr_exclusion.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_response.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/payments/payment_address.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_response.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

// If the merchant requests shipping information, but the browser does not
// provide the shipping option, reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingShippingOption) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "US";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests shipping information, but the browser does not
// provide a shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingAddress) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "standardShipping";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests a payer name, but the browser does not provide it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingName) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests an email address, but the browser does not provide
// it, reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingEmail) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests a phone number, but the browser does not provide it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingPhone) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests shipping information, but the browser provides an
// empty string for shipping option, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyShippingOption) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "";
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "US";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests shipping information, but the browser provides an
// empty shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyAddress) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  ;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "standardShipping";
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests a payer name, but the browser provides an empty
// string for name, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyName) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->name = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests an email, but the browser provides an empty string
// for email, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyEmail) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->email = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests a phone number, but the browser provides an empty
// string for the phone number, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyPhone) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->phone = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant does not request shipping information, but the browser
// provides a shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedAddress) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "US";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant does not request shipping information, but the browser
// provides a shipping option, reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedShippingOption) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant does not request a payer name, but the browser provides it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedName) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->name = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant does not request an email, but the browser provides it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedEmail) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->email = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant does not request a phone number, but the browser provides it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedPhone) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->phone = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

// If the merchant requests shipping information, but the browser provides an
// invalid shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectInvalidAddress) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "standardShipping";
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "Atlantis";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

class PaymentResponseFunction
    : public ThenCallable<PaymentResponse, PaymentResponseFunction> {
 public:
  void React(ScriptState*, PaymentResponse* response) { response_ = response; }
  PaymentResponse* Response() const { return response_; }
  void Trace(Visitor* visitor) const override {
    ThenCallable<PaymentResponse, PaymentResponseFunction>::Trace(visitor);
    visitor->Trace(response_);
  }

 private:
  Member<PaymentResponse> response_;
};

// If the merchant requests shipping information, the resolved show() promise
// should contain a shipping option and an address.
TEST(OnPaymentResponseTest, CanRequestShippingInformation) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "standardShipping";
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "US";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* response_function = MakeGarbageCollected<PaymentResponseFunction>();
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .React(scope.GetScriptState(), response_function);

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("standardShipping",
            response_function->Response()->shippingOption());
}

// If the merchant requests a payer name, the resolved show() promise should
// contain a payer name.
TEST(OnPaymentResponseTest, CanRequestName) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer = payments::mojom::blink::PayerDetail::New();
  response->payer->name = "Jon Doe";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* response_function = MakeGarbageCollected<PaymentResponseFunction>();
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .React(scope.GetScriptState(), response_function);

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("Jon Doe", response_function->Response()->payerName());
}

// If the merchant requests an email address, the resolved show() promise should
// contain an email address.
TEST(OnPaymentResponseTest, CanRequestEmail) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->email = "abc@gmail.com";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* response_function = MakeGarbageCollected<PaymentResponseFunction>();
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .React(scope.GetScriptState(), response_function);

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("abc@gmail.com", response_function->Response()->payerEmail());
}

// If the merchant requests a phone number, the resolved show() promise should
// contain a phone number.
TEST(OnPaymentResponseTest, CanRequestPhone) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->phone = "0123";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* response_function = MakeGarbageCollected<PaymentResponseFunction>();
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .React(scope.GetScriptState(), response_function);

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("0123", response_function->Response()->payerPhone());
}

// If the merchant does not request shipping information, the resolved show()
// promise should contain null shipping option and address.
TEST(OnPaymentResponseTest, ShippingInformationNotRequired) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* response_function = MakeGarbageCollected<PaymentResponseFunction>();
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .React(scope.GetScriptState(), response_function);

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(response_function->Response()->shippingOption().IsNull());
  EXPECT_EQ(nullptr, response_function->Response()->shippingAddress());
}

// If the merchant does not request a phone number, the resolved show() promise
// should contain null phone number.
TEST(OnPaymentResponseTest, PhoneNotRequired) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->phone = String();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* response_function = MakeGarbageCollected<PaymentResponseFunction>();
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .React(scope.GetScriptState(), response_function);

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(response_function->Response()->payerPhone().IsNull());
}

// If the merchant does not request a payer name, the resolved show() promise
// should contain null payer name.
TEST(OnPaymentResponseTest, NameNotRequired) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->name = String();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* response_function = MakeGarbageCollected<PaymentResponseFunction>();
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .React(scope.GetScriptState(), response_function);

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(response_function->Response()->payerName().IsNull());
}

// If the merchant does not request an email address, the resolved show()
// promise should contain null email address.
TEST(OnPaymentResponseTest, EmailNotRequired) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->email = String();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* response_function = MakeGarbageCollected<PaymentResponseFunction>();
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .React(scope.GetScriptState(), response_function);

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(response_function->Response()->payerEmail().IsNull());
}

}  // namespace
}  // namespace blink
