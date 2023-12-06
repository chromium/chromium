// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::abort().

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

// If request.abort() is called without calling request.show() first, then
// abort() should reject with exception.
TEST(AbortTest, CannotAbortBeforeShow) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  request->abort(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

// If request.abort() is called again before the previous abort() resolved, then
// the second abort() should reject with exception.
TEST(AbortTest, CannotAbortTwiceConcurrently) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  request->abort(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  request->abort(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

// If request.abort() is called after calling request.show(), then abort()
// should not reject with exception.
TEST(AbortTest, CanAbortAfterShow) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  request->abort(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
}

// If the browser is unable to abort the payment, then the request.abort()
// promise should be rejected.
TEST(AbortTest, FailedAbortShouldRejectAbortPromise) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  request->abort(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnAbort(
      false);
}

// After the browser is unable to abort the payment once, the second abort()
// call should not be rejected, as it's not a duplicate request anymore.
TEST(AbortTest, CanAbortAgainAfterFirstAbortRejected) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  request->abort(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnAbort(
      false);

  request->abort(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
}

// If the browser successfully aborts the payment, then the request.show()
// promise should be rejected, and request.abort() promise should be resolved.
TEST(AbortTest, SuccessfulAbortShouldRejectShowPromiseAndResolveAbortPromise) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());
  request->abort(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectCall(), funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnAbort(
      true);
}

}  // namespace
}  // namespace blink
