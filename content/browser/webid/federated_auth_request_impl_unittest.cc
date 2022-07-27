// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/test/mock_active_session_permission_delegate.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_sharing_permission_delegate.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_features.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::LogoutRpsRequest;
using blink::mojom::LogoutRpsRequestPtr;
using blink::mojom::LogoutRpsStatus;
using blink::mojom::RequestTokenStatus;
using AccountList = content::IdpNetworkRequestManager::AccountList;
using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using DismissReason = content::IdentityRequestDialogController::DismissReason;
using FedCmEntry = ukm::builders::Blink_FedCm;
using FedCmIdpEntry = ukm::builders::Blink_FedCmIdp;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

constexpr char kIdpTestOrigin[] = "https://idp.example";
constexpr char kProviderUrl[] = "https://idp.example/";
constexpr char kProviderUrlFull[] = "https://idp.example/fedcm.json";
constexpr char kRpUrl[] = "https://rp.example/";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kCrossOriginAccountsEndpoint[] = "https://idp2.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kClientMetadataEndpoint[] =
    "https://idp.example/client_metadata";
constexpr char kRevocationEndpoint[] = "https://idp.example/revoke";
constexpr char kPrivacyPolicyUrl[] = "https://rp.example/pp";
constexpr char kTermsOfServiceUrl[] = "https://rp.example/tos";
constexpr char kClientId[] = "client_id_123";
constexpr char kNonce[] = "nonce123";
constexpr char kAccountId[] = "1234";

// Values will be added here as token introspection is implemented.
constexpr char kToken[] = "[not a real token]";
constexpr char kEmptyToken[] = "";

static const std::initializer_list<IdentityRequestAccount> kAccounts{{
    kAccountId,         // id
    "ken@idp.example",  // email
    "Ken R. Example",   // name
    "Ken",              // given_name
    GURL()              // picture
}};

static const std::set<std::string> kManifestList{kProviderUrlFull};

// Parameters for a call to RequestToken.
struct RequestParameters {
  const char* provider;
  const char* client_id;
  const char* nonce;
  bool prefer_auto_sign_in;
};

enum FetchedEndpoint {
  MANIFEST = 1,
  CLIENT_METADATA = 1 << 1,
  ACCOUNTS = 1 << 2,
  TOKEN = 1 << 3,
  REVOCATION = 1 << 4,
  MANIFEST_LIST = 1 << 5,
};

// All endpoints which are fetched in a successful
// FederatedAuthRequestImpl::RequestToken() request.
int FETCH_ENDPOINT_ALL_REQUEST_TOKEN =
    FetchedEndpoint::MANIFEST | FetchedEndpoint::CLIENT_METADATA |
    FetchedEndpoint::ACCOUNTS | FetchedEndpoint::TOKEN |
    FetchedEndpoint::MANIFEST_LIST;

// Expected return values from a call to RequestToken.
struct RequestExpectations {
  absl::optional<RequestTokenStatus> return_status;
  absl::optional<FederatedAuthRequestResult> devtools_issue_status;
  // Any combination of FetchedEndpoint flags.
  int fetched_endpoints;
};

// Mock configuration values for test.
struct MockClientIdConfiguration {
  FetchStatus fetch_status;
  const char* privacy_policy_url;
  const char* terms_of_service_url;
};

struct MockManifestList {
  std::set<std::string> provider_urls;
};

struct MockManifest {
  FetchStatus fetch_status;
  const char* accounts_endpoint;
  const char* token_endpoint;
  const char* client_metadata_endpoint;
  const char* revocation_endpoint;
};

struct MockConfiguration {
  const char* token;
  MockManifestList manifest_list;
  MockManifest manifest;
  MockClientIdConfiguration client_metadata;
  FetchStatus accounts_response;
  AccountList accounts;
  FetchStatus token_response;
  bool delay_token_response;
  bool customized_dialog;
  bool wait_for_callback;
  std::string post_request_body;
};

static const MockClientIdConfiguration kDefaultClientMetadata{
    FetchStatus::kSuccess, kPrivacyPolicyUrl, kTermsOfServiceUrl};

static const RequestParameters kDefaultRequestParameters{
    kProviderUrlFull, kClientId, kNonce, /*prefer_auto_sign_in=*/false};

static const MockConfiguration kConfigurationValid{
    kToken,
    {kManifestList},
    {
        FetchStatus::kSuccess,
        kAccountsEndpoint,
        kTokenEndpoint,
        kClientMetadataEndpoint,
        kRevocationEndpoint,
    },
    kDefaultClientMetadata,
    FetchStatus::kSuccess,
    kAccounts,
    FetchStatus::kSuccess,
    false /* delay_token_response */,
    false /* customized_dialog */,
    true /* wait_for_callback */,
    "" /* post_request_body */};

static const RequestExpectations kExpectationSuccess{
    RequestTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
    FETCH_ENDPOINT_ALL_REQUEST_TOKEN};

// Helper class for receiving the mojo method callback.
class AuthRequestCallbackHelper {
 public:
  AuthRequestCallbackHelper() = default;
  ~AuthRequestCallbackHelper() = default;

  AuthRequestCallbackHelper(const AuthRequestCallbackHelper&) = delete;
  AuthRequestCallbackHelper& operator=(const AuthRequestCallbackHelper&) =
      delete;

  absl::optional<RequestTokenStatus> status() const { return status_; }
  absl::optional<std::string> token() const { return token_; }

  // This can only be called once per lifetime of this object.
  base::OnceCallback<void(RequestTokenStatus,
                          const absl::optional<std::string>&)>
  callback() {
    return base::BindOnce(&AuthRequestCallbackHelper::ReceiverMethod,
                          base::Unretained(this));
  }

  bool was_callback_called() const { return was_called_; }

  // Returns when callback() is called, which can be immediately if it has
  // already been called.
  void WaitForCallback() {
    if (was_called_)
      return;
    wait_for_callback_loop_.Run();
  }

