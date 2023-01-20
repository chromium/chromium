// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_user_info_request.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using blink::mojom::RequestUserInfoStatus;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {
namespace {

constexpr char kRpUrl[] = "https://rp.example";
constexpr char kPersonalizedButtonFrameUrl[] = "https://idp.example/button";
constexpr char kProviderUrl[] = "https://idp.example/fedcm.json";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kClientId[] = "client_id_123";
constexpr char kNonce[] = "nonce123";

constexpr char kAccountEmailFormat[] = "%s@foo.com";
constexpr char kAccountName[] = "The Liliputian";
constexpr char kAccountGivenName[] = "Julius";
constexpr char kAccountPicture[] = "https://image.com/yolo";

struct AccountConfig {
  std::string id;
  absl::optional<IdentityRequestAccount::LoginState> login_state;
  bool was_granted_sharing_permission;
};

struct Config {
  absl::optional<bool> idp_signin_status;
  std::vector<AccountConfig> accounts;
  FetchStatus config_fetch_status;
  FetchStatus accounts_fetch_status;
};

Config kValidConfig = {
    /*idp_signin_status=*/true,
    /*accounts=*/
    {{"account1", /*login_state=*/absl::nullopt,
      /*was_granted_sharing_permission=*/true}},
    /*config_fetch_status=*/{ParseStatus::kSuccess, net::HTTP_OK},
    /*accounts_fetch_status=*/{ParseStatus::kSuccess, net::HTTP_OK}};

std::string GenerateEmailForUserId(const std::string& user_id) {
  return base::StringPrintf(kAccountEmailFormat, user_id.c_str());
}

// Helper class for blocking till RequestUserInfoCallback is called.
class UserInfoCallbackHelper {
 public:
  UserInfoCallbackHelper() = default;
  ~UserInfoCallbackHelper() = default;

  UserInfoCallbackHelper(const UserInfoCallbackHelper&) = delete;
  UserInfoCallbackHelper& operator=(const UserInfoCallbackHelper&) = delete;

  // This can only be called once per lifetime of this object.
  blink::mojom::FederatedAuthRequest::RequestUserInfoCallback callback() {
    return base::BindOnce(&UserInfoCallbackHelper::Complete,
                          base::Unretained(this));
  }

  // Returns when callback() is called, which can be immediately if it has
  // already been called.
  void WaitForCallback() {
    if (was_called_) {
      return;
    }
    wait_for_callback_loop_.Run();
  }

  RequestUserInfoStatus user_info_status_;
  absl::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info_;

 private:
  void Complete(RequestUserInfoStatus user_info_status,
                absl::optional<std::vector<blink::mojom::IdentityUserInfoPtr>>
                    user_info) {
    CHECK(!was_called_);
    user_info_status_ = user_info_status;
    user_info_ = std::move(user_info);
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_{false};
  base::RunLoop wait_for_callback_loop_;
};

class TestIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  explicit TestIdpNetworkRequestManager(const Config& config)
      : config_(config) {}
  ~TestIdpNetworkRequestManager() override = default;

  void FetchWellKnown(const GURL& provider,
                      FetchWellKnownCallback callback) override {
    FetchStatus fetch_status = {ParseStatus::kSuccess, net::HTTP_OK};
    std::set<GURL> well_known_urls = {GURL(kProviderUrl)};
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), fetch_status, well_known_urls));
  }

  void FetchConfig(const GURL& provider,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    has_fetched_config_ = true;

    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.accounts = GURL(kAccountsEndpoint);
    endpoints.token = GURL(kTokenEndpoint);

    IdentityProviderMetadata idp_metadata;
    idp_metadata.config_url = GURL(kProviderUrl);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), config_.config_fetch_status,
                       endpoints, idp_metadata));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    has_fetched_accounts_endpoint_ = true;

    std::vector<IdentityRequestAccount> accounts;
    for (const AccountConfig& account_config : config_.accounts) {
      accounts.emplace_back(account_config.id,
                            GenerateEmailForUserId(account_config.id),
                            kAccountName, kAccountGivenName,
                            GURL(kAccountPicture), account_config.login_state);
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), config_.accounts_fetch_status,
                       std::move(accounts)));
  }

 protected:
  bool has_fetched_config_{false};
  bool has_fetched_accounts_endpoint_{false};

 private:
  const Config config_;
};

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    return ApiPermissionStatus::GRANTED;
  }
};

