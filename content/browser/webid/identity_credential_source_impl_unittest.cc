// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/identity_credential_source_impl.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "content/browser/webid/identity_provider_info.h"
#include "content/browser/webid/request.h"
#include "content/browser/webid/request_page_data.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_auto_reauthn_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_registry.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/test/navigation_simulator.h"
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

class TestIdentityCredentialSourceImpl : public IdentityCredentialSourceImpl {
 public:
  explicit TestIdentityCredentialSourceImpl(RenderFrameHost* rfh)
      : IdentityCredentialSourceImpl(rfh) {}

  static void InitializeRequest(
      Request* request,
      std::unique_ptr<IdpNetworkRequestManager> network_manager) {
    if (!request->fedcm_metrics_) {
      request->fedcm_metrics_ = request->CreateFedCmMetrics();
    }
    request->network_manager_ = std::move(network_manager);
    request->accounts_dialog_display_time_ = base::TimeTicks::Now();
  }

  static void SetAccounts(
      Request* request,
      std::vector<scoped_refptr<IdentityRequestAccount>> accounts) {
    request->accounts_ = std::move(accounts);
  }

  static void SetIdpInfo(Request* request,
                         const GURL& idp_config_url,
                         std::unique_ptr<IdentityProviderInfo> idp_info) {
    request->idp_infos_[idp_config_url] = std::move(idp_info);
  }

