// Copyright 2021 The Chromium Authors
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
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/test/delegated_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_features.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::RequestTokenStatus;
using AccountList = content::IdpNetworkRequestManager::AccountList;
using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using DismissReason = content::IdentityRequestDialogController::DismissReason;
using FedCmEntry = ukm::builders::Blink_FedCm;
using FedCmIdpEntry = ukm::builders::Blink_FedCmIdp;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

constexpr char kProviderUrlFull[] = "https://idp.example/fedcm.json";
constexpr char kRpUrl[] = "https://rp.example/";
constexpr char kRpOtherUrl[] = "https://rp.example/random/";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kCrossOriginAccountsEndpoint[] = "https://idp2.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kClientMetadataEndpoint[] =
    "https://idp.example/client_metadata";
constexpr char kMetricsEndpoint[] = "https://idp.example/metrics";
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

static const std::initializer_list<IdentityRequestAccount> kMultipleAccounts{
    {
        "nico_the_great",                 // id
        "nicolas_the_great@idp.example",  // email
        "Nicolas The Great",              // name
        "Nicolas",                        // given_name
        GURL(),                           // picture
        LoginState::kSignUp               // login_state
    },
    {
        "account_id",         // id
        "email@idp.example",  // email
        "This Is Me",         // name
        "Name",               // given_name
        GURL(),               // picture
        LoginState::kSignIn   // login_state
    },
    {
        "other_account_id",         // id
        "other_email@idp.example",  // email
        "Name",                     // name
        "Given Name",               // given_name
        GURL(),                     // picture
        LoginState::kSignUp         // login_state
    }};

static const std::set<std::string> kWellKnown{kProviderUrlFull};

struct IdentityProviderParameters {
  const char* provider;
  const char* client_id;
  const char* nonce;
};

// Parameters for a call to RequestToken.
struct RequestParameters {
  std::vector<IdentityProviderParameters> identity_providers;
  bool prefer_auto_sign_in;
};

// Bitshift to get from CONFIG->CONFIG_MULTI,
// CLIENT_METADATA->CLIENT_METADATA_MULTI etc.
const int kFetchedEndpointMultiBitshift = 5;

enum FetchedEndpoint {
  CONFIG = 1,
  CLIENT_METADATA = 1 << 1,
  ACCOUNTS = 1 << 2,
  TOKEN = 1 << 3,
  WELL_KNOWN = 1 << 4,

  CONFIG_MULTI = CONFIG | (CONFIG << kFetchedEndpointMultiBitshift),
  CLIENT_METADATA_MULTI =
      CLIENT_METADATA | (CLIENT_METADATA << kFetchedEndpointMultiBitshift),
  ACCOUNTS_MULTI = ACCOUNTS | (ACCOUNTS << kFetchedEndpointMultiBitshift),
  WELL_KNOWN_MULTI = WELL_KNOWN | (WELL_KNOWN << kFetchedEndpointMultiBitshift),
};

// All endpoints which are fetched in a successful
// FederatedAuthRequestImpl::RequestToken() request.
int FETCH_ENDPOINT_ALL_REQUEST_TOKEN =
    FetchedEndpoint::CONFIG | FetchedEndpoint::CLIENT_METADATA |
    FetchedEndpoint::ACCOUNTS | FetchedEndpoint::TOKEN |
    FetchedEndpoint::WELL_KNOWN;

int FETCH_ENDPOINT_ALL_REQUEST_TOKEN_MULTI =
    FetchedEndpoint::CONFIG_MULTI | FetchedEndpoint::CLIENT_METADATA_MULTI |
    FetchedEndpoint::ACCOUNTS_MULTI | FetchedEndpoint::TOKEN |
    FetchedEndpoint::WELL_KNOWN_MULTI;

// Expected return values from a call to RequestToken.
struct RequestExpectations {
  absl::optional<RequestTokenStatus> return_status;
  std::vector<FederatedAuthRequestResult> devtools_issue_statuses;
  absl::optional<std::string> selected_idp_config_url;
  // Any combination of FetchedEndpoint flags.
  int fetched_endpoints;
};

// Mock configuration values for test.
struct MockClientIdConfiguration {
  FetchStatus fetch_status;
  std::string privacy_policy_url;
  std::string terms_of_service_url;
};

struct MockWellKnown {
  std::set<std::string> provider_urls;
};

struct MockConfig {
  FetchStatus fetch_status;
  std::string accounts_endpoint;
  std::string token_endpoint;
  std::string client_metadata_endpoint;
  std::string metrics_endpoint;
};

struct MockIdpInfo {
  MockWellKnown well_known;
  MockConfig config;
  MockClientIdConfiguration client_metadata;
  FetchStatus accounts_response;
  AccountList accounts;
};

struct MockConfiguration {
  const char* token;
  base::flat_map<std::string, MockIdpInfo> idp_info;
  FetchStatus token_response;
  bool delay_token_response;
  bool customized_dialog;
  bool wait_for_callback;
};

static const MockClientIdConfiguration kDefaultClientMetadata{
    {ParseStatus::kSuccess, net::HTTP_OK},
    kPrivacyPolicyUrl,
    kTermsOfServiceUrl};

static const IdentityProviderParameters kDefaultIdentityProviderConfig{
    kProviderUrlFull, kClientId, kNonce};

static const RequestParameters kDefaultRequestParameters{
    std::vector<IdentityProviderParameters>{kDefaultIdentityProviderConfig},
    /*prefer_auto_sign_in=*/false};

static const MockIdpInfo kDefaultIdentityProviderInfo{
    {kWellKnown},
    {
        {ParseStatus::kSuccess, net::HTTP_OK},
        kAccountsEndpoint,
        kTokenEndpoint,
        kClientMetadataEndpoint,
        kMetricsEndpoint,
    },
    kDefaultClientMetadata,
    {ParseStatus::kSuccess, net::HTTP_OK},
    kAccounts,
};

static const base::flat_map<std::string, MockIdpInfo> kSingleProviderInfo{
    {kProviderUrlFull, kDefaultIdentityProviderInfo}};

constexpr char kProviderTwoUrlFull[] = "https://idp2.example/fedcm.json";
static const MockIdpInfo kProviderTwoInfo{
    {{kProviderTwoUrlFull}},
    {
        {ParseStatus::kSuccess, net::HTTP_OK},
        "https://idp2.example/accounts",
        "https://idp2.example/token",
        "https://idp2.example/client_metadata",
        "https://idp2.example/metrics",
    },
    kDefaultClientMetadata,
    {ParseStatus::kSuccess, net::HTTP_OK},
    kMultipleAccounts};

static const MockConfiguration kConfigurationValid{
    kToken,
    kSingleProviderInfo,
    {ParseStatus::kSuccess, net::HTTP_OK},
    /*delay_token_response=*/false,
    /*customized_dialog=*/false,
    /*wait_for_callback=*/true};

static const RequestExpectations kExpectationSuccess{
    RequestTokenStatus::kSuccess,
    {FederatedAuthRequestResult::kSuccess},
    kProviderUrlFull,
    FETCH_ENDPOINT_ALL_REQUEST_TOKEN};

static const RequestExpectations kExpectationSuccessMultiIdp{
    RequestTokenStatus::kSuccess,
    {FederatedAuthRequestResult::kSuccess},
    kProviderUrlFull,
    FETCH_ENDPOINT_ALL_REQUEST_TOKEN_MULTI};

static const RequestParameters kDefaultMultiIdpRequestParameters{
    std::vector<IdentityProviderParameters>{
        {kProviderUrlFull, kClientId, kNonce},
        {kProviderTwoUrlFull, kClientId, kNonce}},
    /*prefer_auto_sign_in=*/false};

