// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_login_element.h"

#include "base/run_loop.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_credential_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class QuitListener : public NativeEventListener {
 public:
  explicit QuitListener(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void Invoke(ExecutionContext*, Event*) override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

class MockFederatedAuthRequest : public mojom::blink::FederatedAuthRequest {
 public:
  MockFederatedAuthRequest() = default;

  MOCK_METHOD(void,
              RequestToken,
              (Vector<mojom::blink::IdentityProviderGetParametersPtr>,
               mojom::blink::CredentialMediationRequirement,
               RequestTokenCallback),
              (override));
  MOCK_METHOD(void,
              RequestUserInfo,
              (mojom::blink::IdentityProviderConfigPtr,
               RequestUserInfoCallback),
              (override));
  MOCK_METHOD(void, CancelTokenRequest, (), (override));
  MOCK_METHOD(void,
              ResolveTokenRequest,
              (const String&,
               mojom::blink::FedCmRedirectMethod,
               const std::optional<KURL>&,
               const String&,
               base::Value,
               ResolveTokenRequestCallback),
              (override));
  MOCK_METHOD(void,
              SetIdpSigninStatus,
              (const scoped_refptr<const SecurityOrigin>&,
               mojom::blink::IdpSigninStatus,
               mojom::blink::LoginStatusOptionsPtr,
               SetIdpSigninStatusCallback),
              (override));
  MOCK_METHOD(void,
              RegisterIdP,
              (const KURL&, RegisterIdPCallback),
              (override));
  MOCK_METHOD(void,
              UnregisterIdP,
              (const KURL&, UnregisterIdPCallback),
              (override));
  MOCK_METHOD(void, CloseModalDialogView, (), (override));
  MOCK_METHOD(void,
              Disconnect,
              (mojom::blink::IdentityCredentialDisconnectOptionsPtr,
               DisconnectCallback),
              (override));
  MOCK_METHOD(void,
              PreventSilentAccess,
              (PreventSilentAccessCallback),
              (override));
};

class HTMLLoginElementClickTest : public PageTestBase {
 public:
  void SetUp() override {
    EnablePlatform();
    PageTestBase::SetUp();
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::FederatedAuthRequest::Name_,
        BindRepeating(
            [](mojo::Receiver<mojom::blink::FederatedAuthRequest>* receiver,
               mojo::ScopedMessagePipeHandle handle) {
              receiver->Bind(
                  mojo::PendingReceiver<mojom::blink::FederatedAuthRequest>(
                      std::move(handle)));
            },
            Unretained(&receiver_)));
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::FederatedAuthRequest::Name_, {});
    PageTestBase::TearDown();
  }

 protected:
  testing::NiceMock<MockFederatedAuthRequest> mock_federated_auth_request_;
  mojo::Receiver<mojom::blink::FederatedAuthRequest> receiver_{
      &mock_federated_auth_request_};
};

TEST_F(HTMLLoginElementClickTest, TagName) {
  auto* element = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  EXPECT_EQ(element->tagName(), "LOGIN");
  EXPECT_EQ(element->localName(), "login");
}

TEST_F(HTMLLoginElementClickTest, ClickInitiatesFedCm) {
  // Set a secure origin.
  NavigateTo(KURL("https://example.com"));

  auto* login = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  GetDocument().body()->AppendChild(login);

  auto* credential = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential->setAttribute(html_names::kConfigurlAttr,
                           AtomicString("https://example.com/fedcm.json"));
  credential->setAttribute(html_names::kClientidAttr,
                           AtomicString("client123"));
  login->AppendChild(credential);

  EXPECT_CALL(mock_federated_auth_request_, RequestToken)
      .WillOnce([](Vector<mojom::blink::IdentityProviderGetParametersPtr>
                       idp_get_params,
                   mojom::blink::CredentialMediationRequirement mediation,
                   MockFederatedAuthRequest::RequestTokenCallback callback) {
        ASSERT_EQ(idp_get_params.size(), 1u);
        ASSERT_EQ(idp_get_params[0]->providers.size(), 1u);
        EXPECT_EQ(idp_get_params[0]->providers[0]->config->config_url,
                  "https://example.com/fedcm.json");
        EXPECT_EQ(idp_get_params[0]->providers[0]->config->client_id,
                  "client123");
        std::move(callback).Run(mojom::blink::RequestTokenStatus::kSuccess,
                                std::nullopt, base::Value("dummy-token"),
                                nullptr, false);
      });

  // Simulate click on the login element.
  base::RunLoop run_loop;
  auto* listener = MakeGarbageCollected<QuitListener>(run_loop.QuitClosure());
  login->addEventListener(event_type_names::kComplete, listener);
  login->click();

  run_loop.Run();

  // Verify that the login element received the credential.
  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState::Scope scope(script_state);
  ScriptValue credential_value = login->credential(script_state);
  EXPECT_TRUE(credential_value.V8Value()->IsString());
  EXPECT_EQ(ToCoreString(script_state->GetIsolate(),
                         credential_value.V8Value().As<v8::String>()),
            "dummy-token");
}