 private:
  void ReceiverMethod(RequestTokenStatus status,
                      const absl::optional<std::string>& token) {
    CHECK(!was_called_);
    status_ = status;
    token_ = token;
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  absl::optional<RequestTokenStatus> status_;
  absl::optional<std::string> token_;
};

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

// Forwards IdpNetworkRequestManager calls to delegate. The purpose of this
// class is to enable querying the delegate after FederatedAuthRequestImpl
// destroys DelegatedIdpNetworkRequestManager.
class DelegatedIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  explicit DelegatedIdpNetworkRequestManager(IdpNetworkRequestManager* delegate)
      : delegate_(delegate) {
    DCHECK(delegate_);
  }

  void FetchManifestList(FetchManifestListCallback callback) override {
    delegate_->FetchManifestList(std::move(callback));
  }

  void FetchManifest(absl::optional<int> idp_brand_icon_ideal_size,
                     absl::optional<int> idp_brand_icon_minimum_size,
                     FetchManifestCallback callback) override {
    delegate_->FetchManifest(idp_brand_icon_ideal_size,
                             idp_brand_icon_minimum_size, std::move(callback));
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    delegate_->FetchClientMetadata(endpoint, client_id, std::move(callback));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    delegate_->SendAccountsRequest(accounts_url, client_id,
                                   std::move(callback));
  }

  void SendTokenRequest(const GURL& token_url,
                        const std::string& account,
                        const std::string& request,
                        TokenRequestCallback callback) override {
    delegate_->SendTokenRequest(token_url, account, request,
                                std::move(callback));
  }

  void SendLogout(const GURL& logout_url, LogoutCallback callback) override {
    delegate_->SendLogout(logout_url, std::move(callback));
  }

 private:
  raw_ptr<IdpNetworkRequestManager> delegate_;
};

class TestIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  void SetTestConfig(const MockConfiguration& configuration) {
    config_ = configuration;
  }

  void RunDelayedCallbacks() {
    for (base::OnceClosure& delayed_callback : delayed_callbacks_) {
      std::move(delayed_callback).Run();
    }
    delayed_callbacks_.clear();
  }

  void FetchManifestList(FetchManifestListCallback callback) override {
    fetched_endpoints_ |= FetchedEndpoint::MANIFEST_LIST;
    std::set<GURL> url_set(config_.manifest_list.provider_urls.begin(),
                           config_.manifest_list.provider_urls.end());
    std::move(callback).Run(FetchStatus::kSuccess, url_set);
  }

  void FetchManifest(absl::optional<int> idp_brand_icon_ideal_size,
                     absl::optional<int> idp_brand_icon_minimum_size,
                     FetchManifestCallback callback) override {
    fetched_endpoints_ |= FetchedEndpoint::MANIFEST;

    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.token = config_.manifest.token_endpoint;
    endpoints.accounts = config_.manifest.accounts_endpoint;
    endpoints.client_metadata = config_.manifest.client_metadata_endpoint;
    endpoints.revocation = config_.manifest.revocation_endpoint;
    std::move(callback).Run(config_.manifest.fetch_status, endpoints,
                            IdentityProviderMetadata());
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    fetched_endpoints_ |= FetchedEndpoint::CLIENT_METADATA;
    std::move(callback).Run(config_.client_metadata.fetch_status,
                            IdpNetworkRequestManager::ClientMetadata{
                                config_.client_metadata.privacy_policy_url,
                                config_.client_metadata.terms_of_service_url});
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    fetched_endpoints_ |= FetchedEndpoint::ACCOUNTS;
    std::move(callback).Run(config_.accounts_response, config_.accounts);
  }

  void SendTokenRequest(const GURL& token_url,
                        const std::string& account,
                        const std::string& request,
                        TokenRequestCallback callback) override {
    if (!config_.post_request_body.empty()) {
      EXPECT_EQ(config_.post_request_body, request);
    }
    fetched_endpoints_ |= FetchedEndpoint::TOKEN;
    std::string delivered_token =
        config_.token_response == FetchStatus::kSuccess ? config_.token
                                                        : std::string();
    base::OnceCallback bound_callback = base::BindOnce(
        std::move(callback), config_.token_response, delivered_token);
    if (config_.delay_token_response)
      delayed_callbacks_.push_back(std::move(bound_callback));
    else
      std::move(bound_callback).Run();
  }

  int get_fetched_endpoints() { return fetched_endpoints_; }

 protected:
  MockConfiguration config_{kConfigurationValid};
  int fetched_endpoints_{0};
  std::vector<base::OnceClosure> delayed_callbacks_;
};

class TestLogoutIdpNetworkRequestManager : public TestIdpNetworkRequestManager {
 public:
  void SendLogout(const GURL& logout_url, LogoutCallback callback) override {
    ++num_logout_requests_;
    std::move(callback).Run();
  }

  size_t num_logout_requests() { return num_logout_requests_; }

 protected:
  size_t num_logout_requests_{0};
};