MockConfiguration kConfigurationMultiIdpValid{
    kToken,
    {{kProviderUrlFull, kDefaultIdentityProviderInfo},
     {kProviderTwoUrlFull, kProviderTwoInfo}},
    {ParseStatus::kSuccess, net::HTTP_OK},
    false /* delay_token_response */,
    false /* customized_dialog */,
    true /* wait_for_callback */};

url::Origin OriginFromString(const std::string& url_string) {
  return url::Origin::Create(GURL(url_string));
}

// Helper class for receiving the mojo method callback.
class AuthRequestCallbackHelper {
 public:
  AuthRequestCallbackHelper() = default;
  ~AuthRequestCallbackHelper() = default;

  AuthRequestCallbackHelper(const AuthRequestCallbackHelper&) = delete;
  AuthRequestCallbackHelper& operator=(const AuthRequestCallbackHelper&) =
      delete;

  absl::optional<RequestTokenStatus> status() const { return status_; }
  absl::optional<GURL> selected_idp_config_url() const {
    return selected_idp_config_url_;
  }
  absl::optional<std::string> token() const { return token_; }

  base::OnceClosure quit_closure() {
    return base::BindOnce(&AuthRequestCallbackHelper::Quit,
                          base::Unretained(this));
  }

  // This can only be called once per lifetime of this object.
  base::OnceCallback<void(RequestTokenStatus,
                          const absl::optional<GURL>&,
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
                      const absl::optional<GURL>& selected_idp_config_url,
                      const absl::optional<std::string>& token) {
    CHECK(!was_called_);
    status_ = status;
    selected_idp_config_url_ = selected_idp_config_url;
    token_ = token;
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  void Quit() { wait_for_callback_loop_.Quit(); }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  absl::optional<RequestTokenStatus> status_;
  absl::optional<GURL> selected_idp_config_url_;
  absl::optional<std::string> token_;
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

  void FetchWellKnown(const GURL& provider,
                      FetchWellKnownCallback callback) override {
    add_fetched_endpoint(FetchedEndpoint::WELL_KNOWN);

    std::string provider_key = provider.spec();
    std::set<GURL> url_set(
        config_.idp_info[provider_key].well_known.provider_urls.begin(),
        config_.idp_info[provider_key].well_known.provider_urls.end());
    FetchStatus success{ParseStatus::kSuccess, net::HTTP_OK};
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), success, url_set));
  }

  void FetchConfig(const GURL& provider,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    add_fetched_endpoint(FetchedEndpoint::CONFIG);

    std::string provider_key = provider.spec();
    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.token =
        GURL(config_.idp_info[provider_key].config.token_endpoint);
    endpoints.accounts =
        GURL(config_.idp_info[provider_key].config.accounts_endpoint);
    endpoints.client_metadata =
        GURL(config_.idp_info[provider_key].config.client_metadata_endpoint);
    endpoints.metrics =
        GURL(config_.idp_info[provider_key].config.metrics_endpoint);

    IdentityProviderMetadata idp_metadata;
    idp_metadata.config_url = provider;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       config_.idp_info[provider_key].config.fetch_status,
                       endpoints, idp_metadata));
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    add_fetched_endpoint(FetchedEndpoint::CLIENT_METADATA);

    // Find the info of the provider with the same client metadata endpoint.
    MockIdpInfo info;
    for (const auto& idp_info : config_.idp_info) {
      info = idp_info.second;
      if (GURL(info.config.client_metadata_endpoint) == endpoint)
        break;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), info.client_metadata.fetch_status,
                       IdpNetworkRequestManager::ClientMetadata{
                           info.client_metadata.privacy_policy_url,
                           info.client_metadata.terms_of_service_url}));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    add_fetched_endpoint(FetchedEndpoint::ACCOUNTS);

    // Find the info of the provider with the same accounts endpoint.
    MockIdpInfo info;
    for (const auto& idp_info : config_.idp_info) {
      info = idp_info.second;
      if (GURL(info.config.accounts_endpoint) == accounts_url)
        break;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), info.accounts_response,
                                  info.accounts));
  }

  void SendTokenRequest(const GURL& token_url,
                        const std::string& account,
                        const std::string& url_encoded_post_data,
                        TokenRequestCallback callback) override {
    add_fetched_endpoint(FetchedEndpoint::TOKEN);

    std::string delivered_token =
        config_.token_response.parse_status == ParseStatus::kSuccess
            ? config_.token
            : std::string();
    base::OnceCallback bound_callback = base::BindOnce(
        std::move(callback), config_.token_response, delivered_token);
    if (config_.delay_token_response) {
      delayed_callbacks_.push_back(std::move(bound_callback));
    } else {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(bound_callback));
    }
  }

  int get_fetched_endpoints() { return fetched_endpoints_; }

 protected:
  MockConfiguration config_{kConfigurationValid};
  int fetched_endpoints_{0};
  std::vector<base::OnceClosure> delayed_callbacks_;

 private:
  void add_fetched_endpoint(int fetched_endpoint) {
    if ((fetched_endpoints_ & fetched_endpoint) != 0) {
      // Endpoint has already been fetched. Mark endpoint as fetched multiple
      // times (Example: CONFIG_MULTI).
      fetched_endpoint <<= kFetchedEndpointMultiBitshift;
    }
    fetched_endpoints_ |= fetched_endpoint;
  }
};

