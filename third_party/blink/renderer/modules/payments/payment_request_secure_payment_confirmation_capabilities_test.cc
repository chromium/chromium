// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/secure_payment_confirmation_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

using payments::mojom::blink::SecurePaymentConfirmationCapability;
using payments::mojom::blink::SecurePaymentConfirmationCapabilityPtr;
using payments::mojom::blink::SecurePaymentConfirmationService;

class FakeSecurePaymentConfirmationService
    : public SecurePaymentConfirmationService {
 public:
  explicit FakeSecurePaymentConfirmationService(
      Vector<SecurePaymentConfirmationCapabilityPtr> spc_capabilities)
      : spc_capabilities_(std::move(spc_capabilities)) {}

  FakeSecurePaymentConfirmationService(
      const FakeSecurePaymentConfirmationService&) = delete;
  FakeSecurePaymentConfirmationService& operator=(
      const FakeSecurePaymentConfirmationService&) = delete;

  void Bind(mojo::PendingReceiver<SecurePaymentConfirmationService> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void SecurePaymentConfirmationAvailability(
      SecurePaymentConfirmationAvailabilityCallback callback) override {}

  void GetSecurePaymentConfirmationCapabilities(
      GetSecurePaymentConfirmationCapabilitiesCallback callback) override {
    std::move(callback).Run(std::move(spc_capabilities_));
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
  mojo::Receiver<SecurePaymentConfirmationService> receiver_{this};
  Vector<SecurePaymentConfirmationCapabilityPtr> spc_capabilities_;
};

// A RAII class that creates and installs a mocked
// SecurePaymentConfirmationService on allocation, and uninstalls it on
// deletion.
class ScopedFakeSecurePaymentConfirmationService {
  STACK_ALLOCATED();

 public:
  explicit ScopedFakeSecurePaymentConfirmationService(
      PaymentRequestV8TestingScope* scope,
      Vector<SecurePaymentConfirmationCapabilityPtr> spc_capabilities = {})
      : scope_(scope) {
    mock_service_ = std::make_unique<FakeSecurePaymentConfirmationService>(
        std::move(spc_capabilities));
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

TEST(PaymentRequestTest, SecurePaymentConfirmationCapabilities) {
  ScopedSecurePaymentConfirmationForTest scoped_spc(true);
  ScopedSecurePaymentConfirmationCapabilitiesForTest scoped_spc_capabilities(
      true);

  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  // Capabilities should be out of alphabetical order to ensure that they get
  // sorted by the `OnGetSecurePaymentConfirmationCapabilitiesComplete` method.
  Vector<SecurePaymentConfirmationCapabilityPtr> spc_capabilities;
  spc_capabilities.emplace_back(
      SecurePaymentConfirmationCapability::New("capability_b", true));
  spc_capabilities.emplace_back(
      SecurePaymentConfirmationCapability::New("capability_a", false));
  spc_capabilities.emplace_back(
      SecurePaymentConfirmationCapability::New("capability_c", false));
  ScopedFakeSecurePaymentConfirmationService scoped_mock_service(
      &scope, std::move(spc_capabilities));

  ScriptPromise<IDLRecord<IDLString, IDLBoolean>> promise =
      PaymentRequest::getSecurePaymentConfirmationCapabilities(
          scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  const auto& got_spc_capabilities =
      NativeValueTraits<IDLRecord<IDLString, IDLBoolean>>::NativeValue(
          scope.GetIsolate(), tester.Value().V8Value(),
          scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  // SPC capabilities should be sorted by name.
  Vector<std::pair<String, bool>> want_spc_capabilities = {
      {"capability_a", false}, {"capability_b", true}, {"capability_c", false}};
  EXPECT_EQ(got_spc_capabilities, want_spc_capabilities);
  EXPECT_TRUE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestGetSecurePaymentConfirmationCapabilities));
}

TEST(PaymentRequestTest,
     SecurePaymentConfirmationCapabilities_FeatureDisabled) {
  ScopedSecurePaymentConfirmationForTest scoped_spc(true);
  ScopedSecurePaymentConfirmationCapabilitiesForTest scoped_spc_capabilities(
      false);

  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  ScopedFakeSecurePaymentConfirmationService scoped_mock_service(&scope);

  ScriptPromise<IDLRecord<IDLString, IDLBoolean>> promise =
      PaymentRequest::getSecurePaymentConfirmationCapabilities(
          scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
  auto* exception = V8DOMException::ToWrappable(
      scope.GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "NotSupportedError");
  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestGetSecurePaymentConfirmationCapabilities));
}

TEST(PaymentRequestTest,
     SecurePaymentConfirmationCapabilities_PaymentPermissionsPolicyDisabled) {
  ScopedSecurePaymentConfirmationForTest scoped_spc(true);
  ScopedSecurePaymentConfirmationCapabilitiesForTest scoped_spc_capabilities(
      true);

  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;

  network::ParsedPermissionsPolicy parsed_policy;
  DisallowFeature(network::mojom::PermissionsPolicyFeature::kPayment,
                  parsed_policy);
  auto origin = SecurityOrigin::CreateFromString("https://example.test");
  scope.GetExecutionContext()->GetSecurityContext().SetPermissionsPolicy(
      network::PermissionsPolicy::CreateFromParsedPolicy(
          parsed_policy, origin->ToUrlOrigin()));

  ScopedFakeSecurePaymentConfirmationService scoped_mock_service(&scope);

  ScriptPromise<IDLRecord<IDLString, IDLBoolean>> promise =
      PaymentRequest::getSecurePaymentConfirmationCapabilities(
          scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
  auto* exception = V8DOMException::ToWrappable(
      scope.GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "NotAllowedError");
  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestGetSecurePaymentConfirmationCapabilities));
}

}  // namespace
}  // namespace blink