// TestIdpNetworkRequestManager subclass which checks the values of the method
// params when executing an endpoint request.
class IdpNetworkRequestManagerParamChecker
    : public TestIdpNetworkRequestManager {
 public:
  void SetExpectations(const std::string& expected_client_id,
                       const std::string& expected_selected_account_id,
                       const std::string& expected_revocation_hint) {
    expected_client_id_ = expected_client_id;
    expected_selected_account_id_ = expected_selected_account_id;
    expected_revocation_hint_ = expected_revocation_hint;
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    EXPECT_EQ(config_.manifest.client_metadata_endpoint, endpoint);
    EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::FetchClientMetadata(endpoint, client_id,
                                                      std::move(callback));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    EXPECT_EQ(config_.manifest.accounts_endpoint, accounts_url);
    EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::SendAccountsRequest(accounts_url, client_id,
                                                      std::move(callback));
  }

  void SendTokenRequest(const GURL& token_url,
                        const std::string& account,
                        const std::string& request,
                        TokenRequestCallback callback) override {
    EXPECT_EQ(config_.manifest.token_endpoint, token_url);
    EXPECT_EQ(expected_selected_account_id_, account);
    TestIdpNetworkRequestManager::SendTokenRequest(token_url, account, request,
                                                   std::move(callback));
  }

 private:
  std::string expected_client_id_;
  std::string expected_selected_account_id_;
  std::string expected_revocation_hint_;
};

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  std::pair<url::Origin, ApiPermissionStatus> permission_override_ =
      std::make_pair(url::Origin(), ApiPermissionStatus::GRANTED);
  std::set<url::Origin> embargoed_origins_;

  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    if (embargoed_origins_.count(origin))
      return ApiPermissionStatus::BLOCKED_EMBARGO;

    return (origin == permission_override_.first)
               ? permission_override_.second
               : ApiPermissionStatus::GRANTED;
  }

  void RecordDismissAndEmbargo(const url::Origin& origin) override {
    embargoed_origins_.insert(origin);
  }

  void RemoveEmbargoAndResetCounts(const url::Origin& origin) override {
    embargoed_origins_.erase(origin);
  }
};

}  // namespace

