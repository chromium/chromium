// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/authentication_credentials_container.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/gc_object_liveness_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/password_credential.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

class MockCredentialManager : public mojom::blink::CredentialManager {
 public:
  MockCredentialManager() = default;

  MockCredentialManager(const MockCredentialManager&) = delete;
  MockCredentialManager& operator=(const MockCredentialManager&) = delete;

  ~MockCredentialManager() override {}

  void Bind(mojo::PendingReceiver<::blink::mojom::blink::CredentialManager>
                receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &MockCredentialManager::Disconnected, WTF::Unretained(this)));
  }

  void Disconnected() { disconnected_ = true; }

  bool IsDisconnected() const { return disconnected_; }

  void WaitForCallToGet() {
    if (get_callback_)
      return;

    loop_.Run();
  }

  void InvokeGetCallback() {
    EXPECT_TRUE(receiver_.is_bound());

    auto info = blink::mojom::blink::CredentialInfo::New();
    info->type = blink::mojom::blink::CredentialType::EMPTY;
    std::move(get_callback_)
        .Run(blink::mojom::blink::CredentialManagerError::SUCCESS,
             std::move(info));
  }

 protected:
  void Store(blink::mojom::blink::CredentialInfoPtr credential,
             StoreCallback callback) override {}
  void PreventSilentAccess(PreventSilentAccessCallback callback) override {}
  void Get(blink::mojom::blink::CredentialMediationRequirement mediation,
           int requested_credential_types,
           const WTF::Vector<::blink::KURL>& federations,
           GetCallback callback) override {
    get_callback_ = std::move(callback);
    loop_.Quit();
  }

 private:
  mojo::Receiver<::blink::mojom::blink::CredentialManager> receiver_{this};

  GetCallback get_callback_;
  bool disconnected_ = false;
  base::RunLoop loop_;
};

class MockAuthenticatorInterface : public mojom::blink::Authenticator {
 public:
  MockAuthenticatorInterface() { loop_ = std::make_unique<base::RunLoop>(); }

  MockAuthenticatorInterface(const MockAuthenticatorInterface&) = delete;
  MockAuthenticatorInterface& operator=(const MockAuthenticatorInterface&) =
      delete;

  void Bind(
      mojo::PendingReceiver<::blink::mojom::blink::Authenticator> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void WaitForCallToGet() {
    if (get_callback_) {
      return;
    }

    loop_->Run();
  }

  void InvokeGetCallback() {
    EXPECT_TRUE(receiver_.is_bound());
    std::move(get_callback_)
        .Run(blink::mojom::blink::AuthenticatorStatus::NOT_ALLOWED_ERROR,
             nullptr, nullptr);
  }

  void Reset() { loop_ = std::make_unique<base::RunLoop>(); }

 protected:
  void MakeCredential(
      blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr options,
      MakeCredentialCallback callback) override {}
  void GetAssertion(
      blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr options,
      GetAssertionCallback callback) override {
    get_callback_ = std::move(callback);
    loop_->Quit();
  }
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) override {
  }
  void IsConditionalMediationAvailable(
      IsConditionalMediationAvailableCallback callback) override {}
  void Report(blink::mojom::blink::PublicKeyCredentialReportOptionsPtr options,
              ReportCallback callback) override {}
  void GetClientCapabilities(GetClientCapabilitiesCallback callback) override {}
  void Cancel() override {}

 private:
  mojo::Receiver<::blink::mojom::blink::Authenticator> receiver_{this};

  GetAssertionCallback get_callback_;
  std::unique_ptr<base::RunLoop> loop_;
};

class CredentialManagerTestingContext {
  STACK_ALLOCATED();

 public:
  explicit CredentialManagerTestingContext(
      MockCredentialManager* mock_credential_manager,
      MockAuthenticatorInterface* mock_authenticator = nullptr)
      : dummy_context_(KURL("https://example.test")) {
    if (mock_credential_manager) {
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
    if (mock_authenticator) {
      DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
          ::blink::mojom::blink::Authenticator::Name_,
          WTF::BindRepeating(
              [](MockAuthenticatorInterface* mock_authenticator,
                 mojo::ScopedMessagePipeHandle handle) {
                mock_authenticator->Bind(
                    mojo::PendingReceiver<::blink::mojom::blink::Authenticator>(
                        std::move(handle)));
              },
              WTF::Unretained(mock_authenticator)));
    }
  }