TEST_F(HTMLLoginElementClickTest, ClickInitiatesFedCmWithSimpleToken) {
  // Set a secure origin.
  NavigateTo(KURL("https://example.com"));

  auto* login = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  GetDocument().body()->AppendChild(login);

  auto* credential = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential->setAttribute(html_names::kConfigurlAttr,
                           AtomicString("https://example.com/fedcm.json"));
  credential->setAttribute(html_names::kClientidAttr,
                           AtomicString("client123"));
  login->AppendChild(credential);

  EXPECT_CALL(mock_federated_auth_request_, RequestToken)
      .WillOnce([](Vector<mojom::blink::IdentityProviderGetParametersPtr>
                       idp_get_params,
                   mojom::blink::CredentialMediationRequirement mediation,
                   MockFederatedAuthRequest::RequestTokenCallback callback) {
        std::move(callback).Run(mojom::blink::RequestTokenStatus::kSuccess,
                                std::nullopt, base::Value(123), nullptr, false);
      });

  // Simulate click on the login element.
  base::RunLoop run_loop;
  auto* listener = MakeGarbageCollected<QuitListener>(run_loop.QuitClosure());
  login->addEventListener(event_type_names::kComplete, listener);
  login->click();

  run_loop.Run();

  // Verify that the login element received the simple credential.
  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState::Scope scope(script_state);
  ScriptValue credential_value = login->credential(script_state);
  EXPECT_TRUE(credential_value.V8Value()->IsNumber());
  EXPECT_EQ(credential_value.V8Value().As<v8::Number>()->Value(), 123);
}

TEST_F(HTMLLoginElementClickTest, NonLeftClickDoesNotInitiateFedCm) {
  // Set a secure origin.
  NavigateTo(KURL("https://example.com"));

  auto* login = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  GetDocument().body()->AppendChild(login);

  auto* credential = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential->setAttribute(html_names::kConfigurlAttr,
                           AtomicString("https://example.com/fedcm.json"));
  credential->setAttribute(html_names::kClientidAttr,
                           AtomicString("client123"));
  login->AppendChild(credential);

  EXPECT_CALL(mock_federated_auth_request_, RequestToken).Times(0);

  // Simulate a right-click.
  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState::Scope scope(script_state);

  PointerEventInit* init = PointerEventInit::Create();
  init->setButton(2);  // Right button
  init->setPointerId(1);
  PointerEvent* event = PointerEvent::Create(event_type_names::kClick, init);
  login->DispatchEvent(*event);
}

TEST_F(HTMLLoginElementClickTest, EnterKeyInitiatesFedCm) {
  // Set a secure origin.
  NavigateTo(KURL("https://example.com"));

  auto* login = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  login->setAttribute(html_names::kTabindexAttr, AtomicString("0"));
  GetDocument().body()->AppendChild(login);
  login->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), login);

  auto* credential = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential->setAttribute(html_names::kConfigurlAttr,
                           AtomicString("https://example.com/fedcm.json"));
  credential->setAttribute(html_names::kClientidAttr,
                           AtomicString("client123"));
  login->AppendChild(credential);

  EXPECT_CALL(mock_federated_auth_request_, RequestToken)
      .WillOnce([](Vector<mojom::blink::IdentityProviderGetParametersPtr>
                       idp_get_params,
                   mojom::blink::CredentialMediationRequirement mediation,
                   MockFederatedAuthRequest::RequestTokenCallback callback) {
        std::move(callback).Run(mojom::blink::RequestTokenStatus::kSuccess,
                                std::nullopt, base::Value("dummy-token"),
                                nullptr, false);
      });

  // Simulate Enter keydown.
  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState::Scope scope(script_state);

  KeyboardEventInit* init = KeyboardEventInit::Create();
  init->setKey("Enter");
  KeyboardEvent* event =
      KeyboardEvent::Create(script_state, event_type_names::kKeydown, init);
  base::RunLoop run_loop;
  auto* listener = MakeGarbageCollected<QuitListener>(run_loop.QuitClosure());
  login->addEventListener(event_type_names::kComplete, listener);
  login->DispatchEvent(*event);

  run_loop.Run();
}

TEST_F(HTMLLoginElementClickTest, InsecureContextDoesNotInitiateFedCm) {
  // Navigate to an insecure origin.
  NavigateTo(KURL("http://example.com"));

  ConsoleMessageStorage& storage = GetPage().GetConsoleMessageStorage();
  wtf_size_t initial_size = storage.size();

  auto* login = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  GetDocument().body()->AppendChild(login);

  // Check that a console message was emitted immediately on insertion.
  EXPECT_EQ(storage.size(), initial_size + 1);
  EXPECT_TRUE(
      storage.at(storage.size() - 1)->Message().contains("secure context"));

  auto* credential = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential->setAttribute(html_names::kConfigurlAttr,
                           AtomicString("https://example.com/fedcm.json"));
  credential->setAttribute(html_names::kClientidAttr,
                           AtomicString("client123"));
  login->AppendChild(credential);

  EXPECT_CALL(mock_federated_auth_request_, RequestToken).Times(0);

  // Simulate click.
  login->click();

  // Ensure no additional message is emitted on click.
  EXPECT_EQ(storage.size(), initial_size + 1);
}

