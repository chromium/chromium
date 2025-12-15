// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
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
#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
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
  void Get(std::vector<blink::mojom::DigitalCredentialGetRequestPtr> requests,
           GetCallback callback) override {
    std::move(callback).Run(mojom::RequestDigitalIdentityStatus::kSuccess,
                            "protocol", base::Value("token"));
  }

  void Create(
      std::vector<blink::mojom::DigitalCredentialCreateRequestPtr> requests,
      CreateCallback callback) override {
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

CredentialRequestOptions* CreateOptionsWithProtocol(ScriptState* script_state,
                                                    const String& protocol) {
  DigitalCredentialGetRequest* request = DigitalCredentialGetRequest::Create();
  request->setProtocol(protocol);
  v8::Local<v8::Object> request_data =
      v8::Object::New(script_state->GetIsolate());

  request->setData(ScriptObject(script_state->GetIsolate(), request_data));
  HeapVector<Member<DigitalCredentialGetRequest>> requests;
  requests.push_back(request);
  return CreateGetOptionsWithRequests(requests);
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

CredentialCreationOptions* CreateCreateOptionsWithProtocol(
    ScriptState* script_state,
    const String& protocol) {
  DigitalCredentialCreateRequest* request =
      DigitalCredentialCreateRequest::Create();
  request->setProtocol(protocol);
  v8::Local<v8::Object> request_data =
      v8::Object::New(script_state->GetIsolate());

  request->setData(ScriptObject(script_state->GetIsolate(), request_data));
  HeapVector<Member<DigitalCredentialCreateRequest>> requests;
  requests.push_back(request);
  return CreateCreateOptionsWithRequests(requests);
}

}  // namespace

class DigitalIdentityCredentialProtocolTest : public PageTestBase {
 public:
  DigitalIdentityCredentialProtocolTest() = default;
  ~DigitalIdentityCredentialProtocolTest() override = default;

  void SetUp() override {
    EnablePlatform();
    PageTestBase::SetUp();

    NavigateTo(KURL("https://example.test"));

    mock_request_ = std::make_unique<MockDigitalIdentityRequest>();
    GetFrame().DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::DigitalIdentityRequest::Name_,
        BindRepeating(
            [](MockDigitalIdentityRequest* mock_request_ptr,
               mojo::ScopedMessagePipeHandle handle) {
              mock_request_ptr->Bind(
                  mojo::PendingReceiver<mojom::DigitalIdentityRequest>(
                      std::move(handle)));
            },
            Unretained(mock_request_.get())));
  }

  void TearDown() override {
    GetFrame().DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::DigitalIdentityRequest::Name_, {});
    PageTestBase::TearDown();
  }

 protected:
  std::unique_ptr<MockDigitalIdentityRequest> mock_request_;
};

TEST_F(DigitalIdentityCredentialProtocolTest, DiscoverProtocolUseCounters) {
  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  struct TestCase {
    String protocol;
    mojom::WebFeature feature;
  };

  TestCase test_cases[] = {
      {"openid4vp-v1-unsigned",
       mojom::WebFeature::kDigitalCredentialsProtocolOpenId4VpUnsigned},
      {"openid4vp-v1-signed",
       mojom::WebFeature::kDigitalCredentialsProtocolOpenId4VpSigned},
      {"openid4vp-v1-multisigned",
       mojom::WebFeature::kDigitalCredentialsProtocolOpenId4VpMultisigned},
      {"org-iso-mdoc",
       mojom::WebFeature::kDigitalCredentialsProtocolOrgIsoMdoc},
  };

  for (const auto& test_case : test_cases) {
    ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
    ScriptState::Scope scope(script_state);
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
            script_state);

    DiscoverDigitalIdentityCredentialFromExternalSource(
        resolver, *CreateOptionsWithProtocol(script_state, test_case.protocol));

    test::RunPendingTasks();

    EXPECT_TRUE(GetDocument().IsUseCounted(test_case.feature))
        << "Feature not counted for protocol: " << test_case.protocol;
  }
}

TEST_F(DigitalIdentityCredentialProtocolTest, CreateProtocolUseCounters) {
  ScopedWebIdentityDigitalCredentialsCreationForTest
      scoped_digital_credentials_creation(
          /*enabled=*/true);

  struct TestCase {
    String protocol;
    mojom::WebFeature feature;
  };

  TestCase test_cases[] = {
      {"openid4vci", mojom::WebFeature::kDigitalCredentialsProtocolOpenId4Vci},
  };

  for (const auto& test_case : test_cases) {
    ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
    ScriptState::Scope scope(script_state);
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
            script_state);

    CreateDigitalIdentityCredentialInExternalSource(
        resolver,
        *CreateCreateOptionsWithProtocol(script_state, test_case.protocol));

    test::RunPendingTasks();

    EXPECT_TRUE(GetDocument().IsUseCounted(test_case.feature))
        << "Feature not counted for protocol: " << test_case.protocol;
  }
}

TEST_F(DigitalIdentityCredentialProtocolTest,
       DiscoverProtocolUseCountersMultipleRequests) {
  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);

  HeapVector<Member<DigitalCredentialGetRequest>> requests;
  {
    DigitalCredentialGetRequest* request =
        DigitalCredentialGetRequest::Create();
    request->setProtocol("org-iso-mdoc");
    v8::Local<v8::Object> request_data =
        v8::Object::New(script_state->GetIsolate());
    request->setData(ScriptObject(script_state->GetIsolate(), request_data));
    requests.push_back(request);
  }
  {
    DigitalCredentialGetRequest* request =
        DigitalCredentialGetRequest::Create();
    request->setProtocol("openid4vp-v1-unsigned");
    v8::Local<v8::Object> request_data =
        v8::Object::New(script_state->GetIsolate());
    request->setData(ScriptObject(script_state->GetIsolate(), request_data));
    requests.push_back(request);
  }

  DiscoverDigitalIdentityCredentialFromExternalSource(
      resolver, *CreateGetOptionsWithRequests(requests));

  test::RunPendingTasks();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      mojom::WebFeature::kDigitalCredentialsProtocolOrgIsoMdoc));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      mojom::WebFeature::kDigitalCredentialsProtocolOpenId4VpUnsigned));
}

TEST_F(DigitalIdentityCredentialProtocolTest,
       DiscoverProtocolUseCountersUnknownProtocol) {
  ScopedWebIdentityDigitalCredentialsForTest scoped_digital_credentials(
      /*enabled=*/true);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);

  DiscoverDigitalIdentityCredentialFromExternalSource(
      resolver, *CreateOptionsWithProtocol(script_state, "unknown-protocol"));

  test::RunPendingTasks();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      mojom::WebFeature::kDigitalCredentialsProtocolUnknown));
}

TEST_F(DigitalIdentityCredentialProtocolTest,
       CreateProtocolUseCountersUnknownProtocol) {
  ScopedWebIdentityDigitalCredentialsCreationForTest
      scoped_digital_credentials_creation(
          /*enabled=*/true);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);

  CreateDigitalIdentityCredentialInExternalSource(
      resolver,
      *CreateCreateOptionsWithProtocol(script_state, "unknown-protocol"));

  test::RunPendingTasks();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      mojom::WebFeature::kDigitalCredentialsProtocolUnknown));
}

}  // namespace blink