// TestIdpNetworkRequestManager subclass which checks the values of the method
// params when executing an endpoint request.
class IdpNetworkRequestManagerParamChecker
    : public TestIdpNetworkRequestManager {
 public:
  void SetExpectations(const std::string& expected_client_id,
                       const std::string& expected_selected_account_id) {
    expected_client_id_ = expected_client_id;
    expected_selected_account_id_ = expected_selected_account_id;
  }

  void SetExpectedTokenPostData(
      const std::string& expected_url_encoded_post_data) {
    expected_url_encoded_post_data_ = expected_url_encoded_post_data;
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    if (expected_client_id_)
      EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::FetchClientMetadata(endpoint, client_id,
                                                      std::move(callback));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    if (expected_client_id_)
      EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::SendAccountsRequest(accounts_url, client_id,
                                                      std::move(callback));
  }

  void SendTokenRequest(const GURL& token_url,
                        const std::string& account,
                        const std::string& url_encoded_post_data,
                        TokenRequestCallback callback) override {
    if (expected_selected_account_id_)
      EXPECT_EQ(expected_selected_account_id_, account);
    if (expected_url_encoded_post_data_)
      EXPECT_EQ(expected_url_encoded_post_data_, url_encoded_post_data);
    TestIdpNetworkRequestManager::SendTokenRequest(
        token_url, account, url_encoded_post_data, std::move(callback));
  }

 private:
  absl::optional<std::string> expected_client_id_;
  absl::optional<std::string> expected_selected_account_id_;
  absl::optional<std::string> expected_url_encoded_post_data_;
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
    mock_permission_delegate_ =
        std::make_unique<NiceMock<MockPermissionDelegate>>();

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kRpUrl), ui::PAGE_TRANSITION_LINK);

    federated_auth_request_impl_ = &FederatedAuthRequestImpl::CreateForTesting(
        *main_test_rfh(), test_api_permission_delegate_.get(),
        mock_permission_delegate_.get(),
        request_remote_.BindNewPipeAndPassReceiver());
    auto mock_dialog_controller =
        std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
    mock_dialog_controller_ = mock_dialog_controller.get();
    federated_auth_request_impl_->SetDialogControllerForTests(
        std::move(mock_dialog_controller));

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

    std::vector<blink::mojom::IdentityProviderGetParametersPtr> idp_get_params;
    for (const auto& identity_provider :
         request_parameters.identity_providers) {
      std::vector<blink::mojom::IdentityProviderConfigPtr> idp_ptrs;
      blink::mojom::IdentityProviderConfigPtr idp_ptr =
          blink::mojom::IdentityProviderConfig::New(
              GURL(identity_provider.provider), identity_provider.client_id,
              identity_provider.nonce);
      idp_ptrs.push_back(std::move(idp_ptr));
      blink::mojom::IdentityProviderGetParametersPtr get_params =
          blink::mojom::IdentityProviderGetParameters::New(
              std::move(idp_ptrs), request_parameters.prefer_auto_sign_in);
      idp_get_params.push_back(std::move(get_params));
    }

    auto auth_response = PerformAuthRequest(std::move(idp_get_params),
                                            configuration.wait_for_callback);
    ASSERT_EQ(std::get<0>(auth_response), expectation.return_status);
    if (std::get<0>(auth_response) == RequestTokenStatus::kSuccess) {
      EXPECT_EQ(configuration.token, std::get<2>(auth_response));
    } else {
      EXPECT_TRUE(std::get<2>(auth_response) == absl::nullopt ||
                  std::get<2>(auth_response) == kEmptyToken);
    }

    if (expectation.selected_idp_config_url) {
      EXPECT_EQ(std::get<1>(auth_response),
                GURL(*expectation.selected_idp_config_url));
    } else {
      EXPECT_FALSE(std::get<1>(auth_response).has_value());
    }

    EXPECT_EQ(expectation.fetched_endpoints,
              test_network_request_manager_->get_fetched_endpoints());

    if (!expectation.devtools_issue_statuses.empty()) {
      std::map<FederatedAuthRequestResult, int> devtools_issue_counts;
      for (FederatedAuthRequestResult devtools_issue_status :
           expectation.devtools_issue_statuses) {
        if (devtools_issue_status == FederatedAuthRequestResult::kSuccess)
          continue;

        ++devtools_issue_counts[devtools_issue_status];
      }

      for (auto& [devtools_issue_status, expected_count] :
           devtools_issue_counts) {
        int issue_count = main_test_rfh()->GetFederatedAuthRequestIssueCount(
            devtools_issue_status);
        EXPECT_LE(expected_count, issue_count);
      }
      if (devtools_issue_counts.empty()) {
        int issue_count =
            main_test_rfh()->GetFederatedAuthRequestIssueCount(absl::nullopt);
        EXPECT_EQ(0, issue_count);
      }
      CheckConsoleMessages(expectation.devtools_issue_statuses);
    }
  }

  void CheckConsoleMessages(
      const std::vector<FederatedAuthRequestResult>& devtools_issue_statuses) {
    static std::unordered_map<FederatedAuthRequestResult,
                              absl::optional<std::string>>
        status_to_message = {
            {FederatedAuthRequestResult::kSuccess, absl::nullopt},
            {FederatedAuthRequestResult::kShouldEmbargo,
             "User declined or dismissed prompt. API exponential cool down "
             "triggered."},
            {FederatedAuthRequestResult::kErrorDisabledInSettings,
             "Third-party sign in was disabled in browser Site Settings."},
            {FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound,
             "The provider's FedCM well-known file cannot be found."},
            {FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse,
             "The provider's FedCM well-known file fetch resulted in an "
             "error response code."},
            {FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse,
             "Provider's FedCM well-known file is invalid."},
            {FederatedAuthRequestResult::kErrorConfigNotInWellKnown,
             "Provider's FedCM config file not listed in its well-known file."},
            {FederatedAuthRequestResult::kErrorWellKnownTooBig,
             "Provider's FedCM well-known contains too many providers."},
            {FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound,
             "The provider's FedCM config file cannot be found."},
            {FederatedAuthRequestResult::kErrorFetchingConfigNoResponse,
             "The provider's FedCM config file fetch resulted in an "
             "error response code."},
            {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse,
             "Provider's FedCM config file is invalid."},
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
            {FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse,
             "Provider's token is invalid."},
            {FederatedAuthRequestResult::kErrorRpPageNotVisible,
             "RP page is not visible."},
        };
    std::vector<std::string> messages =
        RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();

    bool did_expect_any_messages = false;
    size_t expected_message_index = messages.size() - 1;
    for (auto statuses_reverse_it = devtools_issue_statuses.rbegin();
         statuses_reverse_it != devtools_issue_statuses.rend();
         ++statuses_reverse_it) {
      absl::optional<std::string> expected_message =
          status_to_message[*statuses_reverse_it];
      if (!expected_message)
        continue;

      did_expect_any_messages = true;
      ASSERT_GE(expected_message_index, 0u);
      EXPECT_EQ(expected_message.value(), messages[expected_message_index]);
      --expected_message_index;
    }

    if (!did_expect_any_messages)
      EXPECT_EQ(0u, messages.size());
  }

  std::tuple<absl::optional<RequestTokenStatus>,
             absl::optional<GURL>,
             absl::optional<std::string>>
  PerformAuthRequest(std::vector<blink::mojom::IdentityProviderGetParametersPtr>
                         idp_get_params,
                     bool wait_for_callback) {
    request_remote_->RequestToken(std::move(idp_get_params),
                                  auth_helper_.callback());

    if (wait_for_callback)
      request_remote_.set_disconnect_handler(auth_helper_.quit_closure());

    // Ensure that the request makes its way to FederatedAuthRequestImpl.
    request_remote_.FlushForTesting();
    base::RunLoop().RunUntilIdle();
    if (wait_for_callback) {
      // Fast forward clock so that the pending
      // FederatedAuthRequestImpl::OnRejectRequest() task, if any, gets a
      // chance to run.
      task_environment()->FastForwardBy(base::Minutes(10));
      auth_helper_.WaitForCallback();

      request_remote_.set_disconnect_handler(base::OnceClosure());
    }
    return std::make_tuple(auth_helper_.status(),
                           auth_helper_.selected_idp_config_url(),
                           auth_helper_.token());
  }

  void SetMockExpectations(const RequestParameters& request_parameters,
                           const RequestExpectations& expectations,
                           const MockConfiguration& config) {
    bool is_all_accounts_response_successful{true};
    for (const auto& idp_info : config.idp_info) {
      if (idp_info.second.accounts_response.parse_status !=
          ParseStatus::kSuccess) {
        is_all_accounts_response_successful = false;
        break;
      }
    }

    if ((expectations.fetched_endpoints & FetchedEndpoint::ACCOUNTS) != 0 &&
        is_all_accounts_response_successful) {
      if (!request_parameters.prefer_auto_sign_in &&
          !config.customized_dialog) {
        // Expects a dialog if prefer_auto_sign_in is not set by RP. However,
        // even though the bit is set we may not exercise the AutoSignIn flow.
        // e.g. for sign up flow, multiple accounts, user opt-out etc. In this
        // case, it's up to the test to expect this mock function call.
        EXPECT_CALL(*mock_dialog_controller_,
                    ShowAccountsDialog(_, _, _, _, _, _))
            .WillOnce(Invoke(
                [&](content::WebContents* rp_web_contents,
                    const std::string& rp_for_display,
                    const std::vector<IdentityProviderData>&
                        identity_provider_data,
                    SignInMode sign_in_mode,
                    IdentityRequestDialogController::AccountSelectionCallback
                        on_selected,
                    IdentityRequestDialogController::DismissCallback
                        dismiss_callback) {
                  base::span<const content::IdentityRequestAccount> accounts =
                      identity_provider_data[0].accounts;
                  displayed_accounts_ =
                      AccountList(accounts.begin(), accounts.end());
                  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                      FROM_HERE,
                      base::BindOnce(
                          std::move(on_selected),
                          identity_provider_data[0].idp_metadata.config_url,
                          accounts[0].id,
                          accounts[0].login_state == LoginState::kSignIn));
                }));
      }
    } else {
      EXPECT_CALL(*mock_dialog_controller_,
                  ShowAccountsDialog(_, _, _, _, _, _))
          .Times(0);
    }
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

  void ComputeLoginStateAndReorderAccounts(
      const blink::mojom::IdentityProviderConfig& identity_provider,
      IdpNetworkRequestManager::AccountList& accounts) {
    federated_auth_request_impl_->ComputeLoginStateAndReorderAccounts(
        identity_provider, accounts);
  }

 protected:
  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  raw_ptr<FederatedAuthRequestImpl> federated_auth_request_impl_;

  std::unique_ptr<TestIdpNetworkRequestManager> test_network_request_manager_;
  raw_ptr<NiceMock<MockIdentityRequestDialogController>>
      mock_dialog_controller_;

  std::unique_ptr<TestApiPermissionDelegate> test_api_permission_delegate_;
  std::unique_ptr<NiceMock<MockPermissionDelegate>> mock_permission_delegate_;

  AuthRequestCallbackHelper auth_helper_;

  // Storage for displayed accounts
  AccountList displayed_accounts_;

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Test successful FedCM request.
TEST_F(FederatedAuthRequestImplTest, SuccessfulRequest) {
  // Use IdpNetworkRequestManagerParamChecker to validate passed-in parameters
  // to IdpNetworkRequestManager methods.
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kClientId, kAccountId);
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test successful well-known fetching.
TEST_F(FederatedAuthRequestImplTest, WellKnownSuccess) {
  // Use IdpNetworkRequestManagerParamChecker to validate passed-in parameters
  // to IdpNetworkRequestManager methods.
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kClientId, kAccountId);
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test the provider url is not in the well-known.
TEST_F(FederatedAuthRequestImplTest, WellKnownNotInList) {
  RequestExpectations request_not_in_list = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::WELL_KNOWN | FetchedEndpoint::CONFIG};

  const char* idp_config_url =
      kDefaultRequestParameters.identity_providers[0].provider;
  const char* kWellKnownMismatchConfigUrl = "https://mismatch.example";
  EXPECT_NE(std::string(idp_config_url), kWellKnownMismatchConfigUrl);

  MockConfiguration config = kConfigurationValid;
  config.idp_info[idp_config_url].well_known = {{kWellKnownMismatchConfigUrl}};
  RunAuthTest(kDefaultRequestParameters, request_not_in_list, config);
}