TEST_F(HTMLLoginElementClickTest,
       PermissionsPolicyDisabledDoesNotInitiateFedCm) {
  // Set a secure origin but disable the permissions policy.
  NavigateTo(KURL("https://example.com"));

  ExecutionContext* context = GetDocument().GetExecutionContext();
  context->GetSecurityContext().SetPermissionsPolicy(
      network::PermissionsPolicy::CreateFromParsedPolicy(
          {}, context->GetSecurityOrigin()->ToUrlOrigin()));

  ConsoleMessageStorage& storage = GetPage().GetConsoleMessageStorage();
  wtf_size_t initial_size = storage.size();

  auto* login = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  GetDocument().body()->AppendChild(login);

  // No console message on insertion for permissions policy.
  EXPECT_EQ(storage.size(), initial_size);

  auto* credential = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential->setAttribute(html_names::kConfigurlAttr,
                           AtomicString("https://example.com/fedcm.json"));
  credential->setAttribute(html_names::kClientidAttr,
                           AtomicString("client123"));
  login->AppendChild(credential);

  EXPECT_CALL(mock_federated_auth_request_, RequestToken).Times(0);

  // Simulate click.
  login->click();

  // Check that a console message was emitted on click.
  EXPECT_EQ(storage.size(), initial_size + 1);
  EXPECT_TRUE(storage.at(storage.size() - 1)
                  ->Message()
                  .contains("permissions policy is not enabled"));
}

TEST_F(HTMLLoginElementClickTest, CSPConnectSrcBlocksIDP) {
  // Set a secure origin.
  NavigateTo(KURL("https://example.com"));

  ExecutionContext* context = GetDocument().GetExecutionContext();

  // Set up CSP to block one of the IDPs.
  context->GetContentSecurityPolicy()->AddPolicies(ParseContentSecurityPolicies(
      "connect-src https://allowed.com",
      network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      *(context->GetSecurityOrigin())));

  auto* login = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  GetDocument().body()->AppendChild(login);

  // Add an allowed IDP.
  auto* credential1 =
      MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential1->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential1->setAttribute(html_names::kConfigurlAttr,
                            AtomicString("https://allowed.com/fedcm.json"));
  credential1->setAttribute(html_names::kClientidAttr, AtomicString("123"));
  login->AppendChild(credential1);

  // Add a blocked IDP.
  auto* credential2 =
      MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential2->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential2->setAttribute(html_names::kConfigurlAttr,
                            AtomicString("https://blocked.com/fedcm.json"));
  credential2->setAttribute(html_names::kClientidAttr, AtomicString("456"));
  login->AppendChild(credential2);

  EXPECT_CALL(mock_federated_auth_request_, RequestToken)
      .WillOnce([](Vector<mojom::blink::IdentityProviderGetParametersPtr>
                       idp_get_params,
                   mojom::blink::CredentialMediationRequirement mediation,
                   MockFederatedAuthRequest::RequestTokenCallback callback) {
        // Verify that only the allowed IDP was included in the request.
        ASSERT_EQ(idp_get_params.size(), 1u);
        EXPECT_EQ(idp_get_params[0]->providers[0]->config->config_url,
                  "https://allowed.com/fedcm.json");
        std::move(callback).Run(mojom::blink::RequestTokenStatus::kSuccess,
                                std::nullopt, base::Value("dummy-token"),
                                nullptr, false);
      });

  // Simulate click.
  base::RunLoop run_loop;
  auto* listener = MakeGarbageCollected<QuitListener>(run_loop.QuitClosure());
  login->addEventListener(event_type_names::kComplete, listener);
  login->click();

  run_loop.Run();
}

TEST_F(HTMLLoginElementClickTest, IsFocusable) {
  auto* login = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  GetDocument().body()->AppendChild(login);

  // Initially not focusable without credentials.
  EXPECT_FALSE(login->IsFocusable());

  auto* credential = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential->setAttribute(html_names::kConfigurlAttr,
                           AtomicString("https://example.com/fedcm.json"));
  credential->setAttribute(html_names::kClientidAttr,
                           AtomicString("client123"));
  login->AppendChild(credential);

  // Focusable after adding a valid credential.
  EXPECT_TRUE(login->IsFocusable());
}

TEST_F(HTMLLoginElementClickTest, ClickWithInvalidParamsDoesNotInitiateFedCm) {
  // Set a secure origin.
  NavigateTo(KURL("https://example.com"));

  auto* login = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  GetDocument().body()->AppendChild(login);

  auto* credential = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  credential->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  credential->setAttribute(html_names::kConfigurlAttr,
                           AtomicString("https://example.com/fedcm.json"));
  credential->setAttribute(html_names::kClientidAttr,
                           AtomicString("client123"));
  // Set invalid JSON in params.
  credential->setAttribute(html_names::kParamsAttr,
                           AtomicString("{invalid: json}"));
  login->AppendChild(credential);

  EXPECT_CALL(mock_federated_auth_request_, RequestToken).Times(0);

  // Simulate click.
  login->click();
}

}  // namespace blink
