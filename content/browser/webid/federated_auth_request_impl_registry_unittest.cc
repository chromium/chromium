// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/test/delegated_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_auto_reauthn_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_registry.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_modal_dialog_view_delegate.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/public/common/content_features.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using blink::mojom::RegisterIdpStatus;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

constexpr char kIdpUrl[] = "https://idp.example/";

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    return ApiPermissionStatus::GRANTED;
  }
};

}  // namespace

class FederatedAuthRequestImplRegistryTest
    : public RenderViewHostImplTestHarness {
 protected:
  FederatedAuthRequestImplRegistryTest() = default;
  ~FederatedAuthRequestImplRegistryTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    test_api_permission_delegate_ =
        std::make_unique<TestApiPermissionDelegate>();
    mock_permission_delegate_ =
        std::make_unique<StrictMock<MockPermissionDelegate>>();

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kIdpUrl), ui::PAGE_TRANSITION_LINK);

    mock_auto_reauthn_permission_delegate_ =
        std::make_unique<NiceMock<MockAutoReauthnPermissionDelegate>>();
    mock_identity_registry_ = std::make_unique<NiceMock<MockIdentityRegistry>>(
        web_contents(), /*delegate=*/nullptr, GURL(kIdpUrl));

    federated_auth_request_impl_ = &FederatedAuthRequestImpl::CreateForTesting(
        *main_test_rfh(), test_api_permission_delegate_.get(),
        mock_auto_reauthn_permission_delegate_.get(),
        mock_permission_delegate_.get(), mock_identity_registry_.get(),
        request_remote_.BindNewPipeAndPassReceiver());
    auto mock_dialog_controller =
        std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
    federated_auth_request_impl_->SetDialogControllerForTests(
        std::move(mock_dialog_controller));
  }

  void TearDown() override {
    federated_auth_request_impl_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  raw_ptr<FederatedAuthRequestImpl> federated_auth_request_impl_;

  std::unique_ptr<TestApiPermissionDelegate> test_api_permission_delegate_;
  std::unique_ptr<StrictMock<MockPermissionDelegate>> mock_permission_delegate_;
  std::unique_ptr<NiceMock<MockAutoReauthnPermissionDelegate>>
      mock_auto_reauthn_permission_delegate_;
  std::unique_ptr<NiceMock<MockIdentityRegistry>> mock_identity_registry_;
};

// Test Registering an IdP successfully.
TEST_F(FederatedAuthRequestImplRegistryTest, RegistersIdPSuccessfully) {
  GURL configURL = GURL(kIdpUrl);

  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  auto controller =
      std::make_unique<NiceMock<MockIdentityRequestDialogController>>();

  EXPECT_CALL(*controller, RequestIdPRegistrationPermision(_, _))
      .WillOnce(::testing::WithArg<1>(
          [](base::OnceCallback<void(bool accepted)> callback) {
            std::move(callback).Run(true);
          }));

  federated_auth_request_impl_->SetDialogControllerForTests(
      std::move(controller));

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  EXPECT_CALL(*mock_permission_delegate_, RegisterIdP(_)).WillOnce(Return());

  base::RunLoop loop;
  request_remote_->RegisterIdP(
      std::move(configURL),
      base::BindLambdaForTesting([&loop](RegisterIdpStatus result) {
        EXPECT_EQ(RegisterIdpStatus::kSuccess, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Registering denied without user activation.
TEST_F(FederatedAuthRequestImplRegistryTest,
       RegistersIdPDeniedWithoutUserActivation) {
  GURL configURL = GURL(kIdpUrl);

  auto controller =
      std::make_unique<NiceMock<MockIdentityRequestDialogController>>();

  federated_auth_request_impl_->SetDialogControllerForTests(
      std::move(controller));

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  base::RunLoop loop;
  request_remote_->RegisterIdP(
      std::move(configURL),
      base::BindLambdaForTesting([&loop](RegisterIdpStatus result) {
        EXPECT_EQ(RegisterIdpStatus::kErrorNoTransientActivation, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Registering an IdP without the feature enabled.
TEST_F(FederatedAuthRequestImplRegistryTest, RegistersWithoutFeature) {
  GURL configURL = GURL(kIdpUrl);

  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  base::RunLoop loop;
  request_remote_->RegisterIdP(
      std::move(configURL),
      base::BindLambdaForTesting([&loop](RegisterIdpStatus result) {
        EXPECT_EQ(RegisterIdpStatus::kErrorFeatureDisabled, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Registering a configURL of a different origin.
TEST_F(FederatedAuthRequestImplRegistryTest, RegistersCrossOriginNotAllowed) {
  GURL configURL = GURL("https://another.example");

  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  base::RunLoop loop;
  request_remote_->RegisterIdP(
      std::move(configURL),
      base::BindLambdaForTesting([&loop](RegisterIdpStatus result) {
        EXPECT_EQ(RegisterIdpStatus::kErrorCrossOriginConfig, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Unregistering an IdP without the feature enabled.
TEST_F(FederatedAuthRequestImplRegistryTest, UnregistersWithoutFeature) {
  GURL configURL = GURL(kIdpUrl);

  // no call to the mock_permission_delegate_ (which is a strict)
  // mock) expected.

  base::RunLoop loop;
  request_remote_->UnregisterIdP(
      std::move(configURL), base::BindLambdaForTesting([&loop](bool result) {
        EXPECT_EQ(false, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Unregistering an IdP with the feature enabled but for a different
// origin.
TEST_F(FederatedAuthRequestImplRegistryTest, UnregisterAcrossOrigin) {
  GURL configURL = GURL("https://another.example");

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  // no call to the mock_permission_delegate_ (which is a strict)
  // mock) expected.
  base::RunLoop loop;
  request_remote_->UnregisterIdP(
      std::move(configURL), base::BindLambdaForTesting([&loop](bool result) {
        EXPECT_EQ(false, result);
        loop.Quit();
      }));
  loop.Run();
}

// Test Unregistering an IdP Successfully.
TEST_F(FederatedAuthRequestImplRegistryTest, UnregistersIdP) {
  GURL configURL = GURL(kIdpUrl);

  feature_list_.InitAndEnableFeature(features::kFedCmIdPRegistration);

  EXPECT_CALL(*mock_permission_delegate_, UnregisterIdP(_)).WillOnce(Return());

  base::RunLoop loop;
  request_remote_->UnregisterIdP(
      std::move(configURL), base::BindLambdaForTesting([&loop](bool result) {
        EXPECT_EQ(true, result);
        loop.Quit();
      }));
  loop.Run();
}

}  // namespace content