class FederatedAuthRequestImplTest : public RenderViewHostImplTestHarness {
 protected:
  FederatedAuthRequestImplTest() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  ~FederatedAuthRequestImplTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    test_api_permission_delegate_ =
        std::make_unique<TestApiPermissionDelegate>();
    mock_sharing_permission_delegate_ =
        std::make_unique<NiceMock<MockSharingPermissionDelegate>>();
    mock_active_session_permission_delegate_ =
        std::make_unique<NiceMock<MockActiveSessionPermissionDelegate>>();

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kRpUrl), ui::PAGE_TRANSITION_LINK);

    federated_auth_request_impl_ = &FederatedAuthRequestImpl::CreateForTesting(
        *main_test_rfh(), request_remote_.BindNewPipeAndPassReceiver());
    auto mock_dialog_controller =
        std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
    mock_dialog_controller_ = mock_dialog_controller.get();
    federated_auth_request_impl_->SetDialogControllerForTests(
        std::move(mock_dialog_controller));

    federated_auth_request_impl_->SetApiPermissionDelegateForTests(
        test_api_permission_delegate_.get());
    federated_auth_request_impl_->SetSharingPermissionDelegateForTests(
        mock_sharing_permission_delegate_.get());
    federated_auth_request_impl()->SetActiveSessionPermissionDelegateForTests(
        mock_active_session_permission_delegate_.get());

    std::unique_ptr<TestIdpNetworkRequestManager> network_request_manager =
        std::make_unique<TestIdpNetworkRequestManager>();
    SetNetworkRequestManager(std::move(network_request_manager));

    federated_auth_request_impl_->SetTokenRequestDelayForTests(
        base::TimeDelta());
  }

  void SetNetworkRequestManager(
      std::unique_ptr<TestIdpNetworkRequestManager> manager) {
    test_network_request_manager_ = std::move(manager);
    // DelegatedIdpNetworkRequestManager is owned by
    // |federated_auth_request_impl_|.
    federated_auth_request_impl_->SetNetworkManagerForTests(
        std::make_unique<DelegatedIdpNetworkRequestManager>(
            test_network_request_manager_.get()));
  }

  void RunAuthTest(const RequestParameters& request_parameters,
                   const RequestExpectations& expectation,
                   const MockConfiguration& configuration) {
    test_network_request_manager_->SetTestConfig(configuration);
    SetMockExpectations(request_parameters, expectation, configuration);
    auto auth_response = PerformAuthRequest(
        GURL(request_parameters.provider), request_parameters.client_id,
        request_parameters.nonce, request_parameters.prefer_auto_sign_in,
        configuration.wait_for_callback);
    ASSERT_EQ(auth_response.first, expectation.return_status);
    if (auth_response.first == RequestTokenStatus::kSuccess) {
      EXPECT_EQ(configuration.token, auth_response.second);
    } else {
      EXPECT_TRUE(auth_response.second == absl::nullopt ||
                  auth_response.second == kEmptyToken);
    }

    EXPECT_EQ(expectation.fetched_endpoints,
              test_network_request_manager_->get_fetched_endpoints());

    if (expectation.devtools_issue_status) {
      int issue_count = main_test_rfh()->GetFederatedAuthRequestIssueCount(
          *expectation.devtools_issue_status);
      if (auth_response.first == RequestTokenStatus::kSuccess) {
        EXPECT_EQ(0, issue_count);
      } else {
        EXPECT_LT(0, issue_count);
      }
      CheckConsoleMessages(*expectation.devtools_issue_status);
    }
  }

  void CheckConsoleMessages(FederatedAuthRequestResult devtools_issue_status) {
    static std::unordered_map<FederatedAuthRequestResult,
                              absl::optional<std::string>>
        status_to_message = {
            {FederatedAuthRequestResult::kSuccess, absl::nullopt},
            {FederatedAuthRequestResult::kApprovalDeclined,
             "User declined the sign-in attempt."},
            {FederatedAuthRequestResult::kErrorDisabledInSettings,
             "Third-party sign in was disabled in browser Site Settings."},
            {FederatedAuthRequestResult::kErrorFetchingManifestListHttpNotFound,
             "The provider's FedCM manifest list file cannot be found."},
            {FederatedAuthRequestResult::kErrorFetchingManifestListNoResponse,
             "The provider's FedCM manifest list file fetch resulted in an "
             "error response code."},
            {FederatedAuthRequestResult::
                 kErrorFetchingManifestListInvalidResponse,
             "Provider's FedCM manifest list file is invalid."},
            {FederatedAuthRequestResult::kErrorManifestNotInManifestList,
             "Provider's FedCM manifest not listed in its manifest list."},
            {FederatedAuthRequestResult::kErrorManifestListTooBig,
             "Provider's FedCM manifest list contains too many providers."},
            {FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound,
             "The provider's FedCM manifest configuration cannot be found."},
            {FederatedAuthRequestResult::kErrorFetchingManifestNoResponse,
             "The provider's FedCM manifest configuration fetch resulted in an "
             "error response code."},
            {FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
             "Provider's FedCM manifest configuration is invalid."},
            {FederatedAuthRequestResult::kError, "Error retrieving a token."},
            {FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse,
             "The provider's accounts list fetch resulted in an error response "
             "code."},
            {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
             "Provider's accounts list is invalid. Should have received an "
             "\"accounts\" list, where each account must "
             "have at least \"id\", \"name\", and \"email\"."},
            {FederatedAuthRequestResult::
                 kErrorFetchingClientMetadataHttpNotFound,
             "The provider's client metadata endpoint cannot be found."},
            {FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse,
             "The provider's client metadata fetch resulted in an error "
             "response "
             "code."},
            {FederatedAuthRequestResult::
                 kErrorFetchingClientMetadataInvalidResponse,
             "Provider's client metadata is invalid."},
            {FederatedAuthRequestResult::
                 kErrorClientMetadataMissingPrivacyPolicyUrl,
             "Provider's client metadata is missing or has an invalid privacy "
             "policy url."},
            {FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse,
             "Provider's token is invalid."},
        };
    std::vector<std::string> messages =
        RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
    absl::optional<std::string> expected_message =
        status_to_message[devtools_issue_status];
    if (!expected_message) {
      EXPECT_EQ(0u, messages.size());
    } else {
      ASSERT_LE(1u, messages.size());
      EXPECT_EQ(expected_message.value(), messages[messages.size() - 1]);
    }
  }

  std::pair<absl::optional<RequestTokenStatus>, absl::optional<std::string>>
  PerformAuthRequest(const GURL& provider,
                     const std::string& client_id,
                     const std::string& nonce,
                     bool prefer_auto_sign_in,
                     bool wait_for_callback) {
    request_remote_->RequestToken(provider, client_id, nonce,
                                  prefer_auto_sign_in, auth_helper_.callback());
    // Ensure that the request makes its way to FederatedAuthRequestImpl.
    request_remote_.FlushForTesting();
    if (wait_for_callback) {
      // Fast forward clock so that the pending
      // FederatedAuthRequestImpl::OnRejectRequest() task, if any, gets a
      // chance to run.
      task_environment()->FastForwardBy(base::Minutes(10));
      auth_helper_.WaitForCallback();
    }
    return std::make_pair(auth_helper_.status(), auth_helper_.token());
  }

  LogoutRpsStatus PerformLogoutRequest(
      std::vector<LogoutRpsRequestPtr> logout_requests) {
    LogoutRpsRequestCallbackHelper logout_helper;
    request_remote_->LogoutRps(std::move(logout_requests),
                               logout_helper.callback());
    logout_helper.WaitForCallback();
    return logout_helper.status();
  }

  void SetMockExpectations(const RequestParameters& request_parameters,
                           const RequestExpectations& expectations,
                           const MockConfiguration& config) {
    if ((expectations.fetched_endpoints & FetchedEndpoint::ACCOUNTS) != 0 &&
        config.accounts_response == FetchStatus::kSuccess) {
      if (!request_parameters.prefer_auto_sign_in &&
          !config.customized_dialog) {
        // Expects a dialog if prefer_auto_sign_in is not set by RP. However,
        // even though the bit is set we may not exercise the AutoSignIn flow.
        // e.g. for sign up flow, multiple accounts, user opt-out etc. In this
        // case, it's up to the test to expect this mock function call.
        EXPECT_CALL(*mock_dialog_controller_,
                    ShowAccountsDialog(_, _, _, _, _, _, _, _))
            .WillOnce(Invoke(
                [&](content::WebContents* rp_web_contents,
                    const GURL& idp_signin_url,
                    base::span<const content::IdentityRequestAccount> accounts,
                    const IdentityProviderMetadata& idp_metadata,
                    const ClientIdData& client_id_data, SignInMode sign_in_mode,
                    IdentityRequestDialogController::AccountSelectionCallback
                        on_selected,
                    IdentityRequestDialogController::DismissCallback
                        dismiss_callback) {
                  displayed_accounts_ =
                      AccountList(accounts.begin(), accounts.end());
                  std::move(on_selected)
                      .Run(accounts[0].id,
                           accounts[0].login_state == LoginState::kSignIn);
                }));
      }
    } else {
      EXPECT_CALL(*mock_dialog_controller_,
                  ShowAccountsDialog(_, _, _, _, _, _, _, _))
          .Times(0);
    }
  }

  FederatedAuthRequestImpl* federated_auth_request_impl() {
    return federated_auth_request_impl_;
  }

  base::span<const content::IdentityRequestAccount> displayed_accounts() const {
    return displayed_accounts_;
  }
  MockIdentityRequestDialogController* mock_dialog_controller() const {
    return mock_dialog_controller_;
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  void ExpectRequestTokenStatusUKM(TokenStatus status) {
    ExpectRequestTokenStatusUKMInternal(status, FedCmEntry::kEntryName);
    ExpectRequestTokenStatusUKMInternal(status, FedCmIdpEntry::kEntryName);
  }

  void ExpectRequestTokenStatusUKMInternal(TokenStatus status,
                                           const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    if (entries.empty())
      FAIL() << "No RequestTokenStatus was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status_RequestToken");
      if (metric && *metric != static_cast<int>(status))
        FAIL() << "Unexpected status was recorded";
    }

    SUCCEED();
  }

  void ExpectTimingUKM(const std::string& metric_name) {
    ExpectTimingUKMInternal(metric_name, FedCmEntry::kEntryName);
    ExpectTimingUKMInternal(metric_name, FedCmIdpEntry::kEntryName);
  }

  void ExpectTimingUKMInternal(const std::string& metric_name,
                               const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty());

    for (const auto* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected UKM was not recorded";
  }

  void ExpectNoTimingUKM(const std::string& metric_name) {
    ExpectNoTimingUKMInternal(metric_name, FedCmEntry::kEntryName);
    ExpectNoTimingUKMInternal(metric_name, FedCmIdpEntry::kEntryName);
  }

  void ExpectNoTimingUKMInternal(const std::string& metric_name,
                                 const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty());

    for (const auto* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name))
        FAIL() << "Unexpected UKM was recorded";
    }
    SUCCEED();
  }

  void ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus status) {
    auto entries = ukm_recorder()->GetEntriesByName(FedCmIdpEntry::kEntryName);

    if (entries.empty())
      FAIL() << "No SignInStateMatchStatus was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status_SignInStateMatch");
      if (metric && *metric != static_cast<int>(status))
        FAIL() << "Unexpected status was recorded";
    }

    SUCCEED();
  }

  void CheckAllFedCmSessionIDs() {
    absl::optional<int> session_id;
    auto CheckUKMSessionID = [&](const auto& ukm_entries) {
      ASSERT_FALSE(ukm_entries.empty());
      for (const auto* const entry : ukm_entries) {
        const auto* const metric =
            ukm_recorder()->GetEntryMetric(entry, "FedCmSessionID");
        EXPECT_TRUE(metric)
            << "All UKM events should have the SessionID metric";
        if (!session_id.has_value()) {
          session_id = *metric;
        } else {
          ASSERT_EQ(*metric, *session_id)
              << "All UKM events should have the same SessionID";
        }
      }
    };
    CheckUKMSessionID(ukm_recorder()->GetEntriesByName(FedCmEntry::kEntryName));
    CheckUKMSessionID(
        ukm_recorder()->GetEntriesByName(FedCmIdpEntry::kEntryName));
  }

 protected:
  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  raw_ptr<FederatedAuthRequestImpl> federated_auth_request_impl_;

  std::unique_ptr<TestIdpNetworkRequestManager> test_network_request_manager_;
  raw_ptr<NiceMock<MockIdentityRequestDialogController>>
      mock_dialog_controller_;

  std::unique_ptr<TestApiPermissionDelegate> test_api_permission_delegate_;
  std::unique_ptr<NiceMock<MockActiveSessionPermissionDelegate>>
      mock_active_session_permission_delegate_;
  std::unique_ptr<NiceMock<MockSharingPermissionDelegate>>
      mock_sharing_permission_delegate_;

  AuthRequestCallbackHelper auth_helper_;

  // Storage for displayed accounts
  AccountList displayed_accounts_;

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

