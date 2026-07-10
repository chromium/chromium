// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/authentication_credentials_container.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "components/ukm/test_ukm_recorder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_cmtg_key_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authenticator_selection_criteria.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_ui_mode_requirement.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/gc_object_liveness_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/password_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"
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
    receiver_.set_disconnect_handler(
        BindOnce(&MockCredentialManager::Disconnected, Unretained(this)));
  }

  void Disconnected() { disconnected_ = true; }

  bool IsDisconnected() const { return disconnected_; }

  void WaitForCallToGet() {
    if (get_callback_) {
      return;
    }

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
           bool include_passwords,
           const Vector<::blink::KURL>& federations,
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
    auto assertion_response = mojom::blink::GetAssertionResponse::New(
        blink::mojom::blink::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
        nullptr);
    auto credential_response =
        mojom::blink::GetCredentialResponse::NewGetAssertionResponse(
            std::move(assertion_response));
    std::move(get_callback_).Run(std::move(credential_response));
  }

  void InvokeGetCallbackWithCrossDeviceFallback() {
    EXPECT_TRUE(receiver_.is_bound());
    auto assertion_response = mojom::blink::GetAssertionResponse::New(
        blink::mojom::blink::AuthenticatorStatus::CROSS_DEVICE_FALLBACK,
        nullptr, nullptr);
    auto credential_response =
        mojom::blink::GetCredentialResponse::NewGetAssertionResponse(
            std::move(assertion_response));
    std::move(get_callback_).Run(std::move(credential_response));
  }

  void Reset() {
    loop_ = std::make_unique<base::RunLoop>();
    last_mediation_ = std::nullopt;
    make_credential_callback_.Reset();
    last_creation_options_.reset();
    last_get_options_.reset();
  }

  std::optional<mojom::blink::Mediation> last_mediation() const {
    return last_mediation_;
  }

  const mojom::blink::PublicKeyCredentialCreationOptionsPtr&
  last_creation_options() const {
    return last_creation_options_;
  }

  const mojom::blink::GetCredentialOptionsPtr& last_get_options() const {
    return last_get_options_;
  }

  void WaitForCallToMakeCredential() {
    if (make_credential_callback_) {
      return;
    }
    loop_->Run();
  }

  void InvokeMakeCredentialCallback() {
    EXPECT_TRUE(receiver_.is_bound());
    std::move(make_credential_callback_)
        .Run(mojom::blink::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
             nullptr);
  }

  void InvokeMakeCredentialSuccessCallback() {
    EXPECT_TRUE(receiver_.is_bound());
    auto info = mojom::blink::CommonCredentialInfo::New();
    info->id = "id";
    info->raw_id = Vector<uint8_t>{1, 2, 3, 4};
    info->client_data_json = Vector<uint8_t>{5, 6, 7, 8};
    info->authenticator_data = Vector<uint8_t>{9, 10, 11, 12};
    auto response = mojom::blink::MakeCredentialAuthenticatorResponse::New();
    response->info = std::move(info);
    response->attestation_object = Vector<uint8_t>{13, 14, 15, 16};
    std::move(make_credential_callback_)
        .Run(mojom::blink::AuthenticatorStatus::SUCCESS, std::move(response),
             nullptr);
  }

  void InvokeMakeCredentialSuccessWithCmtgKeyCallback(
      Vector<uint8_t> cmtg_key_val,
      Vector<uint8_t> signature_val) {
    EXPECT_TRUE(receiver_.is_bound());
    auto info = mojom::blink::CommonCredentialInfo::New();
    info->id = "id";
    info->raw_id = Vector<uint8_t>{1, 2, 3, 4};
    info->client_data_json = Vector<uint8_t>{5, 6, 7, 8};
    info->authenticator_data = Vector<uint8_t>{9, 10, 11, 12};
    auto response = mojom::blink::MakeCredentialAuthenticatorResponse::New();
    response->info = std::move(info);
    response->attestation_object = Vector<uint8_t>{13, 14, 15, 16};
    response->cmtg_key = mojom::blink::CmtgKeyResponse::New(
        std::move(cmtg_key_val), std::move(signature_val));
    std::move(make_credential_callback_)
        .Run(mojom::blink::AuthenticatorStatus::SUCCESS, std::move(response),
             nullptr);
  }

  void InvokeGetAssertionSuccessWithCmtgKeyCallback(
      Vector<uint8_t> cmtg_key_val,
      Vector<uint8_t> signature_val) {
    EXPECT_TRUE(receiver_.is_bound());
    auto info = mojom::blink::CommonCredentialInfo::New();
    info->id = "id";
    info->raw_id = Vector<uint8_t>{1, 2, 3, 4};
    info->client_data_json = Vector<uint8_t>{5, 6, 7, 8};
    info->authenticator_data = Vector<uint8_t>{9, 10, 11, 12};

    auto response = mojom::blink::GetAssertionAuthenticatorResponse::New();
    response->info = std::move(info);
    response->signature = Vector<uint8_t>{13, 14, 15, 16};

    auto cmtg_response = mojom::blink::CmtgKeyResponse::New(
        std::move(cmtg_key_val), std::move(signature_val));
    response->extensions =
        mojom::blink::AuthenticationExtensionsClientOutputs::New();
    response->extensions->cmtg_key = std::move(cmtg_response);

    auto assertion_response = mojom::blink::GetAssertionResponse::New(
        blink::mojom::blink::AuthenticatorStatus::SUCCESS, std::move(response),
        nullptr);
    auto credential_response =
        mojom::blink::GetCredentialResponse::NewGetAssertionResponse(
            std::move(assertion_response));
    std::move(get_callback_).Run(std::move(credential_response));
  }

  void InvokeGetAssertionSuccessWithCrossDeviceFallbackUrlCallback(
      bool cross_device_fallback_val) {
    EXPECT_TRUE(receiver_.is_bound());
    auto info = mojom::blink::CommonCredentialInfo::New();
    info->id = "id";
    info->raw_id = Vector<uint8_t>{1, 2, 3, 4};
    info->client_data_json = Vector<uint8_t>{5, 6, 7, 8};
    info->authenticator_data = Vector<uint8_t>{9, 10, 11, 12};

    auto response = mojom::blink::GetAssertionAuthenticatorResponse::New();
    response->info = std::move(info);
    response->signature = Vector<uint8_t>{13, 14, 15, 16};

    response->extensions =
        mojom::blink::AuthenticationExtensionsClientOutputs::New();
    response->extensions->cross_device_fallback_url = cross_device_fallback_val;

    auto assertion_response = mojom::blink::GetAssertionResponse::New(
        blink::mojom::blink::AuthenticatorStatus::SUCCESS, std::move(response),
        nullptr);
    auto credential_response =
        mojom::blink::GetCredentialResponse::NewGetAssertionResponse(
            std::move(assertion_response));
    std::move(get_callback_).Run(std::move(credential_response));
  }

 protected:
  void MakeCredential(
      blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr options,
      MakeCredentialCallback callback) override {
    last_creation_options_ = std::move(options);
    make_credential_callback_ = std::move(callback);
    loop_->Quit();
  }
  void GetCredential(blink::mojom::blink::GetCredentialOptionsPtr options,
                     GetCredentialCallback callback) override {
    last_get_options_ = std::move(options);
    last_mediation_ = last_get_options_->mediation;
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

  GetCredentialCallback get_callback_;
  MakeCredentialCallback make_credential_callback_;
  std::unique_ptr<base::RunLoop> loop_;
  std::optional<mojom::blink::Mediation> last_mediation_;
  mojom::blink::PublicKeyCredentialCreationOptionsPtr last_creation_options_;
  mojom::blink::GetCredentialOptionsPtr last_get_options_;
};

