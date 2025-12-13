// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"

#include <memory>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_create_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_get_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_request_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

using testing::_;
using testing::AllOf;
using testing::SizeIs;
using testing::WithArg;

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
  void Get(std::vector<blink::mojom::DigitalCredentialGetRequestPtr> requests,
           GetCallback callback) override {
    // Return a Value::String instead of a Dict because V8ValueConverterForTest
    // doesn't support converting Dict.
    std::move(callback).Run(mojom::RequestDigitalIdentityStatus::kSuccess,
                            "protocol", base::Value("token"));
  }

  void Create(
      std::vector<blink::mojom::DigitalCredentialCreateRequestPtr> requests,
      CreateCallback callback) override {
    // Return a Value::String instead of a Dict because V8ValueConverterForTest
    // doesn't support converting Dict.
    std::move(callback).Run(mojom::RequestDigitalIdentityStatus::kSuccess,
                            "protocol", base::Value("token"));
  }

  void Abort() override {}

 private:
  mojo::Receiver<mojom::DigitalIdentityRequest> receiver_{this};
};

CredentialRequestOptions* CreateGetOptionsWithRequests(
    const HeapVector<Member<DigitalCredentialGetRequest>>& requests) {
  DigitalCredentialRequestOptions* digital_credential_request =
      DigitalCredentialRequestOptions::Create();
  digital_credential_request->setRequests(requests);
  CredentialRequestOptions* options = CredentialRequestOptions::Create();
  options->setDigital(digital_credential_request);
  return options;
}

CredentialCreationOptions* CreateCreateOptionsWithRequests(
    const HeapVector<Member<DigitalCredentialCreateRequest>>& requests) {
  DigitalCredentialCreationOptions* digital_credential_request =
      DigitalCredentialCreationOptions::Create();
  digital_credential_request->setRequests(requests);
  CredentialCreationOptions* options = CredentialCreationOptions::Create();
  options->setDigital(digital_credential_request);
  return options;
}

CredentialRequestOptions* CreateValidGetOptions(ScriptState* script_state) {
  v8::Local<v8::Context> context = script_state->GetContext();
  DigitalCredentialGetRequest* request = DigitalCredentialGetRequest::Create();
  request->setProtocol("openid4vp");
  v8::Local<v8::Object> request_data =
      v8::Object::New(script_state->GetIsolate());
  v8::Maybe<bool> maybe =
      request_data->Set(context, V8String(script_state->GetIsolate(), "key"),
                        V8String(script_state->GetIsolate(), "value"));
  CHECK(maybe.IsJust() || maybe.FromJust());

  request->setData(ScriptObject(script_state->GetIsolate(), request_data));
  HeapVector<Member<DigitalCredentialGetRequest>> requests;
  requests.push_back(request);
  return CreateGetOptionsWithRequests(requests);
}

CredentialCreationOptions* CreateValidCreateOptions() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  DigitalCredentialCreateRequest* request =
      DigitalCredentialCreateRequest::Create();
  request->setProtocol("openid4vci");
  request->setData(ScriptObject(isolate, v8::Object::New(isolate)));
  HeapVector<Member<DigitalCredentialCreateRequest>> requests;
  requests.push_back(request);
  return CreateCreateOptionsWithRequests(requests);
}

}  // namespace

class DigitalIdentityCredentialTest : public PageTestBase {
 public:
  DigitalIdentityCredentialTest() = default;
  ~DigitalIdentityCredentialTest() override = default;

  DigitalIdentityCredentialTest(const DigitalIdentityCredentialTest&) = delete;
  DigitalIdentityCredentialTest& operator=(
      const DigitalIdentityCredentialTest&) = delete;

  void SetUp() override {
    EnablePlatform();
    PageTestBase::SetUp();
  }

};

// Test that navigator.credentials.get() increments the feature use counter when
// one of the identity requests is a digital identity credential.
TEST_F(DigitalIdentityCredentialTest, IdentityDigitalCredentialUseCounter) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  std::unique_ptr mock_request = std::make_unique<MockDigitalIdentityRequest>();
  auto mock_request_ptr = mock_request.get();
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_,
      BindRepeating(
          [](MockDigitalIdentityRequest* mock_request_ptr,
             mojo::ScopedMessagePipeHandle handle) {
            mock_request_ptr->Bind(
                mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                    std::move(handle)));
          },
          Unretained(mock_request_ptr)));

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);

  DiscoverDigitalIdentityCredentialFromExternalSource(
      resolver, *CreateValidGetOptions(context.GetScriptState()));

  test::RunPendingTasks();

  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentials));
  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsSuccess));

  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_, {});
}

