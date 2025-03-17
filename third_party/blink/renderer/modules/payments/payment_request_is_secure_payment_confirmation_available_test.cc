// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/secure_payment_confirmation_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

class FakeSecurePaymentConfirmationService
    : public payments::mojom::blink::SecurePaymentConfirmationService {
 public:
  explicit FakeSecurePaymentConfirmationService(bool spc_available)
      : spc_available_(spc_available) {}

  FakeSecurePaymentConfirmationService(
      const FakeSecurePaymentConfirmationService&) = delete;
  FakeSecurePaymentConfirmationService& operator=(
      const FakeSecurePaymentConfirmationService&) = delete;

  void Bind(
      mojo::PendingReceiver<
          payments::mojom::blink::SecurePaymentConfirmationService> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void IsSecurePaymentConfirmationAvailable(
      IsSecurePaymentConfirmationAvailableCallback callback) override {
    std::move(callback).Run(spc_available_);
  }

  void StorePaymentCredential(
      const WTF::Vector<uint8_t>& credential_id,
      const WTF::String& rp_id,
      const WTF::Vector<uint8_t>& user_id,
      StorePaymentCredentialCallback callback) override {}

  void MakePaymentCredential(
      ::blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr options,
      MakePaymentCredentialCallback callback) override {}

 private:
  mojo::Receiver<payments::mojom::blink::SecurePaymentConfirmationService>
      receiver_{this};
  bool spc_available_;
};

// A RAII class that creates and installs a mocked
// SecurePaymentConfirmationService on allocation, and uninstalls it on
// deletion.
class ScopedFakeSecurePaymentConfirmationService {
  STACK_ALLOCATED();

 public:
  explicit ScopedFakeSecurePaymentConfirmationService(
      PaymentRequestV8TestingScope* scope,
      bool spc_available = true)
      : scope_(scope) {
    mock_service_ =
        std::make_unique<FakeSecurePaymentConfirmationService>(spc_available);
    scope_->GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        payments::mojom::blink::SecurePaymentConfirmationService::Name_,
        WTF::BindRepeating(
            [](FakeSecurePaymentConfirmationService* mock_service_ptr,
               mojo::ScopedMessagePipeHandle handle) {
              mock_service_ptr->Bind(
                  mojo::PendingReceiver<
                      payments::mojom::blink::SecurePaymentConfirmationService>(
                      std::move(handle)));
            },
            WTF::Unretained(mock_service_.get())));
  }

  ~ScopedFakeSecurePaymentConfirmationService() {
    scope_->GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        payments::mojom::blink::SecurePaymentConfirmationService::Name_, {});
  }

 private:
  raw_ptr<PaymentRequestV8TestingScope> scope_;
  std::unique_ptr<FakeSecurePaymentConfirmationService> mock_service_;
};

TEST(PaymentRequestTest, IsSecurePaymentConfirmationAvailable) {
  ScopedSecurePaymentConfirmationForTest scoped_spc(true);

  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  ScopedFakeSecurePaymentConfirmationService scoped_mock_service(&scope);

  ScriptPromise<IDLBoolean> promise =
      PaymentRequest::isSecurePaymentConfirmationAvailable(
          scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.Value().V8Value()->IsTrue());
}

TEST(PaymentRequestTest, IsSecurePaymentConfirmationAvailable_FeatureDisabled) {
  ScopedSecurePaymentConfirmationForTest scoped_spc(false);

  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;

  ScopedFakeSecurePaymentConfirmationService scoped_mock_service(&scope);

  ScriptPromise<IDLBoolean> promise =
      PaymentRequest::isSecurePaymentConfirmationAvailable(
          scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_FALSE(tester.Value().V8Value()->IsTrue());
}

// TODO(crbug.com/40258712): Test that the 'payment' permission policy affects
// the outcome of PaymentRequest.isSecurePaymentConfirmationAvailable()

TEST(PaymentRequestTest,
     IsSecurePaymentConfirmationAvailable_BrowserReturnsFalse) {
  ScopedSecurePaymentConfirmationForTest scoped_spc(true);

  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;

  ScopedFakeSecurePaymentConfirmationService scoped_mock_service(
      &scope, /*spc_available=*/false);

  ScriptPromise<IDLBoolean> promise =
      PaymentRequest::isSecurePaymentConfirmationAvailable(
          scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_FALSE(tester.Value().V8Value()->IsTrue());
}

}  // namespace
}  // namespace blink