class MockFederatedRequestService
    : public mojom::blink::FederatedRequestService {
 public:
  MockFederatedRequestService() = default;

  MockFederatedRequestService(const MockFederatedRequestService&) = delete;
  MockFederatedRequestService& operator=(const MockFederatedRequestService&) =
      delete;

  ~MockFederatedRequestService() override = default;

  void BindRequestService(
      mojo::PendingReceiver<::blink::mojom::blink::FederatedRequestService>
          receiver) {
    request_service_receiver_.Bind(std::move(receiver));
    request_service_receiver_.set_disconnect_handler(
        BindOnce(&MockFederatedRequestService::Disconnected, Unretained(this)));
  }

  void Disconnected() { disconnected_ = true; }

  bool IsDisconnected() const { return disconnected_; }

  size_t GetTotalPendingCallbacks() const {
    return start_token_request_callbacks_.size();
  }

  void WaitForCallToStartToken(size_t count = 1) {
    if (GetTotalPendingCallbacks() >= count) {
      return;
    }
    expected_callbacks_ = count;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void InvokeStartTokenRequestCallback(wtf_size_t index = 0) {
    EXPECT_TRUE(request_service_receiver_.is_bound());
    EXPECT_LT(index, start_token_request_callbacks_.size());
    auto [callback, receiver] =
        std::move(start_token_request_callbacks_[index]);
    start_token_request_callbacks_.EraseAt(index);

    auto success = mojom::blink::TokenRequestSuccess::New();
    success->selected_idp_config_url = KURL("https://idp.example");
    success->token = base::Value("token");
    success->is_auto_selected = false;

    std::move(callback).Run(std::move(success));
  }

  void InvokeStartTokenRequestCallbackWithError(
      mojom::RequestTokenStatus status,
      wtf_size_t index = 0) {
    EXPECT_TRUE(request_service_receiver_.is_bound());
    EXPECT_LT(index, start_token_request_callbacks_.size());
    auto [callback, receiver] =
        std::move(start_token_request_callbacks_[index]);
    start_token_request_callbacks_.EraseAt(index);

    auto failure = mojom::blink::TokenRequestFailure::New();
    failure->status = status;
    failure->error = nullptr;

    std::move(callback).Run(base::unexpected(std::move(failure)));
  }

 protected:
  void StartTokenRequest(
      Vector<blink::mojom::blink::IdentityProviderGetParametersPtr>
          idp_get_params_ptrs,
      mojom::CredentialMediationRequirement requirement,
      mojo::PendingReceiver<mojom::blink::FederatedRequest> request_receiver,
      StartTokenRequestCallback callback) override {
    start_token_request_callbacks_.push_back(
        std::make_pair(std::move(callback), std::move(request_receiver)));

    if (GetTotalPendingCallbacks() >= expected_callbacks_ && quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void RequestUserInfo(
      mojom::blink::IdentityProviderConfigPtr provider,
      mojom::blink::FederatedRequestService::RequestUserInfoCallback callback)
      override {}

  void ResolveTokenRequest(
      const String& account_id,
      mojom::blink::ResolveTokenParamsPtr params,
      mojom::blink::FederatedRequestService::ResolveTokenRequestCallback
          callback) override {}
  void SetIdpSigninStatus(
      const ::scoped_refptr<const ::blink::SecurityOrigin>& origin,
      mojom::IdpSigninStatus status,
      mojom::blink::LoginStatusOptionsPtr options,
      mojom::blink::FederatedRequestService::SetIdpSigninStatusCallback
          callback) override {}
  void RegisterIdP(const ::blink::KURL& url,
                   mojom::blink::FederatedRequestService::RegisterIdPCallback
                       callback) override {}
  void UnregisterIdP(
      const ::blink::KURL& url,
      mojom::blink::FederatedRequestService::UnregisterIdPCallback callback)
      override {}
  void CloseModalDialogView() override {}
  void PreventSilentAccess(
      mojom::blink::FederatedRequestService::PreventSilentAccessCallback
          callback) override {}
  void Disconnect(mojom::blink::IdentityCredentialDisconnectOptionsPtr options,
                  mojom::blink::FederatedRequestService::DisconnectCallback
                      callback) override {}

 private:
  mojo::Receiver<::blink::mojom::blink::FederatedRequestService>
      request_service_receiver_{this};

  Vector<std::pair<StartTokenRequestCallback,
                   mojo::PendingReceiver<mojom::blink::FederatedRequest>>>
      start_token_request_callbacks_;
  size_t expected_callbacks_ = 1;
  base::OnceClosure quit_closure_;
  bool disconnected_ = false;
};

class CredentialManagerTestingContext {
  STACK_ALLOCATED();

 public:
  explicit CredentialManagerTestingContext(
      MockCredentialManager* mock_credential_manager,
      MockAuthenticatorInterface* mock_authenticator = nullptr,
      MockFederatedRequestService* mock_federated_request_service = nullptr)
      : dummy_context_(KURL("https://example.test")) {
    if (mock_credential_manager) {
      DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
          ::blink::mojom::blink::CredentialManager::Name_,
          BindRepeating(
              [](MockCredentialManager* mock_credential_manager,
                 mojo::ScopedMessagePipeHandle handle) {
                mock_credential_manager->Bind(
                    mojo::PendingReceiver<
                        ::blink::mojom::blink::CredentialManager>(
                        std::move(handle)));
              },
              Unretained(mock_credential_manager)));
    }
    if (mock_authenticator) {
      DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
          ::blink::mojom::blink::Authenticator::Name_,
          BindRepeating(
              [](MockAuthenticatorInterface* mock_authenticator,
                 mojo::ScopedMessagePipeHandle handle) {
                mock_authenticator->Bind(
                    mojo::PendingReceiver<::blink::mojom::blink::Authenticator>(
                        std::move(handle)));
              },
              Unretained(mock_authenticator)));
    }
    if (mock_federated_request_service) {
      DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
          ::blink::mojom::blink::FederatedRequestService::Name_,
          BindRepeating(
              [](MockFederatedRequestService* mock_federated_request_service,
                 mojo::ScopedMessagePipeHandle handle) {
                mock_federated_request_service->BindRequestService(
                    mojo::PendingReceiver<
                        ::blink::mojom::blink::FederatedRequestService>(
                        std::move(handle)));
              },
              Unretained(mock_federated_request_service)));
    }
  }

  ~CredentialManagerTestingContext() {
    DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        ::blink::mojom::blink::CredentialManager::Name_, {});
    DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        ::blink::mojom::blink::Authenticator::Name_, {});
    DomWindow().GetBrowserInterfaceBroker().SetBinderForTesting(
        ::blink::mojom::blink::FederatedRequestService::Name_, {});
  }

  LocalDOMWindow& DomWindow() { return dummy_context_.GetWindow(); }
  ScriptState* GetScriptState() { return dummy_context_.GetScriptState(); }

 private:
  V8TestingScope dummy_context_;
};

}  // namespace