class TestPermissionDelegate : public MockPermissionDelegate {
 public:
  bool HasSharingPermission(const url::Origin& relying_party_requester,
                            const url::Origin& relying_party_embedder,
                            const url::Origin& identity_provider,
                            const std::string& account_id) override {
    url::Origin rp_origin_with_data = url::Origin::Create(GURL(kRpUrl));
    url::Origin idp_origin_with_data =
        url::Origin::Create(GURL(kPersonalizedButtonFrameUrl));
    return (relying_party_requester == rp_origin_with_data &&
            relying_party_embedder == rp_origin_with_data &&
            identity_provider == idp_origin_with_data &&
            accounts_with_sharing_permission_.count(account_id));
  }

  absl::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override {
    return idp_signin_status_;
  }

  void SetConfig(const Config& config) {
    idp_signin_status_ = config.idp_signin_status;

    accounts_with_sharing_permission_.clear();
    for (const AccountConfig& account_config : config.accounts) {
      if (account_config.was_granted_sharing_permission) {
        accounts_with_sharing_permission_.insert(account_config.id);
      }
    }
  }

 private:
  absl::optional<bool> idp_signin_status_;
  std::set<std::string> accounts_with_sharing_permission_;
};

}  // namespace

class FederatedAuthUserInfoRequestTest : public RenderViewHostImplTestHarness {
 public:
  ~FederatedAuthUserInfoRequestTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    api_permission_delegate_ = std::make_unique<TestApiPermissionDelegate>();
    permission_delegate_ = std::make_unique<TestPermissionDelegate>();
    metrics_ = std::make_unique<NiceMock<FedCmMetrics>>(
        GURL(kProviderUrl), ukm::kInvalidSourceId, 0, true);

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kRpUrl), ui::PAGE_TRANSITION_LINK);

    // Add a subframe that navigates to kPersonalizedButtonFrameUrl.
    content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
    iframe_render_frame_host_ =
        static_cast<TestRenderFrameHost*>(contents()
                                              ->GetPrimaryFrameTree()
                                              .root()
                                              ->child_at(0)
                                              ->current_frame_host());
    iframe_render_frame_host_ = static_cast<TestRenderFrameHost*>(
        NavigationSimulator::NavigateAndCommitFromDocument(
            GURL(kPersonalizedButtonFrameUrl), iframe_render_frame_host_));
  }

  void RunUserInfoTest(
      const Config& config,
      RequestUserInfoStatus expected_user_info_status,
      const std::vector<std::string>& expected_account_user_ids) {
    permission_delegate_->SetConfig(config);

    auto network_manager =
        std::make_unique<TestIdpNetworkRequestManager>(config);

    blink::mojom::IdentityProviderConfigPtr idp_ptr =
        blink::mojom::IdentityProviderConfig::New(
            GURL(kProviderUrl), kClientId, kNonce, /*login_hit=*/nullptr);

    UserInfoCallbackHelper callback_helper;
    std::unique_ptr<FederatedAuthUserInfoRequest> request =
        FederatedAuthUserInfoRequest::CreateAndStart(
            std::move(network_manager), api_permission_delegate_.get(),
            permission_delegate_.get(), iframe_render_frame_host_,
            metrics_.get(), std::move(idp_ptr), callback_helper.callback());
    callback_helper.WaitForCallback();

    EXPECT_EQ(expected_user_info_status, callback_helper.user_info_status_);
    CheckUserInfo(expected_account_user_ids, callback_helper.user_info_);
  }

  void CheckUserInfo(
      const std::vector<std::string>& expected_account_ids,
      const absl::optional<std::vector<blink::mojom::IdentityUserInfoPtr>>&
          actual_user_info) {
    if (expected_account_ids.empty()) {
      EXPECT_EQ(actual_user_info, absl::nullopt);
      return;
    }

    EXPECT_EQ(expected_account_ids.size(), actual_user_info->size());
    for (size_t i = 0; i < expected_account_ids.size(); ++i) {
      EXPECT_EQ(GenerateEmailForUserId(expected_account_ids[i]),
                actual_user_info->at(i)->email);
    }
  }

 protected:
  base::raw_ptr<RenderFrameHost> iframe_render_frame_host_;
  std::unique_ptr<TestIdpNetworkRequestManager> network_manager_;
  std::unique_ptr<TestApiPermissionDelegate> api_permission_delegate_;
  std::unique_ptr<TestPermissionDelegate> permission_delegate_;
  std::unique_ptr<NiceMock<FedCmMetrics>> metrics_;
};