class BasicFederatedAuthRequestImplTest : public FederatedAuthRequestImplTest {
};

// Test successful FedCM request.
TEST_F(BasicFederatedAuthRequestImplTest, SuccessfulRequest) {
  // Use IdpNetworkRequestManagerParamChecker to validate passed-in parameters
  // to IdpNetworkRequestManager methods.
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kDefaultRequestParameters.client_id,
                           kConfigurationValid.accounts[0].id,
                           /* expected_revocation_hint=*/"");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test successful manifest list fetching.
TEST_F(BasicFederatedAuthRequestImplTest, ManifestListSuccess) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmManifestValidation);

  // Use IdpNetworkRequestManagerParamChecker to validate passed-in parameters
  // to IdpNetworkRequestManager methods.
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kDefaultRequestParameters.client_id,
                           kConfigurationValid.accounts[0].id,
                           /* expected_revocation_hint=*/"");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test the provider url is not in the manifest list.
TEST_F(BasicFederatedAuthRequestImplTest, ManifestListNotInList) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmManifestValidation);

  RequestExpectations request_not_in_list = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorManifestNotInManifestList,
      FetchedEndpoint::MANIFEST_LIST};

  RequestParameters parameters{"https://not-in-list.example", kClientId, kNonce,
                               /*prefer_auto_sign_in=*/false};
  RunAuthTest(parameters, request_not_in_list, kConfigurationValid);
}

// Test that not having the filename in the manifest list fails
// (kProviderUrl vs kProviderUrlFull).
TEST_F(BasicFederatedAuthRequestImplTest, ManifestListHasNoFilename) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmManifestValidation);

  RequestParameters parameters{"https://idp.example/foo", kClientId, kNonce,
                               /*prefer_auto_sign_in=*/false};
  MockConfiguration config{kConfigurationValid};
  config.manifest_list.provider_urls = std::set<std::string>{kProviderUrl};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorManifestNotInManifestList,
      FetchedEndpoint::MANIFEST_LIST};
  RunAuthTest(parameters, expectations, config);
}

// Test that request fails if manifest is missing token endpoint.
TEST_F(BasicFederatedAuthRequestImplTest, MissingTokenEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.manifest.token_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
      FetchedEndpoint::MANIFEST | FetchedEndpoint::MANIFEST_LIST};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Manifest is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"id_token_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM manifest configuration is invalid.", messages[1]);
}

// Test that request fails if manifest is missing accounts endpoint.
TEST_F(BasicFederatedAuthRequestImplTest, MissingAccountsEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.manifest.accounts_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
      FetchedEndpoint::MANIFEST | FetchedEndpoint::MANIFEST_LIST};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Manifest is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM manifest configuration is invalid.", messages[1]);
}

// Test that client metadata endpoint is not required in manifest.
TEST_F(BasicFederatedAuthRequestImplTest, MissingClientMetadataEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.manifest.client_metadata_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::CLIENT_METADATA};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that request fails if the accounts endpoint is in a different origin
// than identity provider.
TEST_F(BasicFederatedAuthRequestImplTest, AccountEndpointDifferentOriginIdp) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.manifest.accounts_endpoint = kCrossOriginAccountsEndpoint;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
      FetchedEndpoint::MANIFEST | FetchedEndpoint::MANIFEST_LIST};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that request fails if accounts endpoint cannot be reached.