TEST(AuthenticationCredentialsContainerTest, FedCmDisabledRejectsPromise) {
  test::TaskEnvironment task_environment;
  ScopedFedCmForTest fedcm_disabled(false);

  MockFederatedRequestService mock_federated_request_service;
  CredentialManagerTestingContext context(
      /*mock_credential_manager=*/nullptr, /*mock_authenticator=*/nullptr,
      /*mock_federated_request_service=*/&mock_federated_request_service);

  CredentialRequestOptions* options = CredentialRequestOptions::Create();
  IdentityCredentialRequestOptions* identity =
      IdentityCredentialRequestOptions::Create();
  auto* idp = IdentityProviderRequestOptions::Create();
  idp->setConfigURL("https://idp.example/config.json");
  idp->setClientId("clientId");
  identity->setProviders({idp});
  options->setIdentity(identity);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), options,
                           IGNORE_EXCEPTION_FOR_TESTING);

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "NotSupportedError");
  EXPECT_EQ(exception->message(), "FedCM is not supported.");
}

class MockPublicKeyCredential : public Credential {
 public:
  MockPublicKeyCredential() : Credential("test", "public-key") {}
  bool IsPublicKeyCredential() const override { return true; }
};

TEST(AuthenticationCredentialsContainerTest, GetRequest_DefaultOptions) {
  test::TaskEnvironment task_environment;
  MockCredentialManager mock_credential_manager;
  CredentialManagerTestingContext context(&mock_credential_manager);

  // Calling get() with default options (no credential type set) should reject
  // with NotSupportedError.
  CredentialRequestOptions* options = CredentialRequestOptions::Create();
  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), options,
                           IGNORE_EXCEPTION_FOR_TESTING);

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "NotSupportedError");
  EXPECT_EQ(exception->message(),
            "No credential type was specified in the request.");
}

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
    CredentialRequestOptions* options = CredentialRequestOptions::Create();
    options->setPassword(true);
    AuthenticationCredentialsContainer::credentials(
        *context.DomWindow().navigator())
        ->get(context.GetScriptState(), options, IGNORE_EXCEPTION_FOR_TESTING);
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

  CredentialRequestOptions* options = CredentialRequestOptions::Create();
  options->setPassword(true);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), options,
                           IGNORE_EXCEPTION_FOR_TESTING);
  mock_credential_manager.WaitForCallToGet();

  context.DomWindow().FrameDestroyed();

  mock_credential_manager.InvokeGetCallback();

  EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());
}

