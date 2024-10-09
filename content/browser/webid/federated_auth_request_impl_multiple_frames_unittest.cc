// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/test/federated_auth_request_request_token_callback_helper.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_auto_reauthn_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_registry.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_modal_dialog_view_delegate.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using AuthRequestCallbackHelper =
    content::FederatedAuthRequestRequestTokenCallbackHelper;
using FedCmEntry = ukm::builders::Blink_FedCm;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using RequestTokenCallback =
    content::FederatedAuthRequestImpl::RequestTokenCallback;
using blink::mojom::RequestTokenStatus;
using ::testing::NiceMock;

namespace content {

namespace {

constexpr char kIdpUrl[] = "https://idp.example/";
constexpr char kProviderUrlFull[] = "https://idp.example/fedcm.json";
constexpr char kTopFrameUrl[] = "https://top-frame.example/";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kLoginUrl[] = "https://idp.example/login";
constexpr char kClientId[] = "client_id_123";
constexpr char kNonce[] = "nonce123";
constexpr char kAccountId[] = "1234";
constexpr char kToken[] = "[not a real token]";

static std::vector<IdentityRequestAccountPtr> kAccounts;

// IdpNetworkRequestManager which returns valid data from IdP.
class TestIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  void FetchWellKnown(const GURL& provider,
                      FetchWellKnownCallback callback) override {
    IdpNetworkRequestManager::WellKnown well_known;
    std::set<GURL> well_known_configs;
    well_known_configs.insert(GURL(kProviderUrlFull));
    well_known.provider_urls = std::move(well_known_configs);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), kFetchStatusSuccess, well_known));
  }

  void FetchConfig(const GURL& provider,
                   blink::mojom::RpMode rp_mode,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.token = GURL(kTokenEndpoint);
    endpoints.accounts = GURL(kAccountsEndpoint);

    IdentityProviderMetadata idp_metadata;
    idp_metadata.idp_login_url = GURL(kLoginUrl);
    idp_metadata.config_url = provider;

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), kFetchStatusSuccess,
                                  endpoints, idp_metadata));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), kFetchStatusSuccess, kAccounts));
  }

  void SendTokenRequest(
      const GURL& token_url,
      const std::string& account,
      const std::string& url_encoded_post_data,
      TokenRequestCallback callback,
      ContinueOnCallback continue_on,
      RecordErrorMetricsCallback record_error_metrics_callback) override {
    TokenResult result;
    result.token = kToken;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), kFetchStatusSuccess, result));
  }

 private:
  FetchStatus kFetchStatusSuccess{
      IdpNetworkRequestManager::ParseStatus::kSuccess, net::HTTP_OK};
};

class TestDialogController
    : public NiceMock<MockIdentityRequestDialogController> {
 public:
  struct State {
    bool did_show_accounts_dialog{false};
    std::string rp_for_display;
  };

  enum class AccountsDialogAction {
    kNone,
    kSelectAccount,
  };

  // `state` is a pointer parameter so that it can outlive TestDialogController.
  TestDialogController(AccountsDialogAction accounts_dialog_action,
                       State* state)
      : accounts_dialog_action_(accounts_dialog_action), state_(state) {}

  ~TestDialogController() override = default;
  TestDialogController(TestDialogController&) = delete;
  TestDialogController& operator=(TestDialogController&) = delete;

  bool ShowAccountsDialog(
      const std::string& rp_for_display,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      IdentityRequestAccount::SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode,
      const std::vector<IdentityRequestAccountPtr>& new_accounts,
      IdentityRequestDialogController::AccountSelectionCallback on_selected,
      IdentityRequestDialogController::LoginToIdPCallback on_add_account,
      IdentityRequestDialogController::DismissCallback dismiss_callback,
      IdentityRequestDialogController::AccountsDisplayedCallback
          accounts_displayed_callback) override {
    state_->did_show_accounts_dialog = true;
    state_->rp_for_display = rp_for_display;
    if (accounts_dialog_action_ == AccountsDialogAction::kSelectAccount) {
      std::move(on_selected)
          .Run(GURL(kProviderUrlFull), kAccountId, /*is_sign_in=*/true);
    }
    return true;
  }

 private:
  AccountsDialogAction accounts_dialog_action_{AccountsDialogAction::kNone};
  raw_ptr<State> state_;
};

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    return ApiPermissionStatus::GRANTED;
  }
};

class TestFederatedIdentityModalDialogViewDelegate
    : public NiceMock<MockModalDialogViewDelegate> {
 public:
  base::WeakPtr<TestFederatedIdentityModalDialogViewDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestFederatedIdentityModalDialogViewDelegate>
      weak_ptr_factory_{this};
};

}  // namespace

