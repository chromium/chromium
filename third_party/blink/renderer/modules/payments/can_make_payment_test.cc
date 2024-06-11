// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::canMakePayment().

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

using payments::mojom::blink::CanMakePaymentQueryResult;
using payments::mojom::blink::HasEnrolledInstrumentQueryResult;
using payments::mojom::blink::PaymentErrorReason;
using payments::mojom::blink::PaymentRequestClient;

TEST(HasEnrolledInstrumentTest, RejectPromiseOnUserCancel) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  request->hasEnrolledInstrument(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::USER_CANCEL, "User closed UI.");
}

TEST(HasEnrolledInstrumentTest, RejectPromiseOnUnknownError) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  request->hasEnrolledInstrument(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::UNKNOWN, "Unknown error.");
}

TEST(HasEnrolledInstrumentTest, RejectDuplicateRequest) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);
  request->hasEnrolledInstrument(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  request->hasEnrolledInstrument(scope.GetScriptState(),
                                 scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST(HasEnrolledInstrumentTest, RejectQueryQuotaExceeded) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  request->hasEnrolledInstrument(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnHasEnrolledInstrument(
      HasEnrolledInstrumentQueryResult::QUERY_QUOTA_EXCEEDED);
}

TEST(HasEnrolledInstrumentTest, ReturnHasNoEnrolledInstrument) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);
  String captor;
  request->hasEnrolledInstrument(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectCall(&captor), funcs.ExpectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnHasEnrolledInstrument(
      HasEnrolledInstrumentQueryResult::HAS_NO_ENROLLED_INSTRUMENT);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("false", captor);
}

TEST(HasEnrolledInstrumentTest, ReturnHasEnrolledInstrument) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);
  String captor;
  request->hasEnrolledInstrument(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectCall(&captor), funcs.ExpectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnHasEnrolledInstrument(
      HasEnrolledInstrumentQueryResult::HAS_ENROLLED_INSTRUMENT);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("true", captor);
}

TEST(CanMakePaymentTest, RejectPromiseOnUserCancel) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  request->canMakePayment(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::USER_CANCEL, "User closed the UI.");
}

TEST(CanMakePaymentTest, RejectPromiseOnUnknownError) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;

  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  request->canMakePayment(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::UNKNOWN, "Unknown error.");
}

TEST(CanMakePaymentTest, RejectDuplicateRequest) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);
  request->canMakePayment(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  request->canMakePayment(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST(CanMakePaymentTest, ReturnCannotMakePayment) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);
  String captor;
  request->canMakePayment(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectCall(&captor), funcs.ExpectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakePayment(
      CanMakePaymentQueryResult::CANNOT_MAKE_PAYMENT);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("false", captor);
}

TEST(CanMakePaymentTest, ReturnCanMakePayment) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);
  String captor;
  request->canMakePayment(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectCall(&captor), funcs.ExpectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakePayment(
      CanMakePaymentQueryResult::CAN_MAKE_PAYMENT);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("true", captor);
}

}  // namespace
}  // namespace blink
