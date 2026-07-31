// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/webid/request.h"
#include "content/browser/webid/request_service.h"
#include "content/browser/webid/test/delegated_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_auto_reauthn_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_registry.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_modal_dialog_view_delegate.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/webid/login_status_options.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using ApiPermissionStatus =
    FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using blink::mojom::RegisterIdpStatus;

namespace {

constexpr char kIdpUrl[] = "https://idp.example/";

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    return ApiPermissionStatus::GRANTED;
  }
};

class TestIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  void FetchWellKnown(const GURL& provider,
                      FetchWellKnownCallback callback) override {
    // Assume that the well-known file is not found for a registered IDP.
    IdpNetworkRequestManager::WellKnown well_known;
    FetchStatus fetch_status = {ParseStatus::kHttpNotFoundError, 404};
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), fetch_status, well_known));
  }

  void FetchConfig(const GURL& provider,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.token = GURL("https://idp.example/token");
    endpoints.accounts = GURL("https://idp.example/accounts");

    IdentityProviderMetadata idp_metadata;
    idp_metadata.config_url = provider;
    idp_metadata.idp_login_url = GURL("https://idp.example/login");
    idp_metadata.types = {"idp-type"};
    FetchStatus fetch_status = {ParseStatus::kSuccess, 200};
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), fetch_status, endpoints,
                                  idp_metadata));
  }
};

}  // namespace

class RequestRegistryTest : public RenderViewHostImplTestHarness {
 protected:
  RequestRegistryTest() = default;
  ~RequestRegistryTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    test_api_permission_delegate_ =
        std::make_unique<TestApiPermissionDelegate>();
    mock_permission_delegate_ =
        std::make_unique<StrictMock<MockPermissionDelegate>>();

    std::unique_ptr<NavigationSimulator> simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(kIdpUrl),
                                                     main_test_rfh());
    network::ParsedPermissionsPolicy policy(1);
    policy[0].feature =
        network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet;
    policy[0].matches_all_origins = true;
    simulator->SetPermissionsPolicyHeader(std::move(policy));
    simulator->Commit();

    mock_auto_reauthn_permission_delegate_ =
        std::make_unique<NiceMock<MockAutoReauthnPermissionDelegate>>();
    auto mock_identity_registry =
        std::make_unique<NiceMock<MockIdentityRegistry>>(
            web_contents(), /*delegate=*/nullptr, GURL(kIdpUrl));
    mock_identity_registry_ = mock_identity_registry->GetWeakPtr();
    web_contents()->SetUserData(IdentityRegistry::UserDataKey(),
                                std::move(mock_identity_registry));

    auto* service =
        RequestService::GetOrCreateForCurrentDocument(main_test_rfh());
    service->SetDelegatesForTesting(
        test_api_permission_delegate_.get(),
        mock_auto_reauthn_permission_delegate_.get(),
        mock_permission_delegate_.get(), mock_identity_registry_.get());
    service->BindFederatedRequestService(
        request_service_remote_.BindNewPipeAndPassReceiver());
    request_ = service->GetOrCreateActiveRequest()->GetWeakPtr();

    auto mock_dialog_controller =
        std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
    service->SetDialogControllerForTests(std::move(mock_dialog_controller));
    std::unique_ptr<TestIdpNetworkRequestManager> network_request_manager =
        std::make_unique<TestIdpNetworkRequestManager>();
    service->SetNetworkManagerForTests(std::move(network_request_manager));
  }

  void TearDown() override {
    request_ = nullptr;
    mock_identity_registry_ = nullptr;
    request_service_remote_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  mojo::Remote<blink::mojom::FederatedRequestService> request_service_remote_;
  base::WeakPtr<Request> request_;

  std::unique_ptr<TestApiPermissionDelegate> test_api_permission_delegate_;
  std::unique_ptr<StrictMock<MockPermissionDelegate>> mock_permission_delegate_;
  std::unique_ptr<NiceMock<MockAutoReauthnPermissionDelegate>>
      mock_auto_reauthn_permission_delegate_;
  base::WeakPtr<MockIdentityRegistry> mock_identity_registry_ = nullptr;
};