TEST_F(BasicFederatedAuthRequestImplTest, AccountEndpointCannotBeReached) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_response = FetchStatus::kNoResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse,
      FetchedEndpoint::MANIFEST | FetchedEndpoint::CLIENT_METADATA |
          FetchedEndpoint::ACCOUNTS | FetchedEndpoint::MANIFEST_LIST};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that request fails if account endpoint response cannot be parsed.
TEST_F(BasicFederatedAuthRequestImplTest, AccountsCannotBeParsed) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_response = FetchStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
      FetchedEndpoint::MANIFEST | FetchedEndpoint::CLIENT_METADATA |
          FetchedEndpoint::ACCOUNTS | FetchedEndpoint::MANIFEST_LIST};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that privacy policy URL or terms of service is not required in client
// metadata.
TEST_F(BasicFederatedAuthRequestImplTest,
       ClientMetadataNoPrivacyPolicyOrTermsOfServiceUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.client_metadata = kDefaultClientMetadata;
  configuration.client_metadata.privacy_policy_url = "";
  configuration.client_metadata.terms_of_service_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that privacy policy URL is not required in client metadata.
TEST_F(BasicFederatedAuthRequestImplTest, ClientMetadataNoPrivacyPolicyUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.client_metadata = kDefaultClientMetadata;
  configuration.client_metadata.privacy_policy_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that terms of service URL is not required in client metadata.
TEST_F(BasicFederatedAuthRequestImplTest, ClientMetadataNoTermsOfServiceUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.client_metadata = kDefaultClientMetadata;
  configuration.client_metadata.terms_of_service_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that request fails if all of the endpoints in the manifest are invalid.
TEST_F(BasicFederatedAuthRequestImplTest, AllInvalidEndpoints) {
  // Both an empty url and cross origin urls are invalid endpoints.
  MockConfiguration configuration = kConfigurationValid;
  configuration.manifest.accounts_endpoint = "https://cross-origin-1.com";
  configuration.manifest.token_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
      FetchedEndpoint::MANIFEST | FetchedEndpoint::MANIFEST_LIST};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Manifest is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"id_token_endpoint\"\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM manifest configuration is invalid.", messages[1]);
}

// Test Logout method success with multiple relying parties.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutSuccessMultiple) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSignoutFieldTrialParamName, "true"}});

  std::vector<LogoutRpsRequestPtr> logout_requests;
  logout_requests.push_back(
      MakeLogoutRequest("https://rp1.example", "user123"));
  logout_requests.push_back(
      MakeLogoutRequest("https://rp2.example", "user456"));
  logout_requests.push_back(
      MakeLogoutRequest("https://rp3.example", "user789"));

  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(*mock_active_session_permission_delegate_,
                HasActiveSession(_, _, _))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  SetNetworkRequestManager(
      std::make_unique<TestLogoutIdpNetworkRequestManager>());

  auto logout_response = PerformLogoutRequest(std::move(logout_requests));
  EXPECT_EQ(logout_response, LogoutRpsStatus::kSuccess);
  EXPECT_EQ(3u, static_cast<TestLogoutIdpNetworkRequestManager*>(
                    test_network_request_manager_.get())
                    ->num_logout_requests());
}

// Test Logout without session permission granted.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutWithoutPermission) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSignoutFieldTrialParamName, "true"}});

  SetNetworkRequestManager(
      std::make_unique<TestLogoutIdpNetworkRequestManager>());

  std::vector<LogoutRpsRequestPtr> logout_requests;
  logout_requests.push_back(
      MakeLogoutRequest("https://rp1.example", "user123"));

  auto logout_response = PerformLogoutRequest(std::move(logout_requests));
  EXPECT_EQ(logout_response, LogoutRpsStatus::kSuccess);
}

// Test Logout method with an empty endpoint vector.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutNoEndpoints) {
  SetNetworkRequestManager(
      std::make_unique<TestLogoutIdpNetworkRequestManager>());

  auto logout_response =
      PerformLogoutRequest(std::vector<LogoutRpsRequestPtr>());
  EXPECT_EQ(logout_response, LogoutRpsStatus::kError);
}

// Tests for Login State
TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateShouldBeSignUpForFirstTimeUser) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignUp, displayed_accounts()[0].login_state);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateShouldBeSignInForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kRpUrl)),
                                   url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(true));
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateSuccessfulSignUpGrantsSharingPermission) {
  EXPECT_CALL(*mock_sharing_permission_delegate_, HasSharingPermission(_, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              GrantSharingPermission(url::Origin::Create(GURL(kRpUrl)),
                                     url::Origin::Create(GURL(kIdpTestOrigin)),
                                     kAccountId))
      .Times(1);
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateFailedSignUpNotGrantSharingPermission) {
  EXPECT_CALL(*mock_sharing_permission_delegate_, HasSharingPermission(_, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              GrantSharingPermission(_, _, _))
      .Times(0);

  MockConfiguration configuration = kConfigurationValid;
  configuration.token_response = FetchStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

TEST_F(BasicFederatedAuthRequestImplTest, AutoSignInForReturningUser) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  AccountList displayed_accounts;

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kRpUrl)),
                                   url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            EXPECT_EQ(sign_in_mode, SignInMode::kAuto);
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            std::move(on_selected).Run(accounts[0].id, /*is_sign_in=*/true);
          }));

  ASSERT_EQ(kConfigurationValid.accounts.size(), 1u);
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignIn);
}

TEST_F(BasicFederatedAuthRequestImplTest, AutoSignInForFirstTimeUser) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  AccountList displayed_accounts;
  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            EXPECT_EQ(sign_in_mode, SignInMode::kExplicit);
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            std::move(on_selected).Run(accounts[0].id, /*is_sign_in=*/true);
          }));

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignUp);
}