// Test that not having the filename in the well-known fails.
TEST_F(FederatedAuthRequestImplTest, WellKnownHasNoFilename) {
  MockConfiguration config{kConfigurationValid};
  config.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{GURL(kProviderUrlFull).GetWithoutFilename().spec()};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::WELL_KNOWN | FetchedEndpoint::CONFIG};
  RunAuthTest(kDefaultRequestParameters, expectations, config);
}

// Test that request fails if config is missing token endpoint.
TEST_F(FederatedAuthRequestImplTest, MissingTokenEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.token_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG | FetchedEndpoint::WELL_KNOWN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"id_assertion_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Test that request fails if config is missing accounts endpoint.
TEST_F(FederatedAuthRequestImplTest, MissingAccountsEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG | FetchedEndpoint::WELL_KNOWN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Test that client metadata endpoint is not required in config.
TEST_F(FederatedAuthRequestImplTest, MissingClientMetadataEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.client_metadata_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kSuccess,
      {FederatedAuthRequestResult::kSuccess},
      kProviderUrlFull,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::CLIENT_METADATA};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that request fails if the accounts endpoint is in a different origin
// than identity provider.
TEST_F(FederatedAuthRequestImplTest, AccountEndpointDifferentOriginIdp) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint =
      kCrossOriginAccountsEndpoint;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG | FetchedEndpoint::WELL_KNOWN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that request fails if the idp is not https.
TEST_F(FederatedAuthRequestImplTest, ProviderNotTrustworthy) {
  IdentityProviderParameters identity_provider{"http://idp.example/fedcm.json",
                                               kClientId, kNonce};
  RequestParameters request{
      std::vector<IdentityProviderParameters>{identity_provider},
      /*prefer_auto_sign_in=*/false};
  MockConfiguration configuration = kConfigurationValid;
  RequestExpectations expectations = {RequestTokenStatus::kError,
                                      {FederatedAuthRequestResult::kError},
                                      /*selected_idp_config_url=*/absl::nullopt,
                                      /*fetched_endpoints=*/0};
  RunAuthTest(request, expectations, configuration);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.RequestIdToken",
      TokenStatus::kIdpNotPotentiallyTrustworthy, 1);
}

// Test that request fails if accounts endpoint cannot be reached.
TEST_F(FederatedAuthRequestImplTest, AccountEndpointCannotBeReached) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kNoResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG | FetchedEndpoint::ACCOUNTS |
          FetchedEndpoint::WELL_KNOWN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that request fails if account endpoint response cannot be parsed.
TEST_F(FederatedAuthRequestImplTest, AccountsCannotBeParsed) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG | FetchedEndpoint::ACCOUNTS |
          FetchedEndpoint::WELL_KNOWN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that privacy policy URL or terms of service is not required in client
// metadata.
TEST_F(FederatedAuthRequestImplTest,
       ClientMetadataNoPrivacyPolicyOrTermsOfServiceUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull].client_metadata.privacy_policy_url =
      "";
  configuration.idp_info[kProviderUrlFull]
      .client_metadata.terms_of_service_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that privacy policy URL is not required in client metadata.
TEST_F(FederatedAuthRequestImplTest, ClientMetadataNoPrivacyPolicyUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull].client_metadata.privacy_policy_url =
      "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that terms of service URL is not required in client metadata.
TEST_F(FederatedAuthRequestImplTest, ClientMetadataNoTermsOfServiceUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull]
      .client_metadata.terms_of_service_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that request fails if all of the endpoints in the config are invalid.
TEST_F(FederatedAuthRequestImplTest, AllInvalidEndpoints) {
  // Both an empty url and cross origin urls are invalid endpoints.
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint =
      "https://cross-origin-1.com";
  configuration.idp_info[kProviderUrlFull].config.token_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG | FetchedEndpoint::WELL_KNOWN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"id_assertion_endpoint\"\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Tests for Login State
TEST_F(FederatedAuthRequestImplTest, LoginStateShouldBeSignUpForFirstTimeUser) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignUp, displayed_accounts()[0].login_state);
}

TEST_F(FederatedAuthRequestImplTest, LoginStateShouldBeSignInForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  RequestExpectations expectations = kExpectationSuccess;
  // CLIENT_METADATA only needs to be fetched for obtaining links to display in
  // the disclosure text. The disclosure text is not displayed for returning
  // users, thus fetching the client metadata endpoint should be skipped.
  expectations.fetched_endpoints &= ~FetchedEndpoint::CLIENT_METADATA;

  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);
}

TEST_F(FederatedAuthRequestImplTest,
       LoginStateSuccessfulSignUpGrantsSharingPermission) {
  EXPECT_CALL(*mock_permission_delegate_, HasSharingPermission(_, _, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(
      *mock_permission_delegate_,
      GrantSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                             OriginFromString(kProviderUrlFull), kAccountId))
      .Times(1);
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

TEST_F(FederatedAuthRequestImplTest,
       LoginStateFailedSignUpNotGrantSharingPermission) {
  EXPECT_CALL(*mock_permission_delegate_, HasSharingPermission(_, _, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_permission_delegate_, GrantSharingPermission(_, _, _, _))
      .Times(0);

  MockConfiguration configuration = kConfigurationValid;
  configuration.token_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

TEST_F(FederatedAuthRequestImplTest, AutoSignInForReturningUser) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  AccountList displayed_accounts;

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            EXPECT_EQ(sign_in_mode, SignInMode::kAuto);
            base::span<const content::IdentityRequestAccount> accounts =
                identity_provider_data[0].accounts;
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            std::move(on_selected)
                .Run(identity_provider_data[0].idp_metadata.config_url,
                     accounts[0].id, /*is_sign_in=*/true);
          }));

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;
  RequestExpectations expectations = kExpectationSuccess;
  expectations.fetched_endpoints &= ~FetchedEndpoint::CLIENT_METADATA;
  RunAuthTest(request_parameters, expectations, kConfigurationValid);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignIn);
}

