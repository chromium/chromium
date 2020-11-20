// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/credentials_container.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/credentialmanager/credential_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/gc_object_liveness_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanager/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanager/password_credential.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class MockCredentialManager : public mojom::blink::CredentialManager {
 public:
  MockCredentialManager() {}
  ~MockCredentialManager() override {}

  void Bind(mojo::PendingReceiver<::blink::mojom::blink::CredentialManager>
                receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void WaitForConnectionError() {
    if (!receiver_.is_bound())
      return;

    receiver_.set_disconnect_handler(WTF::Bind(&test::ExitRunLoop));
    test::EnterRunLoop();
  }

  void WaitForCallToGet() {
    if (get_callback_)
      return;

    test::EnterRunLoop();
  }

  void InvokeGetCallback() {
    EXPECT_TRUE(receiver_.is_bound());

    auto info = blink::mojom::blink::CredentialInfo::New();
    info->type = blink::mojom::blink::CredentialType::EMPTY;
    info->federation = SecurityOrigin::CreateUniqueOpaque();
    std::move(get_callback_)
        .Run(blink::mojom::blink::CredentialManagerError::SUCCESS,
             std::move(info));
  }

 protected:
  void Store(blink::mojom::blink::CredentialInfoPtr credential,
             StoreCallback callback) override {}
  void PreventSilentAccess(PreventSilentAccessCallback callback) override {}
  void Get(blink::mojom::blink::CredentialMediationRequirement mediation,
           bool include_passwords,
           const WTF::Vector<::blink::KURL>& federations,
           GetCallback callback) override {
    get_callback_ = std::move(callback);
    test::ExitRunLoop();
  }

 private:
  mojo::Receiver<::blink::mojom::blink::CredentialManager> receiver_{this};

  GetCallback get_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockCredentialManager);
};

class CredentialManagerTestingContext {
  STACK_ALLOCATED();

 public:
  CredentialManagerTestingContext(
      MockCredentialManager* mock_credential_manager)
      : dummy_context_(KURL("https://example.test")) {
    DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        ::blink::mojom::blink::CredentialManager::Name_,
        WTF::BindRepeating(
            [](MockCredentialManager* mock_credential_manager,
               mojo::ScopedMessagePipeHandle handle) {
              mock_credential_manager->Bind(
                  mojo::PendingReceiver<
                      ::blink::mojom::blink::CredentialManager>(
                      std::move(handle)));
            },
            WTF::Unretained(mock_credential_manager)));
  }

  ~CredentialManagerTestingContext() {
    DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        ::blink::mojom::blink::CredentialManager::Name_, {});
  }

  LocalDOMWindow& DomWindow() { return dummy_context_.GetWindow(); }
  ScriptState* GetScriptState() { return dummy_context_.GetScriptState(); }

 private:
  V8TestingScope dummy_context_;
};

}  // namespace

class MockPublicKeyCredential : public Credential {
 public:
  MockPublicKeyCredential() : Credential("test", "public-key") {}
  bool IsPublicKeyCredential() const override { return true; }
};

// The completion callbacks for pending mojom::CredentialManager calls each own
// a persistent handle to a ScriptPromiseResolver instance. Ensure that if the
// document is destored while a call is pending, it can still be freed up.
TEST(CredentialsContainerTest, PendingGetRequest_NoGCCycles) {
  MockCredentialManager mock_credential_manager;
  GCObjectLivenessObserver<Document> document_observer;

  {
    CredentialManagerTestingContext context(&mock_credential_manager);
    document_observer.Observe(context.DomWindow().document());
    CredentialsContainer::credentials(*context.DomWindow().navigator())
        ->get(context.GetScriptState(), CredentialRequestOptions::Create());
    mock_credential_manager.WaitForCallToGet();
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(document_observer.WasCollected());

  mock_credential_manager.InvokeGetCallback();
  mock_credential_manager.WaitForConnectionError();
}

// If the document is detached before the request is resolved, the promise
// should be left unresolved, and there should be no crashes.
TEST(CredentialsContainerTest,
     PendingGetRequest_NoCrashOnResponseAfterDocumentShutdown) {
  MockCredentialManager mock_credential_manager;
  CredentialManagerTestingContext context(&mock_credential_manager);

  auto promise =
      CredentialsContainer::credentials(*context.DomWindow().navigator())
          ->get(context.GetScriptState(), CredentialRequestOptions::Create());
  mock_credential_manager.WaitForCallToGet();

  context.DomWindow().FrameDestroyed();

  mock_credential_manager.InvokeGetCallback();

  EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());
}

