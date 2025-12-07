// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/secure_payment_confirmation_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_secure_payment_confirmation_availability.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

using payments::mojom::SecurePaymentConfirmationAvailabilityEnum;

class FakeSecurePaymentConfirmationService
    : public payments::mojom::blink::SecurePaymentConfirmationService {
 public:
  explicit FakeSecurePaymentConfirmationService(
      SecurePaymentConfirmationAvailabilityEnum spc_availability)
      : spc_availability_(spc_availability) {}

  FakeSecurePaymentConfirmationService(
      const FakeSecurePaymentConfirmationService&) = delete;
  FakeSecurePaymentConfirmationService& operator=(
      const FakeSecurePaymentConfirmationService&) = delete;

  void Bind(
      mojo::PendingReceiver<
          payments::mojom::blink::SecurePaymentConfirmationService> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void SecurePaymentConfirmationAvailability(
      SecurePaymentConfirmationAvailabilityCallback callback) override {
    std::move(callback).Run(spc_availability_);
  }

  void StorePaymentCredential(
      const Vector<uint8_t>& credential_id,
      const String& rp_id,
      const Vector<uint8_t>& user_id,
      StorePaymentCredentialCallback callback) override {}

  void MakePaymentCredential(
      ::blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr options,
      MakePaymentCredentialCallback callback) override {}

 private:
  mojo::Receiver<payments::mojom::blink::SecurePaymentConfirmationService>
      receiver_{this};
  SecurePaymentConfirmationAvailabilityEnum spc_availability_;
};

// A RAII class that creates and installs a mocked
// SecurePaymentConfirmationService on allocation, and uninstalls it on
// deletion.
class ScopedFakeSecurePaymentConfirmationService {
  STACK_ALLOCATED();

 public:
  explicit ScopedFakeSecurePaymentConfirmationService(
      PaymentRequestV8TestingScope* scope,
      SecurePaymentConfirmationAvailabilityEnum spc_availability = payments::
          mojom::SecurePaymentConfirmationAvailabilityEnum::kAvailable)
      : scope_(scope) {
    mock_service_ = std::make_unique<FakeSecurePaymentConfirmationService>(
        spc_availability);
    scope_->GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        payments::mojom::blink::SecurePaymentConfirmationService::Name_,
        BindRepeating(
            [](FakeSecurePaymentConfirmationService* mock_service_ptr,
               mojo::ScopedMessagePipeHandle handle) {
              mock_service_ptr->Bind(
                  mojo::PendingReceiver<
                      payments::mojom::blink::SecurePaymentConfirmationService>(
                      std::move(handle)));
            },
            Unretained(mock_service_.get())));
  }

  ~ScopedFakeSecurePaymentConfirmationService() {
    scope_->GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        payments::mojom::blink::SecurePaymentConfirmationService::Name_, {});
  }

 private:
  raw_ptr<PaymentRequestV8TestingScope> scope_;
  std::unique_ptr<FakeSecurePaymentConfirmationService> mock_service_;
};

TEST(PaymentRequestTest, SecurePaymentConfirmationAvailability_Available) {
  ScopedSecurePaymentConfirmationForTest scoped_spc(true);

  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  ScopedFakeSecurePaymentConfirmationService scoped_mock_service(&scope);

  ScriptPromise<V8SecurePaymentConfirmationAvailability> promise =
      PaymentRequest::securePaymentConfirmationAvailability(
          scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  V8SecurePaymentConfirmationAvailability result =
      V8SecurePaymentConfirmationAvailability::Create(
          scope.GetIsolate(), tester.Value().V8Value(),
          scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(result,
            V8SecurePaymentConfirmationAvailability(
                V8SecurePaymentConfirmationAvailability::Enum::kAvailable));
}

TEST(PaymentRequestTest,
     SecurePaymentConfirmationAvailability_FeatureDisabled) {
  ScopedSecurePaymentConfirmationForTest scoped_spc(false);

  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;

  ScopedFakeSecurePaymentConfirmationService scoped_mock_service(&scope);

  ScriptPromise<V8SecurePaymentConfirmationAvailability> promise =
      PaymentRequest::securePaymentConfirmationAvailability(
          scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  V8SecurePaymentConfirmationAvailability result =
      V8SecurePaymentConfirmationAvailability::Create(
          scope.GetIsolate(), tester.Value().V8Value(),
          scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(result, V8SecurePaymentConfirmationAvailability(
                        V8SecurePaymentConfirmationAvailability::Enum::
                            kUnavailableFeatureNotEnabled));
}

// TODO(crbug.com/40258712): Test that the 'payment' permission policy affects
// the outcome of PaymentRequest.isSecurePaymentConfirmationAvailable()

// This test checks that each failure outcome that could be returned by the
// browser is correctly handled and translated into the equivalent V8 enum.
TEST(PaymentRequestTest,
     IsSecurePaymentConfirmationAvailable_ServiceReturnsFailure) {
  ScopedSecurePaymentConfirmationForTest scoped_spc(true);

  const struct {
    const char* name;
    SecurePaymentConfirmationAvailabilityEnum service_result;
    V8SecurePaymentConfirmationAvailability::Enum expected_output;
  } kTestCases[] = {
      {"kUnavailableUnknownReason",
       SecurePaymentConfirmationAvailabilityEnum::kUnavailableUnknownReason,
       V8SecurePaymentConfirmationAvailability::Enum::
           kUnavailableUnknownReason},
      {"kUnavailableFeatureNotEnabled",
       SecurePaymentConfirmationAvailabilityEnum::kUnavailableFeatureNotEnabled,
       V8SecurePaymentConfirmationAvailability::Enum::
           kUnavailableFeatureNotEnabled},
      {"kUnavailableNoPermissionPolicy",
       SecurePaymentConfirmationAvailabilityEnum::
           kUnavailableNoPermissionPolicy,
       V8SecurePaymentConfirmationAvailability::Enum::
           kUnavailableNoPermissionPolicy},
      {"kUnavailableNoUserVerifyingPlatformAuthenticator",
       SecurePaymentConfirmationAvailabilityEnum::
           kUnavailableNoUserVerifyingPlatformAuthenticator,
       V8SecurePaymentConfirmationAvailability::Enum::
           kUnavailableNoUserVerifyingPlatformAuthenticator},
  };

  for (const auto& test : kTestCases) {
    test::TaskEnvironment task_environment;
    PaymentRequestV8TestingScope scope;

    ScopedFakeSecurePaymentConfirmationService scoped_mock_service(
        &scope, /*spc_availability=*/test.service_result);

    ScriptPromise<V8SecurePaymentConfirmationAvailability> promise =
        PaymentRequest::securePaymentConfirmationAvailability(
            scope.GetScriptState());
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();

    V8SecurePaymentConfirmationAvailability result =
        V8SecurePaymentConfirmationAvailability::Create(
            scope.GetIsolate(), tester.Value().V8Value(),
            scope.GetExceptionState());
    ASSERT_FALSE(scope.GetExceptionState().HadException());
    EXPECT_EQ(result,
              V8SecurePaymentConfirmationAvailability(test.expected_output))
        << "Unexpected output for " << test.name;
  }
}

}  // namespace
}  // namespace blink