TEST_F(FederatedAuthRequestImplTest, AutoSignInForFirstTimeUser) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  AccountList displayed_accounts;
  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            EXPECT_EQ(sign_in_mode, SignInMode::kExplicit);
            base::span<const content::IdentityRequestAccount> accounts =
                identity_provider_data[0].accounts;
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            std::move(on_selected)
                .Run(identity_provider_data[0].idp_metadata.config_url,
                     accounts[0].id, /*is_sign_in=*/true);
          }));

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignUp);
}

TEST_F(FederatedAuthRequestImplTest, AutoSignInWithScreenReader) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  content::BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::AXMode::kScreenReader);

  AccountList displayed_accounts;

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            // Auto sign in replaced by explicit sign in if screen reader is on.
            EXPECT_EQ(sign_in_mode, SignInMode::kExplicit);
            base::span<const content::IdentityRequestAccount> accounts =
                identity_provider_data[0].accounts;
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            std::move(on_selected)
                .Run(identity_provider_data[0].idp_metadata.config_url,
                     accounts[0].id, /*is_sign_in=*/true);
          }));

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;
  RequestExpectations expectations = kExpectationSuccess;
  expectations.fetched_endpoints &= ~FetchedEndpoint::CLIENT_METADATA;
  RunAuthTest(request_parameters, expectations, kConfigurationValid);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignIn);
}

TEST_F(FederatedAuthRequestImplTest, MetricsForSuccessfulSignInCase) {
  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_permission_delegate_,
              HasSharingPermission(_, _, OriginFromString(kProviderUrlFull),
                                   kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RequestExpectations expectations = kExpectationSuccess;
  expectations.fetched_endpoints &= ~FetchedEndpoint::CLIENT_METADATA;
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
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
TEST_F(FederatedAuthRequestImplTest, MetricsForUIExplicitlyDismissed) {
  base::HistogramTester histogram_tester_;

  AccountList displayed_accounts;
  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            base::span<const content::IdentityRequestAccount> accounts =
                identity_provider_data[0].accounts;
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            // Pretends that the user did not select any account.
            std::move(dismiss_callback).Run(DismissReason::CLOSE_BUTTON);
          }));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kShouldEmbargo},
      /*selected_idp_config_url=*/absl::nullopt,
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
                                       TokenStatus::kShouldEmbargo, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.CancelOnDialog");
  ExpectNoTimingUKM("Timing.ContinueOnDialog");
  ExpectNoTimingUKM("Timing.IdTokenResponse");
  ExpectNoTimingUKM("Timing.TurnaroundTime");

  ExpectRequestTokenStatusUKM(TokenStatus::kShouldEmbargo);
  CheckAllFedCmSessionIDs();
}

// Test that request is not completed if user ignores the UI.
TEST_F(FederatedAuthRequestImplTest, UIIsIgnored) {
  base::HistogramTester histogram_tester_;

  // The UI will not be destroyed during the test.
  EXPECT_CALL(*mock_dialog_controller(), DestructorCalled()).Times(0);

  AccountList displayed_accounts;
  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            base::span<const content::IdentityRequestAccount> accounts =
                identity_provider_data[0].accounts;
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            // Pretends that the user ignored the UI by not selecting an
            // account.
          }));

  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt,
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

TEST_F(FederatedAuthRequestImplTest, MetricsForWebContentsVisible) {
  base::HistogramTester histogram_tester;
  // Sets RenderFrameHost to visible
  test_rvh()->SimulateWasShown();
  ASSERT_EQ(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_permission_delegate_,
              HasSharingPermission(_, _, OriginFromString(kProviderUrlFull),
                                   kAccountId))
      .WillOnce(Return(true));

  RequestExpectations expectations = kExpectationSuccess;
  expectations.fetched_endpoints &= ~FetchedEndpoint::CLIENT_METADATA;
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 1, 1);
}

// Test that request fails if the web contents are hidden.
TEST_F(FederatedAuthRequestImplTest, MetricsForWebContentsInvisible) {
  base::HistogramTester histogram_tester;
  test_rvh()->SimulateWasShown();
  ASSERT_EQ(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  // Sets the RenderFrameHost to invisible
  test_rvh()->SimulateWasHidden();
  ASSERT_NE(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  MockConfiguration configuration = kConfigurationValid;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorRpPageNotVisible},
      /*selected_idp_config_url=*/absl::nullopt,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::TOKEN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 0, 1);
}

TEST_F(FederatedAuthRequestImplTest, DisabledWhenThirdPartyCookiesBlocked) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED);

  RequestExpectations expectations = {RequestTokenStatus::kError,
                                      {FederatedAuthRequestResult::kError},
                                      /*selected_idp_config_url=*/absl::nullopt,
                                      /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kThirdPartyCookiesBlocked,
                                       1);
  ExpectRequestTokenStatusUKM(TokenStatus::kThirdPartyCookiesBlocked);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, MetricsForFeatureIsDisabled) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  RequestExpectations expectations = {RequestTokenStatus::kError,
                                      {FederatedAuthRequestResult::kError},
                                      /*selected_idp_config_url=*/absl::nullopt,
                                      /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithUnhandledRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  RequestExpectations expectations = {/*return_status=*/absl::nullopt,
                                      /*devtools_issue_statuses=*/{},
                                      /*selected_idp_config_url=*/absl::nullopt,
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

TEST_F(FederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithAbortedRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  RequestExpectations expectations = {/*return_status=*/absl::nullopt,
                                      /*devtools_issue_statuses=*/{},
                                      /*selected_idp_config_url=*/absl::nullopt,
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
TEST_F(FederatedAuthRequestImplTest, MetricsForSignedInOnBothIdpAndBrowser) {
  // Set browser observes user is signed in.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  AccountList displayed_accounts =
      AccountList(kAccounts.begin(), kAccounts.end());
  displayed_accounts[0].login_state = LoginState::kSignIn;
  configuration.idp_info[kProviderUrlFull].accounts = displayed_accounts;
  RequestExpectations expectations = kExpectationSuccess;
  expectations.fetched_endpoints &= ~FetchedEndpoint::CLIENT_METADATA;
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.SignInStateMatch",
                                       SignInStateMatchStatus::kMatch, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kMatch);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states match if IDP claims that user is not signed in and
// browser also observes that user is not signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForNotSignedInOnBothIdpAndBrowser) {
  // Set browser observes user is not signed in.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
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
TEST_F(FederatedAuthRequestImplTest, MetricsForOnlyIdpClaimedSignIn) {
  // Set browser observes user is not signed in.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(false));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  AccountList displayed_accounts =
      AccountList(kAccounts.begin(), kAccounts.end());
  displayed_accounts[0].login_state = LoginState::kSignIn;
  configuration.idp_info[kProviderUrlFull].accounts = displayed_accounts;
  RequestExpectations expectations = kExpectationSuccess;
  expectations.fetched_endpoints &= ~FetchedEndpoint::CLIENT_METADATA;
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.SignInStateMatch",
      SignInStateMatchStatus::kIdpClaimedSignIn, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kIdpClaimedSignIn);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states mismatch if IDP claims that user is not signed in
// but browser observes that user is signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForOnlyBrowserObservedSignIn) {
  // Set browser observes user is signed in.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // By default, IDP claims user is not signed in.
  RequestExpectations expectations = kExpectationSuccess;
  expectations.fetched_endpoints &= ~FetchedEndpoint::CLIENT_METADATA;
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);

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
TEST_F(FederatedAuthRequestImplTest, RequestEmbargo) {
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kShouldEmbargo},
      /*selected_idp_config_url=*/absl::nullopt,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN & ~FetchedEndpoint::TOKEN};

  MockConfiguration configuration = kConfigurationValid;
  configuration.customized_dialog = true;

  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            base::span<const content::IdentityRequestAccount> accounts =
                identity_provider_data[0].accounts;
            displayed_accounts_ = AccountList(accounts.begin(), accounts.end());
            std::move(dismiss_callback).Run(DismissReason::CLOSE_BUTTON);
          }));

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.count(
      main_test_rfh()->GetLastCommittedOrigin()));
}