TEST(AuthenticationCredentialsContainerTest,
     RejectPublicKeyCredentialStoreOperation) {
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
    AuthenticationCredentialsContainer::credentials(
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
    AuthenticationCredentialsContainer::credentials(
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
    AuthenticationCredentialsContainer::credentials(
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
    AuthenticationCredentialsContainer::credentials(
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
  request_options->setMediation(
      V8CredentialMediationRequirement::Enum::kConditional);
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
      private ScopedFedCmMultipleIdentityProvidersForTest {
 protected:
  AuthenticationCredentialsContainerActiveModeMultiIdpTest()
      : ScopedFedCmMultipleIdentityProvidersForTest(true) {}
};

TEST_F(AuthenticationCredentialsContainerActiveModeMultiIdpTest,
       RejectActiveModeWithMultipleIdps) {
  test::TaskEnvironment task_environment;
  MockFederatedRequestService mock_federated_request_service;
  CredentialManagerTestingContext context(
      /*mock_credential_manager=*/nullptr, /*mock_authenticator=*/nullptr,
      /*mock_federated_request_service=*/&mock_federated_request_service);

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
  identity->setMode(V8IdentityCredentialRequestOptionsMode::Enum::kActive);
  options->setIdentity(identity);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), options,
                           IGNORE_EXCEPTION_FOR_TESTING);

  task_environment.RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());
}

TEST(AuthenticationCredentialsContainerTest,
     WebAuthenticationUiModeImmediateRequiresUserActivation) {
  test::TaskEnvironment task_environment;

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  auto* request_options = CredentialRequestOptions::Create();
  request_options->setUiMode(V8CredentialUiModeRequirement::Enum::kImmediate);
  auto* public_key_request_options =
      PublicKeyCredentialRequestOptions::Create();
  public_key_request_options->setRpId("https://www.example.com");
  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_request_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));
  request_options->setPublicKey(public_key_request_options);

  // No user activation, so it should be rejected.
  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), request_options,
                           IGNORE_EXCEPTION_FOR_TESTING);

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "NotAllowedError");
  EXPECT_EQ(exception->message(),
            "A user activation is required to request immediate credentials.");
}

TEST(AuthenticationCredentialsContainerTest,
     WebAuthenticationUiModeImmediateWithUserActivation) {
  test::TaskEnvironment task_environment;

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  auto* request_options = CredentialRequestOptions::Create();
  request_options->setUiMode(V8CredentialUiModeRequirement::Enum::kImmediate);
  auto* public_key_request_options =
      PublicKeyCredentialRequestOptions::Create();
  public_key_request_options->setRpId("https://www.example.com");
  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_request_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));
  request_options->setPublicKey(public_key_request_options);

  LocalFrame::NotifyUserActivation(
      context.DomWindow().GetFrame(),
      mojom::blink::UserActivationNotificationType::kTest);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), request_options,
                           IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToGet();
  mock_authenticator.InvokeGetCallback();

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  // Rejection from MockAuthenticatorInterface::InvokeGetCallback
  EXPECT_EQ(exception->name(), "NotAllowedError");
}

TEST(AuthenticationCredentialsContainerTest,
     WebAuthenticationUiModeImmediateIncompatibleWithConditionalMediation) {
  test::TaskEnvironment task_environment;

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  auto* request_options = CredentialRequestOptions::Create();
  request_options->setUiMode(V8CredentialUiModeRequirement::Enum::kImmediate);
  request_options->setMediation(
      V8CredentialMediationRequirement::Enum::kConditional);
  auto* public_key_request_options =
      PublicKeyCredentialRequestOptions::Create();
  public_key_request_options->setRpId("https://www.example.com");
  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_request_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));
  request_options->setPublicKey(public_key_request_options);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), request_options,
                           IGNORE_EXCEPTION_FOR_TESTING);

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "NotSupportedError");
  EXPECT_EQ(exception->message(),
            "Immediate uiMode is not compatible with conditional mediation");
}

TEST(AuthenticationCredentialsContainerTest,
     WebAuthenticationUiModeImmediateIncompatibleWithAllowCredentials) {
  test::TaskEnvironment task_environment;

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  auto* request_options = CredentialRequestOptions::Create();
  request_options->setUiMode(V8CredentialUiModeRequirement::Enum::kImmediate);
  auto* public_key_request_options =
      PublicKeyCredentialRequestOptions::Create();
  public_key_request_options->setRpId("https://www.example.com");
  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_request_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));

  auto* credential = PublicKeyCredentialDescriptor::Create();
  credential->setId(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      DOMArrayBuffer::Create(challenge)));
  credential->setType("public-key");
  public_key_request_options->setAllowCredentials({credential});

  request_options->setPublicKey(public_key_request_options);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), request_options,
                           IGNORE_EXCEPTION_FOR_TESTING);

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "NotAllowedError");
  EXPECT_EQ(exception->message(),
            "An allowCredentials is not allowed with immediate mediation.");
}