TEST(CredentialsContainerTest, RejectPublicKeyCredentialStoreOperation) {
  MockCredentialManager mock_credential_manager;
  CredentialManagerTestingContext context(&mock_credential_manager);

  auto promise =
      CredentialsContainer::credentials(*context.DomWindow().navigator())
          ->store(context.GetScriptState(),
                  MakeGarbageCollected<MockPublicKeyCredential>());

  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());
}

TEST(CredentialsContainerTest,
     RejectStoringPasswordCredentialWithInvalidIconURL) {
  MockCredentialManager mock_credential_manager;
  CredentialManagerTestingContext context(&mock_credential_manager);

  KURL invalid_url("an invalid URL");
  auto* credential = MakeGarbageCollected<PasswordCredential>(
      "id", "password", "name", invalid_url);

  auto promise =
      CredentialsContainer::credentials(*context.DomWindow().navigator())
          ->store(context.GetScriptState(), credential);

  auto v8promise = promise.V8Promise();
  EXPECT_EQ(v8::Promise::kRejected, v8promise->State());

  auto* exception = ToScriptWrappable(v8promise->Result().As<v8::Object>())
                        ->ToImpl<DOMException>();
  EXPECT_EQ("SecurityError", exception->name());
  EXPECT_EQ("'iconURL' should be a secure URL", exception->message());
}

TEST(CredentialsContainerTest,
     RejectStoringFederatedCredentialWithInvalidIconURL) {
  MockCredentialManager mock_credential_manager;
  CredentialManagerTestingContext context(&mock_credential_manager);

  KURL invalid_url("an invalid URL");
  auto origin = SecurityOrigin::CreateFromString("https://example.test");
  auto* credential = MakeGarbageCollected<FederatedCredential>(
      "id", origin, "name", invalid_url);

  auto promise =
      CredentialsContainer::credentials(*context.DomWindow().navigator())
          ->store(context.GetScriptState(), credential);

  auto v8promise = promise.V8Promise();
  EXPECT_EQ(v8::Promise::kRejected, v8promise->State());

  auto* exception = ToScriptWrappable(v8promise->Result().As<v8::Object>())
                        ->ToImpl<DOMException>();
  EXPECT_EQ("SecurityError", exception->name());
  EXPECT_EQ("'iconURL' should be a secure URL", exception->message());
}

TEST(CredentialsContainerTest,
     RejectCreatingPublicKeyCredentialWithInvalidIconURL) {
  MockCredentialManager mock_credential_manager;
  CredentialManagerTestingContext context(&mock_credential_manager);

  auto* rp_options = PublicKeyCredentialRpEntity::Create();
  rp_options->setId("example.test");
  rp_options->setName("Example RP");

  auto* user_options = PublicKeyCredentialUserEntity::Create();
  int dummy_buffer_source = 1;
  auto dummy_buffer =
      ArrayBufferOrArrayBufferView::FromArrayBuffer(DOMArrayBuffer::Create(
          &dummy_buffer_source, sizeof(dummy_buffer_source)));
  user_options->setId(dummy_buffer);
  user_options->setIcon("invalid URL");

  auto* public_key_options = PublicKeyCredentialCreationOptions::Create();
  public_key_options->setChallenge(dummy_buffer);
  public_key_options->setUser(user_options);
  public_key_options->setRp(rp_options);
  public_key_options->setAttestation("none");

  auto* public_key_param = PublicKeyCredentialParameters::Create();
  public_key_param->setAlg(1);
  public_key_param->setType("public-key");
  auto params = HeapVector<Member<PublicKeyCredentialParameters>>();
  params.push_back(public_key_param);
  public_key_options->setPubKeyCredParams(params);

  auto* credential_options = CredentialCreationOptions::Create();
  credential_options->setPublicKey(public_key_options);

  auto promise =
      CredentialsContainer::credentials(*context.DomWindow().navigator())
          ->create(context.GetScriptState(), credential_options,
                   IGNORE_EXCEPTION_FOR_TESTING);

  auto v8promise = promise.V8Promise();
  EXPECT_EQ(v8::Promise::kRejected, v8promise->State());

  auto* exception = ToScriptWrappable(v8promise->Result().As<v8::Object>())
                        ->ToImpl<DOMException>();
  EXPECT_EQ("SecurityError", exception->name());
  EXPECT_EQ("'user.icon' should be a secure URL", exception->message());
}

}  // namespace blink