// Test that the embargo dismiss count is reset when the user grants consent via
// the FedCM dialog.
TEST_F(FederatedAuthRequestImplTest, RemoveEmbargoOnUserConsent) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.empty());
}

// Test that token request fails if FEDERATED_IDENTITY_API content setting is
// disabled for the RP origin.
TEST_F(FederatedAuthRequestImplTest, ApiBlockedForOrigin) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_SETTINGS);
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorDisabledInSettings},
      /*selected_idp_config_url=*/absl::nullopt,
      /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
}

// Test that token request succeeds if FEDERATED_IDENTITY_API content setting is
// enabled for RP origin but disabled for an unrelated origin.
TEST_F(FederatedAuthRequestImplTest, ApiBlockedForUnrelatedOrigin) {
  const url::Origin kUnrelatedOrigin = OriginFromString("https://rp2.example/");

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
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt,
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
TEST_F(FederatedAuthRequestImplTest, ApiDisabledAfterAccountsDialogShown) {
  base::HistogramTester histogram_tester_;

  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            // Disable FedCM API
            test_api_permission_delegate_->permission_override_ =
                std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                               ApiPermissionStatus::BLOCKED_SETTINGS);

            base::span<const content::IdentityRequestAccount> accounts =
                identity_provider_data[0].accounts;
            std::move(on_selected)
                .Run(identity_provider_data[0].idp_metadata.config_url,
                     accounts[0].id, /*is_sign_in=*/false);
          }));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  MockConfiguration configuration = kConfigurationValid;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorDisabledInSettings},
      /*selected_idp_config_url=*/absl::nullopt,
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

// Test the disclosure_text_shown value in the token post data for sign-up case.
TEST_F(FederatedAuthRequestImplTest, DisclosureTextShownForFirstTimeUser) {
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) + "&disclosure_text_shown=true");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test the disclosure_text_shown value in the token post data for returning
// user case.
TEST_F(FederatedAuthRequestImplTest, DisclosureTextNotShownForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData("client_id=" + std::string(kClientId) +
                                    "&nonce=" + std::string(kNonce) +
                                    "&account_id=" + std::string(kAccountId) +
                                    "&disclosure_text_shown=false");
  SetNetworkRequestManager(std::move(checker));

  RequestExpectations expectations = kExpectationSuccess;
  expectations.fetched_endpoints &= ~FetchedEndpoint::CLIENT_METADATA;
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
}

// Test that the values in the token post data are escaped according to the
// application/x-www-form-urlencoded spec.
TEST_F(FederatedAuthRequestImplTest, TokenEndpointPostDataEscaping) {
  const std::string kAccountIdWithSpace("account id");
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts[0].id = kAccountIdWithSpace;

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=account+id&disclosure_text_shown=true");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

namespace {

// TestIdpNetworkRequestManager subclass which runs the `account_list_task`
// passed-in to the constructor prior to the accounts endpoint returning.
class IdpNetworkRequestManagerClientMetadataTaskRunner
    : public TestIdpNetworkRequestManager {
 public:
  explicit IdpNetworkRequestManagerClientMetadataTaskRunner(
      base::OnceClosure client_metadata_task)
      : client_metadata_task_(std::move(client_metadata_task)) {}

  IdpNetworkRequestManagerClientMetadataTaskRunner(
      const IdpNetworkRequestManagerClientMetadataTaskRunner&) = delete;
  IdpNetworkRequestManagerClientMetadataTaskRunner& operator=(
      const IdpNetworkRequestManagerClientMetadataTaskRunner&) = delete;

  void FetchClientMetadata(const GURL& client_metadata_endpoint_url,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    // Make copies because running the task might destroy
    // FederatedAuthRequestImpl and invalidate the references.
    GURL client_metadata_endpoint_url_copy = client_metadata_endpoint_url;
    std::string client_id_copy = client_id;

    if (client_metadata_task_)
      std::move(client_metadata_task_).Run();
    TestIdpNetworkRequestManager::FetchClientMetadata(
        client_metadata_endpoint_url_copy, client_id_copy, std::move(callback));
  }

 private:
  base::OnceClosure client_metadata_task_;
};

void NavigateToUrl(content::WebContents* web_contents, const GURL& url) {
  static_cast<TestWebContents*>(web_contents)
      ->NavigateAndCommit(url, ui::PAGE_TRANSITION_LINK);
}

}  // namespace

// Test that the account chooser is not shown if the page navigates prior to the
// client metadata endpoint request completing and BFCache is enabled.
TEST_F(FederatedAuthRequestImplTest,
       NavigateDuringClientMetadataFetchBFCacheEnabled) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures(
      /*enabled_features=*/{features::kBackForwardCache},
      /*disabled_features=*/{features::kBackForwardCacheMemoryControls});
  ASSERT_TRUE(content::IsBackForwardCacheEnabled());

  SetNetworkRequestManager(
      std::make_unique<IdpNetworkRequestManagerClientMetadataTaskRunner>(
          base::BindOnce(&NavigateToUrl, web_contents(), GURL(kRpOtherUrl))));

  EXPECT_CALL(*mock_dialog_controller_, ShowAccountsDialog(_, _, _, _, _, _))
      .Times(0);
  MockConfiguration configuration = kConfigurationValid;
  configuration.customized_dialog = true;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG | FetchedEndpoint::CLIENT_METADATA |
          FetchedEndpoint::WELL_KNOWN | FetchedEndpoint::ACCOUNTS};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that the account chooser is not shown if the page navigates prior to the
// accounts endpoint request completing and BFCache is disabled.
TEST_F(FederatedAuthRequestImplTest,
       NavigateDuringClientMetadataFetchBFCacheDisabled) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kBackForwardCache);
  ASSERT_FALSE(content::IsBackForwardCacheEnabled());

  SetNetworkRequestManager(
      std::make_unique<IdpNetworkRequestManagerClientMetadataTaskRunner>(
          base::BindOnce(&NavigateToUrl, web_contents(), GURL(kRpOtherUrl))));

  EXPECT_CALL(*mock_dialog_controller_, ShowAccountsDialog(_, _, _, _, _, _))
      .Times(0);
  MockConfiguration configuration = kConfigurationValid;
  configuration.customized_dialog = true;

  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG | FetchedEndpoint::CLIENT_METADATA |
          FetchedEndpoint::WELL_KNOWN | FetchedEndpoint::ACCOUNTS};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that the accounts are reordered so that accounts with a LoginState equal