TEST(AuthenticationCredentialsContainerTest, PublicKeyCspMetric) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  // Set CSP to block connections to everything except self.
  context.DomWindow().GetContentSecurityPolicy()->AddPolicies(
      ParseContentSecurityPolicies(
          "connect-src 'self'",
          network::mojom::blink::ContentSecurityPolicyType::kEnforce,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP,
          KURL("https://example.test")));

  auto* request_options = CredentialRequestOptions::Create();
  auto* public_key_request_options =
      PublicKeyCredentialRequestOptions::Create();
  // 'self' is example.test.
  public_key_request_options->setRpId("example.test");

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
  mock_authenticator.InvokeGetCallback();

  histogram_tester.ExpectUniqueSample("WebAuthentication.CspAllow.Get", true,
                                      1);

  // Now try one that is blocked.
  mock_authenticator.Reset();
  public_key_request_options->setRpId("blocked.com");
  promise = AuthenticationCredentialsContainer::credentials(
                *context.DomWindow().navigator())
                ->get(context.GetScriptState(), request_options,
                      IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToGet();
  mock_authenticator.InvokeGetCallback();

  histogram_tester.ExpectBucketCount("WebAuthentication.CspAllow.Get", false,
                                     1);
  histogram_tester.ExpectTotalCount("WebAuthentication.CspAllow.Get", 2);
}

TEST(AuthenticationCredentialsContainerTest, PublicKeyCreateCspMetric) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  // Set CSP to block connections to everything except self.
  context.DomWindow().GetContentSecurityPolicy()->AddPolicies(
      ParseContentSecurityPolicies(
          "connect-src 'self'",
          network::mojom::blink::ContentSecurityPolicyType::kEnforce,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP,
          KURL("https://example.test")));

  auto* creation_options = CredentialCreationOptions::Create();
  auto* public_key_creation_options =
      PublicKeyCredentialCreationOptions::Create();
  auto* rp = PublicKeyCredentialRpEntity::Create();
  rp->setId("example.test");
  rp->setName("Example");
  public_key_creation_options->setRp(rp);

  auto* user = PublicKeyCredentialUserEntity::Create();
  user->setId(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      DOMArrayBuffer::Create(Vector<uint8_t>{1, 2, 3, 4})));
  user->setName("user");
  user->setDisplayName("User");
  public_key_creation_options->setUser(user);

  auto* param = PublicKeyCredentialParameters::Create();
  param->setAlg(-7);
  param->setType("public-key");
  public_key_creation_options->setPubKeyCredParams({param});

  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_creation_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));
  creation_options->setPublicKey(public_key_creation_options);

  AuthenticationCredentialsContainer::credentials(
      *context.DomWindow().navigator())
      ->create(context.GetScriptState(), creation_options,
               IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToMakeCredential();
  mock_authenticator.InvokeMakeCredentialCallback();

  histogram_tester.ExpectUniqueSample("WebAuthentication.CspAllow.Create", true,
                                      1);

  // Now try one that is blocked.
  mock_authenticator.Reset();
  public_key_creation_options->rp()->setId("blocked.com");
  AuthenticationCredentialsContainer::credentials(
      *context.DomWindow().navigator())
      ->create(context.GetScriptState(), creation_options,
               IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToMakeCredential();
  mock_authenticator.InvokeMakeCredentialCallback();

  histogram_tester.ExpectBucketCount("WebAuthentication.CspAllow.Create", false,
                                     1);
  histogram_tester.ExpectTotalCount("WebAuthentication.CspAllow.Create", 2);
}

TEST(AuthenticationCredentialsContainerTest,
     PublicKeyConditionalCreateUseCounter) {
  test::TaskEnvironment task_environment;

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  context.DomWindow().document()->ClearUseCounterForTesting(
      WebFeature::kWebAuthnConditionalCreate);
  context.DomWindow().document()->ClearUseCounterForTesting(
      WebFeature::kWebAuthnConditionalCreateSuccess);

  auto* creation_options = CredentialCreationOptions::Create();
  creation_options->setMediation(
      V8CredentialMediationRequirement::Enum::kConditional);
  auto* public_key_creation_options =
      PublicKeyCredentialCreationOptions::Create();
  auto* rp = PublicKeyCredentialRpEntity::Create();
  rp->setId("example.test");
  rp->setName("Example");
  public_key_creation_options->setRp(rp);

  auto* user = PublicKeyCredentialUserEntity::Create();
  user->setId(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      DOMArrayBuffer::Create(Vector<uint8_t>{1, 2, 3, 4})));
  user->setName("marisa");
  user->setDisplayName("Marisa Kirisame");
  public_key_creation_options->setUser(user);

  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_creation_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));
  creation_options->setPublicKey(public_key_creation_options);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->create(context.GetScriptState(), creation_options,
                              IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToMakeCredential();

  EXPECT_TRUE(context.DomWindow().document()->IsUseCounted(
      WebFeature::kWebAuthnConditionalCreate));
  EXPECT_FALSE(context.DomWindow().document()->IsUseCounted(
      WebFeature::kWebAuthnConditionalCreateSuccess));

  mock_authenticator.InvokeMakeCredentialSuccessCallback();
  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(context.DomWindow().document()->IsUseCounted(
      WebFeature::kWebAuthnConditionalCreateSuccess));
}