TEST_F(BasicFederatedAuthRequestImplTest, AutoSignInWithScreenReader) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  content::BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::AXMode::kScreenReader);

  AccountList displayed_accounts;

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kRpUrl)),
                                   url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            // Auto sign in replaced by explicit sign in if screen reader is on.
            EXPECT_EQ(sign_in_mode, SignInMode::kExplicit);
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            std::move(on_selected).Run(accounts[0].id, /*is_sign_in=*/true);
          }));

  EXPECT_EQ(kConfigurationValid.accounts.size(), 1u);
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignIn);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForSuccessfulSignInCase) {
  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(_, url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kSuccess, 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.IsSignInUser", 1, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.ContinueOnDialog");
  ExpectTimingUKM("Timing.IdTokenResponse");
  ExpectTimingUKM("Timing.TurnaroundTime");
  ExpectNoTimingUKM("Timing.CancelOnDialog");

  ExpectRequestTokenStatusUKM(TokenStatus::kSuccess);
  CheckAllFedCmSessionIDs();
}

// Test that request fails if account picker is explicitly dismissed.
TEST_F(BasicFederatedAuthRequestImplTest, MetricsForUIExplicitlyDismissed) {
  base::HistogramTester histogram_tester_;

  AccountList displayed_accounts;
  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            // Pretends that the user did not select any account.
            std::move(dismiss_callback).Run(DismissReason::CLOSE_BUTTON);
          }));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_EQ(kConfigurationValid.accounts.size(), 1u);
  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kError,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::TOKEN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  ukm_loop.Run();

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignUp);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kNotSelectAccount, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.CancelOnDialog");
  ExpectNoTimingUKM("Timing.ContinueOnDialog");
  ExpectNoTimingUKM("Timing.IdTokenResponse");
  ExpectNoTimingUKM("Timing.TurnaroundTime");

  ExpectRequestTokenStatusUKM(TokenStatus::kNotSelectAccount);
  CheckAllFedCmSessionIDs();
}

// Test that request is not completed if user ignores the UI.
TEST_F(BasicFederatedAuthRequestImplTest, UIIsIgnored) {
  base::HistogramTester histogram_tester_;

  // The UI will not be destroyed during the test.
  EXPECT_CALL(*mock_dialog_controller(), DestructorCalled()).Times(0);

  AccountList displayed_accounts;
  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            // Pretends that the user ignored the UI by not selecting an
            // account.
          }));

  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_status=*/absl::nullopt,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::TOKEN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  task_environment()->FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(auth_helper_.was_callback_called());
  ASSERT_FALSE(displayed_accounts.empty());

  // Only the time to show the account dialog gets recorded.
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Status.RequestIdToken", 0);

  // The UI will be destroyed after the test is done.
  EXPECT_CALL(*mock_dialog_controller(), DestructorCalled()).Times(1);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForWebContentsVisible) {
  base::HistogramTester histogram_tester;
  // Sets the WebContents to visible
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->UpdateWebContentsVisibility(Visibility::VISIBLE);
  ASSERT_EQ(web_contents_impl->GetVisibility(), Visibility::VISIBLE);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(_, url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(true));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 1, 1);
}

// Test that request fails if the web contents are hidden.
TEST_F(BasicFederatedAuthRequestImplTest, MetricsForWebContentsInvisible) {
  base::HistogramTester histogram_tester;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->UpdateWebContentsVisibility(Visibility::VISIBLE);
  ASSERT_EQ(web_contents_impl->GetVisibility(), Visibility::VISIBLE);

  // Sets the WebContents to invisible
  web_contents_impl->UpdateWebContentsVisibility(Visibility::HIDDEN);
  ASSERT_NE(web_contents_impl->GetVisibility(), Visibility::VISIBLE);

  MockConfiguration configuration = kConfigurationValid;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kError,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::TOKEN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 0, 1);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       DisabledWhenThirdPartyCookiesBlocked) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED);

  RequestExpectations expectations = {RequestTokenStatus::kError,
                                      FederatedAuthRequestResult::kError,
                                      /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kThirdPartyCookiesBlocked,
                                       1);
  ExpectRequestTokenStatusUKM(TokenStatus::kThirdPartyCookiesBlocked);
  CheckAllFedCmSessionIDs();
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForFeatureIsDisabled) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  RequestExpectations expectations = {RequestTokenStatus::kError,
                                      FederatedAuthRequestResult::kError,
                                      /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

TEST_F(BasicFederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithUnhandledRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  RequestExpectations expectations = {/*return_status=*/absl::nullopt,
                                      /*devtools_issue_status*/ absl::nullopt,
                                      /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  // Delete the request before DelayTimer kicks in.
  federated_auth_request_impl_->ResetAndDeleteThis();

  // If double counted, these samples would not be unique so the following
  // checks will fail.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

TEST_F(BasicFederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithAbortedRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  RequestExpectations expectations = {/*return_status=*/absl::nullopt,
                                      /*devtools_issue_status*/ absl::nullopt,
                                      /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  // Abort the request before DelayTimer kicks in.
  federated_auth_request_impl_->CancelTokenRequest();

  // If double counted, these samples would not be unique so the following
  // checks will fail.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states match if IDP claims that user is signed in and
// browser also observes that user is signed in.
TEST_F(BasicFederatedAuthRequestImplTest,
       MetricsForSignedInOnBothIdpAndBrowser) {
  // Set browser observes user is signed in.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kRpUrl)),
                                   url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  AccountList displayed_accounts =
      AccountList(kAccounts.begin(), kAccounts.end());
  displayed_accounts[0].login_state = LoginState::kSignIn;
  configuration.accounts = displayed_accounts;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.SignInStateMatch",
                                       SignInStateMatchStatus::kMatch, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kMatch);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states match if IDP claims that user is not signed in and
// browser also observes that user is not signed in.
TEST_F(BasicFederatedAuthRequestImplTest,
       MetricsForNotSignedInOnBothIdpAndBrowser) {
  // Set browser observes user is not signed in.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kRpUrl)),
                                   url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(false));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // By default, IDP claims user is not signed in.
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.SignInStateMatch",
                                       SignInStateMatchStatus::kMatch, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kMatch);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states mismatch if IDP claims that user is signed in but
// browser observes that user is not signed in.
TEST_F(BasicFederatedAuthRequestImplTest, MetricsForOnlyIdpClaimedSignIn) {
  // Set browser observes user is not signed in.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kRpUrl)),
                                   url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(false));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  AccountList displayed_accounts =
      AccountList(kAccounts.begin(), kAccounts.end());
  displayed_accounts[0].login_state = LoginState::kSignIn;
  configuration.accounts = displayed_accounts;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.SignInStateMatch",
      SignInStateMatchStatus::kIdpClaimedSignIn, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kIdpClaimedSignIn);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states mismatch if IDP claims that user is not signed in
// but browser observes that user is signed in.
TEST_F(BasicFederatedAuthRequestImplTest, MetricsForOnlyBrowserObservedSignIn) {
  // Set browser observes user is signed in.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kRpUrl)),
                                   url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // By default, IDP claims user is not signed in.
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.SignInStateMatch",
      SignInStateMatchStatus::kBrowserObservedSignIn, 1);
  ExpectSignInStateMatchStatusUKM(
      SignInStateMatchStatus::kBrowserObservedSignIn);
  CheckAllFedCmSessionIDs();
}

// Test that embargo is requested if the
// IdentityRequestDialogController::ShowAccountsDialog() callback requests it.
TEST_F(BasicFederatedAuthRequestImplTest, RequestEmbargo) {
  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kError,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::TOKEN};

  MockConfiguration configuration = kConfigurationValid;
  configuration.customized_dialog = true;

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            displayed_accounts_ = AccountList(accounts.begin(), accounts.end());
            std::move(dismiss_callback).Run(DismissReason::CLOSE_BUTTON);
          }));

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.count(
      main_test_rfh()->GetLastCommittedOrigin()));
}