// to kSignIn are listed before accounts with a LoginState equal to kSignUp.
TEST_F(FederatedAuthRequestImplTest, ReorderMultipleAccounts) {
  // Run an auth test to initialize variables.
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  AccountList multiple_accounts = kMultipleAccounts;
  blink::mojom::IdentityProviderConfigPtr identity_provider =
      blink::mojom::IdentityProviderConfig::New(GURL(kProviderUrlFull),
                                                kClientId, kNonce);
  ComputeLoginStateAndReorderAccounts(*identity_provider, multiple_accounts);

  // Check the account order using the account ids.
  ASSERT_EQ(multiple_accounts.size(), 3u);
  EXPECT_EQ(multiple_accounts[0].id, "account_id");
  EXPECT_EQ(multiple_accounts[1].id, "nico_the_great");
  EXPECT_EQ(multiple_accounts[2].id, "other_account_id");
}

// Test that first API call with a given IDP is not affected by the
// IdpSigninStatus bit.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusTestFirstTimeFetchSuccess) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*mock_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), true))
      .Times(1);

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kClientId, kAccountId);
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that first API call with a given IDP will not show a UI in case of
// failure during fetching accounts.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusTestFirstTimeFetchNoFailureUi) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*mock_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), false))
      .Times(1);
  EXPECT_CALL(*mock_dialog_controller_, ShowFailureDialog(_, _, _, _)).Times(0);
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG | FetchedEndpoint::ACCOUNTS |
          FetchedEndpoint::WELL_KNOWN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that a failure UI will be displayed if the accounts fetch is failed but
// the IdpSigninStatus claims that the user is signed in.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusTestShowFailureUi) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*mock_dialog_controller_, ShowFailureDialog(_, _, _, _))
      .WillOnce(
          Invoke([&](content::WebContents* rp_web_contents,
                     const std::string& rp_url, const std::string& idp_url,
                     IdentityRequestDialogController::DismissCallback
                         dismiss_callback) {
            std::move(dismiss_callback).Run(DismissReason::CLOSE_BUTTON);
          }));

  EXPECT_CALL(*mock_permission_delegate_,
              GetIdpSigninStatus(OriginFromString(kProviderUrlFull)))
      .WillRepeatedly(Return(true));

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {RequestTokenStatus::kError,
                                      {FederatedAuthRequestResult::kError},
                                      /*selected_idp_config_url=*/absl::nullopt,
                                      FetchedEndpoint::CONFIG |
                                          FetchedEndpoint::ACCOUNTS |
                                          FetchedEndpoint::WELL_KNOWN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that API calls will fail before sending any network request if
// IdpSigninStatus shows that the user is not signed in with the IDP. No failure
// UI is displayed.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusTestApiFailedIfUserNotSignedInWithIdp) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*mock_permission_delegate_,
              GetIdpSigninStatus(OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_dialog_controller_, ShowFailureDialog(_, _, _, _)).Times(0);
  MockConfiguration configuration = kConfigurationValid;
  RequestExpectations expectations = {RequestTokenStatus::kError,
                                      {FederatedAuthRequestResult::kError},
                                      /*selected_idp_config_url=*/absl::nullopt,
                                      /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Test that when IdpSigninStatus API is in the metrics-only mode, that an IDP
// signed-out status stays signed-out regardless of what is returned by the
// accounts endpoint.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusMetricsModeStaysSignedout) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "true"}});

  EXPECT_CALL(*mock_permission_delegate_, GetIdpSigninStatus(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_permission_delegate_, SetIdpSigninStatus(_, _)).Times(0);

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that when IdpSigninStatus API does not have any state for an IDP, that
// the state transitions to sign-in if the accounts endpoint returns a
// non-empty list of accounts.
TEST_F(
    FederatedAuthRequestImplTest,
    IdpSigninStatusMetricsModeUndefinedTransitionsToSignedinWhenHaveAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "true"}});

  EXPECT_CALL(*mock_permission_delegate_, GetIdpSigninStatus(_))
      .WillRepeatedly(Return(absl::nullopt));
  EXPECT_CALL(*mock_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), true));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that when IdpSigninStatus API is in metrics-only mode, that IDP sign-in
// status transitions to signed-out if the accounts endpoint returns no
// information.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusMetricsModeTransitionsToSignedoutWhenNoAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "true"}});

  EXPECT_CALL(*mock_permission_delegate_, GetIdpSigninStatus(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), false));

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {RequestTokenStatus::kError,
                                      {},
                                      absl::nullopt,
                                      FetchedEndpoint::ACCOUNTS |
                                          FetchedEndpoint::CONFIG |
                                          FetchedEndpoint::WELL_KNOWN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// Tests that multiple IDPs provided results in an error if the
// `kFedCmMultipleIdentityProviders` flag is disabled.
TEST_F(FederatedAuthRequestImplTest, MultiIdpError) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kFedCmMultipleIdentityProviders);

  RequestExpectations expectations = {
      RequestTokenStatus::kError, {}, absl::nullopt, 0};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations,
              kConfigurationMultiIdpValid);
}

// Test successful multi IDP FedCM request.
TEST_F(FederatedAuthRequestImplTest, AllSuccessfulMultiIdpRequest) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  RunAuthTest(kDefaultMultiIdpRequestParameters, kExpectationSuccessMultiIdp,
              kConfigurationMultiIdpValid);
}

// Test fetching information for the 1st IdP failing, and succeeding for the
// second.
TEST_F(FederatedAuthRequestImplTest, FirstIdpWellKnownInvalid) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Intentionally fail the 1st provider's request by having an invalid
  // well-known file.
  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};

  RequestExpectations expectations = {
      RequestTokenStatus::kSuccess,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/kProviderTwoUrlFull,
      FetchedEndpoint::CONFIG_MULTI | FetchedEndpoint::WELL_KNOWN_MULTI |
          FetchedEndpoint::CLIENT_METADATA | FetchedEndpoint::ACCOUNTS |
          FetchedEndpoint::TOKEN};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
}

// Test fetching information for the 1st IdP succeeding, and failing for the
// second.
TEST_F(FederatedAuthRequestImplTest, SecondIdpWellKnownInvalid) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Intentionally fail the 2nd provider's request by having an invalid
  // well-known file.
  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.idp_info[kProviderTwoUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};

  RequestExpectations expectations = {
      RequestTokenStatus::kSuccess,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/kProviderUrlFull,
      FetchedEndpoint::CONFIG_MULTI | FetchedEndpoint::WELL_KNOWN_MULTI |
          FetchedEndpoint::CLIENT_METADATA | FetchedEndpoint::ACCOUNTS |
          FetchedEndpoint::TOKEN};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
}

// Test fetching information for all of the IdPs failing.
TEST_F(FederatedAuthRequestImplTest, AllWellKnownsInvalid) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Intentionally fail the requests for both IdPs by returning an invalid
  // well-known file.
  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};
  configuration.idp_info[kProviderTwoUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/absl::nullopt,
      FetchedEndpoint::CONFIG_MULTI | FetchedEndpoint::WELL_KNOWN_MULTI};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
}

// Test multi IDP FedCM request with duplicate IDPs should throw an error.
TEST_F(FederatedAuthRequestImplTest, DuplicateIdpMultiIdpRequest) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  RequestParameters request_parameters = kDefaultMultiIdpRequestParameters;
  request_parameters.identity_providers =
      std::vector<IdentityProviderParameters>{
          request_parameters.identity_providers[0],
          request_parameters.identity_providers[0]};

  EXPECT_CALL(*mock_dialog_controller_, ShowAccountsDialog(_, _, _, _, _, _))
      .Times(0);

  RequestExpectations expectations = {RequestTokenStatus::kError,
                                      /*devtools_issue_statuses=*/{},
                                      /*selected_idp_config_url=*/absl::nullopt,
                                      /*fetched_endpoints=*/0};

  RunAuthTest(request_parameters, expectations, kConfigurationMultiIdpValid);
}

