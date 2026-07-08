// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/identity_provider.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

using mojom::blink::CredentialMediationRequirement;
using mojom::blink::FederatedRequest;
using mojom::blink::FederatedRequestService;
using mojom::blink::IdentityCredentialDisconnectOptionsPtr;
using mojom::blink::IdentityProviderConfigPtr;
using mojom::blink::IdentityProviderGetParametersPtr;
using mojom::blink::LoginStatusOptionsPtr;
using mojom::blink::ResolveTokenParamsPtr;

class MockFederatedRequestService : public FederatedRequestService {
 public:
  MockFederatedRequestService() = default;
  ~MockFederatedRequestService() override = default;

  void BindRequestService(
      mojo::PendingReceiver<FederatedRequestService> receiver) {
    request_service_receiver_.Bind(std::move(receiver));
  }

  bool IsRequestServiceInterfaceBound() const {
    return request_service_receiver_.is_bound();
  }

  // mojom::blink::FederatedRequestService:
  MOCK_METHOD(void, CloseModalDialogView, (), (override));
  void StartTokenRequest(
      Vector<IdentityProviderGetParametersPtr> idp_get_params,
      CredentialMediationRequirement mediation,
      mojo::PendingReceiver<FederatedRequest> request_receiver,
      StartTokenRequestCallback callback) override {}
  void RequestUserInfo(IdentityProviderConfigPtr provider,
                       RequestUserInfoCallback callback) override {}
  void ResolveTokenRequest(const String& account_id,
                           ResolveTokenParamsPtr params,
                           ResolveTokenRequestCallback callback) override {}
  void SetIdpSigninStatus(
      const ::scoped_refptr<const ::blink::SecurityOrigin>& origin,
      mojom::IdpSigninStatus status,
      LoginStatusOptionsPtr options,
      SetIdpSigninStatusCallback callback) override {}
  void RegisterIdP(const ::blink::KURL& url,
                   RegisterIdPCallback callback) override {}

  void UnregisterIdP(const ::blink::KURL& url,
                     UnregisterIdPCallback callback) override {}

  void PreventSilentAccess(PreventSilentAccessCallback callback) override {}
  void Disconnect(IdentityCredentialDisconnectOptionsPtr options,
                  DisconnectCallback callback) override {}

 private:
  mojo::Receiver<FederatedRequestService> request_service_receiver_{this};
};

class IdentityProviderTestContext {
 public:
  IdentityProviderTestContext(LocalDOMWindow& window,
                              MockFederatedRequestService* mock) {
    window.GetBrowserInterfaceBroker().SetBinderForTesting(
        FederatedRequestService::Name_,
        BindRepeating(
            [](MockFederatedRequestService* mock,
               mojo::ScopedMessagePipeHandle handle) {
              mock->BindRequestService(
                  mojo::PendingReceiver<FederatedRequestService>(
                      std::move(handle)));
            },
            Unretained(mock)));
  }

  void Reset(LocalDOMWindow& window) {
    window.GetBrowserInterfaceBroker().SetBinderForTesting(
        FederatedRequestService::Name_, {});
  }
};

}  // namespace

TEST(IdentityProviderTest, Close) {
  test::TaskEnvironment task_environment;

  V8TestingScope scope;
  MockFederatedRequestService mock;
  IdentityProviderTestContext context(scope.GetWindow(), &mock);

  base::RunLoop run_loop;
  EXPECT_CALL(mock, CloseModalDialogView()).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  IdentityProvider::close(scope.GetScriptState());
  run_loop.Run();

  EXPECT_TRUE(mock.IsRequestServiceInterfaceBound());
  context.Reset(scope.GetWindow());
}

}  // namespace blink
