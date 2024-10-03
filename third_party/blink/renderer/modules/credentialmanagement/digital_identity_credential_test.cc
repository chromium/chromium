// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_request_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

// Mock mojom::DigitalIdentityRequest which succeeds and returns "token".
class MockDigitalIdentityRequest : public mojom::DigitalIdentityRequest {
 public:
  MockDigitalIdentityRequest() = default;

  MockDigitalIdentityRequest(const MockDigitalIdentityRequest&) = delete;
  MockDigitalIdentityRequest& operator=(const MockDigitalIdentityRequest&) =
      delete;

  void Bind(mojo::PendingReceiver<mojom::DigitalIdentityRequest> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void Request(
      std::vector<blink::mojom::DigitalCredentialProviderPtr> providers,
      RequestCallback callback) override {
    std::move(callback).Run(mojom::RequestDigitalIdentityStatus::kSuccess,
                            "protocol", "token");
  }
  void Abort() override {}

 private:
  mojo::Receiver<mojom::DigitalIdentityRequest> receiver_{this};
};

CredentialRequestOptions* CreateOptionsWithProviders(
    const HeapVector<Member<IdentityRequestProvider>>& providers) {
  DigitalCredentialRequestOptions* digital_credential_request =
      DigitalCredentialRequestOptions::Create();
  digital_credential_request->setProviders(providers);
  CredentialRequestOptions* options = CredentialRequestOptions::Create();
  options->setDigital(digital_credential_request);
  return options;
}

CredentialRequestOptions* CreateValidOptions() {
  IdentityRequestProvider* identity_provider =
      IdentityRequestProvider::Create();
  identity_provider->setRequest(
      MakeGarbageCollected<V8UnionObjectOrString>(String()));
  HeapVector<Member<IdentityRequestProvider>> identity_providers;
  identity_providers.push_back(identity_provider);
  return CreateOptionsWithProviders(identity_providers);
}

}  // namespace

class DigitalIdentityCredentialTest : public testing::Test {
 public:
  DigitalIdentityCredentialTest() = default;
  ~DigitalIdentityCredentialTest() override = default;

  DigitalIdentityCredentialTest(const DigitalIdentityCredentialTest&) = delete;
  DigitalIdentityCredentialTest& operator=(
      const DigitalIdentityCredentialTest&) = delete;

 private:
  test::TaskEnvironment task_environment_;
};

// Test that navigator.credentials.get() increments the feature use counter when
// one of the identity providers is a digital identity credential.
TEST_F(DigitalIdentityCredentialTest, IdentityDigitalCredentialUseCounter) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  std::unique_ptr mock_request = std::make_unique<MockDigitalIdentityRequest>();
  auto mock_request_ptr = mock_request.get();
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_,
      WTF::BindRepeating(
          [](MockDigitalIdentityRequest* mock_request_ptr,
             mojo::ScopedMessagePipeHandle handle) {
            mock_request_ptr->Bind(
                mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                    std::move(handle)));
          },
          WTF::Unretained(mock_request_ptr)));

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);
  DiscoverDigitalIdentityCredentialFromExternalSource(
      resolver, context.GetExceptionState(), *CreateValidOptions());

  test::RunPendingTasks();

  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentials));
  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsSuccess));
}

}  // namespace blink
