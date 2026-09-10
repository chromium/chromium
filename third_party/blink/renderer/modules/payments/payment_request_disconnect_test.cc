// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_response.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

// Mock implementation of the browser-side PaymentRequest object. This mock
// immediately disconnects from the mojo pipe on receiving a Show call,
// optionally sending back an OnError message first.
class MockPaymentProvider : public payments::mojom::blink::PaymentRequest {
 public:
  explicit MockPaymentProvider(
      mojo::PendingReceiver<payments::mojom::blink::PaymentRequest> receiver)
      : payment_provider_receiver_(this, std::move(receiver)) {}

  void Init(
      mojo::PendingRemote<payments::mojom::blink::PaymentRequestClient> client,
      Vector<payments::mojom::blink::PaymentMethodDataPtr>,
      payments::mojom::blink::PaymentDetailsPtr,
      payments::mojom::blink::PaymentOptionsPtr) override {
    client_.Bind(std::move(client));
  }

  void Show(bool, bool) override {
    if (send_error_on_show_) {
      // Post the error callback so that the receiver disconnect is dispatched
      // first, simulating the IPC race between the two independent Mojo pipes.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](mojo::Remote<payments::mojom::blink::PaymentRequestClient>
                     client) {
                client->OnError(
                    payments::mojom::blink::PaymentErrorReason::NOT_SUPPORTED,
                    "The payment method \"foo\" is not supported");
              },
              std::move(client_)));
    } else {
      client_.reset();
    }
    // Simulate the browser destroying the PaymentRequest service immediately.
    payment_provider_receiver_.reset();
  }

  void UpdateWith(payments::mojom::blink::PaymentDetailsPtr) override {}
  void OnPaymentDetailsNotUpdated() override {}
  void Abort() override {}
  void Complete(payments::mojom::blink::PaymentComplete) override {}
  void Retry(payments::mojom::blink::PaymentValidationErrorsPtr) override {}
  void CanMakePayment() override {}
  void HasEnrolledInstrument() override {}

  void set_send_error_on_show(bool send_error) {
    send_error_on_show_ = send_error;
  }

 private:
  mojo::Receiver<payments::mojom::blink::PaymentRequest>
      payment_provider_receiver_;
  mojo::Remote<payments::mojom::blink::PaymentRequestClient> client_;
  bool send_error_on_show_ = false;
};

// Tests the behavior when the browser sends an OnError message and then
// immediately disconnects from the mojo endpoint. The error message should
// still be received and used to reject the JavaScript promise.
TEST(PaymentRequestDisconnectTest,
     RejectShowPromiseOnProviderDisconnectAfterError) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;

  mojo::PendingRemote<payments::mojom::blink::PaymentRequest> provider_remote;
  MockPaymentProvider mock_provider(
      provider_remote.InitWithNewPipeAndPassReceiver());
  mock_provider.set_send_error_on_show(true);

  PaymentRequest* request = MakeGarbageCollected<PaymentRequest>(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), PaymentOptions::Create(),
      std::move(provider_remote), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_EQ("NotSupportedError: The payment method \"foo\" is not supported",
            promise_tester.ValueAsString());
}

// Tests the behavior when the browser immediately disconnects without sending
// an OnError message; this is a true 'lost IPC' case.
TEST(PaymentRequestDisconnectTest,
     RejectShowPromiseOnProviderDisconnectWithoutError) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;

  mojo::PendingRemote<payments::mojom::blink::PaymentRequest> provider_remote;
  MockPaymentProvider mock_provider(
      provider_remote.InitWithNewPipeAndPassReceiver());
  mock_provider.set_send_error_on_show(false);

  PaymentRequest* request = MakeGarbageCollected<PaymentRequest>(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), PaymentOptions::Create(),
      std::move(provider_remote), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_EQ(
      "UnknownError: Renderer process could not establish or lost IPC "
      "connection to the PaymentRequest service in the browser process.",
      promise_tester.ValueAsString());
}

// Tests the after-error behavior when kPaymentRequestAvoidConnectionErrorRace
// is disabled, which was to (incorrectly) drop the OnError call and report a
// 'lost IPC' error to the website.
TEST(PaymentRequestDisconnectTest,
     RejectShowPromiseOnProviderDisconnectAfterError_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPaymentRequestAvoidConnectionErrorRace);

  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;

  mojo::PendingRemote<payments::mojom::blink::PaymentRequest> provider_remote;
  MockPaymentProvider mock_provider(
      provider_remote.InitWithNewPipeAndPassReceiver());
  mock_provider.set_send_error_on_show(true);

  PaymentRequest* request = MakeGarbageCollected<PaymentRequest>(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), PaymentOptions::Create(),
      std::move(provider_remote), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_EQ(
      "UnknownError: Renderer process could not establish or lost IPC "
      "connection to the PaymentRequest service in the browser process.",
      promise_tester.ValueAsString());
}

}  // namespace
}  // namespace blink