// Test that the embargo dismiss count is reset when the user grants consent via
// the FedCM dialog.
TEST_F(BasicFederatedAuthRequestImplTest, RemoveEmbargoOnUserConsent) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.empty());
}

// Test that token request fails if FEDERATED_IDENTITY_API content setting is
// disabled for the RP origin.
TEST_F(BasicFederatedAuthRequestImplTest, ApiBlockedForOrigin) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_SETTINGS);
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorDisabledInSettings,
      /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
}

// Test that token request succeeds if FEDERATED_IDENTITY_API content setting is
// enabled for RP origin but disabled for an unrelated origin.
TEST_F(BasicFederatedAuthRequestImplTest, ApiBlockedForUnrelatedOrigin) {
  const url::Origin kUnrelatedOrigin =
      url::Origin::Create(GURL("https://rp2.example/"));

  test_api_permission_delegate_->permission_override_ =
      std::make_pair(kUnrelatedOrigin, ApiPermissionStatus::BLOCKED_SETTINGS);
  ASSERT_NE(main_test_rfh()->GetLastCommittedOrigin(), kUnrelatedOrigin);
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

class FederatedAuthRequestImplTestCancelConsistency
    : public FederatedAuthRequestImplTest,
      public ::testing::WithParamInterface<int> {};
INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         FederatedAuthRequestImplTestCancelConsistency,
                         ::testing::Values(false, true),
                         ::testing::PrintToStringParamName());

// Test that the RP cannot use CancelTokenRequest() to determine whether
// Option 1: FedCM dialog is shown but user has not interacted with it
// Option 2: FedCM API is disabled via variations
TEST_P(FederatedAuthRequestImplTestCancelConsistency, AccountNotSelected) {
  const bool fedcm_disabled = GetParam();

  if (fedcm_disabled) {
    test_api_permission_delegate_->permission_override_ =
        std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                       ApiPermissionStatus::BLOCKED_VARIATIONS);
  }

  MockConfiguration configuration = kConfigurationValid;
  configuration.customized_dialog = true;
  configuration.wait_for_callback = false;
  RequestExpectations expectation = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_status*/ absl::nullopt,
      /*fetched_endpoints=*/
      fedcm_disabled
          ? 0
          : FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::TOKEN};
  RunAuthTest(kDefaultRequestParameters, expectation, configuration);
  EXPECT_FALSE(auth_helper_.was_callback_called());

  request_remote_->CancelTokenRequest();
  request_remote_.FlushForTesting();
  EXPECT_TRUE(auth_helper_.was_callback_called());
  EXPECT_EQ(RequestTokenStatus::kErrorCanceled, auth_helper_.status());
}

// Test that the request fails if user proceeds with the sign in workflow after
// disabling the API while an existing accounts dialog is shown.
TEST_F(BasicFederatedAuthRequestImplTest, ApiDisabledAfterAccountsDialogShown) {
  base::HistogramTester histogram_tester_;

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            // Disable FedCM API
            test_api_permission_delegate_->permission_override_ =
                std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                               ApiPermissionStatus::BLOCKED_SETTINGS);

            std::move(on_selected).Run(accounts[0].id, /*is_sign_in=*/false);
          }));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  MockConfiguration configuration = kConfigurationValid;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kErrorDisabledInSettings,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::TOKEN};

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInSettings, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectNoTimingUKM("Timing.ContinueOnDialog");
  ExpectNoTimingUKM("Timing.IdTokenResponse");
  ExpectNoTimingUKM("Timing.TurnaroundTime");

  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInSettings);
  CheckAllFedCmSessionIDs();
}

// Test that disclosure text is shown for first time user.
TEST_F(BasicFederatedAuthRequestImplTest, DisclosureTextShownForFirstTimeUser) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.post_request_body =
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) + "&disclosure_text_shown=true";

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that disclosure text is not shown for returning user.
TEST_F(BasicFederatedAuthRequestImplTest,
       DisclosureTextNotShownForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermission(url::Origin::Create(GURL(kRpUrl)),
                                   url::Origin::Create(GURL(kIdpTestOrigin)),
                                   kAccountId))
      .WillOnce(Return(true));

  MockConfiguration configuration = kConfigurationValid;
  configuration.post_request_body =
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) + "&disclosure_text_shown=false";

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

}  // namespace content