TEST(AuthenticationCredentialsContainerTest, PublicKeyCreateCmtgKeyExtension) {
  test::TaskEnvironment task_environment;
  ScopedWebAuthenticationCmtgKeyForTest cmtg_key_enabled(true);

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  auto* creation_options = CredentialCreationOptions::Create();
  auto* public_key_creation_options =
      PublicKeyCredentialCreationOptions::Create();
  auto* rp = PublicKeyCredentialRpEntity::Create();
  rp->setId("example.test");
  rp->setName("Example");
  public_key_creation_options->setRp(rp);

  auto* user = PublicKeyCredentialUserEntity::Create();
  user->setId(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      DOMArrayBuffer::Create(Vector<uint8_t>{1, 2, 3, 4})));
  user->setName("user");
  user->setDisplayName("User");
  public_key_creation_options->setUser(user);

  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_creation_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));

  auto* extensions = AuthenticationExtensionsClientInputs::Create();
  extensions->setCmtgKey(true);
  public_key_creation_options->setExtensions(extensions);

  creation_options->setPublicKey(public_key_creation_options);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->create(context.GetScriptState(), creation_options,
                              IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToMakeCredential();

  ASSERT_TRUE(mock_authenticator.last_creation_options());
  EXPECT_TRUE(mock_authenticator.last_creation_options()->cmtg_key);

  const Vector<uint8_t> expected_cmtg_key = {3, 3, 3};
  const Vector<uint8_t> expected_signature = {4, 4, 4};
  mock_authenticator.InvokeMakeCredentialSuccessWithCmtgKeyCallback(
      expected_cmtg_key, expected_signature);

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto* credential = To<PublicKeyCredential>(V8PublicKeyCredential::ToWrappable(
      context.GetScriptState()->GetIsolate(), tester.Value().V8Value()));
  auto* cmtg_outputs = credential->getClientExtensionResults()->cmtgKey();
  ASSERT_TRUE(cmtg_outputs);
  DOMArrayBuffer* cmtg_key_buffer = cmtg_outputs->cmtgKey();
  EXPECT_EQ(cmtg_key_buffer->ByteSpan(), base::as_byte_span(expected_cmtg_key));
  DOMArrayBuffer* signature_buffer = cmtg_outputs->signature();
  EXPECT_EQ(signature_buffer->ByteSpan(),
            base::as_byte_span(expected_signature));
}

TEST(AuthenticationCredentialsContainerTest, PublicKeyGetCmtgKeyExtension) {
  test::TaskEnvironment task_environment;
  ScopedWebAuthenticationCmtgKeyForTest cmtg_key_enabled(true);

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  auto* request_options = CredentialRequestOptions::Create();
  auto* public_key_request_options =
      PublicKeyCredentialRequestOptions::Create();
  public_key_request_options->setRpId("example.test");

  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_request_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));

  auto* extensions = AuthenticationExtensionsClientInputs::Create();
  extensions->setCmtgKey(true);
  public_key_request_options->setExtensions(extensions);

  request_options->setPublicKey(public_key_request_options);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), request_options,
                           IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToGet();

  EXPECT_TRUE(
      mock_authenticator.last_get_options()->public_key->extensions->cmtg_key);

  const Vector<uint8_t> expected_cmtg_key = {3, 3, 3};
  const Vector<uint8_t> expected_signature = {4, 4, 4};
  mock_authenticator.InvokeGetAssertionSuccessWithCmtgKeyCallback(
      expected_cmtg_key, expected_signature);

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto* credential = To<PublicKeyCredential>(V8PublicKeyCredential::ToWrappable(
      context.GetScriptState()->GetIsolate(), tester.Value().V8Value()));
  auto* cmtg_outputs = credential->getClientExtensionResults()->cmtgKey();
  DOMArrayBuffer* cmtg_key_buffer = cmtg_outputs->cmtgKey();
  EXPECT_EQ(cmtg_key_buffer->ByteSpan(), base::as_byte_span(expected_cmtg_key));
  DOMArrayBuffer* signature_buffer = cmtg_outputs->signature();
  EXPECT_EQ(signature_buffer->ByteSpan(),
            base::as_byte_span(expected_signature));
}

TEST(AuthenticationCredentialsContainerTest, PublicKeyCrossDeviceFallbackUrl) {
  test::TaskEnvironment task_environment;
  ScopedWebAuthenticationCrossDeviceFallbackUrlForTest enabled(true);

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  mock_authenticator.Reset();
  auto* request_options = CredentialRequestOptions::Create();
  auto* public_key_request_options =
      PublicKeyCredentialRequestOptions::Create();
  public_key_request_options->setRpId("example.test");
  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_request_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));

  auto* extensions = AuthenticationExtensionsClientInputs::Create();
  extensions->setCrossDeviceFallbackUrl("https://allowed.com/fallback");
  public_key_request_options->setExtensions(extensions);
  request_options->setPublicKey(public_key_request_options);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), request_options,
                           IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToGet();
  mock_authenticator
      .InvokeGetAssertionSuccessWithCrossDeviceFallbackUrlCallback(
          /*cross_device_fallback_val=*/true);

  const auto& last_options = mock_authenticator.last_get_options();
  ASSERT_TRUE(last_options);
  ASSERT_TRUE(last_options->public_key);
  ASSERT_TRUE(last_options->public_key->extensions);
  ASSERT_TRUE(last_options->public_key->extensions->cross_device_fallback_url
                  .has_value());
  EXPECT_EQ((*last_options->public_key->extensions->cross_device_fallback_url)
                .GetString(),
            "https://allowed.com/fallback");

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());
  auto* credential = To<PublicKeyCredential>(V8PublicKeyCredential::ToWrappable(
      context.GetScriptState()->GetIsolate(), tester.Value().V8Value()));
  ASSERT_TRUE(
      credential->getClientExtensionResults()->hasCrossDeviceFallbackUrl());
  EXPECT_TRUE(
      credential->getClientExtensionResults()->crossDeviceFallbackUrl());
}

