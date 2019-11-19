// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::abort().

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"

namespace blink {
namespace {

// If request.abort() is called without calling request.show() first, then
// abort() should reject with exception.
TEST(AbortTest, CannotAbortBeforeShow) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  request->abort(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());
}

// If request.abort() is called again before the previous abort() resolved, then
// the second abort() should reject with exception.
TEST(AbortTest, CannotAbortTwiceConcurrently) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  request->show(scope.GetScriptState());
  request->abort(scope.GetScriptState());

  request->abort(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());
}

// If request.abort() is called after calling request.show(), then abort()
// should not reject with exception.
TEST(AbortTest, CanAbortAfterShow) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  request->show(scope.GetScriptState());

  request->abort(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
}

// If the browser is unable to abort the payment, then the request.abort()
// promise should be rejected.
TEST(AbortTest, FailedAbortShouldRejectAbortPromise) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  request->show(scope.GetScriptState());

  request->abort(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnAbort(
      false);
}

// After the browser is unable to abort the payment once, the second abort()
// call should not be rejected, as it's not a duplicate request anymore.
TEST(AbortTest, CanAbortAgainAfterFirstAbortRejected) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  request->show(scope.GetScriptState());
  request->abort(scope.GetScriptState());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnAbort(
      false);

  request->abort(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
}

// If the browser successfully aborts the payment, then the request.show()
// promise should be rejected, and request.abort() promise should be resolved.
TEST(AbortTest, SuccessfulAbortShouldRejectShowPromiseAndResolveAbortPromise) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  request->show(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());
  request->abort(scope.GetScriptState())
      .Then(funcs.ExpectCall(), funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnAbort(
      true);
}

}  // namespace
}  // namespace blink