TEST_F(FederatedAuthUserInfoRequestTest, PreviouslySignedIn) {
  const char kAccount1Id[] = "account1";
  const char kAccount2Id[] = "account2";

  Config config = kValidConfig;
  config.accounts = {{kAccount1Id, /*login_state=*/absl::nullopt,
                      /*was_granted_sharing_permission=*/true},
                     {kAccount2Id, /*login_state=*/absl::nullopt,
                      /*was_granted_sharing_permission=*/false}};
  RunUserInfoTest(config, RequestUserInfoStatus::kSuccess,
                  {kAccount1Id, kAccount2Id});
}

TEST_F(FederatedAuthUserInfoRequestTest, NoSignedInAccount) {
  const char kAccount1Id[] = "account1";
  const char kAccount2Id[] = "account2";

  Config config = kValidConfig;
  config.accounts = {{kAccount1Id, /*login_state=*/absl::nullopt,
                      /*was_granted_sharing_permission=*/false},
                     {kAccount2Id, /*login_state=*/absl::nullopt,
                      /*was_granted_sharing_permission=*/false}};
  RunUserInfoTest(config, RequestUserInfoStatus::kError, {});
}

TEST_F(FederatedAuthUserInfoRequestTest, NotInApprovedClientsList) {
  const char kAccount1Id[] = "account1";
  const char kAccount2Id[] = "account2";

  Config config = kValidConfig;
  config.accounts = {{kAccount1Id, /*login_state=*/LoginState::kSignUp,
                      /*was_granted_sharing_permission=*/true},
                     {kAccount2Id, /*login_state=*/LoginState::kSignUp,
                      /*was_granted_sharing_permission=*/true}};
  RunUserInfoTest(config, RequestUserInfoStatus::kError, {});
}

TEST_F(FederatedAuthUserInfoRequestTest, InApprovedClientsList) {
  const char kAccount1Id[] = "account1";
  const char kAccount2Id[] = "account2";

  Config config = kValidConfig;
  config.accounts = {{kAccount1Id, /*login_state=*/LoginState::kSignIn,
                      /*was_granted_sharing_permission=*/true},
                     {kAccount2Id, /*login_state=*/LoginState::kSignUp,
                      /*was_granted_sharing_permission=*/true}};
  RunUserInfoTest(config, RequestUserInfoStatus::kSuccess,
                  {kAccount1Id, kAccount2Id});
}

TEST_F(FederatedAuthUserInfoRequestTest, ConfigFetchFailed) {
  Config config = kValidConfig;
  config.config_fetch_status = {ParseStatus::kHttpNotFoundError, 404};

  RunUserInfoTest(config, RequestUserInfoStatus::kError, {});
}

TEST_F(FederatedAuthUserInfoRequestTest,
       IdpSigninStatusClearedWhenAccountsRequestFails) {
  std::vector<absl::optional<bool>> kTestCases = {absl::nullopt, true};

  for (const absl::optional<bool>& test_case : kTestCases) {
    EXPECT_CALL(*permission_delegate_, SetIdpSigninStatus(_, false));

    Config config = kValidConfig;
    config.idp_signin_status = test_case;
    config.accounts_fetch_status = {ParseStatus::kHttpNotFoundError, 404};

    RunUserInfoTest(config, RequestUserInfoStatus::kError, {});

    testing::Mock::VerifyAndClearExpectations(permission_delegate_.get());
  }
}

}  // namespace content