class FederatedAuthRequestImplMultipleFramesTest
    : public RenderViewHostImplTestHarness {
 protected:
  FederatedAuthRequestImplMultipleFramesTest() = default;
  ~FederatedAuthRequestImplMultipleFramesTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    // Initialize `kAccounts` on SetUp() to ensure it is initialized correctly
    // in every test.
    kAccounts = {base::MakeRefCounted<IdentityRequestAccount>(
        kAccountId,                  // id
        "ken@idp.example",           // email
        "Ken R. Example",            // name
        "Ken",                       // given_name
        GURL(),                      // picture
        std::vector<std::string>(),  // login_hints
        std::vector<std::string>(),  // domain_hints
        std::vector<std::string>()   // labels
        )};
    test_api_permission_delegate_ =
        std::make_unique<TestApiPermissionDelegate>();
    mock_auto_reauthn_permission_delegate_ =
        std::make_unique<NiceMock<MockAutoReauthnPermissionDelegate>>();
    mock_permission_delegate_ =
        std::make_unique<NiceMock<MockPermissionDelegate>>();
    test_modal_dialog_view_delegate_ =
        std::make_unique<TestFederatedIdentityModalDialogViewDelegate>();
    mock_identity_registry_ = std::make_unique<NiceMock<MockIdentityRegistry>>(
        web_contents(), test_modal_dialog_view_delegate_->GetWeakPtr(),
        GURL(kIdpUrl));

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kTopFrameUrl), ui::PAGE_TRANSITION_LINK);
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Does token request and waits for result.
  void DoRequestTokenAndWait(
      mojo::Remote<blink::mojom::FederatedAuthRequest>& request_remote,
      AuthRequestCallbackHelper& callback_helper) {
    DoRequestToken(request_remote, callback_helper.callback());
    request_remote.set_disconnect_handler(callback_helper.quit_closure());

    // Ensure that the request makes its way to FederatedAuthRequestImpl.
    base::RunLoop().RunUntilIdle();
    // Fast forward clock so that the pending
    // FederatedAuthRequestImpl::OnRejectRequest() task, if any, gets a
    // chance to run.
    task_environment()->FastForwardBy(base::Minutes(10));

    callback_helper.WaitForCallback();
    request_remote.set_disconnect_handler(base::OnceClosure());
  }

  FederatedAuthRequestImpl* CreateFederatedAuthRequestImpl(
      RenderFrameHost& render_frame_host,
      mojo::Remote<blink::mojom::FederatedAuthRequest>& request_remote,
      TestDialogController::AccountsDialogAction accounts_dialog_action,
      TestDialogController::State* dialog_controller_state) {
    FederatedAuthRequestImpl* federated_auth_request_impl =
        &FederatedAuthRequestImpl::CreateForTesting(
            render_frame_host, test_api_permission_delegate_.get(),
            mock_auto_reauthn_permission_delegate_.get(),
            mock_permission_delegate_.get(), mock_identity_registry_.get(),
            request_remote.BindNewPipeAndPassReceiver());
    federated_auth_request_impl->SetDialogControllerForTests(
        std::make_unique<TestDialogController>(accounts_dialog_action,
                                               dialog_controller_state));
    federated_auth_request_impl->SetNetworkManagerForTests(
        std::make_unique<TestIdpNetworkRequestManager>());
    return federated_auth_request_impl;
  }

  void DoRequestToken(
      mojo::Remote<blink::mojom::FederatedAuthRequest>& request_remote,
      RequestTokenCallback callback) {
    auto config_ptr = blink::mojom::IdentityProviderConfig::New();
    config_ptr->config_url = GURL(kProviderUrlFull);
    config_ptr->client_id = kClientId;
    auto federated = blink::mojom::IdentityProviderRequestOptions::New();
    federated->config = std::move(config_ptr);
    federated->nonce = kNonce;
    std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> idp_ptrs;
    idp_ptrs.push_back(std::move(federated));
    auto get_params = blink::mojom::IdentityProviderGetParameters::New(
        std::move(idp_ptrs),
        /*rp_context=*/blink::mojom::RpContext::kSignIn,
        /*rp_mode=*/blink::mojom::RpMode::kPassive);
    std::vector<blink::mojom::IdentityProviderGetParametersPtr> idp_get_params;
    idp_get_params.push_back(std::move(get_params));

    request_remote->RequestToken(std::move(idp_get_params),
                                 MediationRequirement::kOptional,
                                 std::move(callback));
    request_remote.FlushForTesting();
  }

 protected:
  std::unique_ptr<TestApiPermissionDelegate> test_api_permission_delegate_;
  std::unique_ptr<NiceMock<MockAutoReauthnPermissionDelegate>>
      mock_auto_reauthn_permission_delegate_;
  std::unique_ptr<NiceMock<MockPermissionDelegate>> mock_permission_delegate_;
  std::unique_ptr<TestFederatedIdentityModalDialogViewDelegate>
      test_modal_dialog_view_delegate_;
  std::unique_ptr<NiceMock<MockIdentityRegistry>> mock_identity_registry_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Test that test harness can execute successful FedCM flow for iframe.
