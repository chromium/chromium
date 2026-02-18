// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/identity_credential_source_impl.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "content/browser/webid/identity_provider_info.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;

namespace content::webid {

namespace {
constexpr char kIdpUrl[] = "https://idp.example";
constexpr char kConfigUrl[] = "https://idp.example/fedcm.json";
constexpr char kAccountsUrl[] = "https://idp.example/accounts";
constexpr char kTokenUrl[] = "https://idp.example/token";
constexpr char kAccountId[] = "123";
constexpr char kAccountIdSignIn[] = "signin";
constexpr char kAccountIdSignUp[] = "signup";
}  // namespace

class IdentityCredentialSourceImplTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://www.example.com"));
    source_ =
        IdentityCredentialSourceImpl::GetOrCreateForCurrentDocument(main_rfh());

    auto network_manager =
        std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
    network_manager_ = network_manager.get();
    source_->SetNetworkManagerForTests(std::move(network_manager));

    permission_delegate_ = std::make_unique<NiceMock<MockPermissionDelegate>>();
    source_->SetPermissionDelegateForTests(permission_delegate_.get());
  }

  void TearDown() override {
    source_ = nullptr;
    network_manager_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  raw_ptr<IdentityCredentialSourceImpl> source_;
  raw_ptr<MockIdpNetworkRequestManager> network_manager_;
  std::unique_ptr<MockPermissionDelegate> permission_delegate_;
};