// Test that navigator.credentials.create() increments the feature use counter
// when one of the identity providers is a digital identity credential.
TEST_F(DigitalIdentityCredentialTest,
       IdentityDigitalCredentialCreateUseCounter) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsCreationForTest scoped_digital_credentials(
      /*enabled=*/true);

  std::unique_ptr mock_request = std::make_unique<MockDigitalIdentityRequest>();
  auto mock_request_ptr = mock_request.get();
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_,
      BindRepeating(
          [](MockDigitalIdentityRequest* mock_request_ptr,
             mojo::ScopedMessagePipeHandle handle) {
            mock_request_ptr->Bind(
                mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                    std::move(handle)));
          },
          Unretained(mock_request_ptr)));

  ScriptState* script_state = context.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);
  CreateDigitalIdentityCredentialInExternalSource(resolver,
                                                  *CreateValidCreateOptions());

  test::RunPendingTasks();

  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsCreation));
  EXPECT_TRUE(context.GetWindow().document()->IsUseCounted(
      blink::mojom::WebFeature::kIdentityDigitalCredentialsCreationSuccess));

  // Remove the binding for other tests to be able to set their own binding.
  // Otherwise, it will be bound already.
  context.GetWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::DigitalIdentityRequest::Name_, {});
}

// Test that navigator.credentials.get() throws when at least one identity
// request contains data that cannot be JSON-stringified.
// TODO(crbug.com/427885787): add Symbol case and handle them properly.
TEST_F(DigitalIdentityCredentialTest,
       IdentityDigitalCredentialGetThrowsOnUnstringifiableData) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  ScriptState* script_state = context.GetScriptState();
  v8::Local<v8::Value> bad_bigint = v8::BigIntObject::New(
      script_state->GetIsolate(), static_cast<int64_t>(123));

  v8::Local<v8::Object> bad_circular =
      v8::Object::New(script_state->GetIsolate());
  v8::Maybe<bool> maybe = bad_circular->Set(
      script_state->GetContext(), V8String(script_state->GetIsolate(), "self"),
      bad_circular);
  ASSERT_TRUE(maybe.IsJust() || maybe.FromJust());

  std::vector<v8::Local<v8::Value>> bad_values = {bad_bigint, bad_circular};

  for (const auto& value : bad_values) {
    HeapVector<Member<DigitalCredentialGetRequest>> requests;
    DigitalCredentialGetRequest* request =
        DigitalCredentialGetRequest::Create();
    request->setProtocol("openid4vp");
    request->setData(ScriptObject(script_state->GetIsolate(), value));
    requests.push_back(request);

    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
            script_state);

    DiscoverDigitalIdentityCredentialFromExternalSource(
        resolver, *CreateGetOptionsWithRequests(requests));

    ScriptPromiseTester tester(script_state, resolver->Promise());

    tester.WaitUntilSettled();

    ASSERT_TRUE(tester.IsRejected());
  }
}

// Test that navigator.credentials.create() throws when at least one identity
// provider request contains data that cannot be JSON-stringified.
// TODO(crbug.com/427885787): add Symbol case and handle them properly.
TEST_F(DigitalIdentityCredentialTest,
       IdentityDigitalCredentialCreateThrowsOnUnstringifiableData) {
  V8TestingScope context(::blink::KURL("https://example.test"));

  ScopedWebIdentityDigitalCredentialsCreationForTest scoped_digital_credentials(
      /*enabled=*/true);

  ScriptState* script_state = context.GetScriptState();
  v8::Local<v8::Value> bad_bigint = v8::BigIntObject::New(
      script_state->GetIsolate(), static_cast<int64_t>(123));

  v8::Local<v8::Object> bad_circular =
      v8::Object::New(script_state->GetIsolate());
  v8::Maybe<bool> maybe = bad_circular->Set(
      script_state->GetContext(), V8String(script_state->GetIsolate(), "self"),
      bad_circular);
  ASSERT_TRUE(maybe.IsJust() || maybe.FromJust());

  std::vector<v8::Local<v8::Value>> bad_values = {bad_bigint, bad_circular};

  for (const auto& value : bad_values) {
    HeapVector<Member<DigitalCredentialCreateRequest>> requests;
    DigitalCredentialCreateRequest* request =
        DigitalCredentialCreateRequest::Create();
    request->setProtocol("openid4vci");
    request->setData(ScriptObject(script_state->GetIsolate(), value));
    requests.push_back(request);

    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
            script_state);

    CreateDigitalIdentityCredentialInExternalSource(
        resolver, *CreateCreateOptionsWithRequests(requests));

    ScriptPromiseTester tester(script_state, resolver->Promise());

    tester.WaitUntilSettled();

    ASSERT_TRUE(tester.IsRejected());
  }
}

}  // namespace blink