  static void SetIdentitySelectionType(Request* request,
                                       Request::IdentitySelectionType type) {
    request->identity_selection_type_ = type;
  }
};

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

  EXPECT_CALL(*network_manager_, SendAccountsRequest(_, accounts_url, _))
      .WillOnce(
          [&](const url::Origin&, const GURL&,
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
  EXPECT_CALL(*network_manager_, SendAccountsRequest(_, _, _)).Times(0);

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

  EXPECT_CALL(*network_manager_, SendAccountsRequest(_, accounts_url, _))
      .WillOnce(
          [&](const url::Origin&, const GURL&,
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

TEST_F(IdentityCredentialSourceImplTest, SelectAccountSameSite) {
  GURL config_url(kConfigUrl);
  url::Origin idp_origin = url::Origin::Create(config_url);

  MockApiPermissionDelegate api_permission_delegate;
  EXPECT_CALL(api_permission_delegate, GetApiPermissionStatus(_))
      .WillRepeatedly(Return(FederatedIdentityApiPermissionContextDelegate::
                                 PermissionStatus::GRANTED));

  MockAutoReauthnPermissionDelegate auto_reauthn_permission_delegate;
  EXPECT_CALL(auto_reauthn_permission_delegate, IsAutoReauthnSettingEnabled())
      .WillRepeatedly(Return(false));

  MockIdentityRegistry identity_registry(web_contents(), nullptr,
                                         idp_origin.GetURL());
  mojo::Remote<blink::mojom::FederatedAuthRequest> remote;

  Request& request = Request::CreateForTesting(
      *main_rfh(), &api_permission_delegate, &auto_reauthn_permission_delegate,
      permission_delegate_.get(), &identity_registry,
      remote.BindNewPipeAndPassReceiver());

  TestIdentityCredentialSourceImpl::InitializeRequest(
      &request, std::make_unique<NiceMock<MockIdpNetworkRequestManager>>());
  request.SetDialogControllerForTests(
      std::make_unique<NiceMock<MockIdentityRequestDialogController>>());

  RequestPageData::GetOrCreateForPage(main_rfh()->GetPage())
      ->SetPendingWebIdentityRequest(&request);

  blink::mojom::IdentityProviderRequestOptionsPtr options =
      blink::mojom::IdentityProviderRequestOptions::New();
  options->config = blink::mojom::IdentityProviderConfig::New();
  options->config->config_url = config_url;

  auto idp_info = std::make_unique<IdentityProviderInfo>(
      options, IdpNetworkRequestManager::Endpoints(),
      IdentityProviderMetadata(), blink::mojom::RpContext::kSignIn,
      blink::mojom::RpMode::kPassive, std::nullopt);
  idp_info->client_is_third_party_to_top_frame_origin = false;
  IdentityProviderMetadata idp_metadata;
  idp_metadata.config_url = config_url;
  idp_info->data = base::MakeRefCounted<IdentityProviderData>(
      "idp", idp_metadata, ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
      blink::mojom::RpContext::kSignIn, std::nullopt,
      std::vector<IdentityRequestDialogDisclosureField>(),
      /*has_login_status_mismatch=*/false);

  IdentityRequestAccountPtr account =
      base::MakeRefCounted<IdentityRequestAccount>(
          kAccountId, "test@example.com", "Test User", "test@example.com",
          "Test User", "Test", GURL(), "", "", std::vector<std::string>(),
          std::vector<std::string>(), std::vector<std::string>(),
          std::vector<std::string>(),
          content::IdentityRequestAccount::LoginState::kSignIn);
  account->identity_provider = idp_info->data;

  IdpNetworkRequestManager::AccountsResponse accounts_response;
  accounts_response.accounts.push_back(account);

  TestIdentityCredentialSourceImpl::SetAccounts(
      &request, std::move(accounts_response.accounts));
  TestIdentityCredentialSourceImpl::SetIdpInfo(&request, config_url,
                                               std::move(idp_info));
  TestIdentityCredentialSourceImpl::SetIdentitySelectionType(
      &request, Request::kAutoPassive);

  // Should succeed because it is same-site (main frame)
  EXPECT_TRUE(source_->SelectAccount(idp_origin, kAccountId));

  RequestPageData::GetOrCreateForPage(main_rfh()->GetPage())
      ->SetPendingWebIdentityRequest(nullptr);
}

TEST_F(IdentityCredentialSourceImplTest, SelectAccountCrossSiteFail) {
  GURL config_url(kConfigUrl);
  url::Origin idp_origin = url::Origin::Create(config_url);

  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other.com"), subframe);

  MockApiPermissionDelegate api_permission_delegate;
  EXPECT_CALL(api_permission_delegate, GetApiPermissionStatus(_))
      .WillRepeatedly(Return(FederatedIdentityApiPermissionContextDelegate::
                                 PermissionStatus::GRANTED));

  MockAutoReauthnPermissionDelegate auto_reauthn_permission_delegate;
  EXPECT_CALL(auto_reauthn_permission_delegate, IsAutoReauthnSettingEnabled())
      .WillRepeatedly(Return(false));

  MockIdentityRegistry identity_registry(web_contents(), nullptr,
                                         idp_origin.GetURL());
  mojo::Remote<blink::mojom::FederatedAuthRequest> remote;

  Request& request = Request::CreateForTesting(
      *subframe, &api_permission_delegate, &auto_reauthn_permission_delegate,
      permission_delegate_.get(), &identity_registry,
      remote.BindNewPipeAndPassReceiver());

  TestIdentityCredentialSourceImpl::InitializeRequest(
      &request, std::make_unique<NiceMock<MockIdpNetworkRequestManager>>());
  request.SetDialogControllerForTests(
      std::make_unique<NiceMock<MockIdentityRequestDialogController>>());

  RequestPageData::GetOrCreateForPage(main_rfh()->GetPage())
      ->SetPendingWebIdentityRequest(&request);

  blink::mojom::IdentityProviderRequestOptionsPtr options =
      blink::mojom::IdentityProviderRequestOptions::New();
  options->config = blink::mojom::IdentityProviderConfig::New();
  options->config->config_url = config_url;

  auto idp_info = std::make_unique<IdentityProviderInfo>(
      options, IdpNetworkRequestManager::Endpoints(),
      IdentityProviderMetadata(), blink::mojom::RpContext::kSignIn,
      blink::mojom::RpMode::kPassive, std::nullopt);
  idp_info->client_is_third_party_to_top_frame_origin = true;
  IdentityProviderMetadata idp_metadata;
  idp_metadata.config_url = config_url;
  idp_info->data = base::MakeRefCounted<IdentityProviderData>(
      "idp", idp_metadata, ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
      blink::mojom::RpContext::kSignIn, std::nullopt,
      std::vector<IdentityRequestDialogDisclosureField>(),
      /*has_login_status_mismatch=*/false);

  IdentityRequestAccountPtr account =
      base::MakeRefCounted<IdentityRequestAccount>(
          kAccountId, "test@example.com", "Test User", "test@example.com",
          "Test User", "Test", GURL(), "", "", std::vector<std::string>(),
          std::vector<std::string>(), std::vector<std::string>(),
          std::vector<std::string>(),
          content::IdentityRequestAccount::LoginState::kSignIn);
  account->identity_provider = idp_info->data;

  IdpNetworkRequestManager::AccountsResponse accounts_response;
  accounts_response.accounts.push_back(account);

  TestIdentityCredentialSourceImpl::SetAccounts(
      &request, std::move(accounts_response.accounts));
  TestIdentityCredentialSourceImpl::SetIdpInfo(&request, config_url,
                                               std::move(idp_info));
  TestIdentityCredentialSourceImpl::SetIdentitySelectionType(
      &request, Request::kAutoPassive);

  // Should fail because it is cross-site and third party.
  EXPECT_FALSE(source_->SelectAccount(idp_origin, kAccountId));

  RequestPageData::GetOrCreateForPage(main_rfh()->GetPage())
      ->SetPendingWebIdentityRequest(nullptr);
}

TEST_F(IdentityCredentialSourceImplTest,
       SelectAccountCrossSiteButNotThirdPartySuccess) {
  GURL config_url(kConfigUrl);
  url::Origin idp_origin = url::Origin::Create(config_url);

  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other.com"), subframe);

  MockApiPermissionDelegate api_permission_delegate;
  EXPECT_CALL(api_permission_delegate, GetApiPermissionStatus(_))
      .WillRepeatedly(Return(FederatedIdentityApiPermissionContextDelegate::
                                 PermissionStatus::GRANTED));

  MockAutoReauthnPermissionDelegate auto_reauthn_permission_delegate;
  EXPECT_CALL(auto_reauthn_permission_delegate, IsAutoReauthnSettingEnabled())
      .WillRepeatedly(Return(false));

  MockIdentityRegistry identity_registry(web_contents(), nullptr,
                                         idp_origin.GetURL());
  mojo::Remote<blink::mojom::FederatedAuthRequest> remote;

  Request& request = Request::CreateForTesting(
      *subframe, &api_permission_delegate, &auto_reauthn_permission_delegate,
      permission_delegate_.get(), &identity_registry,
      remote.BindNewPipeAndPassReceiver());

  TestIdentityCredentialSourceImpl::InitializeRequest(
      &request, std::make_unique<NiceMock<MockIdpNetworkRequestManager>>());
  request.SetDialogControllerForTests(
      std::make_unique<NiceMock<MockIdentityRequestDialogController>>());

  RequestPageData::GetOrCreateForPage(main_rfh()->GetPage())
      ->SetPendingWebIdentityRequest(&request);

  blink::mojom::IdentityProviderRequestOptionsPtr options =
      blink::mojom::IdentityProviderRequestOptions::New();
  options->config = blink::mojom::IdentityProviderConfig::New();
  options->config->config_url = config_url;

  auto idp_info = std::make_unique<IdentityProviderInfo>(
      options, IdpNetworkRequestManager::Endpoints(),
      IdentityProviderMetadata(), blink::mojom::RpContext::kSignIn,
      blink::mojom::RpMode::kPassive, std::nullopt);
  idp_info->client_is_third_party_to_top_frame_origin = false;
  IdentityProviderMetadata idp_metadata;
  idp_metadata.config_url = config_url;
  idp_info->data = base::MakeRefCounted<IdentityProviderData>(
      "idp", idp_metadata, ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
      blink::mojom::RpContext::kSignIn, std::nullopt,
      std::vector<IdentityRequestDialogDisclosureField>(),
      /*has_login_status_mismatch=*/false);

  IdentityRequestAccountPtr account =
      base::MakeRefCounted<IdentityRequestAccount>(
          kAccountId, "test@example.com", "Test User", "test@example.com",
          "Test User", "Test", GURL(), "", "", std::vector<std::string>(),
          std::vector<std::string>(), std::vector<std::string>(),
          std::vector<std::string>(),
          content::IdentityRequestAccount::LoginState::kSignIn);
  account->identity_provider = idp_info->data;

  IdpNetworkRequestManager::AccountsResponse accounts_response;
  accounts_response.accounts.push_back(account);

  TestIdentityCredentialSourceImpl::SetAccounts(
      &request, std::move(accounts_response.accounts));
  TestIdentityCredentialSourceImpl::SetIdpInfo(&request, config_url,
                                               std::move(idp_info));
  TestIdentityCredentialSourceImpl::SetIdentitySelectionType(
      &request, Request::kAutoPassive);

  // Should succeed because it is cross site but same party.
  EXPECT_TRUE(source_->SelectAccount(idp_origin, kAccountId));

  RequestPageData::GetOrCreateForPage(main_rfh()->GetPage())
      ->SetPendingWebIdentityRequest(nullptr);
}

// Tests that GetIdentityCredentialSuggestions() filters out accounts from an
// existing pending request if the iframe is third-party to the top-frame
// origin.
TEST_F(IdentityCredentialSourceImplTest,
       GetIdentityCredentialSuggestionsFilterCrossSiteThirdParty) {
  GURL config_url(kConfigUrl);

  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other.com"), subframe);

  IdentityCredentialSourceImpl* subframe_source =
      IdentityCredentialSourceImpl::GetOrCreateForCurrentDocument(subframe);

  auto subframe_network_manager =
      std::make_unique<NiceMock<MockIdpNetworkRequestManager>>();
  MockIdpNetworkRequestManager* subframe_network_manager_ptr =
      subframe_network_manager.get();
  subframe_source->SetNetworkManagerForTests(
      std::move(subframe_network_manager));

  MockApiPermissionDelegate api_permission_delegate;
  EXPECT_CALL(api_permission_delegate, GetApiPermissionStatus)
      .WillRepeatedly(Return(FederatedIdentityApiPermissionContextDelegate::
                                 PermissionStatus::GRANTED));

  MockAutoReauthnPermissionDelegate auto_reauthn_permission_delegate;
  EXPECT_CALL(auto_reauthn_permission_delegate, IsAutoReauthnSettingEnabled)
      .WillRepeatedly(Return(false));

  MockIdentityRegistry identity_registry(web_contents(), nullptr, config_url);
  mojo::Remote<blink::mojom::FederatedAuthRequest> remote;

  Request& request = Request::CreateForTesting(
      *subframe, &api_permission_delegate, &auto_reauthn_permission_delegate,
      permission_delegate_.get(), &identity_registry,
      remote.BindNewPipeAndPassReceiver());

  TestIdentityCredentialSourceImpl::InitializeRequest(
      &request, std::make_unique<NiceMock<MockIdpNetworkRequestManager>>());
  request.SetDialogControllerForTests(
      std::make_unique<NiceMock<MockIdentityRequestDialogController>>());

  RequestPageData::GetOrCreateForPage(subframe->GetPage())
      ->SetPendingWebIdentityRequest(&request);

  blink::mojom::IdentityProviderRequestOptionsPtr options =
      blink::mojom::IdentityProviderRequestOptions::New();
  options->config = blink::mojom::IdentityProviderConfig::New();
  options->config->config_url = config_url;

  auto idp_info = std::make_unique<IdentityProviderInfo>(
      options, IdpNetworkRequestManager::Endpoints(),
      IdentityProviderMetadata(), blink::mojom::RpContext::kSignIn,
      blink::mojom::RpMode::kPassive, std::nullopt);
  idp_info->client_is_third_party_to_top_frame_origin = true;
  IdentityProviderMetadata idp_metadata;
  idp_metadata.config_url = config_url;
  idp_info->data = base::MakeRefCounted<IdentityProviderData>(
      "idp", idp_metadata, ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
      blink::mojom::RpContext::kSignIn, std::nullopt,
      std::vector<IdentityRequestDialogDisclosureField>(),
      /*has_login_status_mismatch=*/false);

  IdentityRequestAccountPtr account =
      base::MakeRefCounted<IdentityRequestAccount>(
          kAccountId, "test@example.com", "Test User", "test@example.com",
          "Test User", "Test", GURL(), "", "", std::vector<std::string>(),
          std::vector<std::string>(), std::vector<std::string>(),
          std::vector<std::string>(),
          content::IdentityRequestAccount::LoginState::kSignIn);
  account->identity_provider = idp_info->data;

  IdpNetworkRequestManager::AccountsResponse accounts_response;
  accounts_response.site_salt = "fc432178f9155c4e24762de5b9505f2e";
  accounts_response.accounts.push_back(account);

  IdpNetworkRequestManager::AccountsResponse accounts_response_copy;
  accounts_response_copy.site_salt = accounts_response.site_salt;
  accounts_response_copy.accounts.push_back(account);

  TestIdentityCredentialSourceImpl::SetAccounts(
      &request, std::move(accounts_response.accounts));
  TestIdentityCredentialSourceImpl::SetIdpInfo(&request, config_url,
                                               std::move(idp_info));

  // If the accounts are filtered out from the pending request, it will
  // proceed to fetch. We mock the fetch here.
  EXPECT_CALL(*permission_delegate_,
              GetIdpSigninStatus(url::Origin::Create(config_url)))
      .WillRepeatedly(Return(true));

  IdpNetworkRequestManager::WellKnown well_known;
  well_known.provider_urls = std::set<GURL>{config_url};
  EXPECT_CALL(*subframe_network_manager_ptr, FetchWellKnown(config_url, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          FetchStatus{ParseStatus::kSuccess, 200}, well_known));

  IdpNetworkRequestManager::Endpoints endpoints;
  endpoints.token = GURL(kTokenUrl);
  endpoints.accounts = GURL(kAccountsUrl);
  endpoints.client_metadata = GURL("https://idp.example/client_metadata");
  IdentityProviderMetadata idp_metadata_fetch;
  idp_metadata_fetch.idp_login_url = GURL("https://idp.example/login");
  EXPECT_CALL(*subframe_network_manager_ptr, FetchConfig(config_url, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          FetchStatus{ParseStatus::kSuccess, 200}, endpoints,
          idp_metadata_fetch));

  // Mock SendAccountsRequest returning an account that will be filtered out
  // in OnAccountsFetchCompleted.
  EXPECT_CALL(*subframe_network_manager_ptr,
              SendAccountsRequest(_, GURL(kAccountsUrl), _))
      .WillOnce(
          [&](const url::Origin&, const GURL&,
              IdpNetworkRequestManager::AccountsRequestCallback callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               FetchStatus{ParseStatus::kSuccess, 200},
                               std::move(accounts_response_copy)));
            return true;
          });

  // Mock FetchClientMetadata
  IdpNetworkRequestManager::ClientMetadata client_metadata;
  client_metadata.client_is_third_party_to_top_frame_origin = true;
  EXPECT_CALL(*subframe_network_manager_ptr, FetchClientMetadata)
      .WillOnce(base::test::RunOnceCallback<4>(
          FetchStatus{ParseStatus::kSuccess, 200}, client_metadata));

  base::RunLoop run_loop;
  subframe_source->GetIdentityCredentialSuggestions(
      {config_url},
      base::BindLambdaForTesting(
          [&](const std::optional<std::vector<IdentityRequestAccountPtr>>&
                  accounts) {
            ASSERT_TRUE(accounts.has_value());
            // Should be empty because it was filtered out.
            EXPECT_TRUE(accounts->empty());
            run_loop.Quit();
          }));
  run_loop.Run();

  RequestPageData::GetOrCreateForPage(subframe->GetPage())
      ->SetPendingWebIdentityRequest(nullptr);
}

}  // namespace content::webid