TEST(AuthenticationCredentialsContainerTest,
     PublicKeyCrossDeviceFallbackUrl_Processed) {
  test::TaskEnvironment task_environment;
  ScopedWebAuthenticationCrossDeviceFallbackUrlForTest enabled(true);

  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);

  mock_authenticator.Reset();
  auto* request_options = CredentialRequestOptions::Create();
  auto* public_key_request_options =
      PublicKeyCredentialRequestOptions::Create();
  public_key_request_options->setRpId("example.test");
  const Vector<uint8_t> challenge = {1, 2, 3, 4};
  public_key_request_options->setChallenge(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
          DOMArrayBuffer::Create(challenge)));

  auto* extensions = AuthenticationExtensionsClientInputs::Create();
  extensions->setCrossDeviceFallbackUrl("https://allowed.com/fallback");
  public_key_request_options->setExtensions(extensions);
  request_options->setPublicKey(public_key_request_options);

  auto promise = AuthenticationCredentialsContainer::credentials(
                     *context.DomWindow().navigator())
                     ->get(context.GetScriptState(), request_options,
                           IGNORE_EXCEPTION_FOR_TESTING);
  mock_authenticator.WaitForCallToGet();
  mock_authenticator.InvokeGetCallbackWithCrossDeviceFallback();

  ScriptPromiseTester tester(context.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());

  v8::Local<v8::Value> error = tester.Value().V8Value();
  ASSERT_TRUE(error->IsObject());
  auto* exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), error);
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "OperationError");
  EXPECT_EQ(
      exception->message(),
      "crossDeviceFallbackUrl: The authenticator processed the fallback URL.");
}

TEST(AuthenticationCredentialsContainerTest,
     GetRequest_ActiveThenPassiveCollision) {
  test::TaskEnvironment task_environment;

  MockFederatedRequestService mock_federated_request_service;
  CredentialManagerTestingContext context(
      /*mock_credential_manager=*/nullptr, /*mock_authenticator=*/nullptr,
      /*mock_federated_request_service=*/&mock_federated_request_service);

  // First request: Active mode
  CredentialRequestOptions* active_options = CredentialRequestOptions::Create();
  IdentityCredentialRequestOptions* active_identity =
      IdentityCredentialRequestOptions::Create();
  auto* idp1 = IdentityProviderRequestOptions::Create();
  idp1->setConfigURL("https://idp.example/config.json");
  idp1->setClientId("clientId");
  active_identity->setProviders({idp1});
  active_identity->setMode(
      V8IdentityCredentialRequestOptionsMode::Enum::kActive);
  active_options->setIdentity(active_identity);

  // Second request: Passive mode
  CredentialRequestOptions* passive_options =
      CredentialRequestOptions::Create();
  IdentityCredentialRequestOptions* passive_identity =
      IdentityCredentialRequestOptions::Create();
  auto* idp2 = IdentityProviderRequestOptions::Create();
  idp2->setConfigURL("https://idp.example/config.json");
  idp2->setClientId("clientId");
  passive_identity->setProviders({idp2});
  passive_identity->setMode(
      V8IdentityCredentialRequestOptionsMode::Enum::kPassive);
  passive_options->setIdentity(passive_identity);

  // Call active get() first
  auto active_promise = AuthenticationCredentialsContainer::credentials(
                            *context.DomWindow().navigator())
                            ->get(context.GetScriptState(), active_options,
                                  IGNORE_EXCEPTION_FOR_TESTING);

  // Call passive get() second
  auto passive_promise = AuthenticationCredentialsContainer::credentials(
                             *context.DomWindow().navigator())
                             ->get(context.GetScriptState(), passive_options,
                                   IGNORE_EXCEPTION_FOR_TESTING);

  mock_federated_request_service.WaitForCallToStartToken(2);

  // Invoke second (passive) callback with kErrorTooManyRequests
  mock_federated_request_service.InvokeStartTokenRequestCallbackWithError(
      mojom::blink::RequestTokenStatus::kErrorTooManyRequests, 1);

  ScriptPromiseTester passive_tester(context.GetScriptState(), passive_promise);
  passive_tester.WaitUntilSettled();
  ASSERT_TRUE(passive_tester.IsRejected());

  v8::Local<v8::Value> passive_error = passive_tester.Value().V8Value();
  ASSERT_TRUE(passive_error->IsObject());
  auto* passive_exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), passive_error);
  ASSERT_TRUE(passive_exception);
  EXPECT_EQ(passive_exception->name(), "NotAllowedError");

  // First (active) request is still pending. Now resolve it with error.
  mock_federated_request_service.InvokeStartTokenRequestCallbackWithError(
      mojom::blink::RequestTokenStatus::kError, 0);

  ScriptPromiseTester active_tester(context.GetScriptState(), active_promise);
  active_tester.WaitUntilSettled();
  ASSERT_TRUE(active_tester.IsRejected());

  v8::Local<v8::Value> active_error = active_tester.Value().V8Value();
  ASSERT_TRUE(active_error->IsObject());
  auto* active_exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), active_error);
  ASSERT_TRUE(active_exception);
  EXPECT_EQ(active_exception->name(), "NetworkError");
}