TEST_F(FederatedAuthRequestImplMultipleFramesTest, TestHarness) {
  RenderFrameHost* iframe_rfh = content::RenderFrameHostTester::For(main_rfh())
                                    ->AppendChild(/*frame_name=*/"");

  mojo::Remote<blink::mojom::FederatedAuthRequest> iframe_request_remote;
  TestDialogController::State iframe_dialog_state;
  CreateFederatedAuthRequestImpl(
      *iframe_rfh, iframe_request_remote,
      TestDialogController::AccountsDialogAction::kSelectAccount,
      &iframe_dialog_state);

  AuthRequestCallbackHelper iframe_callback_helper;
  DoRequestTokenAndWait(iframe_request_remote, iframe_callback_helper);
  EXPECT_EQ(RequestTokenStatus::kSuccess, iframe_callback_helper.status());
  EXPECT_TRUE(iframe_dialog_state.did_show_accounts_dialog);
}

// Test that FedCM request fails on iframe if there is an in-progress FedCM
// request for a different frame on the page.
TEST_F(FederatedAuthRequestImplMultipleFramesTest, IframeTooManyRequests) {
  mojo::Remote<blink::mojom::FederatedAuthRequest> main_frame_request_remote;
  TestDialogController::State main_frame_dialog_state;
  CreateFederatedAuthRequestImpl(
      *main_rfh(), main_frame_request_remote,
      TestDialogController::AccountsDialogAction::kNone,
      &main_frame_dialog_state);
  DoRequestToken(main_frame_request_remote, RequestTokenCallback());
  EXPECT_TRUE(main_frame_dialog_state.did_show_accounts_dialog);

  RenderFrameHost* iframe_rfh = content::RenderFrameHostTester::For(main_rfh())
                                    ->AppendChild(/*frame_name=*/"");
  mojo::Remote<blink::mojom::FederatedAuthRequest> iframe_request_remote;
  TestDialogController::State iframe_dialog_state;
  CreateFederatedAuthRequestImpl(
      *iframe_rfh, iframe_request_remote,
      TestDialogController::AccountsDialogAction::kSelectAccount,
      &iframe_dialog_state);

  AuthRequestCallbackHelper iframe_callback_helper;
  DoRequestTokenAndWait(iframe_request_remote, iframe_callback_helper);
  EXPECT_EQ(RequestTokenStatus::kErrorTooManyRequests,
            iframe_callback_helper.status());
  EXPECT_FALSE(iframe_dialog_state.did_show_accounts_dialog);
}

// Test that only top frame URL is available for display when FedCM is called
// within iframes which are same-origin with the top frame.
TEST_F(FederatedAuthRequestImplMultipleFramesTest, SameOriginIframe) {
  const char kSameOriginIframeUrl[] = "https://top-frame.example/iframe.html";
  RenderFrameHost* same_origin_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSameOriginIframeUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("same_origin_iframe"));

  mojo::Remote<blink::mojom::FederatedAuthRequest> iframe_request_remote;
  TestDialogController::State iframe_dialog_state;
  CreateFederatedAuthRequestImpl(
      *same_origin_iframe, iframe_request_remote,
      TestDialogController::AccountsDialogAction::kSelectAccount,
      &iframe_dialog_state);

  AuthRequestCallbackHelper iframe_callback_helper;
  DoRequestTokenAndWait(iframe_request_remote, iframe_callback_helper);
  EXPECT_EQ(RequestTokenStatus::kSuccess, iframe_callback_helper.status());
  EXPECT_TRUE(iframe_dialog_state.did_show_accounts_dialog);
  EXPECT_EQ("top-frame.example", iframe_dialog_state.rp_for_display);
}