  ~CredentialManagerTestingContext() {
    DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        ::blink::mojom::blink::CredentialManager::Name_, {});
    DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        ::blink::mojom::blink::Authenticator::Name_, {});
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
// a persistent handle to a ScriptPromiseResolverBase instance. Ensure that if
// the document is destroyed while a call is pending, it can still be freed up.
TEST(AuthenticationCredentialsContainerTest, PendingGetRequest_NoGCCycles) {
  test::TaskEnvironment task_environment;
  MockCredentialManager mock_credential_manager;
  GCObjectLivenessObserver<Document> document_observer;

  {
    CredentialManagerTestingContext context(&mock_credential_manager);
    document_observer.Observe(context.DomWindow().document());
    AuthenticationCredentialsContainer::credentials(*context.DomWindow().navigator())
        ->get(context.GetScriptState(), CredentialRequestOptions::Create(),
              IGNORE_EXCEPTION_FOR_TESTING);
    mock_credential_manager.WaitForCallToGet();
  }
  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(document_observer.WasCollected());

  mock_credential_manager.InvokeGetCallback();
  ASSERT_TRUE(mock_credential_manager.IsDisconnected());
}

// If the document is detached before the request is resolved, the promise
// should be left unresolved, and there should be no crashes.
TEST(AuthenticationCredentialsContainerTest,
     PendingGetRequest_NoCrashOnResponseAfterDocumentShutdown) {
  test::TaskEnvironment task_environment;
  MockCredentialManager mock_credential_manager;
  CredentialManagerTestingContext context(&mock_credential_manager);

  auto promise =
      AuthenticationCredentialsContainer::credentials(*context.DomWindow().navigator())
          ->get(context.GetScriptState(), CredentialRequestOptions::Create(),
                IGNORE_EXCEPTION_FOR_TESTING);
  mock_credential_manager.WaitForCallToGet();

  context.DomWindow().FrameDestroyed();

  mock_credential_manager.InvokeGetCallback();

  EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());
}

TEST(AuthenticationCredentialsContainerTest, RejectPublicKeyCredentialStoreOperation) {
  test::TaskEnvironment task_environment;
  MockCredentialManager mock_credential_manager;
  CredentialManagerTestingContext context(&mock_credential_manager);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->store(context.GetScriptState(),
                             MakeGarbageCollected<MockPublicKeyCredential>(),
                             IGNORE_EXCEPTION_FOR_TESTING);

  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());
}

TEST(AuthenticationCredentialsContainerTest,
     GetPasswordAndFederatedCredentialUseCounters) {
  test::TaskEnvironment task_environment;
  {
    // Password only.
    MockCredentialManager mock_credential_manager;
    CredentialManagerTestingContext context(&mock_credential_manager);
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kCredentialManagerGetPasswordCredential);
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kCredentialManagerGetLegacyFederatedCredential);
    auto* request_options = CredentialRequestOptions::Create();
    request_options->setPassword(true);
    auto promise = AuthenticationCredentialsContainer::credentials(
                       *context.DomWindow().navigator())
                       ->get(context.GetScriptState(), request_options,
                             IGNORE_EXCEPTION_FOR_TESTING);
    mock_credential_manager.WaitForCallToGet();
    EXPECT_TRUE(context.DomWindow().document()->IsUseCounted(
        WebFeature::kCredentialManagerGetPasswordCredential));
    EXPECT_FALSE(context.DomWindow().document()->IsUseCounted(
        WebFeature::kCredentialManagerGetLegacyFederatedCredential));

    mock_credential_manager.InvokeGetCallback();
  }

  {
    // Federated only.
    MockCredentialManager mock_credential_manager;
    CredentialManagerTestingContext context(&mock_credential_manager);
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kCredentialManagerGetPasswordCredential);
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kCredentialManagerGetLegacyFederatedCredential);
    auto* request_options = CredentialRequestOptions::Create();
    auto* federated_cred_options = FederatedCredentialRequestOptions::Create();
    federated_cred_options->setProviders({"idp.example"});
    request_options->setFederated(federated_cred_options);
    auto promise = AuthenticationCredentialsContainer::credentials(
                       *context.DomWindow().navigator())
                       ->get(context.GetScriptState(), request_options,
                             IGNORE_EXCEPTION_FOR_TESTING);
    mock_credential_manager.WaitForCallToGet();
    EXPECT_FALSE(context.DomWindow().document()->IsUseCounted(
        WebFeature::kCredentialManagerGetPasswordCredential));
    EXPECT_TRUE(context.DomWindow().document()->IsUseCounted(
        WebFeature::kCredentialManagerGetLegacyFederatedCredential));

    mock_credential_manager.InvokeGetCallback();
  }

  {
    // Federated and Password.
    MockCredentialManager mock_credential_manager;
    CredentialManagerTestingContext context(&mock_credential_manager);
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kCredentialManagerGetPasswordCredential);
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kCredentialManagerGetLegacyFederatedCredential);
    auto* request_options = CredentialRequestOptions::Create();
    auto* federated_cred_options = FederatedCredentialRequestOptions::Create();
    federated_cred_options->setProviders({"idp.example"});
    request_options->setFederated(federated_cred_options);
    request_options->setPassword(true);
    auto promise = AuthenticationCredentialsContainer::credentials(
                       *context.DomWindow().navigator())
                       ->get(context.GetScriptState(), request_options,
                             IGNORE_EXCEPTION_FOR_TESTING);
    mock_credential_manager.WaitForCallToGet();
    EXPECT_TRUE(context.DomWindow().document()->IsUseCounted(
        WebFeature::kCredentialManagerGetPasswordCredential));
    EXPECT_TRUE(context.DomWindow().document()->IsUseCounted(
        WebFeature::kCredentialManagerGetLegacyFederatedCredential));

    mock_credential_manager.InvokeGetCallback();
  }

  {
    // Federated and Password but empty federated providers.
    MockCredentialManager mock_credential_manager;
    CredentialManagerTestingContext context(&mock_credential_manager);
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kCredentialManagerGetPasswordCredential);
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kCredentialManagerGetLegacyFederatedCredential);
    auto* request_options = CredentialRequestOptions::Create();
    auto* federated_cred_options = FederatedCredentialRequestOptions::Create();
    federated_cred_options->setProviders({});
    request_options->setFederated(federated_cred_options);
    request_options->setPassword(true);
    auto promise = AuthenticationCredentialsContainer::credentials(
                       *context.DomWindow().navigator())
                       ->get(context.GetScriptState(), request_options,
                             IGNORE_EXCEPTION_FOR_TESTING);
    mock_credential_manager.WaitForCallToGet();
    EXPECT_TRUE(context.DomWindow().document()->IsUseCounted(
        WebFeature::kCredentialManagerGetPasswordCredential));
    EXPECT_FALSE(context.DomWindow().document()->IsUseCounted(
        WebFeature::kCredentialManagerGetLegacyFederatedCredential));

    mock_credential_manager.InvokeGetCallback();
  }
}

