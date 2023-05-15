// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using blink::mojom::LogoutRpsRequest;
using blink::mojom::LogoutRpsRequestPtr;
using blink::mojom::LogoutRpsStatus;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

constexpr char kRpUrl[] = "https://rp.example/";
constexpr char kIdpUrl[] = "https://idp.example/";

// Helper class for receiving the Logout method callback.
class LogoutRpsRequestCallbackHelper {
 public:
  LogoutRpsRequestCallbackHelper() = default;
  ~LogoutRpsRequestCallbackHelper() = default;

  LogoutRpsRequestCallbackHelper(const LogoutRpsRequestCallbackHelper&) =
      delete;
  LogoutRpsRequestCallbackHelper& operator=(
      const LogoutRpsRequestCallbackHelper&) = delete;

  LogoutRpsStatus status() const { return status_; }

  // This can only be called once per lifetime of this object.
  base::OnceCallback<void(LogoutRpsStatus)> callback() {
    return base::BindOnce(&LogoutRpsRequestCallbackHelper::ReceiverMethod,
                          base::Unretained(this));
  }

  // Returns when callback() is called, which can be immediately if it has
  // already been called.
  void WaitForCallback() {
    if (was_called_)
      return;
    wait_for_callback_loop_.Run();
  }

 private:
  void ReceiverMethod(LogoutRpsStatus status) {
    status_ = status;
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  LogoutRpsStatus status_;
};

LogoutRpsRequestPtr MakeLogoutRequest(const std::string& endpoint,
                                      const std::string& account_id) {
  auto request = LogoutRpsRequest::New();
  request->url = GURL(endpoint);
  request->account_id = account_id;
  return request;
}

class TestLogoutIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  void SendLogout(const GURL& logout_url, LogoutCallback callback) override {
    ++num_logout_requests_;
    std::move(callback).Run();
  }

  size_t num_logout_requests() { return num_logout_requests_; }

 protected:
  size_t num_logout_requests_{0};
};

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    return ApiPermissionStatus::GRANTED;
  }
};

}  // namespace

class FederatedAuthRequestImplLogoutTest
    : public RenderViewHostImplTestHarness {
 protected:
  FederatedAuthRequestImplLogoutTest() = default;
  ~FederatedAuthRequestImplLogoutTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kFedCm,
        {{features::kFedCmIdpSignoutFieldTrialParamName, "true"}});

    RenderViewHostImplTestHarness::SetUp();
    test_api_permission_delegate_ =
        std::make_unique<TestApiPermissionDelegate>();
    mock_auto_reauthn_permission_delegate_ =
        std::make_unique<NiceMock<MockAutoReauthnPermissionDelegate>>();
    mock_permission_delegate_ =
        std::make_unique<NiceMock<MockPermissionDelegate>>();
    mock_identity_registry_ = std::make_unique<NiceMock<MockIdentityRegistry>>(
        web_contents(), federated_auth_request_impl_,
        url::Origin::Create(GURL(kIdpUrl)));

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kRpUrl), ui::PAGE_TRANSITION_LINK);

    federated_auth_request_impl_ = &FederatedAuthRequestImpl::CreateForTesting(
        *main_test_rfh(), test_api_permission_delegate_.get(),
        mock_auto_reauthn_permission_delegate_.get(),
        mock_permission_delegate_.get(), mock_identity_registry_.get(),
        request_remote_.BindNewPipeAndPassReceiver());
    auto mock_dialog_controller =
        std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
    federated_auth_request_impl_->SetDialogControllerForTests(
        std::move(mock_dialog_controller));

    network_request_manager_ =
        std::make_unique<TestLogoutIdpNetworkRequestManager>();
    // DelegatedIdpNetworkRequestManager is owned by
    // |federated_auth_request_impl_|.
    federated_auth_request_impl_->SetNetworkManagerForTests(
        std::make_unique<DelegatedIdpNetworkRequestManager>(
            network_request_manager_.get()));

    federated_auth_request_impl_->SetTokenRequestDelayForTests(
        base::TimeDelta());
  }

  LogoutRpsStatus PerformLogoutRequest(
      std::vector<LogoutRpsRequestPtr> logout_requests) {
    LogoutRpsRequestCallbackHelper logout_helper;
    request_remote_->LogoutRps(std::move(logout_requests),
                               logout_helper.callback());
    logout_helper.WaitForCallback();
    return logout_helper.status();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  raw_ptr<FederatedAuthRequestImpl> federated_auth_request_impl_;

  std::unique_ptr<TestLogoutIdpNetworkRequestManager> network_request_manager_;

  std::unique_ptr<TestApiPermissionDelegate> test_api_permission_delegate_;
  std::unique_ptr<NiceMock<MockAutoReauthnPermissionDelegate>>
      mock_auto_reauthn_permission_delegate_;
  std::unique_ptr<NiceMock<MockPermissionDelegate>> mock_permission_delegate_;
  std::unique_ptr<NiceMock<MockIdentityRegistry>> mock_identity_registry_;
};

// Test Logout method success with multiple relying parties.
TEST_F(FederatedAuthRequestImplLogoutTest, LogoutSuccessMultiple) {
  std::vector<LogoutRpsRequestPtr> logout_requests;
  logout_requests.push_back(
      MakeLogoutRequest("https://rp1.example", "user123"));
  logout_requests.push_back(
      MakeLogoutRequest("https://rp2.example", "user456"));
  logout_requests.push_back(
      MakeLogoutRequest("https://rp3.example", "user789"));

  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(*mock_permission_delegate_, HasActiveSession(_, _, _))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  auto logout_response = PerformLogoutRequest(std::move(logout_requests));
  EXPECT_EQ(logout_response, LogoutRpsStatus::kSuccess);
  EXPECT_EQ(3u, static_cast<TestLogoutIdpNetworkRequestManager*>(
                    network_request_manager_.get())
                    ->num_logout_requests());
}

// Test Logout without session permission granted.
TEST_F(FederatedAuthRequestImplLogoutTest, LogoutWithoutPermission) {
  std::vector<LogoutRpsRequestPtr> logout_requests;
  logout_requests.push_back(
      MakeLogoutRequest("https://rp1.example", "user123"));

  auto logout_response = PerformLogoutRequest(std::move(logout_requests));
  EXPECT_EQ(logout_response, LogoutRpsStatus::kSuccess);
}

// Test Logout method with an empty endpoint vector.
TEST_F(FederatedAuthRequestImplLogoutTest, LogoutNoEndpoints) {
  auto logout_response =
      PerformLogoutRequest(std::vector<LogoutRpsRequestPtr>());
  EXPECT_EQ(logout_response, LogoutRpsStatus::kError);
}

}  // namespace content
