// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_request_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

CredentialRequestOptions* CreateOptionsWithProviders(
    const HeapVector<Member<IdentityProviderRequestOptions>>& providers) {
  IdentityCredentialRequestOptions* identity_credential_request =
      IdentityCredentialRequestOptions::Create();
  identity_credential_request->setProviders(providers);
  CredentialRequestOptions* options = CredentialRequestOptions::Create();
  options->setIdentity(identity_credential_request);
  return options;
}

CredentialRequestOptions* CreateValidOptions() {
  IdentityProviderRequestOptions* identity_provider_request =
      IdentityProviderRequestOptions::Create();
  identity_provider_request->setHolder(DigitalCredentialProvider::Create());
  HeapVector<Member<IdentityProviderRequestOptions>> identity_providers;
  identity_providers.push_back(identity_provider_request);
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

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolverTyped<IDLNullable<Credential>>>(
          script_state);
  auto promise = DiscoverDigitalIdentityCredentialFromExternalSource(
      script_state, resolver, *CreateValidOptions(),
      IGNORE_EXCEPTION_FOR_TESTING);

  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentials));
}

}  // namespace blink