TEST(AuthenticationCredentialsContainerTest, PublicKeyConditionalMediationUkm) {
  test::TaskEnvironment task_environment;

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  ukm::TestAutoSetUkmRecorder recorder;
  context.DomWindow().document()->View()->ResetUkmAggregatorForTesting();

  auto* request_options = CredentialRequestOptions::Create();
  request_options->setMediation("conditional");
  auto* public_key_request_options =
      PublicKeyCredentialRequestOptions::Create();
  public_key_request_options->setTimeout(10000);
  public_key_request_options->setRpId("https://www.example.com");
  public_key_request_options->setUserVerification("preferred");
  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_request_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));
  request_options->setPublicKey(public_key_request_options);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), request_options,
                           IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToGet();

  auto entries = recorder.GetEntriesByName("WebAuthn.ConditionalUiGetCall");
  ASSERT_EQ(entries.size(), 1u);

  mock_authenticator.InvokeGetCallback();
  mock_authenticator.Reset();

  // Verify that a second request does not get reported.
  promise = AuthenticationCredentialsContainer::credentials(
                *context.DomWindow().navigator())
                ->get(context.GetScriptState(), request_options,
                      IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToGet();

  entries = recorder.GetEntriesByName("WebAuthn.ConditionalUiGetCall");
  ASSERT_EQ(entries.size(), 1u);

  mock_authenticator.InvokeGetCallback();
}

class AuthenticationCredentialsContainerActiveModeMultiIdpTest
    : public testing::Test,
      private ScopedFedCmMultipleIdentityProvidersForTest,
      ScopedFedCmButtonModeForTest {
 protected:
  AuthenticationCredentialsContainerActiveModeMultiIdpTest()
      : ScopedFedCmMultipleIdentityProvidersForTest(true),
        ScopedFedCmButtonModeForTest(true) {}
};

TEST_F(AuthenticationCredentialsContainerActiveModeMultiIdpTest,
       RejectActiveModeWithMultipleIdps) {
  test::TaskEnvironment task_environment;
  MockCredentialManager mock_credential_manager;
  CredentialManagerTestingContext context(&mock_credential_manager);

  CredentialRequestOptions* options = CredentialRequestOptions::Create();
  IdentityCredentialRequestOptions* identity =
      IdentityCredentialRequestOptions::Create();

  auto* idp1 = IdentityProviderRequestOptions::Create();
  idp1->setConfigURL("https://idp1.example/config.json");
  idp1->setClientId("clientId");

  auto* idp2 = IdentityProviderRequestOptions::Create();
  idp2->setConfigURL("https://idp2.example/config.json");
  idp2->setClientId("clientId");

  identity->setProviders({idp1, idp2});
  identity->setMode("active");
  options->setIdentity(identity);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), options,
                           IGNORE_EXCEPTION_FOR_TESTING);

  task_environment.RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());
}

}  // namespace blink