TEST_F(IdentityCredentialSourceImplTest, SuccessfulFetching) {
  GURL idp_url(kIdpUrl);
  GURL config_url(kConfigUrl);
  GURL accounts_url(kAccountsUrl);

  // Mock IdP Signin Status
  EXPECT_CALL(*permission_delegate_,
              GetIdpSigninStatus(url::Origin::Create(idp_url)))
      .WillRepeatedly(Return(true));

  // Mock FetchWellKnown
  IdpNetworkRequestManager::WellKnown well_known;
  well_known.provider_urls = std::set<GURL>{config_url};
  EXPECT_CALL(*network_manager_, FetchWellKnown(config_url, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          FetchStatus{ParseStatus::kSuccess, 200}, well_known));

  // Mock FetchConfig
  IdpNetworkRequestManager::Endpoints endpoints;
  endpoints.token = GURL(kTokenUrl);
  endpoints.accounts = accounts_url;
  IdentityProviderMetadata idp_metadata;
  idp_metadata.idp_login_url = GURL("https://idp.example/login");
  EXPECT_CALL(*network_manager_, FetchConfig(config_url, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          FetchStatus{ParseStatus::kSuccess, 200}, endpoints, idp_metadata));

  // Mock SendAccountsRequest
  IdentityRequestAccountPtr account = base::MakeRefCounted<
      IdentityRequestAccount>(
      kAccountId, "test@example.com", "Test User", "test@example.com",
      "Test User", "Test", GURL(), "", "",
      /*the original string is fc432178f9155c4e24762de5b9505f2eexample.com*/
      std::vector<std::string>{
          "870f48f3c28efb5dbf46d14881d802a4c34141a36ef9e66d28cec211b1969f7d"},
      std::vector<std::string>(), std::vector<std::string>(),
      std::vector<std::string>(),
      content::IdentityRequestAccount::LoginState::kSignIn);

  IdpNetworkRequestManager::AccountsResponse accounts_response;
  accounts_response.site_salt = "fc432178f9155c4e24762de5b9505f2e";
  accounts_response.accounts.push_back(account);

  EXPECT_CALL(*network_manager_, SendAccountsRequest(_, accounts_url, _, _))
      .WillOnce(
          [&](const url::Origin&, const GURL&, const std::string&,
              IdpNetworkRequestManager::AccountsRequestCallback callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               FetchStatus{ParseStatus::kSuccess, 200},
                               std::move(accounts_response)));
            return true;
          });

  base::RunLoop run_loop;
  source_->GetIdentityCredentialSuggestions(
      {config_url},
      base::BindLambdaForTesting(
          [&](const std::optional<std::vector<IdentityRequestAccountPtr>>&
                  accounts) {
            ASSERT_TRUE(accounts.has_value());
            ASSERT_EQ(1u, accounts->size());
            EXPECT_EQ(kAccountId, accounts->at(0)->id);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(IdentityCredentialSourceImplTest, UserNotSignedInToIdP) {
  GURL idp_url(kIdpUrl);

  // Mock IdP Signin Status to return false (not signed in)
  EXPECT_CALL(*permission_delegate_,
              GetIdpSigninStatus(url::Origin::Create(idp_url)))
      .WillOnce(Return(false));

  // Should NOT call FetchWellKnown or anything else.
  EXPECT_CALL(*network_manager_, FetchWellKnown(_, _)).Times(0);
  EXPECT_CALL(*network_manager_, SendAccountsRequest(_, _, _, _)).Times(0);

  base::RunLoop run_loop;
  source_->GetIdentityCredentialSuggestions(
      {idp_url},
      base::BindLambdaForTesting(
          [&](const std::optional<std::vector<IdentityRequestAccountPtr>>&
                  accounts) {
            // Should return empty list because request was skipped
            ASSERT_TRUE(accounts.has_value());
            EXPECT_TRUE(accounts->empty());
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(IdentityCredentialSourceImplTest, FilterOutSignupAccount) {
  GURL idp_url(kIdpUrl);
  GURL config_url(kConfigUrl);
  GURL accounts_url(kAccountsUrl);

  // Mock IdP Signin Status
  EXPECT_CALL(*permission_delegate_,
              GetIdpSigninStatus(url::Origin::Create(idp_url)))
      .WillRepeatedly(Return(true));

  // Mock FetchWellKnown
  IdpNetworkRequestManager::WellKnown well_known;
  well_known.provider_urls = std::set<GURL>{config_url};
  EXPECT_CALL(*network_manager_, FetchWellKnown(config_url, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          FetchStatus{ParseStatus::kSuccess, 200}, well_known));

  // Mock FetchConfig
  IdpNetworkRequestManager::Endpoints endpoints;
  endpoints.token = GURL(kTokenUrl);
  endpoints.accounts = accounts_url;
  IdentityProviderMetadata idp_metadata;
  idp_metadata.idp_login_url = GURL("https://idp.example/login");
  EXPECT_CALL(*network_manager_, FetchConfig(config_url, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          FetchStatus{ParseStatus::kSuccess, 200}, endpoints, idp_metadata));

  // Mock SendAccountsRequest with two accounts: one matching origin, one not.
  IdentityRequestAccountPtr account_matching = base::MakeRefCounted<
      IdentityRequestAccount>(
      kAccountIdSignIn, "test@example.com", "Test User", "test@example.com",
      "Test User", "Test", GURL(), "", "",
      /*the original string is fc432178f9155c4e24762de5b9505f2eexample.com*/
      std::vector<std::string>{
          "870f48f3c28efb5dbf46d14881d802a4c34141a36ef9e66d28cec211b1969f7d"},
      std::vector<std::string>(), std::vector<std::string>(),
      std::vector<std::string>());

  IdentityRequestAccountPtr account_not_matching =
      base::MakeRefCounted<IdentityRequestAccount>(
          kAccountIdSignUp, "test@example.com", "Test User", "test@example.com",
          "Test User", "Test", GURL(), "", "", std::vector<std::string>(),
          std::vector<std::string>(), std::vector<std::string>(),
          std::vector<std::string>());

  IdpNetworkRequestManager::AccountsResponse accounts_response;
  accounts_response.site_salt = "fc432178f9155c4e24762de5b9505f2e";
  accounts_response.accounts.push_back(account_matching);
  accounts_response.accounts.push_back(account_not_matching);

  EXPECT_CALL(*network_manager_, SendAccountsRequest(_, accounts_url, _, _))
      .WillOnce(
          [&](const url::Origin&, const GURL&, const std::string&,
              IdpNetworkRequestManager::AccountsRequestCallback callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               FetchStatus{ParseStatus::kSuccess, 200},
                               std::move(accounts_response)));
            return true;
          });

  base::RunLoop run_loop;
  source_->GetIdentityCredentialSuggestions(
      {config_url},
      base::BindLambdaForTesting(
          [&](const std::optional<std::vector<IdentityRequestAccountPtr>>&
                  accounts) {
            ASSERT_TRUE(accounts.has_value());
            // Should only have the matching account.
            ASSERT_EQ(1u, accounts->size());
            EXPECT_EQ(kAccountIdSignIn, accounts->at(0)->id);
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace content::webid