// Test that only top frame URL is available for display when FedCM is called
// within iframes which are same-site with the top frame.
TEST_F(FederatedAuthRequestImplMultipleFramesTest, SameSiteIframe) {
  const char kSameSiteIframeUrl[] =
      "https://subdomain.top-frame.example/iframe.html";
  RenderFrameHost* same_site_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSameSiteIframeUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("same_site_iframe"));

  mojo::Remote<blink::mojom::FederatedAuthRequest> iframe_request_remote;
  TestDialogController::State iframe_dialog_state;
  CreateFederatedAuthRequestImpl(
      *same_site_iframe, iframe_request_remote,
      TestDialogController::AccountsDialogAction::kSelectAccount,
      &iframe_dialog_state);

  AuthRequestCallbackHelper iframe_callback_helper;
  DoRequestTokenAndWait(iframe_request_remote, iframe_callback_helper);
  EXPECT_EQ(RequestTokenStatus::kSuccess, iframe_callback_helper.status());
  EXPECT_TRUE(iframe_dialog_state.did_show_accounts_dialog);
  EXPECT_EQ("top-frame.example", iframe_dialog_state.rp_for_display);
}

// Test that both top frame and iframe URLs are available for display when FedCM
// is called within iframes which are cross-site with the top frame.
TEST_F(FederatedAuthRequestImplMultipleFramesTest, CrossSiteIframe) {
  const char kCrossSiteIframeUrl[] = "https://cross-site.example/iframe.html";
  RenderFrameHost* cross_site_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kCrossSiteIframeUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("cross_site_iframe"));

  mojo::Remote<blink::mojom::FederatedAuthRequest> iframe_request_remote;
  TestDialogController::State iframe_dialog_state;
  CreateFederatedAuthRequestImpl(
      *cross_site_iframe, iframe_request_remote,
      TestDialogController::AccountsDialogAction::kSelectAccount,
      &iframe_dialog_state);

  AuthRequestCallbackHelper iframe_callback_helper;
  DoRequestTokenAndWait(iframe_request_remote, iframe_callback_helper);
  EXPECT_EQ(RequestTokenStatus::kSuccess, iframe_callback_helper.status());
  EXPECT_TRUE(iframe_dialog_state.did_show_accounts_dialog);
  EXPECT_EQ("top-frame.example", iframe_dialog_state.rp_for_display);
}

// Tests that preventSilentAccess UKM is not recorded if the embedder does not
// have sharing permissions.
TEST_F(FederatedAuthRequestImplMultipleFramesTest,
       IframePreventSilentAccessNoSharingPermission) {
  const char kSameSiteIframeUrl[] =
      "https://subdomain.top-frame.example/iframe.html";
  RenderFrameHost* same_site_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSameSiteIframeUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("same_site_iframe"));

  mojo::Remote<blink::mojom::FederatedAuthRequest> iframe_request_remote;
  TestDialogController::State iframe_dialog_state;
  auto* federated_auth_request_impl = CreateFederatedAuthRequestImpl(
      *same_site_iframe, iframe_request_remote,
      TestDialogController::AccountsDialogAction::kSelectAccount,
      &iframe_dialog_state);

  // Assume that the embeddder does not have a sharing permission, and hence UKM
  // should not be recorded.
  EXPECT_CALL(*mock_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kTopFrameUrl))))
      .WillOnce(testing::Return(false));

  base::RunLoop ukm_loop;
  ukm_recorder_->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                       ukm_loop.QuitClosure());
  federated_auth_request_impl->PreventSilentAccess(base::DoNothing());

  // Perform an actual FedCM request to log some metrics and flush the ukm
  // recorder.
  AuthRequestCallbackHelper iframe_callback_helper;
  DoRequestTokenAndWait(iframe_request_remote, iframe_callback_helper);

  ukm_loop.Run();

  auto entries = ukm_recorder_->GetEntriesByName(FedCmEntry::kEntryName);
  ASSERT_FALSE(entries.empty());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    const int64_t* metric =
        ukm_recorder_->GetEntryMetric(entry, "PreventSilentAccessFrameType");
    EXPECT_FALSE(metric);
  }
}