TEST_F(FederatedAuthRequestImplTest, TooManyRequests) {
  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            // Does not do anything (user did not close or select an account).
          }));
  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt,
      /*fetched_endpoints=*/FETCH_ENDPOINT_ALL_REQUEST_TOKEN &
          ~FetchedEndpoint::TOKEN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  // Reset the network request manager so we can check that we fetch no
  // endpoints in the subsequent call.
  configuration.customized_dialog = false;
  SetNetworkRequestManager(std::make_unique<TestIdpNetworkRequestManager>());
  // The next FedCM request should fail since the initial request has not yet
  // been finalized.
  expectations = {RequestTokenStatus::kErrorTooManyRequests,
                  /*devtools_issue_statuses=*/{},
                  /*selected_idp_config_url=*/absl::nullopt,
                  /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

TEST_F(FederatedAuthRequestImplTest, IframeTooManyRequests) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures({features::kFedCm, features::kFedCmIframeSupport}, {});
  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            // Does not do anything (user did not close or select an account).
          }));
  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  configuration.customized_dialog = true;
  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt,
      /*fetched_endpoints=*/FETCH_ENDPOINT_ALL_REQUEST_TOKEN &
          ~FetchedEndpoint::TOKEN};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  // Add an iframe and test that it fails to invoke the API. This test could be
  // improved: it is hacky in that it resets the parameters needed to reuse the
  // methods in the test class.
  RenderFrameHost* iframe_rfh = content::RenderFrameHostTester::For(main_rfh())
                                    ->AppendChild(/*frame_name=*/"");
  // We need to keep the main frame's Remote alive so store it in a separate
  // variable so that we can set  |request_remote_| as the iframe's remote and
  // use the test methods.
  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote =
      std::move(request_remote_);
  request_remote_.reset();

  // Initialize the iframe FederatedAuthRequestImpl as well as the helper test
  // classes so that they all now belong to the iframe's
  // FederatedAuthRequestImpl.
  FederatedAuthRequestImpl* iframe_federated_auth_request_impl =
      &FederatedAuthRequestImpl::CreateForTesting(
          *iframe_rfh, test_api_permission_delegate_.get(),
          mock_permission_delegate_.get(),
          request_remote_.BindNewPipeAndPassReceiver());

  auto mock_dialog_controller =
      std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
  mock_dialog_controller_ = mock_dialog_controller.get();
  iframe_federated_auth_request_impl->SetDialogControllerForTests(
      std::move(mock_dialog_controller));

  std::unique_ptr<TestIdpNetworkRequestManager> network_request_manager =
      std::make_unique<TestIdpNetworkRequestManager>();
  test_network_request_manager_ = std::move(network_request_manager);
  iframe_federated_auth_request_impl->SetNetworkManagerForTests(
      std::make_unique<DelegatedIdpNetworkRequestManager>(
          test_network_request_manager_.get()));

  iframe_federated_auth_request_impl->SetTokenRequestDelayForTests(
      base::TimeDelta());
  configuration.customized_dialog = false;
  // The iframe invocation should fail with
  // RequestTokenStatus::kErrorTooManyRequests since the main frame's FedCM
  // request has not yet been finalized.
  expectations = {RequestTokenStatus::kErrorTooManyRequests,
                  /*devtools_issue_statuses=*/{},
                  /*selected_idp_config_url=*/absl::nullopt,
                  /*fetched_endpoints=*/0};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

// TestIdpNetworkRequestManager subclass which records requests to metrics
// endpoint.
class IdpNetworkRequestMetricsRecorder : public TestIdpNetworkRequestManager {
 public:
  IdpNetworkRequestMetricsRecorder() = default;
  IdpNetworkRequestMetricsRecorder(const IdpNetworkRequestMetricsRecorder&) =
      delete;
  IdpNetworkRequestMetricsRecorder& operator=(
      const IdpNetworkRequestMetricsRecorder&) = delete;

  void SendSuccessfulTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      base::TimeDelta api_call_to_show_dialog_time,
      base::TimeDelta show_dialog_to_continue_clicked_time,
      base::TimeDelta account_selected_to_token_response_time,
      base::TimeDelta api_call_to_token_response_time) override {
    metrics_endpoints_notified_success_.push_back(metrics_endpoint_url);
  }

  void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      MetricsEndpointErrorCode error_code) override {
    metrics_endpoints_notified_failure_.push_back(metrics_endpoint_url);
  }

  const std::vector<GURL>& get_metrics_endpoints_notified_success() {
    return metrics_endpoints_notified_success_;
  }

  const std::vector<GURL>& get_metrics_endpoints_notified_failure() {
    return metrics_endpoints_notified_failure_;
  }

 private:
  std::vector<GURL> metrics_endpoints_notified_success_;
  std::vector<GURL> metrics_endpoints_notified_failure_;
};

// Test that the metrics endpoint is notified as a result of a successful
// multi-IDP FederatedAuthRequestImpl::RequestToken() call.
TEST_F(FederatedAuthRequestImplTest, MetricsEndpointMultiIdp) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures(
      /*enabled_features=*/{features::kFedCmMetricsEndpoint,
                            features::kFedCmMultipleIdentityProviders},
      /*disabled_features=*/{});

  std::unique_ptr<IdpNetworkRequestMetricsRecorder> unique_metrics_recorder =
      std::make_unique<IdpNetworkRequestMetricsRecorder>();
  IdpNetworkRequestMetricsRecorder* metrics_recorder =
      unique_metrics_recorder.get();
  SetNetworkRequestManager(std::move(unique_metrics_recorder));

  RunAuthTest(kDefaultMultiIdpRequestParameters, kExpectationSuccessMultiIdp,
              kConfigurationMultiIdpValid);
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_success(),
              ElementsAre(kMetricsEndpoint));
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_failure(),
              ElementsAre("https://idp2.example/metrics"));
}

// Test that the metrics endpoint is notified when
// FederatedAuthRequestImpl::RequestToken() call fails.
TEST_F(FederatedAuthRequestImplTest, MetricsEndpointMultiIdpFail) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures(
      /*enabled_features=*/{features::kFedCmMetricsEndpoint,
                            features::kFedCmMultipleIdentityProviders},
      /*disabled_features=*/{});

  std::unique_ptr<IdpNetworkRequestMetricsRecorder> unique_metrics_recorder =
      std::make_unique<IdpNetworkRequestMetricsRecorder>();
  IdpNetworkRequestMetricsRecorder* metrics_recorder =
      unique_metrics_recorder.get();
  SetNetworkRequestManager(std::move(unique_metrics_recorder));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kShouldEmbargo},
      /* selected_idp_config_url=*/absl::nullopt,
      FETCH_ENDPOINT_ALL_REQUEST_TOKEN_MULTI & ~FetchedEndpoint::TOKEN};

  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.customized_dialog = true;

  EXPECT_CALL(*mock_dialog_controller(), ShowAccountsDialog(_, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              const std::string& rp_for_display,
              const std::vector<IdentityProviderData>& identity_provider_data,
              SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected,
              IdentityRequestDialogController::DismissCallback
                  dismiss_callback) {
            base::span<const content::IdentityRequestAccount> accounts =
                identity_provider_data[0].accounts;
            displayed_accounts_ = AccountList(accounts.begin(), accounts.end());
            std::move(dismiss_callback).Run(DismissReason::CLOSE_BUTTON);
          }));

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);

  EXPECT_TRUE(
      metrics_recorder->get_metrics_endpoints_notified_success().empty());
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_failure(),
              ElementsAre(kMetricsEndpoint, "https://idp2.example/metrics"));
}

}  // namespace content