TEST(AuthenticationCredentialsContainerTest,
     GetRequest_PassiveThenActiveCollision) {
  test::TaskEnvironment task_environment;

  MockFederatedRequestService mock_federated_request_service;
  CredentialManagerTestingContext context(
      /*mock_credential_manager=*/nullptr, /*mock_authenticator=*/nullptr,
      /*mock_federated_request_service=*/&mock_federated_request_service);

  // First request: Passive mode
  CredentialRequestOptions* passive_options =
      CredentialRequestOptions::Create();
  IdentityCredentialRequestOptions* passive_identity =
      IdentityCredentialRequestOptions::Create();
  auto* idp1 = IdentityProviderRequestOptions::Create();
  idp1->setConfigURL("https://idp.example/config.json");
  idp1->setClientId("clientId");
  passive_identity->setProviders({idp1});
  passive_identity->setMode(
      V8IdentityCredentialRequestOptionsMode::Enum::kPassive);
  passive_options->setIdentity(passive_identity);

  // Second request: Active mode
  CredentialRequestOptions* active_options = CredentialRequestOptions::Create();
  IdentityCredentialRequestOptions* active_identity =
      IdentityCredentialRequestOptions::Create();
  auto* idp2 = IdentityProviderRequestOptions::Create();
  idp2->setConfigURL("https://idp.example/config.json");
  idp2->setClientId("clientId");
  active_identity->setProviders({idp2});
  active_identity->setMode(
      V8IdentityCredentialRequestOptionsMode::Enum::kActive);
  active_options->setIdentity(active_identity);

  // Call passive get() first
  auto passive_promise = AuthenticationCredentialsContainer::credentials(
                             *context.DomWindow().navigator())
                             ->get(context.GetScriptState(), passive_options,
                                   IGNORE_EXCEPTION_FOR_TESTING);

  // Call active get() second
  auto active_promise = AuthenticationCredentialsContainer::credentials(
                            *context.DomWindow().navigator())
                            ->get(context.GetScriptState(), active_options,
                                  IGNORE_EXCEPTION_FOR_TESTING);

  mock_federated_request_service.WaitForCallToStartToken(2);

  // First (passive) request is cancelled with kError
  mock_federated_request_service.InvokeStartTokenRequestCallbackWithError(
      mojom::blink::RequestTokenStatus::kError, 0);

  ScriptPromiseTester passive_tester(context.GetScriptState(), passive_promise);
  passive_tester.WaitUntilSettled();
  ASSERT_TRUE(passive_tester.IsRejected());

  v8::Local<v8::Value> passive_error = passive_tester.Value().V8Value();
  ASSERT_TRUE(passive_error->IsObject());
  auto* passive_exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), passive_error);
  ASSERT_TRUE(passive_exception);
  EXPECT_EQ(passive_exception->name(), "NetworkError");

  // Second (active) request is still pending. Now resolve it with error.
  mock_federated_request_service.InvokeStartTokenRequestCallbackWithError(
      mojom::blink::RequestTokenStatus::kError, 0);

  ScriptPromiseTester active_tester(context.GetScriptState(), active_promise);
  active_tester.WaitUntilSettled();
  ASSERT_TRUE(active_tester.IsRejected());

  v8::Local<v8::Value> active_error = active_tester.Value().V8Value();
  ASSERT_TRUE(active_error->IsObject());
  auto* active_exception = V8DOMException::ToWrappable(
      context.GetScriptState()->GetIsolate(), active_error);
  ASSERT_TRUE(active_exception);
  EXPECT_EQ(active_exception->name(), "NetworkError");
}

TEST(AuthenticationCredentialsContainerTest,
     WebAuthnAttachmentAndHintsUseCounters) {
  test::TaskEnvironment task_environment;
  MockAuthenticatorInterface mock_authenticator;
  CredentialManagerTestingContext context(/*mock_credential_manager=*/nullptr,
                                          &mock_authenticator);
  struct TestCase {
    std::optional<String> attachment;
    Vector<String> hints;
    bool expect_attachment_counted;
    bool expect_no_hints_counted;
  } test_cases[] = {
      {std::nullopt, {}, false, false},
      {"invalid", {}, false, false},
      {"platform", {"security-key"}, true, false},
      {"platform", {"invalid-hint"}, true, false},
      {"platform", {}, true, true},
      {"cross-platform", {"hybrid"}, true, false},
      {"cross-platform", {}, true, true},
  };
  for (const auto& test_case : test_cases) {
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kWebAuthnCreatePublicKeyCredentialWithAttachment);
    context.DomWindow().document()->ClearUseCounterForTesting(
        WebFeature::kWebAuthnCreatePublicKeyCredentialWithAttachmentAndNoHints);
    auto* creation_options = CredentialCreationOptions::Create();
    auto* public_key_creation_options =
        PublicKeyCredentialCreationOptions::Create();
    auto* rp = PublicKeyCredentialRpEntity::Create();
    rp->setId("example.test");
    rp->setName("Example");
    public_key_creation_options->setRp(rp);
    auto* user = PublicKeyCredentialUserEntity::Create();
    user->setId(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
        DOMArrayBuffer::Create(Vector<uint8_t>{1, 2, 3, 4})));
    user->setName("user");
    user->setDisplayName("User");
    public_key_creation_options->setUser(user);
    auto* param = PublicKeyCredentialParameters::Create();
    param->setAlg(-7);
    param->setType("public-key");
    public_key_creation_options->setPubKeyCredParams({param});
    const Vector<uint8_t> challenge = {1, 2, 3, 4};
    public_key_creation_options->setChallenge(
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
            DOMArrayBuffer::Create(challenge)));
    if (test_case.attachment) {
      auto* selection = AuthenticatorSelectionCriteria::Create();
      selection->setAuthenticatorAttachment(*test_case.attachment);
      public_key_creation_options->setAuthenticatorSelection(selection);
    }
    public_key_creation_options->setHints(test_case.hints);
    creation_options->setPublicKey(public_key_creation_options);

    AuthenticationCredentialsContainer::credentials(
        *context.DomWindow().navigator())
        ->create(context.GetScriptState(), creation_options,
                 IGNORE_EXCEPTION_FOR_TESTING);
    EXPECT_EQ(
        test_case.expect_attachment_counted,
        context.DomWindow().document()->IsUseCounted(
            WebFeature::kWebAuthnCreatePublicKeyCredentialWithAttachment));
    EXPECT_EQ(
        test_case.expect_no_hints_counted,
        context.DomWindow().document()->IsUseCounted(
            WebFeature::
                kWebAuthnCreatePublicKeyCredentialWithAttachmentAndNoHints));
  }
}

}  // namespace blink