// Tests the preventSilentAccess UKM recorded when invoked from the main frame.
TEST_F(FederatedAuthRequestImplMultipleFramesTest,
       MainFramePreventSilentAccess) {
  // We add an iframe but it should not affect the UKM recording since we will
  // call preventSilentAccess() from the main frame.
  const char kSameSiteIframeUrl[] =
      "https://subdomain.top-frame.example/iframe.html";
  NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kSameSiteIframeUrl),
      RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendChild("same_site_iframe"));

  mojo::Remote<blink::mojom::FederatedAuthRequest> main_frame_request_remote;
  TestDialogController::State main_frame_dialog_state;
  auto* federated_auth_request_impl = CreateFederatedAuthRequestImpl(
      *main_rfh(), main_frame_request_remote,
      TestDialogController::AccountsDialogAction::kNone,
      &main_frame_dialog_state);

  // Assume that the embeddder does has a sharing permission so that UKM is
  // recorded.
  EXPECT_CALL(*mock_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kTopFrameUrl))))
      .WillOnce(testing::Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder_->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                       ukm_loop.QuitClosure());
  federated_auth_request_impl->PreventSilentAccess(base::DoNothing());
  ukm_loop.Run();

  auto entries = ukm_recorder_->GetEntriesByName(FedCmEntry::kEntryName);
  ASSERT_FALSE(entries.empty());
  bool metric_found = false;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    const int64_t* metric =
        ukm_recorder_->GetEntryMetric(entry, "PreventSilentAccessFrameType");
    if (metric) {
      metric_found = true;
      EXPECT_EQ(*metric, static_cast<int>(FedCmRequesterFrameType::kMainFrame));
    }
  }
  EXPECT_TRUE(metric_found);
}

// Tests the preventSilentAccess UKM recorded when invoked from a same site
// iframe.
TEST_F(FederatedAuthRequestImplMultipleFramesTest,
       SameSiteIframePreventSilentAccess) {
  const char kSameSiteIframeUrl[] =
      "https://subdomain.top-frame.example/iframe.html";
  RenderFrameHost* same_site_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSameSiteIframeUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("same_site_iframe"));

  mojo::Remote<blink::mojom::FederatedAuthRequest> iframe_request_remote;
  TestDialogController::State iframe_dialog_state;
  auto* federated_auth_request_impl = CreateFederatedAuthRequestImpl(
      *same_site_iframe, iframe_request_remote,
      TestDialogController::AccountsDialogAction::kSelectAccount,
      &iframe_dialog_state);

  // Assume that the embeddder does has a sharing permission so that UKM is
  // recorded.
  EXPECT_CALL(*mock_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kTopFrameUrl))))
      .WillOnce(testing::Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder_->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                       ukm_loop.QuitClosure());
  federated_auth_request_impl->PreventSilentAccess(base::DoNothing());
  ukm_loop.Run();

  auto entries = ukm_recorder_->GetEntriesByName(FedCmEntry::kEntryName);
  ASSERT_FALSE(entries.empty());
  bool metric_found = false;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    const int64_t* metric =
        ukm_recorder_->GetEntryMetric(entry, "PreventSilentAccessFrameType");
    if (metric) {
      metric_found = true;
      EXPECT_EQ(*metric,
                static_cast<int>(FedCmRequesterFrameType::kSameSiteIframe));
    }
  }
  EXPECT_TRUE(metric_found);
}

// Tests the preventSilentAccess UKM recorded when invoked from a cross site
// iframe.
TEST_F(FederatedAuthRequestImplMultipleFramesTest,
       CrossSiteIframePreventSilentAccess) {
  const char kCrossSiteIframeUrl[] = "https://cross-site.example/iframe.html";
  RenderFrameHost* cross_site_iframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kCrossSiteIframeUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("cross_site_iframe"));

  mojo::Remote<blink::mojom::FederatedAuthRequest> iframe_request_remote;
  TestDialogController::State iframe_dialog_state;
  auto* federated_auth_request_impl = CreateFederatedAuthRequestImpl(
      *cross_site_iframe, iframe_request_remote,
      TestDialogController::AccountsDialogAction::kSelectAccount,
      &iframe_dialog_state);

  // Assume that the embeddder does has a sharing permission so that UKM is
  // recorded.
  EXPECT_CALL(*mock_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kTopFrameUrl))))
      .WillOnce(testing::Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder_->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                       ukm_loop.QuitClosure());
  federated_auth_request_impl->PreventSilentAccess(base::DoNothing());
  ukm_loop.Run();

  auto entries = ukm_recorder_->GetEntriesByName(FedCmEntry::kEntryName);
  ASSERT_FALSE(entries.empty());
  bool metric_found = false;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    const int64_t* metric =
        ukm_recorder_->GetEntryMetric(entry, "PreventSilentAccessFrameType");
    if (metric) {
      metric_found = true;
      EXPECT_EQ(*metric,
                static_cast<int>(FedCmRequesterFrameType::kCrossSiteIframe));
    }
  }
  EXPECT_TRUE(metric_found);
}

}  // namespace content