// Test Registering an IdP successfully.
TEST_F(RequestRegistryTest, RegistersIdPSuccessfully) {
  GURL configURL = GURL(kIdpUrl);

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  EXPECT_CALL(*mock_permission_delegate_, RegisterIdP(_)).WillOnce(Return());

  base::RunLoop loop;
  request_service_remote_->RegisterIdP(
      std::move(configURL),
      base::BindLambdaForTesting([&loop](RegisterIdpStatus result) {
        EXPECT_EQ(RegisterIdpStatus::kSuccess, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Registering an IdP without the feature enabled.
TEST_F(RequestRegistryTest, RegistersWithoutFeature) {
  GURL configURL = GURL(kIdpUrl);

  base::RunLoop loop;
  request_service_remote_->RegisterIdP(
      std::move(configURL),
      base::BindLambdaForTesting([&loop](RegisterIdpStatus result) {
        EXPECT_EQ(RegisterIdpStatus::kErrorFeatureDisabled, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Registering a configURL of a different origin.
TEST_F(RequestRegistryTest, RegistersCrossOriginNotAllowed) {
  GURL configURL = GURL("https://another.example");

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  base::RunLoop loop;
  request_service_remote_->RegisterIdP(
      std::move(configURL),
      base::BindLambdaForTesting([&loop](RegisterIdpStatus result) {
        EXPECT_EQ(RegisterIdpStatus::kErrorCrossOriginConfig, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Unregistering an IdP without the feature enabled.
TEST_F(RequestRegistryTest, UnregistersWithoutFeature) {
  GURL configURL = GURL(kIdpUrl);

  // no call to the mock_permission_delegate_ (which is a strict)
  // mock) expected.

  base::RunLoop loop;
  request_service_remote_->UnregisterIdP(
      std::move(configURL), base::BindLambdaForTesting([&loop](bool result) {
        EXPECT_EQ(false, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Unregistering an IdP with the feature enabled but for a different
// origin.
TEST_F(RequestRegistryTest, UnregisterAcrossOrigin) {
  GURL configURL = GURL("https://another.example");

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  // no call to the mock_permission_delegate_ (which is a strict)
  // mock) expected.
  base::RunLoop loop;
  request_service_remote_->UnregisterIdP(
      std::move(configURL), base::BindLambdaForTesting([&loop](bool result) {
        EXPECT_EQ(false, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Unregistering an IdP Successfully.
TEST_F(RequestRegistryTest, UnregistersIdP) {
  GURL configURL = GURL(kIdpUrl);

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  EXPECT_CALL(*mock_permission_delegate_, UnregisterIdP(_)).WillOnce(Return());

  base::RunLoop loop;
  request_service_remote_->UnregisterIdP(
      std::move(configURL), base::BindLambdaForTesting([&loop](bool result) {
        EXPECT_EQ(true, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Registering an IdP via FederatedRequestService.
TEST_F(RequestRegistryTest, RequestServiceRegisterIdP) {
  GURL config_url = GURL(kIdpUrl);

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  EXPECT_CALL(*mock_permission_delegate_, RegisterIdP(_)).WillOnce(Return());

  base::RunLoop loop;
  request_service_remote_->RegisterIdP(
      std::move(config_url),
      base::BindLambdaForTesting([&loop](RegisterIdpStatus result) {
        EXPECT_EQ(RegisterIdpStatus::kSuccess, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Unregistering an IdP via FederatedRequestService.
TEST_F(RequestRegistryTest, RequestServiceUnregisterIdP) {
  GURL config_url = GURL(kIdpUrl);

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  EXPECT_CALL(*mock_permission_delegate_, UnregisterIdP(_)).WillOnce(Return());

  base::RunLoop loop;
  request_service_remote_->UnregisterIdP(
      std::move(config_url), base::BindLambdaForTesting([&loop](bool result) {
        EXPECT_EQ(true, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test SetIdpSigninStatus via FederatedRequestService.
TEST_F(RequestRegistryTest, RequestServiceSetIdpSigninStatus) {
  url::Origin origin = url::Origin::Create(GURL(kIdpUrl));

  EXPECT_CALL(*mock_permission_delegate_, SetIdpSigninStatus(origin, true, _))
      .WillOnce(Return());

  base::RunLoop loop;
  request_service_remote_->SetIdpSigninStatus(
      origin, blink::mojom::IdpSigninStatus::kSignedIn, std::nullopt,
      base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
  loop.Run();
}

// Test PreventSilentAccess via FederatedRequestService.
TEST_F(RequestRegistryTest, RequestServicePreventSilentAccess) {
  EXPECT_CALL(*mock_auto_reauthn_permission_delegate_,
              SetRequiresUserMediation(_, true))
      .WillOnce(Return());
  EXPECT_CALL(*mock_permission_delegate_, OnSetRequiresUserMediation(_, _))
      .WillOnce(::testing::WithArg<1>(
          [](base::OnceClosure callback) { std::move(callback).Run(); }));

  base::RunLoop loop;
  request_service_remote_->PreventSilentAccess(
      base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
  loop.Run();
}

// Test RequestUserInfo via FederatedRequestService.
TEST_F(RequestRegistryTest, RequestServiceRequestUserInfoFailure) {
  // Append a child frame (iframe) with identity-credentials-get policy.
  network::ParsedPermissionsPolicy frame_policy(1);
  frame_policy[0].feature =
      network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet;
  frame_policy[0].matches_all_origins = true;
  frame_policy[0].matches_opaque_src = false;
  TestRenderFrameHost* child_rfh =
      static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
          ->AppendChildWithPolicy("child_frame", frame_policy);

  // Navigate child frame to same-origin with IDP.
  auto simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kIdpUrl), child_rfh);
  simulator->Commit();
  child_rfh =
      static_cast<TestRenderFrameHost*>(simulator->GetFinalRenderFrameHost());

  // Set up child service and bind Mojo interfaces.
  mojo::Remote<blink::mojom::FederatedRequestService>
      child_request_service_remote;

  auto* child_service =
      RequestService::GetOrCreateForCurrentDocument(child_rfh);
  child_service->SetDelegatesForTesting(
      test_api_permission_delegate_.get(),
      mock_auto_reauthn_permission_delegate_.get(),
      mock_permission_delegate_.get(), mock_identity_registry_.get());

  child_service->BindFederatedRequestService(
      child_request_service_remote.BindNewPipeAndPassReceiver());

  auto provider = blink::mojom::IdentityProviderConfig::New();
  provider->config_url = GURL(kIdpUrl);
  provider->client_id = "client_id";

  EXPECT_CALL(*mock_permission_delegate_, GetIdpSigninStatus(_))
      .WillOnce(Return(false));

  base::RunLoop loop;
  child_request_service_remote->RequestUserInfo(
      std::move(provider),
      base::BindLambdaForTesting(
          [&loop](blink::mojom::RequestUserInfoResultPtr result) {
            EXPECT_TRUE(result->is_status());
            EXPECT_EQ(blink::mojom::RequestUserInfoStatus::kError,
                      result->get_status());
            loop.Quit();
          }));
  loop.Run();
}

// Test RequestUserInfo with denied Permissions Policy.
TEST_F(RequestRegistryTest,
       RequestServiceRequestUserInfoPermissionsPolicyDenied) {
  // Append a child frame (iframe) but do NOT delegate identity-credentials-get.
  // This means the feature will be disabled for this iframe.
  network::ParsedPermissionsPolicy frame_policy(1);
  frame_policy[0].feature =
      network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet;
  frame_policy[0].matches_all_origins = false;  // Denied!
  frame_policy[0].matches_opaque_src = false;
  TestRenderFrameHost* child_rfh =
      static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
          ->AppendChildWithPolicy("child_frame", frame_policy);

  // Navigate child frame to same-origin with IDP.
  auto simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kIdpUrl), child_rfh);
  simulator->Commit();
  child_rfh =
      static_cast<TestRenderFrameHost*>(simulator->GetFinalRenderFrameHost());

  // Set up child service and bind Mojo interfaces.
  mojo::Remote<blink::mojom::FederatedRequestService>
      child_request_service_remote;

  auto* child_service =
      RequestService::GetOrCreateForCurrentDocument(child_rfh);
  child_service->SetDelegatesForTesting(
      test_api_permission_delegate_.get(),
      mock_auto_reauthn_permission_delegate_.get(),
      mock_permission_delegate_.get(), mock_identity_registry_.get());

  child_service->BindFederatedRequestService(
      child_request_service_remote.BindNewPipeAndPassReceiver());

  auto provider = blink::mojom::IdentityProviderConfig::New();
  provider->config_url = GURL(kIdpUrl);
  provider->client_id = "client_id";

  mojo::test::BadMessageObserver bad_message_observer;
  child_request_service_remote->RequestUserInfo(std::move(provider),
                                                base::DoNothing());

  EXPECT_EQ("identity-credentials-get permissions policy not enabled",
            bad_message_observer.WaitForBadMessage());
}

// Test that SetIdpSigninStatus with an invalid picture URL triggers a Bad
// Message report.
TEST_F(RequestRegistryTest, SetIdpSigninStatusInvalidPictureUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kFedCmLightweightMode);

  url::Origin origin = url::Origin::Create(GURL(kIdpUrl));

  // Create LoginStatusOptions with an invalid picture URL
  blink::common::webid::LoginStatusOptions options;
  blink::common::webid::LoginStatusAccount account;
  account.id = "id";
  account.email = "email";
  account.name = "name";
  account.picture = GURL("invalid_url");  // Invalid!
  options.accounts.push_back(account);

  mojo::test::BadMessageObserver bad_message_observer;
  request_service_remote_->SetIdpSigninStatus(
      origin, blink::mojom::IdpSigninStatus::kSignedIn, std::move(options),
      base::DoNothing());

  EXPECT_THAT(bad_message_observer.WaitForBadMessage(),
              testing::HasSubstr("VALIDATION_ERROR_DESERIALIZATION_FAILED"));
}

// Test that SetIdpSigninStatus with an insecure picture URL triggers a Bad
// Message report.
TEST_F(RequestRegistryTest, SetIdpSigninStatusInsecurePictureUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kFedCmLightweightMode);

  url::Origin origin = url::Origin::Create(GURL(kIdpUrl));

  // Create LoginStatusOptions with an insecure picture URL (plain http)
  blink::common::webid::LoginStatusOptions options;
  blink::common::webid::LoginStatusAccount account;
  account.id = "id";
  account.email = "email";
  account.name = "name";
  account.picture = GURL("http://insecure.example/picture.png");  // Insecure!
  options.accounts.push_back(account);

  mojo::test::BadMessageObserver bad_message_observer;
  request_service_remote_->SetIdpSigninStatus(
      origin, blink::mojom::IdpSigninStatus::kSignedIn, std::move(options),
      base::DoNothing());

  EXPECT_THAT(bad_message_observer.WaitForBadMessage(),
              testing::HasSubstr("VALIDATION_ERROR_DESERIALIZATION_FAILED"));
}

// Test that CloseModalDialogView via FederatedRequestService Mojo remote
// notifies IdentityRegistry.
TEST_F(RequestRegistryTest, RequestServiceCloseModalDialogView) {
  base::RunLoop run_loop;
  url::Origin origin = url::Origin::Create(GURL(kIdpUrl));
  EXPECT_CALL(*mock_identity_registry_, NotifyClose(origin))
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  request_service_remote_->CloseModalDialogView();

  run_loop.Run();
}

}  // namespace content::webid
