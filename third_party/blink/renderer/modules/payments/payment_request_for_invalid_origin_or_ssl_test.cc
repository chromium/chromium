// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {
namespace {

class MockPaymentProvider : public payments::mojom::blink::PaymentRequest {
 public:
  void Init(
      mojo::PendingRemote<payments::mojom::blink::PaymentRequestClient> client,
      WTF::Vector<payments::mojom::blink::PaymentMethodDataPtr> method_data,
      payments::mojom::blink::PaymentDetailsPtr details,
      payments::mojom::blink::PaymentOptionsPtr options) override {
    client_.Bind(std::move(client));
    client_->OnError(payments::mojom::PaymentErrorReason::
                         NOT_SUPPORTED_FOR_INVALID_ORIGIN_OR_SSL,
                     "mock error message");
    has_closed_ = true;
  }

  void Show(bool wait_for_updated_details, bool had_user_activation) override {}
  void Retry(
      payments::mojom::blink::PaymentValidationErrorsPtr errors) override {
    NOTREACHED_IN_MIGRATION();
  }
  void UpdateWith(
      payments::mojom::blink::PaymentDetailsPtr update_with_details) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnPaymentDetailsNotUpdated() override { NOTREACHED_IN_MIGRATION(); }
  void Abort() override { NOTREACHED_IN_MIGRATION(); }
  void Complete(payments::mojom::PaymentComplete result) override {
    NOTREACHED_IN_MIGRATION();
  }
  void CanMakePayment() override {}
  void HasEnrolledInstrument() override {}

  mojo::PendingRemote<payments::mojom::blink::PaymentRequest>
  CreatePendingRemoteAndBind() {
    mojo::PendingRemote<payments::mojom::blink::PaymentRequest> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

 private:
  mojo::Receiver<payments::mojom::blink::PaymentRequest> receiver_{this};
  mojo::Remote<payments::mojom::blink::PaymentRequestClient> client_;
  bool has_closed_ = false;
};

// This tests PaymentRequest API on invalid origin or invalid ssl.
class PaymentRequestForInvalidOriginOrSslTest : public testing::Test {
 public:
  PaymentRequestForInvalidOriginOrSslTest()
      : payment_provider_(std::make_unique<MockPaymentProvider>()) {}

  ScriptValue GetRejectValue(ScriptState* script_state,
                             ScriptPromiseUntyped& promise) {
    ScriptPromiseTester tester(script_state, promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
    return tester.Value();
  }

  bool ResolvePromise(ScriptState* script_state,
                      ScriptPromiseUntyped& promise) {
    ScriptPromiseTester tester(script_state, promise);
    tester.WaitUntilSettled();
    return tester.Value().V8Value()->IsTrue();
  }

  std::string GetRejectString(ScriptState* script_state,
                              ScriptPromiseUntyped& promise) {
    ScriptValue on_reject = GetRejectValue(script_state, promise);
    return ToCoreString(script_state->GetIsolate(),
                        on_reject.V8Value()
                            ->ToString(script_state->GetContext())
                            .ToLocalChecked())
        .Ascii()
        .data();
  }

  PaymentRequest* CreatePaymentRequest(PaymentRequestV8TestingScope& scope) {
    return MakeGarbageCollected<PaymentRequest>(
        scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
        BuildPaymentDetailsInitForTest(), PaymentOptions::Create(),
        payment_provider_->CreatePendingRemoteAndBind(), ASSERT_NO_EXCEPTION);
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockPaymentProvider> payment_provider_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

TEST_F(PaymentRequestForInvalidOriginOrSslTest,
       ShowIsRejected_WhenShowBeforeIdle) {
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = CreatePaymentRequest(scope);
  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseUntyped promise =
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  // PaymentRequest.OnError() runs in this idle.
  platform_->RunUntilIdle();

  EXPECT_EQ("NotSupportedError: mock error message",
            GetRejectString(scope.GetScriptState(), promise));
}

TEST_F(PaymentRequestForInvalidOriginOrSslTest,
       ShowIsRejected_WhenShowAfterIdle) {
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = CreatePaymentRequest(scope);
  // PaymentRequest.OnError() runs in this idle.
  platform_->RunUntilIdle();

  // The show() will be rejected before user activation is checked, so there is
  // no need to trigger user-activation here.
  ScriptPromiseUntyped promise =
      request->show(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kNotSupportedError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

TEST_F(PaymentRequestForInvalidOriginOrSslTest,
       SelfRejectingPromiseCanBeRepeated) {
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = CreatePaymentRequest(scope);
  // PaymentRequest.OnError() runs in this idle.
  platform_->RunUntilIdle();

  // The show()s will be rejected before user activation is checked, so there is
  // no need to trigger user-activation here.
  {
    DummyExceptionStateForTesting exception_state;
    ScriptPromiseUntyped promise1 =
        request->show(scope.GetScriptState(), exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(DOMExceptionCode::kNotSupportedError,
              exception_state.CodeAs<DOMExceptionCode>());
  }

  {
    DummyExceptionStateForTesting exception_state;
    ScriptPromiseUntyped promise2 =
        request->show(scope.GetScriptState(), exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(DOMExceptionCode::kNotSupportedError,
              exception_state.CodeAs<DOMExceptionCode>());
  }
}

TEST_F(PaymentRequestForInvalidOriginOrSslTest,
       CanMakePaymentIsRejected_CheckAfterIdle) {
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = CreatePaymentRequest(scope);
  // PaymentRequest.OnError() runs in this idle.
  platform_->RunUntilIdle();

  ScriptPromiseUntyped promise =
      request->canMakePayment(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(ResolvePromise(scope.GetScriptState(), promise));
}

TEST_F(PaymentRequestForInvalidOriginOrSslTest,
       CanMakePaymentIsRejected_CheckBeforeIdle) {
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = CreatePaymentRequest(scope);
  ScriptPromiseUntyped promise =
      request->canMakePayment(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  // PaymentRequest.OnError() runs in this idle.
  platform_->RunUntilIdle();

  EXPECT_FALSE(ResolvePromise(scope.GetScriptState(), promise));
}

TEST_F(PaymentRequestForInvalidOriginOrSslTest,
       HasEnrolledInstrument_CheckAfterIdle) {
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = CreatePaymentRequest(scope);
  // PaymentRequest.OnError() runs in this idle.
  platform_->RunUntilIdle();

  ScriptPromiseUntyped promise = request->hasEnrolledInstrument(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(ResolvePromise(scope.GetScriptState(), promise));
}

TEST_F(PaymentRequestForInvalidOriginOrSslTest,
       HasEnrolledInstrument_CheckBeforeIdle) {
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = CreatePaymentRequest(scope);
  ScriptPromiseUntyped promise = request->hasEnrolledInstrument(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  // PaymentRequest.OnError() runs in this idle.
  platform_->RunUntilIdle();

  EXPECT_FALSE(ResolvePromise(scope.GetScriptState(), promise));
}

}  // namespace
}  // namespace blink
