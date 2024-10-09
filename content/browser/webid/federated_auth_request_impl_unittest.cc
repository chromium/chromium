// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/test/delegated_idp_network_request_manager.h"
#include "content/browser/webid/test/federated_auth_request_request_token_callback_helper.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_auto_reauthn_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_registry.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/browser/webid/webid_utils.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::RequestTokenStatus;
using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using AuthRequestCallbackHelper =
    content::FederatedAuthRequestRequestTokenCallbackHelper;
using DismissReason = content::IdentityRequestDialogController::DismissReason;
using FedCmEntry = ukm::builders::Blink_FedCm;
using FedCmIdpEntry = ukm::builders::Blink_FedCmIdp;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using Field = content::IdentityRequestDialogDisclosureField;
using TokenError = content::IdentityCredentialTokenError;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using ErrorDialogType = content::IdpNetworkRequestManager::FedCmErrorDialogType;
using TokenResponseType =
    content::IdpNetworkRequestManager::FedCmTokenResponseType;
using ErrorUrlType = content::IdpNetworkRequestManager::FedCmErrorUrlType;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

constexpr char kProviderUrlFull[] = "https://idp.example/fedcm.json";
constexpr char kRpUrl[] = "https://rp.example/";
constexpr char kRpOtherUrl[] = "https://rp.example/random/";
constexpr char kIdpUrl[] = "https://idp.example/";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kCrossOriginAccountsEndpoint[] = "https://idp2.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kClientMetadataEndpoint[] =
    "https://idp.example/client_metadata";
constexpr char kMetricsEndpoint[] = "https://idp.example/metrics";
constexpr char kIdpLoginUrl[] = "https://idp.example/login_url";
constexpr char kIdpDisconnectUrl[] = "https://idp.example/disconnect";
constexpr char kPrivacyPolicyUrl[] = "https://rp.example/pp";
constexpr char kTermsOfServiceUrl[] = "https://rp.example/tos";
constexpr char kRpBrandIconUrl[] = "https://rp.example/icon";
constexpr char kClientId[] = "client_id_123";
constexpr char kNonce[] = "nonce123";
constexpr char kAccountEmailNicolas[] = "nicolas@email.com";
constexpr char kAccountEmailPeter[] = "peter@email.com";
constexpr char kAccountEmailZach[] = "zach@email.com";
constexpr char kAccountId[] = "1234";
constexpr char kAccountIdNicolas[] = "nico_id";
constexpr char kAccountIdPeter[] = "peter_id";
constexpr char kAccountIdZach[] = "zach_id";
constexpr char kAccountPicture[] = "https://idp.example/profilepic";
constexpr char kAccountPicture404[] = "https://idp.example/404";
constexpr int kAccountPictureSize = 10;
constexpr char kEmail[] = "ken@idp.example";
constexpr char kDomainHint[] = "domain@corp.com";
constexpr char kOtherDomainHint[] = "other_domain@corp.com";

// Values will be added here as token introspection is implemented.
constexpr char kToken[] = "[not a real token]";
constexpr char kEmptyToken[] = "";

constexpr char kAccountLabelNoMatchMessage[] =
    "Accounts were received, but none matched the label.";

constexpr char kLoginHintNoMatchMessage[] =
    "Accounts were received, but none matched the loginHint.";

constexpr char kDomainHintNoMatchMessage[] =
    "Accounts were received, but none matched the domainHint.";

static const std::vector<std::string> kDomainHintVector = {kDomainHint};
static const std::vector<std::string> kLabelVector = {"label"};
static const std::vector<std::string> kLoginHints = {kAccountId, kEmail};
static const std::vector<std::string> kNicolasHints = {kAccountIdNicolas,
                                                       kAccountEmailNicolas};
static const std::vector<std::string> kPeterHints = {kAccountIdPeter,
                                                     kAccountEmailPeter};
static const std::vector<std::string> kTwoDomainHints = {kDomainHint,
                                                         kOtherDomainHint};
static const std::vector<std::string> kZachHints = {kAccountIdZach,
                                                    kAccountEmailZach};

static std::vector<IdentityRequestAccountPtr> kMultipleAccounts;
static std::vector<IdentityRequestAccountPtr>
    kMultipleAccountsWithHintsAndDomains;
static std::vector<IdentityRequestAccountPtr> kSingleAccount;
static std::vector<IdentityRequestAccountPtr> kSingleAccountWithDomainHint;
static std::vector<IdentityRequestAccountPtr> kSingleAccountWithHint;
static std::vector<IdentityRequestAccountPtr> kTwoAccounts;

static const std::set<std::string> kWellKnown{kProviderUrlFull};

struct IdentityProviderParameters {
  const char* provider;
  const char* client_id;
  const char* nonce;
  const char* login_hint;
  const char* domain_hint;
  std::optional<std::vector<std::string>> fields;
  base::flat_map<std::string, std::string> params;
};

// Parameters for a call to RequestToken.
struct RequestParameters {
  std::vector<IdentityProviderParameters> identity_providers;
  blink::mojom::RpContext rp_context;
  blink::mojom::RpMode rp_mode;
};

// Expected return values from a call to RequestToken.
//
// DO NOT ADD NEW MEMBERS.
// Having a lot of members in RequestExpectations encourages bad test design.
// Specifically:
// - It encourages making the test harness more magic
// - It makes each test "test everything", making it really hard to determine
//   at a later date what the test was actually testing.

struct RequestExpectations {
  std::optional<RequestTokenStatus> return_status;
  FederatedAuthRequestResult devtools_issue_status;
  std::optional<std::string> standalone_console_message;
  std::optional<std::string> selected_idp_config_url;
  bool is_auto_selected{false};
};

// Mock configuration values for test.
struct MockClientIdConfiguration {
  FetchStatus fetch_status;
  std::string privacy_policy_url;
  std::string terms_of_service_url;
  std::string brand_icon_url;
};

struct MockWellKnown {
  std::set<std::string> provider_urls;
  FetchStatus fetch_status;
};

// Mock information returned from IdpNetworkRequestManager::FetchConfig().
struct MockConfig {
  FetchStatus fetch_status;
  std::string accounts_endpoint;
  std::string token_endpoint;
  std::string client_metadata_endpoint;
  std::string metrics_endpoint;
  std::string idp_login_url;
  std::string disconnect_endpoint;
  std::optional<SkColor> brand_background_color;
  std::optional<SkColor> brand_text_color;
  std::string requested_label;
};

struct MockIdpInfo {
  MockWellKnown well_known;
  MockConfig config;
  MockClientIdConfiguration client_metadata;
  FetchStatus accounts_response;
  std::vector<IdentityRequestAccountPtr> accounts;
};

// Action on accounts dialog taken by TestDialogController. Does not indicate a
// test expectation.
enum class AccountsDialogAction {
  kNone,
  kClose,
  kSelectFirstAccount,
  kAddAccount,
};

// Action on IdP-sign-in-status-mismatch dialog taken by TestDialogController.
// Does not indicate a test expectation.
enum class IdpSigninStatusMismatchDialogAction {
  kNone,
  kClose,
  kClosePopup,
};

// Action on error dialog taken by TestDialogController.
// Does not indicate a test expectation.
enum class ErrorDialogAction {
  kNone,
  kClose,
  kSwipe,
  kGotIt,
  kMoreDetails,
};

// Action on loading dialog taken by TestDialogController.
// Does not indicate a test expectation.
enum class LoadingDialogAction {
  kNone,
  kClose,
};

struct MockConfiguration {
  const char* token;
  base::flat_map<std::string, MockIdpInfo> idp_info;
  FetchStatus token_response;
  bool delay_token_response;
  AccountsDialogAction accounts_dialog_action;
  IdpSigninStatusMismatchDialogAction idp_signin_status_mismatch_dialog_action;
  ErrorDialogAction error_dialog_action;
  LoadingDialogAction loading_dialog_action;
  std::optional<GURL> continue_on;
  MediationRequirement mediation_requirement = MediationRequirement::kOptional;
  std::optional<TokenError> token_error;
  TokenResponseType token_response_type = TokenResponseType::
      kTokenNotReceivedAndErrorNotReceivedAndContinueOnNotReceived;
  std::optional<ErrorDialogType> error_dialog_type;
  std::optional<ErrorUrlType> error_url_type;
  blink::mojom::RpMode rp_mode{blink::mojom::RpMode::kPassive};
};

static const MockClientIdConfiguration kDefaultClientMetadata{
    {ParseStatus::kSuccess, net::HTTP_OK},
    kPrivacyPolicyUrl,
    kTermsOfServiceUrl,
    kRpBrandIconUrl};

static const IdentityProviderParameters kDefaultIdentityProviderRequestOptions{
    kProviderUrlFull, kClientId, kNonce, /*login_hint=*/"",
    /*domain_hint=*/""};

static const RequestParameters kDefaultRequestParameters{
    std::vector<IdentityProviderParameters>{
        kDefaultIdentityProviderRequestOptions},
    blink::mojom::RpContext::kSignIn, blink::mojom::RpMode::kPassive};

static MockIdpInfo kDefaultIdentityProviderInfo;
static MockIdpInfo kProviderTwoInfo;

static base::flat_map<std::string, MockIdpInfo> kSingleProviderInfo;

constexpr char kProviderTwoUrlFull[] = "https://idp2.example/fedcm.json";

static MockConfiguration kConfigurationValid;
static MockConfiguration kConfigurationMultiIdpValid;

static const RequestExpectations kExpectationSuccess{
    RequestTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
    /*standalone_console_message=*/std::nullopt, kProviderUrlFull};

static const RequestParameters kDefaultMultiIdpRequestParameters{
    std::vector<IdentityProviderParameters>{
        {kProviderUrlFull, kClientId, kNonce, /*login_hint=*/"",
         /*domain_hint=*/""},
        {kProviderTwoUrlFull, kClientId, kNonce, /*login_hint=*/"",
         /*domain_hint=*/""}},
    /*rp_context=*/blink::mojom::RpContext::kSignIn,
    /*rp_mode=*/blink::mojom::RpMode::kPassive};

url::Origin OriginFromString(const std::string& url_string) {
  return url::Origin::Create(GURL(url_string));
}

enum class FetchedEndpoint {
  CONFIG,
  CLIENT_METADATA,
  ACCOUNTS,
  TOKEN,
  WELL_KNOWN,
  PICTURE,
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
    ++num_fetched_[FetchedEndpoint::WELL_KNOWN];

    std::string provider_key = provider.spec();
    IdpNetworkRequestManager::WellKnown well_known;
    std::set<GURL> url_set(
        config_.idp_info[provider_key].well_known.provider_urls.begin(),
        config_.idp_info[provider_key].well_known.provider_urls.end());
    well_known.provider_urls = std::move(url_set);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       config_.idp_info[provider_key].well_known.fetch_status,
                       well_known));
  }

  void FetchConfig(const GURL& provider,
                   blink::mojom::RpMode rp_mode,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    ++num_fetched_[FetchedEndpoint::CONFIG];

    std::string provider_key = provider.spec();
    const MockConfig& config = config_.idp_info[provider_key].config;
    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.token = GURL(config.token_endpoint);
    endpoints.accounts = GURL(config.accounts_endpoint);
    endpoints.client_metadata = GURL(config.client_metadata_endpoint);
    endpoints.metrics = GURL(config.metrics_endpoint);
    endpoints.disconnect = GURL(config.disconnect_endpoint);

    IdentityProviderMetadata idp_metadata;
    idp_metadata.config_url = provider;
    idp_metadata.idp_login_url = GURL(config.idp_login_url);
    idp_metadata.brand_background_color = config.brand_background_color;
    idp_metadata.brand_text_color = config.brand_text_color;
    idp_metadata.requested_label = config.requested_label;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       config_.idp_info[provider_key].config.fetch_status,
                       endpoints, idp_metadata));
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           int rp_brand_icon_ideal_size,
                           int rp_brand_icon_minimum_size,
                           FetchClientMetadataCallback callback) override {
    ++num_fetched_[FetchedEndpoint::CLIENT_METADATA];

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
                           GURL(info.client_metadata.privacy_policy_url),
                           GURL(info.client_metadata.terms_of_service_url),
                           GURL(info.client_metadata.brand_icon_url)}));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    ++num_fetched_[FetchedEndpoint::ACCOUNTS];

    // Find the info of the provider with the same accounts endpoint.
    MockIdpInfo info;
    for (const auto& idp_info : config_.idp_info) {
      info = idp_info.second;
      if (GURL(info.config.accounts_endpoint) == accounts_url)
        break;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), info.accounts_response,
                                  accounts_list_.empty() ? info.accounts
                                                         : accounts_list_));
  }

  void SendTokenRequest(
      const GURL& token_url,
      const std::string& account,
      const std::string& url_encoded_post_data,
      TokenRequestCallback callback,
      ContinueOnCallback on_continue,
      RecordErrorMetricsCallback record_error_metrics_callback) override {
    ++num_fetched_[FetchedEndpoint::TOKEN];

    base::OnceCallback bound_record_error_metrics_callback = base::BindOnce(
        std::move(record_error_metrics_callback), config_.token_response_type,
        config_.error_dialog_type, config_.error_url_type);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(bound_record_error_metrics_callback));

    if (config_.token_error) {
      TokenResult result;
      result.error = config_.token_error;

      base::OnceCallback bound_callback =
          base::BindOnce(std::move(callback), config_.token_response, result);
      if (config_.delay_token_response) {
        delayed_callbacks_.push_back(std::move(bound_callback));
      } else {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(bound_callback));
      }
      return;
    }

    if (config_.continue_on) {
      base::OnceCallback bound_callback =
          base::BindOnce(std::move(on_continue), config_.token_response,
                         config_.continue_on.value());
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(bound_callback));
      return;
    }

    std::string delivered_token =
        config_.token_response.parse_status == ParseStatus::kSuccess
            ? config_.token
            : std::string();
    TokenResult result;
    result.token = delivered_token;
    base::OnceCallback bound_callback =
        base::BindOnce(std::move(callback), config_.token_response, result);
    if (config_.delay_token_response) {
      delayed_callbacks_.push_back(std::move(bound_callback));
    } else {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(bound_callback));
    }
  }

  void DownloadAndDecodeImage(const GURL& url,
                              ImageCallback callback) override {
    EXPECT_TRUE(url == GURL(kAccountPicture) || url == GURL(kAccountPicture404))
        << url;
    ++num_fetched_[FetchedEndpoint::PICTURE];
    if (url == GURL(kAccountPicture404)) {
      std::move(callback).Run(gfx::Image());
    } else {
      std::move(callback).Run(gfx::test::CreateImage(kAccountPictureSize));
    }
  }

  std::map<FetchedEndpoint, size_t> num_fetched_;

  // If non-empty, the accounts endpoint will return this accounts list instead
  // of the accounts list in the `config_`.
  std::vector<IdentityRequestAccountPtr> accounts_list_;

 protected:
  MockConfiguration config_{kConfigurationValid};
  std::vector<base::OnceClosure> delayed_callbacks_;
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
                           int rp_brand_icon_ideal_size,
                           int rp_brand_icon_minimum_size,
                           FetchClientMetadataCallback callback) override {
    if (expected_client_id_)
      EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::FetchClientMetadata(
        endpoint, client_id, rp_brand_icon_ideal_size,
        rp_brand_icon_minimum_size, std::move(callback));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    if (expected_client_id_)
      EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::SendAccountsRequest(accounts_url, client_id,
                                                      std::move(callback));
  }

  void SendTokenRequest(
      const GURL& token_url,
      const std::string& account,
      const std::string& url_encoded_post_data,
      TokenRequestCallback callback,
      ContinueOnCallback on_continue,
      RecordErrorMetricsCallback record_error_metrics_callback) override {
    if (expected_selected_account_id_)
      EXPECT_EQ(expected_selected_account_id_, account);
    if (expected_url_encoded_post_data_)
      EXPECT_EQ(expected_url_encoded_post_data_, url_encoded_post_data);
    TestIdpNetworkRequestManager::SendTokenRequest(
        token_url, account, url_encoded_post_data, std::move(callback),
        std::move(on_continue), std::move(record_error_metrics_callback));
  }

 private:
  std::optional<std::string> expected_client_id_;
  std::optional<std::string> expected_selected_account_id_;
  std::optional<std::string> expected_url_encoded_post_data_;
};

class TestDialogController
    : public NiceMock<MockIdentityRequestDialogController> {
 public:
  struct State {
    // State related to ShowAccountsDialog().
    // The list of all accounts passed to the UI.
    std::vector<IdentityRequestAccountPtr> all_accounts_for_display;
    std::optional<IdentityRequestAccount::SignInMode> sign_in_mode;
    blink::mojom::RpContext rp_context;
    std::vector<IdentityRequestAccountPtr> new_accounts;
    // The last seen background/text color from IdP metadata.
    std::optional<SkColor> brand_background_color;
    std::optional<SkColor> brand_text_color;
    // State related to ShowFailureDialog().
    size_t num_show_idp_signin_status_mismatch_dialog_requests{0u};
    // State related to ShowErrorDialog().
    bool did_show_error_dialog{false};
    std::optional<TokenError> token_error;
    // State related to ShowLoadingDialog().
    bool did_show_loading_dialog{false};
    // List of IDP strings for which a mismatch is shown in a test.
    std::vector<std::string> displayed_mismatch_idps;
  };

  explicit TestDialogController(MockConfiguration config)
      : accounts_dialog_action_(config.accounts_dialog_action),
        idp_signin_status_mismatch_dialog_action_(
            config.idp_signin_status_mismatch_dialog_action),
        error_dialog_action_(config.error_dialog_action),
        loading_dialog_action_(config.loading_dialog_action) {}

  ~TestDialogController() override = default;
  TestDialogController(TestDialogController&) = delete;
  TestDialogController& operator=(TestDialogController&) = delete;

  void SetState(State* state) { state_ = state; }

  void SetIdpSigninStatusMismatchDialogAction(
      IdpSigninStatusMismatchDialogAction action) {
    idp_signin_status_mismatch_dialog_action_ = action;
  }

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
    if (!state_) {
      return false;
    }
    state_->all_accounts_for_display.clear();

    state_->sign_in_mode = sign_in_mode;
    state_->rp_context = idp_list[0]->rp_context;

    state_->new_accounts = new_accounts;

    state_->all_accounts_for_display = accounts;
    for (const auto& idp_data : idp_list) {
      if (idp_data->has_login_status_mismatch) {
        state_->displayed_mismatch_idps.push_back(idp_data->idp_for_display);
      }
      state_->brand_background_color =
          idp_data->idp_metadata.brand_background_color;
      state_->brand_text_color = idp_data->idp_metadata.brand_text_color;
    }

    switch (accounts_dialog_action_) {
      case AccountsDialogAction::kSelectFirstAccount: {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                std::move(on_selected),
                accounts[0]->identity_provider->idp_metadata.config_url,
                accounts[0]->id,
                accounts[0]->login_state == LoginState::kSignIn));
        break;
      }
      case AccountsDialogAction::kClose:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(dismiss_callback),
                                      DismissReason::kCloseButton));
        break;
      case AccountsDialogAction::kAddAccount:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(on_add_account),
                                      idp_list[0]->idp_metadata.config_url,
                                      idp_list[0]->idp_metadata.idp_login_url));
        // Set `accounts_dialog_action_` such that subsequent calls will select
        // the first account.
        accounts_dialog_action_ = AccountsDialogAction::kSelectFirstAccount;
        break;
      case AccountsDialogAction::kNone:
        break;
    }
    return true;
  }

  bool ShowFailureDialog(
      const std::string& rp_for_display,
      const std::string& idp_for_display,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const IdentityProviderMetadata& idp_metadata,
      IdentityRequestDialogController::DismissCallback dismiss_callback,
      IdentityRequestDialogController::LoginToIdPCallback
          identity_registry_callback) override {
    if (!state_) {
      return false;
    }

    state_->displayed_mismatch_idps.push_back(idp_for_display);
    ++state_->num_show_idp_signin_status_mismatch_dialog_requests;
    switch (idp_signin_status_mismatch_dialog_action_) {
      case IdpSigninStatusMismatchDialogAction::kClose:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(dismiss_callback),
                                      DismissReason::kCloseButton));
        break;
      case IdpSigninStatusMismatchDialogAction::kClosePopup:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(dismiss_callback), DismissReason::kOther));
        break;
      case IdpSigninStatusMismatchDialogAction::kNone:
        break;
    }
    return true;
  }

  bool ShowErrorDialog(
      const std::string& rp_for_display,
      const std::string& idp_for_display,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const IdentityProviderMetadata& idp_metadata,
      const std::optional<TokenError>& error,
      IdentityRequestDialogController::DismissCallback dismiss_callback,
      IdentityRequestDialogController::MoreDetailsCallback
          more_details_callback) override {
    if (!state_) {
      return false;
    }

    state_->did_show_error_dialog = true;
    state_->token_error = error;
    switch (error_dialog_action_) {
      case ErrorDialogAction::kClose:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(dismiss_callback),
                                      DismissReason::kCloseButton));
        break;
      case ErrorDialogAction::kSwipe:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(dismiss_callback), DismissReason::kSwipe));
        break;
      case ErrorDialogAction::kGotIt:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(dismiss_callback),
                                      DismissReason::kGotItButton));
        break;
      case ErrorDialogAction::kMoreDetails:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(more_details_callback)));
        break;
      case ErrorDialogAction::kNone:
        break;
    }
    return true;
  }

  bool ShowLoadingDialog(const std::string& rp_for_display,
                         const std::string& idp_for_display,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode,
                         IdentityRequestDialogController::DismissCallback
                             dismiss_callback) override {
    if (!state_) {
      return false;
    }

    state_->did_show_loading_dialog = true;
    switch (loading_dialog_action_) {
      case LoadingDialogAction::kClose:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(dismiss_callback),
                                      DismissReason::kCloseButton));
        break;
      case LoadingDialogAction::kNone:
        break;
    }
    return true;
  }

  base::WeakPtr<TestDialogController> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  AccountsDialogAction accounts_dialog_action_{AccountsDialogAction::kNone};
  IdpSigninStatusMismatchDialogAction idp_signin_status_mismatch_dialog_action_{
      IdpSigninStatusMismatchDialogAction::kNone};
  ErrorDialogAction error_dialog_action_{ErrorDialogAction::kNone};
  LoadingDialogAction loading_dialog_action_{LoadingDialogAction::kNone};

  // Pointer so that the state can be queried after FederatedAuthRequestImpl
  // destroys TestDialogController.
  raw_ptr<State> state_;
  base::WeakPtrFactory<TestDialogController> weak_ptr_factory_{this};
};

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  using PermissionOverride = std::pair<url::Origin, ApiPermissionStatus>;
  PermissionOverride permission_override_ =
      std::make_pair(url::Origin(), ApiPermissionStatus::GRANTED);
  std::optional<std::pair<size_t, PermissionOverride>>
      permission_override_for_nth_;
  std::set<url::Origin> embargoed_origins_;
  size_t api_invocation_counter{0};

  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    ++api_invocation_counter;

    if (embargoed_origins_.count(origin))
      return ApiPermissionStatus::BLOCKED_EMBARGO;

    if (permission_override_for_nth_ &&
        permission_override_for_nth_->first == api_invocation_counter) {
      return (origin == permission_override_for_nth_->second.first)
                 ? permission_override_for_nth_->second.second
                 : ApiPermissionStatus::GRANTED;
    }

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

class TestPermissionDelegate : public NiceMock<MockPermissionDelegate> {
 public:
  std::map<url::Origin, std::optional<bool>> idp_signin_statuses_;

  TestPermissionDelegate() = default;
  ~TestPermissionDelegate() override = default;

  TestPermissionDelegate(TestPermissionDelegate&) = delete;
  TestPermissionDelegate& operator=(TestPermissionDelegate&) = delete;

  std::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override {
    auto it = idp_signin_statuses_.find(idp_origin);
    return (it != idp_signin_statuses_.end()) ? it->second : std::nullopt;
  }

  void SetIdpSigninStatus(const url::Origin& idp_origin,
                          bool idp_signin_status) override {
    idp_signin_statuses_[idp_origin] = idp_signin_status;
    // Call parent so that EXPECT_CALL() works.
    NiceMock<MockPermissionDelegate>::SetIdpSigninStatus(idp_origin,
                                                         idp_signin_status);
  }
};

class TestAutoReauthnPermissionDelegate
    : public MockAutoReauthnPermissionDelegate {
 public:
  std::set<url::Origin> embargoed_origins_;

  void RecordEmbargoForAutoReauthn(const url::Origin& origin) override {
    embargoed_origins_.insert(origin);
  }
};

class TestIdentityRegistry : public NiceMock<MockIdentityRegistry> {
 public:
  bool notified_{false};

  explicit TestIdentityRegistry(
      content::WebContents* web_contents,
      base::WeakPtr<FederatedIdentityModalDialogViewDelegate> delegate,
      const GURL& idp_config_url)
      : NiceMock<MockIdentityRegistry>(web_contents, delegate, idp_config_url) {
  }

  void NotifyClose(const url::Origin& notifier_origin) override {
    notified_ = true;
  }
};

}  // namespace

class FederatedAuthRequestImplTest : public RenderViewHostImplTestHarness {
 protected:
  explicit FederatedAuthRequestImplTest(std::string_view rp_url = kRpUrl)
      : rp_url_(rp_url) {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  ~FederatedAuthRequestImplTest() override = default;

  void InitConstants() {
    kSingleAccount = {base::MakeRefCounted<IdentityRequestAccount>(
        kAccountId,                  // id
        kEmail,                      // email
        "Ken R. Example",            // name
        "Ken",                       // given_name
        GURL(),                      // picture
        std::vector<std::string>(),  // login_hints
        std::vector<std::string>(),  // domain_hints
        std::vector<std::string>()   // labels
        )};
    kDefaultIdentityProviderInfo = {
        {kWellKnown, {ParseStatus::kSuccess, net::HTTP_OK}},
        {{ParseStatus::kSuccess, net::HTTP_OK},
         kAccountsEndpoint,
         kTokenEndpoint,
         kClientMetadataEndpoint,
         kMetricsEndpoint,
         kIdpLoginUrl,
         kIdpDisconnectUrl,
         /*brand_background_color=*/std::nullopt,
         /*brand_text_color=*/std::nullopt},
        kDefaultClientMetadata,
        {ParseStatus::kSuccess, net::HTTP_OK},
        kSingleAccount,
    };
    kSingleProviderInfo = {{kProviderUrlFull, kDefaultIdentityProviderInfo}};
    kSingleAccountWithHint = {base::MakeRefCounted<IdentityRequestAccount>(
        kAccountId,                  // id
        kEmail,                      // email
        "Ken R. Example",            // name
        "Ken",                       // given_name
        GURL(),                      // picture
        kLoginHints,                 // login_hints
        std::vector<std::string>(),  // domain_hints
        std::vector<std::string>()   // labels
        )};
    kSingleAccountWithDomainHint = {
        base::MakeRefCounted<IdentityRequestAccount>(
            kAccountId,                  // id
            kEmail,                      // email
            "Ken R. Example",            // name
            "Ken",                       // given_name
            GURL(),                      // picture
            std::vector<std::string>(),  // login_hints
            kDomainHintVector,           // domain_hints
            std::vector<std::string>()   // labels
            )};
    kTwoAccounts = {base::MakeRefCounted<IdentityRequestAccount>(
                        kAccountIdNicolas,           // id
                        kAccountEmailNicolas,        // email
                        "Nicolas P",                 // name
                        "Nicolas",                   // given_name
                        GURL(),                      // picture
                        std::vector<std::string>(),  // login_hints
                        std::vector<std::string>(),  // domain_hints
                        std::vector<std::string>(),  // labels
                        LoginState::kSignUp          // login_state
                        ),
                    base::MakeRefCounted<IdentityRequestAccount>(
                        kAccountIdZach,              // id
                        "zach@email.com",            // email
                        "Zachary T",                 // name
                        "Zach",                      // given_name
                        GURL(),                      // picture
                        std::vector<std::string>(),  // login_hints
                        std::vector<std::string>(),  // domain_hints
                        std::vector<std::string>(),  // labels
                        LoginState::kSignUp          // login_state
                        )};
    kMultipleAccounts = {base::MakeRefCounted<IdentityRequestAccount>(
                             kAccountIdNicolas,           // id
                             kAccountEmailNicolas,        // email
                             "Nicolas P",                 // name
                             "Nicolas",                   // given_name
                             GURL(),                      // picture
                             std::vector<std::string>(),  // login_hints
                             std::vector<std::string>(),  // domain_hints
                             std::vector<std::string>(),  // labels
                             LoginState::kSignUp          // login_state
                             ),
                         base::MakeRefCounted<IdentityRequestAccount>(
                             kAccountIdPeter,             // id
                             kAccountEmailPeter,          // email
                             "Peter K",                   // name
                             "Peter",                     // given_name
                             GURL(),                      // picture
                             std::vector<std::string>(),  // login_hints
                             std::vector<std::string>(),  // domain_hints
                             std::vector<std::string>(),  // labels
                             LoginState::kSignIn          // login_state
                             ),
                         base::MakeRefCounted<IdentityRequestAccount>(
                             kAccountIdZach,              // id
                             "zach@email.com",            // email
                             "Zachary T",                 // name
                             "Zach",                      // given_name
                             GURL(),                      // picture
                             std::vector<std::string>(),  // login_hints
                             std::vector<std::string>(),  // domain_hints
                             std::vector<std::string>(),  // labels
                             LoginState::kSignUp          // login_state
                             )};
    kMultipleAccountsWithHintsAndDomains = {
        base::MakeRefCounted<IdentityRequestAccount>(
            kAccountIdNicolas,           // id
            kAccountEmailNicolas,        // email
            "Nicolas P",                 // name
            "Nicolas",                   // given_name
            GURL(),                      // picture
            kNicolasHints,               // login_hints
            kDomainHintVector,           // domain_hints
            std::vector<std::string>(),  // labels
            LoginState::kSignUp          // login_state
            ),
        base::MakeRefCounted<IdentityRequestAccount>(
            kAccountIdPeter,             // id
            kAccountEmailPeter,          // email
            "Peter K",                   // name
            "Peter",                     // given_name
            GURL(),                      // picture
            kPeterHints,                 // login_hints
            std::vector<std::string>(),  // domain_hints
            kLabelVector,                // labels
            LoginState::kSignIn          // login_state
            ),
        base::MakeRefCounted<IdentityRequestAccount>(
            kAccountIdZach,              // id
            kAccountEmailZach,           // email
            "Zachary T",                 // name
            "Zach",                      // given_name
            GURL(),                      // picture
            kZachHints,                  // login_hints
            kTwoDomainHints,             // domain_hints
            std::vector<std::string>(),  // labels
            LoginState::kSignUp          // login_state
            )};
    kProviderTwoInfo = {{{kProviderTwoUrlFull}},
                        {{ParseStatus::kSuccess, net::HTTP_OK},
                         "https://idp2.example/accounts",
                         "https://idp2.example/token",
                         "https://idp2.example/client_metadata",
                         "https://idp2.example/metrics",
                         "https://idp2.example/login_url",
                         "https://idp2.example/disconnect",
                         /*brand_background_color=*/std::nullopt,
                         /*brand_text_color=*/std::nullopt},
                        kDefaultClientMetadata,
                        {ParseStatus::kSuccess, net::HTTP_OK},
                        kMultipleAccounts};
    kConfigurationValid = {kToken,
                           kSingleProviderInfo,
                           {ParseStatus::kSuccess, net::HTTP_OK},
                           /*delay_token_response=*/false,
                           AccountsDialogAction::kSelectFirstAccount,
                           IdpSigninStatusMismatchDialogAction::kNone,
                           ErrorDialogAction::kClose,
                           LoadingDialogAction::kNone};
    kConfigurationMultiIdpValid = {
        kToken,
        {{kProviderUrlFull, kDefaultIdentityProviderInfo},
         {kProviderTwoUrlFull, kProviderTwoInfo}},
        {ParseStatus::kSuccess, net::HTTP_OK},
        false /* delay_token_response */,
        AccountsDialogAction::kSelectFirstAccount,
        IdpSigninStatusMismatchDialogAction::kNone,
        ErrorDialogAction::kClose,
        LoadingDialogAction::kNone};
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    // Initialize the accounts and account-dependent constants on SetUp() to
    // ensure they are initialized correctly in every test.
    InitConstants();

    test_api_permission_delegate_ =
        std::make_unique<TestApiPermissionDelegate>();
    test_permission_delegate_ = std::make_unique<TestPermissionDelegate>();
    test_auto_reauthn_permission_delegate_ =
        std::make_unique<TestAutoReauthnPermissionDelegate>();
    test_identity_registry_ = std::make_unique<TestIdentityRegistry>(
        web_contents(), /*delegate=*/nullptr, GURL(kIdpUrl));

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(rp_url_), ui::PAGE_TRANSITION_LINK);

    federated_auth_request_impl_ = &FederatedAuthRequestImpl::CreateForTesting(
        *main_test_rfh(), test_api_permission_delegate_.get(),
        test_auto_reauthn_permission_delegate_.get(),
        test_permission_delegate_.get(), test_identity_registry_.get(),
        request_remote_.BindNewPipeAndPassReceiver());

    std::unique_ptr<TestIdpNetworkRequestManager> network_request_manager =
        std::make_unique<TestIdpNetworkRequestManager>();
    SetNetworkRequestManager(std::move(network_request_manager));
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

  // Sets the TestDialogController to be used for the next call of
  // RunAuthTest().
  void SetDialogController(
      std::unique_ptr<TestDialogController> dialog_controller) {
    custom_dialog_controller_ = std::move(dialog_controller);
  }

  void RunAuthTest(const RequestParameters& request_parameters,
                   const RequestExpectations& expectations,
                   const MockConfiguration& configuration) {
    request_remote_.set_disconnect_handler(auth_helper_.quit_closure());

    RunAuthDontWaitForCallback(request_parameters, configuration);
    WaitForCurrentAuthRequest();
    CheckAuthExpectations(configuration, expectations);
  }

  void RunAuthDontWaitForCallback(const RequestParameters& request_parameters,
                                  const MockConfiguration& configuration) {
    if (!custom_dialog_controller_) {
      custom_dialog_controller_ =
          std::make_unique<TestDialogController>(configuration);
    }

    dialog_controller_state_ = TestDialogController::State();
    custom_dialog_controller_->SetState(&dialog_controller_state_);
    federated_auth_request_impl_->SetDialogControllerForTests(
        std::move(custom_dialog_controller_));

    SetConfig(configuration);

    // If multiple IdPs are received, add them to a single get call. Unittests
    // for multiple get calls can be added later as needed.
    std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> idp_ptrs;
    for (const auto& identity_provider :
         request_parameters.identity_providers) {
      blink::mojom::IdentityProviderRequestOptionsPtr options =
          blink::mojom::IdentityProviderRequestOptions::New();
      options->config = blink::mojom::IdentityProviderConfig::New();
      options->config->config_url = GURL(identity_provider.provider);
      options->config->client_id = identity_provider.client_id;
      options->nonce = identity_provider.nonce;
      options->login_hint = identity_provider.login_hint;
      options->domain_hint = identity_provider.domain_hint;
      options->fields = std::move(identity_provider.fields);
      options->params = std::move(identity_provider.params);
      idp_ptrs.push_back(std::move(options));
    }
    blink::mojom::IdentityProviderGetParametersPtr get_params =
        blink::mojom::IdentityProviderGetParameters::New(
            std::move(idp_ptrs), request_parameters.rp_context,
            request_parameters.rp_mode);
    std::vector<blink::mojom::IdentityProviderGetParametersPtr> idp_get_params;
    idp_get_params.push_back(std::move(get_params));

    PerformAuthRequest(std::move(idp_get_params),
                       configuration.mediation_requirement);
  }

  void CheckAuthExpectations(const MockConfiguration& configuration,
                             const RequestExpectations& expectation) {
    ASSERT_EQ(expectation.return_status, auth_helper_.status());
    if (expectation.return_status == RequestTokenStatus::kSuccess) {
      EXPECT_EQ(configuration.token, auth_helper_.token());
    } else {
      EXPECT_TRUE(auth_helper_.token() == std::nullopt ||
                  auth_helper_.token() == kEmptyToken);
    }

    if (expectation.return_status == RequestTokenStatus::kSuccess) {
      EXPECT_TRUE(DidFetchWellKnownAndConfig());
      EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
      EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
      // FetchedEndpoint::CLIENT_METADATA is optional.

      EXPECT_EQ(did_show_accounts_dialog(),
                !expectation.is_auto_selected ||
                    configuration.rp_mode != blink::mojom::RpMode::kActive);
    }

    EXPECT_EQ(expectation.is_auto_selected, auth_helper_.is_auto_selected());

    EXPECT_EQ(expectation.selected_idp_config_url,
              auth_helper_.selected_idp_config_url());

    if (expectation.devtools_issue_status !=
        FederatedAuthRequestResult::kSuccess) {
      int issue_count = main_test_rfh()->GetFederatedAuthRequestIssueCount(
          expectation.devtools_issue_status);
      EXPECT_LE(1, issue_count);
    } else {
      int issue_count =
          main_test_rfh()->GetFederatedAuthRequestIssueCount(std::nullopt);
      if (!expectation.standalone_console_message) {
        EXPECT_EQ(0, issue_count);
      } else {
        EXPECT_GE(1, issue_count);
      }
    }
    CheckConsoleMessages(expectation.devtools_issue_status,
                         expectation.standalone_console_message);
  }

  void SetConfig(const MockConfiguration& config) {
    test_network_request_manager_->SetTestConfig(config);
  }

  void CheckConsoleMessages(
      FederatedAuthRequestResult devtools_issue_status,
      const std::optional<std::string>& standalone_console_message) {
    std::vector<std::string> messages =
        RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();

    bool did_expect_any_messages = false;
    size_t expected_message_index = messages.size() - 1;
    if (devtools_issue_status != FederatedAuthRequestResult::kSuccess) {
      std::string expected_message =
          webid::GetConsoleErrorMessageFromResult(devtools_issue_status);
      did_expect_any_messages = true;
      ASSERT_GE(expected_message_index, 0u);
      EXPECT_EQ(expected_message, messages[expected_message_index--]);
    }
    if (standalone_console_message) {
      did_expect_any_messages = true;
      ASSERT_EQ(expected_message_index, 0u);
      EXPECT_EQ(*standalone_console_message, messages[0]);
    }

    if (!did_expect_any_messages)
      EXPECT_EQ(0u, messages.size());
  }

  void PerformAuthRequest(
      std::vector<blink::mojom::IdentityProviderGetParametersPtr>
          idp_get_params,
      MediationRequirement mediation_requirement) {
    request_remote_->RequestToken(std::move(idp_get_params),
                                  mediation_requirement,
                                  auth_helper_.callback());

    // Ensure that the request makes its way to FederatedAuthRequestImpl.
    request_remote_.FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void WaitForCurrentAuthRequest(bool should_fast_forward = true) {
    request_remote_.set_disconnect_handler(auth_helper_.quit_closure());

    // Fast forward clock so that the pending
    // FederatedAuthRequestImpl::OnRejectRequest() task, if any, gets a
    // chance to run.
    if (should_fast_forward) {
      task_environment()->FastForwardBy(base::Minutes(10));
    }
    auth_helper_.WaitForCallback();

    request_remote_.set_disconnect_handler(base::OnceClosure());
  }

  base::span<const IdentityRequestAccountPtr> all_accounts_for_display() const {
    return dialog_controller_state_.all_accounts_for_display;
  }

  base::span<const IdentityRequestAccountPtr> new_accounts() const {
    return dialog_controller_state_.new_accounts;
  }

  std::vector<std::string> displayed_mismatch_idps() const {
    return dialog_controller_state_.displayed_mismatch_idps;
  }

  bool did_show_accounts_dialog() const {
    return !all_accounts_for_display().empty();
  }
  bool did_show_idp_signin_status_mismatch_dialog() const {
    return dialog_controller_state_
        .num_show_idp_signin_status_mismatch_dialog_requests;
  }

  std::optional<SkColor> brand_background_color() const {
    return dialog_controller_state_.brand_background_color;
  }

  std::optional<SkColor> brand_text_color() const {
    return dialog_controller_state_.brand_text_color;
  }

  int CountNumLoginStateIsSignin() {
    int num_sign_in_login_state = 0;
    for (const auto& account : all_accounts_for_display()) {
      if (account->login_state == LoginState::kSignIn) {
        ++num_sign_in_login_state;
      }
    }
    return num_sign_in_login_state;
  }

  bool DidFetchAnyEndpoint() {
    for (auto& [endpoint, num] : test_network_request_manager_->num_fetched_) {
      if (num > 0) {
        return true;
      }
    }
    return false;
  }

  // Convenience method as WELL_KNOWN and CONFIG endpoints are fetched in
  // parallel.
  bool DidFetchWellKnownAndConfig() {
    return DidFetch(FetchedEndpoint::WELL_KNOWN) &&
           DidFetch(FetchedEndpoint::CONFIG);
  }

  bool DidFetch(FetchedEndpoint endpoint) { return NumFetched(endpoint) > 0u; }

  size_t NumFetched(FetchedEndpoint endpoint) {
    return test_network_request_manager_->num_fetched_[endpoint];
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  void ExpectStatusMetrics(
      TokenStatus status,
      MediationRequirement requirement = MediationRequirement::kOptional) {
    histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                         status, 1);
    histogram_tester_.ExpectUniqueSample(
        "Blink.FedCm.Status.MediationRequirement", requirement, 1);
    ExpectStatusUKMInternal(status, requirement, FedCmEntry::kEntryName);
    ExpectStatusUKMInternal(status, requirement, FedCmIdpEntry::kEntryName);
  }

  void ExpectStatusUKMInternal(TokenStatus status,
                               MediationRequirement requirement,
                               const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty())
        << "No " << entry_name << " entry was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    bool metric_found = false;
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status.RequestIdToken");
      if (!metric) {
        continue;
      }
      EXPECT_FALSE(metric_found)
          << "Found more than one entry with Status.RequestIdToken in "
          << entry_name;
      metric_found = true;
      EXPECT_EQ(static_cast<int>(status), *metric)
          << "Unexpected status recorded in " << entry_name;
    }
    EXPECT_TRUE(metric_found)
        << "No Status.RequestIdToken entry was found in " << entry_name;
  }

  void ExpectUKMPresence(const std::string& metric_name) {
    ExpectUKMPresenceInternal(metric_name, FedCmEntry::kEntryName);
    ExpectUKMPresenceInternal(metric_name, FedCmIdpEntry::kEntryName);
  }

  void ExpectUKMPresenceInternal(const std::string& metric_name,
                                 const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty())
        << "No " << entry_name << " entry was recorded";

    for (const ukm::mojom::UkmEntry* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected UKM " << metric_name << " was not recorded in "
           << entry_name;
  }

  void ExpectNoUKMPresence(const std::string& metric_name) {
    ExpectNoUKMPresenceInternal(metric_name, FedCmEntry::kEntryName);
    ExpectNoUKMPresenceInternal(metric_name, FedCmIdpEntry::kEntryName);
  }

  void ExpectNoUKMPresenceInternal(const std::string& metric_name,
                                   const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty())
        << "No " << entry_name << " entry was recorded";

    for (const ukm::mojom::UkmEntry* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name)) {
        FAIL() << "Unexpectedly found " << metric_name << " UKM in "
               << entry_name;
      }
    }
    SUCCEED();
  }

  void ExpectUKMCount(const std::string& metric_name,
                      const char* entry_name,
                      int expected_count) {
    int count = 0;
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name)) {
        ++count;
      }
    }
    EXPECT_EQ(count, expected_count) << "Did not find the expected count for "
                                     << metric_name << " in " << entry_name;
  }

  void ExpectUkmValue(const std::string& metric_name, int expected_value) {
    ExpectUkmValueInEntry(metric_name, FedCmEntry::kEntryName, expected_value);
    ExpectUkmValueInEntry(metric_name, FedCmIdpEntry::kEntryName,
                          expected_value);
  }

  void ExpectUkmValueInEntry(const std::string& metric_name,
                             const char* entry_name,
                             int expected_value,
                             bool other_values_allowed = false) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);
    int count = 0;
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      const int64_t* value = ukm_recorder()->GetEntryMetric(entry, metric_name);
      if (!value) {
        continue;
      }
      ++count;
      if (!other_values_allowed) {
        EXPECT_EQ(*value, expected_value);
      }
    }
    EXPECT_GT(count, 0) << "Did not find " << metric_name << " in "
                        << entry_name;
  }

  void ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus status) {
    ExpectSignInStateMatchStatusUKMInternal(status, FedCmEntry::kEntryName);
    ExpectSignInStateMatchStatusUKMInternal(status, FedCmIdpEntry::kEntryName);
  }

  void ExpectSignInStateMatchStatusUKMInternal(SignInStateMatchStatus status,
                                               const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty()) << "No FedCm entry was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    bool metric_found = false;
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status.SignInStateMatch");
      if (!metric) {
        continue;
      }
      EXPECT_FALSE(metric_found)
          << "Found more than one Status.SignInStateMatch";
      metric_found = true;
      EXPECT_EQ(static_cast<int>(status), *metric);
    }
    EXPECT_TRUE(metric_found) << "No Status.SignInStateMatch was found";
  }

  void ExpectAutoReauthnMetrics(
      std::optional<FedCmMetrics::NumAccounts> expected_returning_accounts,
      bool expected_succeeded,
      bool expected_auto_reauthn_setting_blocked,
      bool expected_auto_reauthn_embargoed,
      bool expected_prevent_silent_access) {
    // UMA checks
    histogram_tester_.ExpectUniqueSample("Blink.FedCm.AutoReauthn.Succeeded",
                                         expected_succeeded, 1);
    if (expected_returning_accounts.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "Blink.FedCm.AutoReauthn.ReturningAccounts",
          static_cast<int>(*expected_returning_accounts), 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "Blink.FedCm.AutoReauthn.ReturningAccounts", 0);
    }
    histogram_tester_.ExpectUniqueSample(
        "Blink.FedCm.AutoReauthn.BlockedByContentSettings",
        expected_auto_reauthn_setting_blocked, 1);
    histogram_tester_.ExpectUniqueSample(
        "Blink.FedCm.AutoReauthn.BlockedByEmbargo",
        expected_auto_reauthn_embargoed, 1);
    histogram_tester_.ExpectTotalCount(
        "Blink.FedCm.AutoReauthn.TimeFromEmbargoWhenBlocked",
        expected_auto_reauthn_embargoed ? 1 : 0);
    histogram_tester_.ExpectUniqueSample(
        "Blink.FedCm.AutoReauthn.BlockedByPreventSilentAccess",
        expected_prevent_silent_access, 1);
    if (expected_succeeded) {
      histogram_tester_.ExpectTotalCount(
          "Blink.FedCm.Timing.AccountsDialogShownDuration2", 0);
    }

    // UKM checks
    auto entries = ukm_recorder()->GetEntriesByName(FedCmEntry::kEntryName);
    ASSERT_FALSE(entries.empty()) << "No FedCM UKM entry was found!";

    bool metric_found = false;
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "AutoReauthn.Succeeded");
      if (!metric) {
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(
            entry, "AutoReauthn.ReturningAccounts"));
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(
            entry, "AutoReauthn.BlockedByContentSettings"));
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(
            entry, "AutoReauthn.BlockedByEmbargo"));
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(
            entry, "AutoReauthn.TimeFromEmbargoWhenBlocked"));
        EXPECT_FALSE(ukm_recorder()->GetEntryMetric(
            entry, "AutoReauthn.BlockedByPreventSilentAccess"));
        continue;
      }
      EXPECT_FALSE(metric_found) << "Found more than one AutoReauthn entry";
      metric_found = true;
      EXPECT_EQ(expected_succeeded, *metric);

      metric = ukm_recorder()->GetEntryMetric(entry,
                                              "AutoReauthn.ReturningAccounts");
      if (expected_returning_accounts) {
        ASSERT_TRUE(metric) << "AutoReauthn.ReturningAccounts was not found";
        EXPECT_EQ(static_cast<int>(*expected_returning_accounts), *metric);
      } else {
        EXPECT_FALSE(metric)
            << "AutoReauthn.ReturningAccounts should not have been recorded";
      }

      metric = ukm_recorder()->GetEntryMetric(
          entry, "AutoReauthn.BlockedByContentSettings");
      ASSERT_TRUE(metric)
          << "AutoReauthn.BlockedByContentSettings was not found";
      EXPECT_EQ(expected_auto_reauthn_setting_blocked, *metric);

      metric =
          ukm_recorder()->GetEntryMetric(entry, "AutoReauthn.BlockedByEmbargo");
      ASSERT_TRUE(metric) << "AutoReauthn.BlockedByEmbargo was not found";
      EXPECT_EQ(expected_auto_reauthn_embargoed, *metric);

      metric = ukm_recorder()->GetEntryMetric(
          entry, "AutoReauthn.TimeFromEmbargoWhenBlocked");
      EXPECT_EQ(expected_auto_reauthn_embargoed, !!metric);

      metric = ukm_recorder()->GetEntryMetric(
          entry, "AutoReauthn.BlockedByPreventSilentAccess");
      ASSERT_TRUE(metric)
          << "AutoReauthn.BlockedByPreventSilentAccess was not found";
      EXPECT_EQ(expected_prevent_silent_access, *metric);
    }
    EXPECT_TRUE(metric_found) << "Did not find AutoReauthn metrics";
    if (expected_succeeded) {
      ExpectNoUKMPresence("Timing.AccountsDialogShownDuration");
    }
    CheckAllFedCmSessionIDs();
  }

  void CheckAllFedCmSessionIDs(size_t expected_num_session_ids = 1u,
                               bool check_request_id_token = false) {
    auto CheckUKMSessionID = [&](const auto& ukm_entries) {
      std::set<int> session_ids;
      std::set<int> session_ids_with_request_id_token;
      ASSERT_FALSE(ukm_entries.empty());
      for (const ukm::mojom::UkmEntry* const entry : ukm_entries) {
        const auto* metric =
            ukm_recorder()->GetEntryMetric(entry, "NumRequestsPerDocument");
        if (metric) {
          continue;
        }
        metric = ukm_recorder()->GetEntryMetric(entry, "FedCmSessionID");
        ASSERT_TRUE(metric)
            << "All UKM events except for NumRequestsPerDocument should have "
               "the SessionID metric";
        int session_id = *metric;
        session_ids.insert(session_id);
        metric = ukm_recorder()->GetEntryMetric(entry, "Status.RequestIdToken");
        if (!metric || !check_request_id_token) {
          continue;
        }
        ASSERT_FALSE(session_ids_with_request_id_token.contains(session_id))
            << "A single session ID should only have one RequestIdToken";
        session_ids_with_request_id_token.insert(session_id);
      }
      EXPECT_EQ(session_ids.size(), expected_num_session_ids);
      if (check_request_id_token) {
        EXPECT_EQ(session_ids_with_request_id_token.size(),
                  expected_num_session_ids);
      }
    };
    CheckUKMSessionID(ukm_recorder()->GetEntriesByName(FedCmEntry::kEntryName));
    CheckUKMSessionID(
        ukm_recorder()->GetEntriesByName(FedCmIdpEntry::kEntryName));
  }

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>
  MaybeAddRegisteredProviders(
      std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>& providers) {
    return federated_auth_request_impl_->MaybeAddRegisteredProviders(providers);
  }

  blink::mojom::IdentityProviderRequestOptionsPtr NewNamedIdP(
      GURL config_url,
      std::string client_id) {
    blink::mojom::IdentityProviderRequestOptionsPtr options =
        blink::mojom::IdentityProviderRequestOptions::New();
    blink::mojom::IdentityProviderConfigPtr config =
        blink::mojom::IdentityProviderConfig::New();
    config->config_url = config_url;
    config->client_id = client_id;
    options->config = std::move(config);
    return options;
  }

  blink::mojom::IdentityProviderRequestOptionsPtr NewRegisteredIdP(
      std::string client_id) {
    blink::mojom::IdentityProviderConfigPtr config =
        blink::mojom::IdentityProviderConfig::New();
    config->use_registered_config_urls = true;
    config->client_id = client_id;
    blink::mojom::IdentityProviderRequestOptionsPtr options =
        blink::mojom::IdentityProviderRequestOptions::New();

    options->config = std::move(config);
    return options;
  }

  blink::mojom::IdentityProviderRequestOptionsPtr NewIDPWithFields(
      const std::optional<std::vector<std::string>>& fields) {
    blink::mojom::IdentityProviderRequestOptionsPtr options =
        blink::mojom::IdentityProviderRequestOptions::New();
    blink::mojom::IdentityProviderConfigPtr config =
        blink::mojom::IdentityProviderConfig::New();
    config->config_url = GURL(kProviderUrlFull);
    config->client_id = "";
    options->config = std::move(config);
    options->fields = fields;
    return options;
  }

  // Helper to call GetDisclosureFields with the desired fields.
  std::vector<Field> GetDisclosureFields(
      const std::vector<std::string>& fields) {
    return federated_auth_request_impl_->GetDisclosureFields(
        *NewIDPWithFields(fields));
  }

  void SimulateLoginToIdP(std::string login_url = kIdpLoginUrl) {
    federated_auth_request_impl_->LoginToIdP(/*can_append_hints=*/true,
                                             GURL(kIdpUrl), GURL(login_url));
  }

  void ExpectSuccessfulActiveFlow() {
    base::test::ScopedFeatureList list;
    list.InitAndEnableFeature(features::kFedCmButtonMode);

    test_permission_delegate_
        ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = false;

    auto dialog_controller =
        std::make_unique<TestDialogController>(kConfigurationValid);
    base::WeakPtr<TestDialogController> weak_dialog_controller =
        dialog_controller->AsWeakPtr();
    SetDialogController(std::move(dialog_controller));

    // Expect a modal dialog to be opened to sign-in to the IdP.
    std::unique_ptr<WebContents> modal(CreateTestWebContents());

    base::RunLoop loop;
    EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
        .WillOnce(::testing::WithArg<0>([&modal, &loop](const GURL& url) {
          loop.Quit();
          return modal.get();
        }));

    RequestParameters parameters = kDefaultRequestParameters;
    parameters.rp_mode = blink::mojom::RpMode::kActive;

    request_remote_.set_disconnect_handler(auth_helper_.quit_closure());

    static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
        ->SimulateUserActivation();

    RunAuthDontWaitForCallback(parameters, kConfigurationValid);

    loop.Run();

    // When the modal dialog is opened, emulate the user signing-in by
    // updating the internal sign-in status state and notifying the
    // observers.
    test_permission_delegate_
        ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;
    federated_auth_request_impl_->OnIdpSigninStatusReceived(
        OriginFromString(kProviderUrlFull), true);

    WaitForCurrentAuthRequest(/*should_fast_forward=*/false);
    CheckAuthExpectations(kConfigurationValid, kExpectationSuccess);

    // These metrics are not recorded when a user's LoginStatus is "logged-out"
    // such that they need to sign in to the IdP in the active flow.
    histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                       0);
    histogram_tester_.ExpectTotalCount(
        "Blink.FedCm.Timing.ShowAccountsDialogBreakdown."
        "WellKnownAndConfigFetch",
        0);
    histogram_tester_.ExpectTotalCount(
        "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.AccountsFetch", 0);
    histogram_tester_.ExpectTotalCount(
        "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch",
        0);
  }

 protected:
  std::string rp_url_;

  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  raw_ptr<FederatedAuthRequestImpl, AcrossTasksDanglingUntriaged>
      federated_auth_request_impl_;

  std::unique_ptr<TestIdpNetworkRequestManager> test_network_request_manager_;

  std::unique_ptr<TestApiPermissionDelegate> test_api_permission_delegate_;
  std::unique_ptr<TestPermissionDelegate> test_permission_delegate_;
  std::unique_ptr<TestAutoReauthnPermissionDelegate>
      test_auto_reauthn_permission_delegate_;
  std::unique_ptr<TestIdentityRegistry> test_identity_registry_;

  AuthRequestCallbackHelper auth_helper_;

  // Enables test to inspect TestDialogController state after
  // FederatedAuthRequestImpl destroys TestDialogController. Recreated during
  // each run of RunAuthTest().
  TestDialogController::State dialog_controller_state_;

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<TestDialogController> custom_dialog_controller_;
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

  // Check that client metadata is fetched. Using `kExpectationSuccess`
  // expectation does not check that the client metadata was fetched because
  // client metadata is optional.
  EXPECT_TRUE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
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
      FederatedAuthRequestResult::kConfigNotInWellKnown,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  const char* idp_config_url =
      kDefaultRequestParameters.identity_providers[0].provider;
  const char* kWellKnownMismatchConfigUrl = "https://mismatch.example";
  EXPECT_NE(std::string(idp_config_url), kWellKnownMismatchConfigUrl);

  MockConfiguration config = kConfigurationValid;
  config.idp_info[idp_config_url].well_known = {
      {kWellKnownMismatchConfigUrl}, {ParseStatus::kSuccess, net::HTTP_OK}};
  RunAuthTest(kDefaultRequestParameters, request_not_in_list, config);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that the well-known file has too many provider urls.
TEST_F(FederatedAuthRequestImplTest, WellKnownHasTooManyProviderUrls) {
  RequestExpectations expectation = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kWellKnownTooBig,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration config = kConfigurationValid;
  config.idp_info[kProviderUrlFull].well_known = {
      {kProviderUrlFull, kProviderTwoUrlFull},
      {ParseStatus::kSuccess, net::HTTP_OK}};
  RunAuthTest(kDefaultRequestParameters, expectation, config);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that the well-known enforcement is bypassed.
TEST_F(FederatedAuthRequestImplTest, WellKnownEnforcementBypassed) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmWithoutWellKnownEnforcement);

  MockConfiguration config = kConfigurationValid;
  // The provider is not in the provider_list from the well-known.
  config.idp_info[kProviderUrlFull].well_known = {
      {kProviderTwoUrlFull}, {ParseStatus::kSuccess, net::HTTP_OK}};
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, config);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that not having the filename in the well-known fails.
TEST_F(FederatedAuthRequestImplTest, WellKnownHasNoFilename) {
  MockConfiguration config{kConfigurationValid};
  config.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{GURL(kProviderUrlFull).GetWithoutFilename().spec()};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kConfigNotInWellKnown,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, config);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that request fails if config is missing token endpoint.
TEST_F(FederatedAuthRequestImplTest, MissingTokenEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.token_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kConfigInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following:\n"
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
      FederatedAuthRequestResult::kConfigInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following:\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Test that request does not fail if config is missing an IDP login URL.
TEST_F(FederatedAuthRequestImplTest, MissingLoginURL) {
  // Login URL is only optional when the signin status API is disabled.
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kFedCmIdpSigninStatusEnabled);

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.idp_login_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
}

// Test that client metadata endpoint is not required in config.
TEST_F(FederatedAuthRequestImplTest, MissingClientMetadataEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.client_metadata_endpoint = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
}

// Test that request fails if the accounts endpoint is in a different origin
// than identity provider.
TEST_F(FederatedAuthRequestImplTest, AccountEndpointDifferentOriginIdp) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint =
      kCrossOriginAccountsEndpoint;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kConfigInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that request fails if IDP login URL is different origin from IDP config
// URL.
TEST_F(FederatedAuthRequestImplTest, LoginUrlDifferentOriginIdp) {
  // We only validate the login_url if IdpSigninStatus is enabled.
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.idp_login_url =
      "https://idp2.example/login_url";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kConfigInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following:\n"
      "\"login_url\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Test that request fails if the idp is not https.
TEST_F(FederatedAuthRequestImplTest, ProviderNotTrustworthy) {
  IdentityProviderParameters identity_provider{
      "http://idp.example/fedcm.json", kClientId, kNonce, /*login_hint=*/"",
      /*domain_hint=*/""};
  RequestParameters request{
      std::vector<IdentityProviderParameters>{identity_provider},
      /*rp_context=*/blink::mojom::RpContext::kSignIn,
      /*rp_mode=*/blink::mojom::RpMode::kPassive};
  MockConfiguration configuration = kConfigurationValid;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdpNotPotentiallyTrustworthy,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(request, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectStatusMetrics(TokenStatus::kIdpNotPotentiallyTrustworthy);
}

// Test that request fails if accounts endpoint cannot be reached.
TEST_F(FederatedAuthRequestImplTest, AccountEndpointCannotBeReached) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kNoResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsNoResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that request fails if account endpoint response cannot be parsed.
TEST_F(FederatedAuthRequestImplTest, AccountsCannotBeParsed) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsSize.Raw", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsSize.ReadyToShow", 0);

  // Only records the following histograms if there are accounts to be shown.
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.WellKnownAndConfigFetch",
      0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.AccountsFetch", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch", 0);
  ExpectNoUKMPresence(
      "Timing.ShowAccountsDialogBreakdown.WellKnownAndConfigFetch");
  ExpectNoUKMPresence("Timing.ShowAccountsDialogBreakdown.AccountsFetch");
  ExpectNoUKMPresence("Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch");
  // Records the following histogram as long as the well-known and config file
  // is fetched.
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.WellKnownAndConfigFetch", 1);
}

// Test that privacy policy, terms of service or RP brand icon URLs are not
// required in client metadata.
TEST_F(FederatedAuthRequestImplTest, ClientMetadataNoUrls) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull].client_metadata.privacy_policy_url =
      "";
  configuration.idp_info[kProviderUrlFull]
      .client_metadata.terms_of_service_url = "";
  configuration.idp_info[kProviderUrlFull].client_metadata.brand_icon_url = "";
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

// Test that RP brand icon URL is not required in client metadata.
TEST_F(FederatedAuthRequestImplTest, ClientMetadataNoRpBrandIconUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull].client_metadata.brand_icon_url = "";
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
      FederatedAuthRequestResult::kConfigInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following:\n"
      "\"id_assertion_endpoint\"\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Tests for Login State
TEST_F(FederatedAuthRequestImplTest, LoginStateShouldBeSignUpForFirstTimeUser) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignUp, all_accounts_for_display()[0]->login_state);
}

TEST_F(FederatedAuthRequestImplTest, LoginStateShouldBeSignInForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::make_optional<base::Time>()));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, all_accounts_for_display()[0]->login_state);

  // CLIENT_METADATA only needs to be fetched for obtaining links to display in
  // the disclosure text. The disclosure text is not displayed for returning
  // users, thus fetching the client metadata endpoint should be skipped.
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.WellKnownAndConfigFetch",
      1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.AccountsFetch", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch", 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch",
      base::TimeDelta(), 1);
}

TEST_F(FederatedAuthRequestImplTest,
       LoginStateSuccessfulSignUpGrantsSharingPermission) {
  EXPECT_CALL(*test_permission_delegate_, GetLastUsedTimestamp(_, _, _, _))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(
      *test_permission_delegate_,
      GrantSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                             OriginFromString(kProviderUrlFull), kAccountId))
      .Times(1);
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

TEST_F(FederatedAuthRequestImplTest,
       LoginStateFailedSignUpNotGrantSharingPermission) {
  EXPECT_CALL(*test_permission_delegate_, GetLastUsedTimestamp(_, _, _, _))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*test_permission_delegate_, GrantSharingPermission(_, _, _, _))
      .Times(0);

  MockConfiguration configuration = kConfigurationValid;
  configuration.token_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
}

// Test that auto re-authn permission is not embargoed upon explicit sign-in.
TEST_F(FederatedAuthRequestImplTest, ExplicitSigninEmbargo) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
  EXPECT_TRUE(
      test_auto_reauthn_permission_delegate_->embargoed_origins_.empty());
}

// Test that auto re-authn permission is embargoed upon successful auto
// re-authn.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnEmbargo) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::make_optional<base::Time>()));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  RequestExpectations expectation = kExpectationSuccess;
  expectation.is_auto_selected = true;
  RunAuthTest(kDefaultRequestParameters, expectation, kConfigurationValid);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);
  EXPECT_TRUE(test_auto_reauthn_permission_delegate_->embargoed_origins_.count(
      OriginFromString(kRpUrl)));

  ExpectAutoReauthnMetrics(FedCmMetrics::NumAccounts::kOne,
                           /*expected_succeeded=*/true,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/false);
}

// Test that auto re-authn with a single account where the account is a
// returning user sets the sign-in mode to auto.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForSingleReturningUserSingleAccount) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::make_optional<base::Time>()));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  RequestExpectations expectation = kExpectationSuccess;
  expectation.is_auto_selected = true;

  RunAuthTest(kDefaultRequestParameters, expectation, kConfigurationValid);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumAccounts::kOne,
                           /*expected_succeeded=*/true,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/false);
}

// Test that auto re-authn with multiple accounts and a single returning user
// sets the sign-in mode to auto.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForSingleReturningUserMultipleAccounts) {
  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderUrlFull), kAccountIdNicolas))
      .WillRepeatedly(Return(std::nullopt));

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdPeter))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdZach))
      .WillRepeatedly(Return(std::nullopt));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;
  RequestExpectations expectation = kExpectationSuccess;
  expectation.is_auto_selected = true;
  RunAuthTest(kDefaultRequestParameters, expectation, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdPeter);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumAccounts::kOne,
                           /*expected_succeeded=*/true,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/false);
}

// Test that auto re-authn with multiple accounts and multiple returning users
// sets the sign-in mode to explicit.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForMultipleReturningUsersMultipleAccounts) {
  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderUrlFull), kAccountIdNicolas))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdPeter))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdZach))
      .WillRepeatedly(Return(std::nullopt));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  std::vector<IdentityRequestAccountPtr> multiple_accounts = kMultipleAccounts;
  multiple_accounts[0]->login_state = LoginState::kSignIn;
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = multiple_accounts;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 3u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 2);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumAccounts::kMultiple,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/false);
}

// Test that auto re-authn with single non-returning account sets the sign-in
// mode to explicit.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnForZeroReturningUsers) {
  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::nullopt));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignUp);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumAccounts::kZero,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/false);
}

// Test that auto re-authn with multiple accounts and a single returning user
// sets the sign-in mode to kExplicit if `mediation: required` is specified.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForSingleReturningUserWithoutSettingAutoReauthn) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  MockConfiguration configuration = kConfigurationValid;
  configuration.mediation_requirement = MediationRequirement::kRequired;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
  ExpectStatusMetrics(TokenStatus::kSuccessUsingTokenInHttpResponse,
                      MediationRequirement::kRequired);
}

// Test that auto re-authn with multiple accounts and a single returning user
// sets the sign-in mode to kExplicit if `RequiresUserMediation` is set
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForSingleReturningUserWithRequiresUserMediation) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend that auto re-authn is not disabled in settings.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));

  // Pretend that auto re-authn is not in embargo state.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  // Pretend that re-authn requires user mediation.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              RequiresUserMediation(url::Origin::Create(GURL(kRpUrl))))
      .WillOnce(Return(true));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumAccounts::kOne,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/true);
}

// Test that auto re-authn with multiple accounts and a single returning user
// sets the sign-in mode to kExplicit if "auto sign-in" is disabled.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnForSingleReturningUserWithAutoSigninDisabled) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend that auto re-authn is not in embargo state.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  // Pretend that re-authn does not require user mediation.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              RequiresUserMediation(url::Origin::Create(GURL(kRpUrl))))
      .WillOnce(Return(false));

  // Pretend that auto re-authn is disabled in settings.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(false));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
}

// Test that if browser has not observed sign-in in the past, the sign-in mode
// is set to explicit regardless the account's login state.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnBrowserNotObservedSigninBefore) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::nullopt));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts[0]->login_state =
      LoginState::kSignIn;

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
}

// Test that if browser has not observed sign-in in the past, but the IdP has
// third-party cookies access, the sign-in mode is set to auto if IdP claims
// that the user is returning.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnBrowserNotObservedSigninButIdpHasThirdPartyCookiesAccess) {
  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::nullopt));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  // Pretend the IdP was given third-party cookies access.
  EXPECT_CALL(*test_api_permission_delegate_,
              HasThirdPartyCookiesAccess(_, GURL(kProviderUrlFull),
                                         OriginFromString(kRpUrl)))
      .WillRepeatedly(Return(true));

  // Sharing permission won't be granted with this setup.
  EXPECT_CALL(*test_permission_delegate_,
              GrantSharingPermission(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderUrlFull), std::string(kAccountId)))
      .Times(0);

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts[0]->login_state =
      LoginState::kSignIn;
  RequestExpectations expectation = kExpectationSuccess;
  expectation.is_auto_selected = true;
  RunAuthTest(kDefaultRequestParameters, expectation, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);
}

// Test that auto re-authn for a first time user sets the sign-in mode to
// explicit.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnForFirstTimeUser) {
  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignUp);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
}

// Test that auto re-authn where the auto re-authn permission is blocked sets
// the sign-in mode to explicit.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnWithBlockedAutoReauthnPermissions) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend the auto re-authn permission has been blocked for this account.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(false));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumAccounts::kOne,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/true,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/false);
}

// Test that auto re-authn where the auto re-authn cooldown is on sets
// the sign-in mode to explicit.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnWithCooldown) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend the auto re-authn permission has been granted for this account.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  // Pretend that auto re-authn is embargoed.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(true));

  RequestExpectations expectations = kExpectationSuccess;
  expectations.standalone_console_message =
      "Auto re-authn was previously triggered less than 10 minutes ago. Only "
      "one auto re-authn request can be made every 10 minutes.";
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumAccounts::kOne,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/true,
                           /*expected_prevent_silent_access=*/false);
}

// Test that no network request is sent if `mediation: silent` is used and user
// has not granted sharing permission in the past.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnMediationSilentFailWithNoSharingPermission) {
  // Pretend the sharing permission has not been granted for any account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(false));

  // Pretend the auto re-authn is disabled in settings.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kSilentMediationFailure,
      /*standalone_console_message=*/
      "Silent mediation issue: the user has not used FedCM on this "
      "site with this identity provider.",
      /*selected_idp_config_url=*/std::nullopt};
  MockConfiguration configuration = kConfigurationValid;
  configuration.mediation_requirement = MediationRequirement::kSilent;

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectStatusMetrics(TokenStatus::kSilentMediationFailure,
                      MediationRequirement::kSilent);

  ExpectAutoReauthnMetrics(/*expected_returning_accounts=*/std::nullopt,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/false);
}

// Test that no network request is sent if `mediation: silent` is used and auto
// re-authn is in cooldown.
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnMediationSilentFailWithEmbargo) {
  // Pretend the sharing permission has been granted for some account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(true));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));

  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(true));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kSilentMediationFailure,
      /*standalone_console_message=*/
      "Silent mediation issue: auto re-authn is in quiet period "
      "because "
      "it was recently used on this site.",
      /*selected_idp_config_url=*/std::nullopt};
  MockConfiguration configuration = kConfigurationValid;
  configuration.mediation_requirement = MediationRequirement::kSilent;

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectStatusMetrics(TokenStatus::kSilentMediationFailure,
                      MediationRequirement::kSilent);

  ExpectAutoReauthnMetrics(/*expected_returning_accounts=*/std::nullopt,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/true,
                           /*expected_prevent_silent_access=*/false);
}

// Test that no network request is sent if `mediation: silent` is used and user
// mediation is required, e.g. `preventSilentAccess` has been invoked
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnMediationSilentFailWithRequiresUserMediation) {
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              RequiresUserMediation(url::Origin::Create(GURL(kRpUrl))))
      .WillOnce(Return(true));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kSilentMediationFailure,
      /*standalone_console_message=*/
      "Silent mediation issue: preventSilentAccess() has been invoked "
      "on the site.",
      /*selected_idp_config_url=*/std::nullopt};
  MockConfiguration configuration = kConfigurationValid;
  configuration.mediation_requirement = MediationRequirement::kSilent;

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectStatusMetrics(TokenStatus::kSilentMediationFailure,
                      MediationRequirement::kSilent);

  ExpectAutoReauthnMetrics(/*expected_returning_accounts=*/std::nullopt,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/true);
}

// Test that no network request is sent if `mediation: silent` is used and user
// has disabled "auto sign-in".
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnMediationSilentFailWithPasswordManagerAutoSigninDisabled) {
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(false));
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              RequiresUserMediation(url::Origin::Create(GURL(kRpUrl))))
      .WillOnce(Return(false));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kSilentMediationFailure,
      /*standalone_console_message=*/
      "Silent mediation issue: the user has disabled auto re-authn.",
      /*selected_idp_config_url=*/std::nullopt};
  MockConfiguration configuration = kConfigurationValid;
  configuration.mediation_requirement = MediationRequirement::kSilent;

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectStatusMetrics(TokenStatus::kSilentMediationFailure,
                      MediationRequirement::kSilent);

  ExpectAutoReauthnMetrics(/*expected_returning_accounts=*/std::nullopt,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/true,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/false);
}

// Test `mediation: silent` could fail silently after fetching accounts
TEST_F(FederatedAuthRequestImplTest,
       AutoReauthnMediationSilentFailWithTwoReturningAccounts) {
  // Pretend the sharing permission has been granted for some account.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(true));

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderUrlFull), kAccountIdNicolas))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdPeter))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend the sharing permission has not been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdZach))
      .WillRepeatedly(Return(std::nullopt));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillRepeatedly(Return(false));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kSilentMediationFailure,
      /*standalone_console_message=*/
      "Silent mediation issue: the user has used FedCM with multiple "
      "accounts on this site.",
      /*selected_idp_config_url=*/std::nullopt};
  MockConfiguration configuration = kConfigurationValid;
  configuration.mediation_requirement = MediationRequirement::kSilent;
  std::vector<IdentityRequestAccountPtr> multiple_accounts = kMultipleAccounts;
  multiple_accounts[0]->login_state = LoginState::kSignIn;
  multiple_accounts[1]->login_state = LoginState::kSignIn;
  configuration.idp_info[kProviderUrlFull].accounts = multiple_accounts;

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));

  ExpectStatusMetrics(TokenStatus::kSilentMediationFailure,
                      MediationRequirement::kSilent);

  ExpectAutoReauthnMetrics(FedCmMetrics::NumAccounts::kMultiple,
                           /*expected_succeeded=*/false,
                           /*expected_auto_reauthn_setting_blocked=*/false,
                           /*expected_auto_reauthn_embargoed=*/false,
                           /*expected_prevent_silent_access=*/false);
}

// Test that `mediation: required` sets the sign-in mode to explicit even though
// other auto re-authn conditions are met.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnMediationRequired) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::nullopt));

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts[0]->login_state =
      LoginState::kSignIn;
  configuration.mediation_requirement = MediationRequirement::kRequired;

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);

  ExpectStatusMetrics(TokenStatus::kSuccessUsingTokenInHttpResponse,
                      MediationRequirement::kRequired);
}

TEST_F(FederatedAuthRequestImplTest, MetricsForSuccessfulSignInCase) {
  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(_, _, OriginFromString(kProviderUrlFull),
                                   kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, all_accounts_for_display()[0]->login_state);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.WellKnownAndConfigFetch",
      1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.AccountsFetch", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 0);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.IsSignInUser", 1, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.Raw", 1, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.ReadyToShow",
                                       1, 1);

  ExpectUKMPresence("Timing.ShowAccountsDialog");
  ExpectUKMPresenceInternal(
      "Timing.ShowAccountsDialogBreakdown.WellKnownAndConfigFetch",
      FedCmEntry::kEntryName);
  ExpectUKMPresenceInternal("Timing.ShowAccountsDialogBreakdown.AccountsFetch",
                            FedCmEntry::kEntryName);
  ExpectUKMPresenceInternal(
      "Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch",
      FedCmEntry::kEntryName);
  ExpectUKMPresence("Timing.ContinueOnDialog");
  ExpectUKMPresence("Timing.IdTokenResponse");
  ExpectUKMPresence("Timing.TurnaroundTime");
  ExpectNoUKMPresence("Timing.CancelOnDialog");
  ExpectUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectNoUKMPresence("Timing.MismatchDialogShownDuration");
  ExpectUkmValue("RpMode", static_cast<int>(RpMode::kPassive));

  ExpectStatusMetrics(TokenStatus::kSuccessUsingTokenInHttpResponse);
  CheckAllFedCmSessionIDs();
}

// Test that request fails if account picker is explicitly dismissed.
TEST_F(FederatedAuthRequestImplTest, MetricsForUIExplicitlyDismissed) {
  base::HistogramTester histogram_tester_;
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;
  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kShouldEmbargo,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));

  ukm_loop.Run();

  ASSERT_TRUE(did_show_accounts_dialog());
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignUp);

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.WellKnownAndConfigFetch",
      1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.AccountsFetch", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 0);

  ExpectUKMPresence("Timing.ShowAccountsDialog");
  ExpectUKMPresence("Timing.CancelOnDialog");
  ExpectNoUKMPresence("Timing.ContinueOnDialog");
  ExpectNoUKMPresence("Timing.IdTokenResponse");
  ExpectNoUKMPresence("Timing.TurnaroundTime");
  ExpectUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectNoUKMPresence("Timing.MismatchDialogShownDuration");

  ExpectStatusMetrics(TokenStatus::kShouldEmbargo);
  CheckAllFedCmSessionIDs();
}

// Test that request is not completed if user ignores the UI.
TEST_F(FederatedAuthRequestImplTest, UIIsIgnored) {
  base::HistogramTester histogram_tester_;

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;

  auto dialog_controller =
      std::make_unique<TestDialogController>(configuration);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  task_environment()->FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(auth_helper_.was_callback_called());

  // The dialog should have been shown. The dialog controller should not be
  // destroyed.
  ASSERT_TRUE(did_show_accounts_dialog());
  EXPECT_TRUE(weak_dialog_controller);

  // Only the time to show the account dialog gets recorded.
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.WellKnownAndConfigFetch",
      1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.AccountsFetch", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Status.RequestIdToken", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 0);
}

TEST_F(FederatedAuthRequestImplTest, MetricsForWebContentsVisible) {
  base::HistogramTester histogram_tester;
  // Sets RenderFrameHost to visible
  test_rvh()->SimulateWasShown();
  ASSERT_EQ(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(_, _, OriginFromString(kProviderUrlFull),
                                   kAccountId))
      .WillRepeatedly(Return(std::make_optional<base::Time>()));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, all_accounts_for_display()[0]->login_state);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 1, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsActive", 1, 1);
}

// Test that request could succeed with auto re-authn even if the web contents
// invisible.
TEST_F(FederatedAuthRequestImplTest, MetricsForWebContentsInvisible) {
  base::HistogramTester histogram_tester;
  test_rvh()->SimulateWasShown();
  ASSERT_EQ(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  // Sets the RenderFrameHost to invisible
  test_rvh()->SimulateWasHidden();
  ASSERT_NE(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(_, _, OriginFromString(kProviderUrlFull),
                                   kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, all_accounts_for_display()[0]->login_state);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 0, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsActive", 1, 1);
}

TEST_F(FederatedAuthRequestImplTest, MetricsForFeatureIsDisabled) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kDisabledInFlags,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  ExpectStatusMetrics(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithUnhandledRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  // Delete the request before DelayTimer kicks in.
  federated_auth_request_impl_->ResetAndDeleteThis();

  // If double counted, these samples would not be unique so the following
  // checks will fail.
  ExpectStatusMetrics(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithAbortedRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  // Abort the request before DelayTimer kicks in.
  federated_auth_request_impl_->CancelTokenRequest();

  // If double counted, these samples would not be unique so the following
  // checks will fail.
  ExpectStatusMetrics(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states match if IDP claims that user is signed in and
// browser also observes that user is signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForSignedInOnBothIdpAndBrowser) {
  // Set browser observes user is signed in.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  std::vector<IdentityRequestAccountPtr> all_accounts_for_display =
      std::vector<IdentityRequestAccountPtr>(kSingleAccount.begin(),
                                             kSingleAccount.end());
  all_accounts_for_display[0]->login_state = LoginState::kSignIn;
  configuration.idp_info[kProviderUrlFull].accounts = all_accounts_for_display;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

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
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::nullopt));

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
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::nullopt));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  std::vector<IdentityRequestAccountPtr> all_accounts_for_display =
      std::vector<IdentityRequestAccountPtr>(kSingleAccount.begin(),
                                             kSingleAccount.end());
  all_accounts_for_display[0]->login_state = LoginState::kSignIn;
  configuration.idp_info[kProviderUrlFull].accounts = all_accounts_for_display;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

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
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

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
      RequestTokenStatus::kError, FederatedAuthRequestResult::kShouldEmbargo,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));
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
      FederatedAuthRequestResult::kDisabledInSettings,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
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
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;
  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  EXPECT_FALSE(auth_helper_.was_callback_called());

  request_remote_->CancelTokenRequest();

  WaitForCurrentAuthRequest();
  FederatedAuthRequestResult result =
      fedcm_disabled ? FederatedAuthRequestResult::kDisabledInFlags
                     : FederatedAuthRequestResult::kCanceled;
  RequestExpectations expectations{RequestTokenStatus::kErrorCanceled, result,
                                   /*standalone_console_message=*/std::nullopt,
                                   /*selected_idp_config_url=*/std::nullopt};
  CheckAuthExpectations(configuration, expectations);
}

namespace {

// TestDialogController which disables FedCM API when FedCM account selection
// dialog is shown.
class DisableApiWhenDialogShownDialogController : public TestDialogController {
 public:
  DisableApiWhenDialogShownDialogController(
      MockConfiguration configuration,
      TestApiPermissionDelegate* api_permission_delegate,
      const url::Origin rp_origin_to_disable)
      : TestDialogController(configuration),
        api_permission_delegate_(api_permission_delegate),
        rp_origin_to_disable_(rp_origin_to_disable) {}
  ~DisableApiWhenDialogShownDialogController() override = default;

  DisableApiWhenDialogShownDialogController(
      const DisableApiWhenDialogShownDialogController&) = delete;
  DisableApiWhenDialogShownDialogController& operator=(
      DisableApiWhenDialogShownDialogController&) = delete;

  bool ShowAccountsDialog(
      const std::string& rp_for_display,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode,
      const std::vector<IdentityRequestAccountPtr>& new_accounts,
      IdentityRequestDialogController::AccountSelectionCallback on_selected,
      IdentityRequestDialogController::LoginToIdPCallback on_add_account,
      IdentityRequestDialogController::DismissCallback dismiss_callback,
      IdentityRequestDialogController::AccountsDisplayedCallback
          accounts_displayed_callback) override {
    // Disable FedCM API
    api_permission_delegate_->permission_override_ = std::make_pair(
        rp_origin_to_disable_, ApiPermissionStatus::BLOCKED_SETTINGS);

    // Call parent class method in order to store callback parameters.
    return TestDialogController::ShowAccountsDialog(
        rp_for_display, idp_list, accounts, sign_in_mode, rp_mode, new_accounts,
        std::move(on_selected), std::move(on_add_account),
        std::move(dismiss_callback), std::move(accounts_displayed_callback));
  }

 private:
  raw_ptr<TestApiPermissionDelegate> api_permission_delegate_;
  url::Origin rp_origin_to_disable_;
};

}  // namespace

// Test that the request fails if user proceeds with the sign in workflow after
// disabling the API while an existing accounts dialog is shown.
TEST_F(FederatedAuthRequestImplTest, ApiDisabledAfterAccountsDialogShown) {
  base::HistogramTester histogram_tester_;

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kDisabledInSettings,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  url::Origin rp_origin_to_disable = main_test_rfh()->GetLastCommittedOrigin();
  SetDialogController(
      std::make_unique<DisableApiWhenDialogShownDialogController>(
          kConfigurationValid, test_api_permission_delegate_.get(),
          rp_origin_to_disable));

  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.WellKnownAndConfigFetch",
      1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.AccountsFetch", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 0);

  ExpectUKMPresence("Timing.ShowAccountsDialog");
  ExpectNoUKMPresence("Timing.ContinueOnDialog");
  ExpectNoUKMPresence("Timing.IdTokenResponse");
  ExpectNoUKMPresence("Timing.TurnaroundTime");
  ExpectUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectNoUKMPresence("Timing.MismatchDialogShownDuration");

  ExpectStatusMetrics(TokenStatus::kDisabledInSettings);
  CheckAllFedCmSessionIDs();
}

// Test the disclosure_text_shown value in the token post data for sign-up case.
TEST_F(FederatedAuthRequestImplTest, DisclosureTextShownForFirstTimeUser) {
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) + "&disclosure_text_shown=true" +
      "&is_auto_selected=false");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test the disclosure_text_shown value in the token post data for returning
// user case.
TEST_F(FederatedAuthRequestImplTest, DisclosureTextNotShownForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) +
      "&disclosure_text_shown=false&is_auto_selected=false");
  SetNetworkRequestManager(std::move(checker));

  MockConfiguration config = kConfigurationValid;
  config.mediation_requirement = MediationRequirement::kRequired;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, config);
}

// Test that the values in the token post data are escaped according to the
// application/x-www-form-urlencoded spec.
TEST_F(FederatedAuthRequestImplTest, TokenEndpointPostDataEscaping) {
  const std::string kAccountIdWithSpace("account id");
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts[0]->id =
      kAccountIdWithSpace;

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData("client_id=" + std::string(kClientId) +
                                    "&nonce=" + std::string(kNonce) +
                                    "&account_id=account+id&disclosure_text_"
                                    "shown=true&is_auto_selected=false");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that the is_auto_selected value in the token post
// data for sign-up case.
TEST_F(FederatedAuthRequestImplTest, AutoSelectedFlagForNewUser) {
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) + "&disclosure_text_shown=true" +
      "&is_auto_selected=false");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that the is_auto_selected value in the token post
// data for returning user with `mediation:required`.
TEST_F(FederatedAuthRequestImplTest,
       AutoSelectedFlagForReturningUserWithMediationRequired) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) +
      "&disclosure_text_shown=false" + "&is_auto_selected=false");
  SetNetworkRequestManager(std::move(checker));

  MockConfiguration config = kConfigurationValid;
  config.mediation_requirement = MediationRequirement::kRequired;

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, config);
}

// Test that the is_auto_selected value in the token post
// data for returning user with `mediation:optional`.
TEST_F(FederatedAuthRequestImplTest,
       AutoSelectedFlagForReturningUserWithMediationOptional) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) +
      "&disclosure_text_shown=false" + "&is_auto_selected=true");
  SetNetworkRequestManager(std::move(checker));

  MockConfiguration config = kConfigurationValid;
  config.mediation_requirement = MediationRequirement::kOptional;
  RequestExpectations expectation = kExpectationSuccess;
  expectation.is_auto_selected = true;

  RunAuthTest(kDefaultRequestParameters, expectation, config);
}

// Test that the is_auto_selected value in the token post
// data for the quiet period use case.
TEST_F(FederatedAuthRequestImplTest, AutoSelectedFlagIfInQuietPeriod) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(Return(std::make_optional<base::Time>()));

  // Pretend the auto re-authn permission has been granted.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));

  // Pretend the auto re-authn is in quiet period.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(true));

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) +
      "&disclosure_text_shown=false&is_auto_selected=false");
  SetNetworkRequestManager(std::move(checker));

  RequestExpectations expectations = kExpectationSuccess;
  expectations.standalone_console_message =
      "Auto re-authn was previously triggered less than 10 minutes ago. Only "
      "one auto re-authn request can be made every 10 minutes.";
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
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
                           int rp_brand_icon_ideal_size,
                           int rp_brand_icon_minimum_size,
                           FetchClientMetadataCallback callback) override {
    // Make copies because running the task might destroy
    // FederatedAuthRequestImpl and invalidate the references.
    GURL client_metadata_endpoint_url_copy = client_metadata_endpoint_url;
    std::string client_id_copy = client_id;

    if (client_metadata_task_)
      std::move(client_metadata_task_).Run();
    TestIdpNetworkRequestManager::FetchClientMetadata(
        client_metadata_endpoint_url_copy, client_id_copy,
        rp_brand_icon_ideal_size, rp_brand_icon_minimum_size,
        std::move(callback));
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
  list.InitWithFeaturesAndParameters(
      GetBasicBackForwardCacheFeatureForTesting(),
      GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  ASSERT_TRUE(IsBackForwardCacheEnabled());

  SetNetworkRequestManager(
      std::make_unique<IdpNetworkRequestManagerClientMetadataTaskRunner>(
          base::BindOnce(&NavigateToUrl, web_contents(), GURL(kRpOtherUrl))));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      // No console message is received, so pass
      // FederatedAuthRequestResult::kSuccess.
      FederatedAuthRequestResult::kSuccess,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsActive", 0, 1);
}

// Test that the account chooser is not shown if the page navigates prior to the
// accounts endpoint request completing and BFCache is disabled.
TEST_F(FederatedAuthRequestImplTest,
       NavigateDuringClientMetadataFetchBFCacheDisabled) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kBackForwardCache);
  ASSERT_FALSE(IsBackForwardCacheEnabled());

  SetNetworkRequestManager(
      std::make_unique<IdpNetworkRequestManagerClientMetadataTaskRunner>(
          base::BindOnce(&NavigateToUrl, web_contents(), GURL(kRpOtherUrl))));

  RequestExpectations expectations = {
      /*return_status=*/std::nullopt,
      // When the RenderFrameHost changes on navigation, no console message is
      // received, so pass FederatedAuthRequestResult::kSuccess.
      main_rfh()->ShouldChangeRenderFrameHostOnSameSiteNavigation()
          ? FederatedAuthRequestResult::kSuccess
          : FederatedAuthRequestResult::kError,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that the accounts are reordered so that accounts with a LoginState equal
// to kSignIn are listed before accounts with a LoginState equal to kSignUp.
TEST_F(FederatedAuthRequestImplTest, ReorderMultipleAccounts) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);

  // Check the account order using the account ids.
  ASSERT_EQ(all_accounts_for_display().size(), 3u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdPeter);
  EXPECT_EQ(all_accounts_for_display()[1]->id, kAccountIdNicolas);
  EXPECT_EQ(all_accounts_for_display()[2]->id, kAccountIdZach);
}

// Test that first API call with a given IDP is not affected by the
// IdpSigninStatus bit.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusTestFirstTimeFetchSuccess) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  EXPECT_CALL(*test_permission_delegate_,
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
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  EXPECT_CALL(*test_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), false))
      .Times(1);
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that a failure UI will be displayed if the accounts fetch is failed but
// the IdpSigninStatus claims that the user is signed in.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusTestShowFailureUi) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  configuration.idp_signin_status_mismatch_dialog_action =
      IdpSigninStatusMismatchDialogAction::kClose;
  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kShouldEmbargo,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that API calls will fail before sending any network request if
// IdpSigninStatus shows that the user is not signed in with the IDP. No failure
// UI is displayed.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusTestApiFailedIfUserNotSignedInWithIdp) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = false;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kNotSignedInWithIdp,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

namespace {

// TestIdpNetworkRequestManager which enables specifying the ParseStatus for
// config and accounts endpoint fetch.
class ParseStatusOverrideIdpNetworkRequestManager
    : public TestIdpNetworkRequestManager {
 public:
  ParseStatus config_parse_status_{ParseStatus::kSuccess};
  ParseStatus accounts_parse_status_{ParseStatus::kSuccess};

  ParseStatusOverrideIdpNetworkRequestManager() = default;
  ~ParseStatusOverrideIdpNetworkRequestManager() override = default;

  ParseStatusOverrideIdpNetworkRequestManager(
      const ParseStatusOverrideIdpNetworkRequestManager&) = delete;
  ParseStatusOverrideIdpNetworkRequestManager& operator=(
      const ParseStatusOverrideIdpNetworkRequestManager&) = delete;

  void FetchConfig(const GURL& provider,
                   blink::mojom::RpMode rp_mode,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    if (config_parse_status_ != ParseStatus::kSuccess) {
      ++num_fetched_[FetchedEndpoint::CONFIG];

      FetchStatus fetch_status{config_parse_status_, net::HTTP_OK};
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), fetch_status,
                                    IdpNetworkRequestManager::Endpoints(),
                                    IdentityProviderMetadata()));
      return;
    }
    TestIdpNetworkRequestManager::FetchConfig(
        provider, rp_mode, idp_brand_icon_ideal_size,
        idp_brand_icon_minimum_size, std::move(callback));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    if (accounts_parse_status_ != ParseStatus::kSuccess) {
      ++num_fetched_[FetchedEndpoint::ACCOUNTS];

      FetchStatus fetch_status{accounts_parse_status_, net::HTTP_OK};
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), fetch_status,
                                    std::vector<IdentityRequestAccountPtr>()));
      return;
    }

    TestIdpNetworkRequestManager::SendAccountsRequest(accounts_url, client_id,
                                                      std::move(callback));
  }
};

}  // namespace

// Test behavior for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) User signs-in
// 3) User selects "Continue" in account chooser dialog.
TEST_F(FederatedAuthRequestImplTest, FailureUiThenSuccessfulSignin) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  // Simulate user signing into IdP by updating the IdP sign-in status and
  // calling the observer.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  network_manager->accounts_parse_status_ = ParseStatus::kSuccess;
  federated_auth_request_impl_->OnIdpSigninStatusReceived(
      kIdpOrigin, /*idp_signin_status=*/true);

  WaitForCurrentAuthRequest();
  CheckAuthExpectations(kConfigurationValid, kExpectationSuccess);

  EXPECT_TRUE(did_show_accounts_dialog());

  // After the IdP sign-in status was updated, the endpoints should have been
  // fetched a 2nd time.
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 2u);

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 1);

  ExpectUKMPresence("AccountsDialogShown");
  ExpectUKMPresence("MismatchDialogShown");
  ExpectUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectUKMPresence("Timing.MismatchDialogShownDuration");
  CheckAllFedCmSessionIDs();
}

// Test behavior for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) User switches tabs
// 3) User signs into IdP in different tab
TEST_F(FederatedAuthRequestImplTest, FailureUiThenSuccessfulSigninButHidden) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  // Simulate the user switching to a different tab.
  test_rvh()->SimulateWasHidden();

  // Simulate user signing into IdP by updating the IdP signin status and
  // calling observer.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  network_manager->accounts_parse_status_ = ParseStatus::kSuccess;
  federated_auth_request_impl_->OnIdpSigninStatusReceived(
      kIdpOrigin, /*idp_signin_status=*/true);

  WaitForCurrentAuthRequest();
  CheckAuthExpectations(kConfigurationValid, kExpectationSuccess);

  // The FedCM dialog should switch to the account picker. The user should
  // see a new dialog when they switch back to the FedCM tab.
  EXPECT_TRUE(did_show_accounts_dialog());

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 1);

  ExpectUKMPresence("AccountsDialogShown");
  ExpectUKMPresence("MismatchDialogShown");
  ExpectUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectUKMPresence("Timing.MismatchDialogShownDuration");
  CheckAllFedCmSessionIDs();
}

// Test behavior for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) In a different tab, user signs into different IdP
TEST_F(FederatedAuthRequestImplTest, FailureUiSigninFromDifferentIdp) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);
  url::Origin kOtherOrigin = OriginFromString("https://idp.other");

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  // Close mismatch dialog upon sign-in status change to check that the
  // appropriate metrics are recorded.
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_signin_status_mismatch_dialog_action =
      IdpSigninStatusMismatchDialogAction::kClose;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  size_t num_well_known_fetches = NumFetched(FetchedEndpoint::WELL_KNOWN);

  // Simulate user signing into different IdP by updating the IdP signin status
  // and calling observer.
  test_permission_delegate_->idp_signin_statuses_[kOtherOrigin] = true;
  federated_auth_request_impl_->OnIdpSigninStatusReceived(
      kOtherOrigin, /*idp_signin_status=*/true);
  base::RunLoop().RunUntilIdle();

  // No fetches should have been triggered.
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), num_well_known_fetches);

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 1);

  ExpectNoUKMPresence("AccountsDialogShown");
  ExpectUKMPresence("MismatchDialogShown");
  ExpectNoUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectUKMPresence("Timing.MismatchDialogShownDuration");
  CheckAllFedCmSessionIDs();
}

// Test that for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) IdP sign-in status is updated
// 3) Accounts endpoint still returns an empty list
// That ShowFailureDialog() is called a 2nd time after the IdP sign-in status
// update.
TEST_F(FederatedAuthRequestImplTest, FailureUiAccountEndpointKeepsFailing) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  MockConfiguration configuration = kConfigurationValid;

  // Setup IdP sign-in status mismatch.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;

  auto dialog_controller =
      std::make_unique<TestDialogController>(configuration);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  // Update IdP sign-in status. Keep accounts endpoint returning empty list.
  // Close mismatch dialog upon sign-in status change to check that the
  // appropriate metrics are recorded.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  weak_dialog_controller->SetIdpSigninStatusMismatchDialogAction(
      IdpSigninStatusMismatchDialogAction::kClose);
  federated_auth_request_impl_->OnIdpSigninStatusReceived(
      kIdpOrigin, /*idp_signin_status=*/true);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, dialog_controller_state_
                    .num_show_idp_signin_status_mismatch_dialog_requests);

  // After the IdP sign-in status was updated, the endpoints should have been
  // fetched a 2nd time.
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 2u);

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 1);

  ExpectNoUKMPresence("AccountsDialogShown");
  ExpectUKMPresence("MismatchDialogShown");
  ExpectNoUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectUKMPresence("Timing.MismatchDialogShownDuration");
  CheckAllFedCmSessionIDs();
}

// Test that for the following sequence of events:
// 1) Failure dialog is shown due to IdP sign-in status mismatch
// 2) IdP sign-in status is updated
// 3) A different endpoint fails during the fetch initiated by the IdP sign-in
// status update.
// That user is shown IdP-sign-in-failure dialog.
TEST_F(FederatedAuthRequestImplTest, FailureUiThenFailDifferentEndpoint) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);

  // Make the fetch triggered by the IdP sign-in status changing fail for a
  // different endpoint.
  network_manager->config_parse_status_ = ParseStatus::kInvalidResponseError;

  // Simulate user signing into IdP by updating the IdP signin status and
  // calling the observer.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  network_manager->accounts_parse_status_ = ParseStatus::kSuccess;
  federated_auth_request_impl_->OnIdpSigninStatusReceived(
      kIdpOrigin, /*idp_signin_status=*/true);

  WaitForCurrentAuthRequest();
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kConfigInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  CheckAuthExpectations(kConfigurationValid, expectations);

  // The user should be shown IdP-sign-in-failure dialog.
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_EQ(1u, dialog_controller_state_
                    .num_show_idp_signin_status_mismatch_dialog_requests);

  // After the IdP sign-in status was updated, the endpoints should have been
  // fetched a 2nd time.
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 1);

  ExpectNoUKMPresence("AccountsDialogShown");
  ExpectUKMPresence("MismatchDialogShown");
  ExpectNoUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectUKMPresence("Timing.MismatchDialogShownDuration");
  CheckAllFedCmSessionIDs();
}

// Test that when IdpSigninStatus API is in the metrics-only mode, that an IDP
// signed-out status stays signed-out regardless of what is returned by the
// accounts endpoint.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusMetricsModeStaysSignedout) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures({}, {features::kFedCmIdpSigninStatusEnabled});

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = false;
  EXPECT_CALL(*test_permission_delegate_, SetIdpSigninStatus(_, _)).Times(0);

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that when IdpSigninStatus API does not have any state for an IDP, that
// the state transitions to sign-in if the accounts endpoint returns a
// non-empty list of accounts.
TEST_F(
    FederatedAuthRequestImplTest,
    IdpSigninStatusMetricsModeUndefinedTransitionsToSignedinWhenHaveAccounts) {
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = std::nullopt;
  EXPECT_CALL(*test_permission_delegate_,
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
  list.InitWithFeatures({}, {features::kFedCmIdpSigninStatusEnabled});

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;
  EXPECT_CALL(*test_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), false));

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsInvalidResponse,
      /*standalone_console_message=*/std::nullopt, std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Tests that multiple IDPs provided results in an error if the
// `kFedCmMultipleIdentityProviders` flag is disabled.
TEST_F(FederatedAuthRequestImplTest, MultiIdpDisabled) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kFedCmMultipleIdentityProviders);

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {},
      /*standalone_console_message=*/std::nullopt,
      std::nullopt};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations,
              kConfigurationMultiIdpValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
}

TEST_F(FederatedAuthRequestImplTest,
       AllSuccessfulMultiIdpRequestWithoutIdpReorder) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set the account from the first IDP as returning as well, so the first
  // selected account should be that one (second IDP also has returning accounts
  // and no reordering should happen).
  MockConfiguration config = kConfigurationMultiIdpValid;
  config.idp_info[kProviderUrlFull].accounts[0]->login_state =
      LoginState::kSignIn;
  RunAuthTest(kDefaultMultiIdpRequestParameters, kExpectationSuccess, config);
  EXPECT_EQ(2u, NumFetched(FetchedEndpoint::ACCOUNTS));

  // Check that the appropriate metrics are recorded upon destruction.
  federated_auth_request_impl_->ResetAndDeleteThis();
  ukm_loop.Run();
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.NumRequestsPerDocument", 1,
                                       1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 1);
  ExpectUKMPresence("Timing.ContinueOnDialog");
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 1);
  ExpectUKMPresence("Timing.IdTokenResponse");

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 1);
  ExpectUKMPresence("Timing.TurnaroundTime");

  ExpectUKMCount("AccountsRequestSent", FedCmEntry::kEntryName, 2);
  ExpectUKMCount("AccountsRequestSent", FedCmIdpEntry::kEntryName, 2);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 2);

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 1);
  ExpectUKMCount("Timing.AccountsDialogShownDuration", FedCmEntry::kEntryName,
                 1);
  ExpectUKMCount("Timing.AccountsDialogShownDuration",
                 FedCmIdpEntry::kEntryName, 2);

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 0);
  ExpectNoUKMPresence("Timing.MismatchDialogShownDuration");
  ExpectNoUKMPresence("Timing.MismatchDialogShown");

  ExpectUKMCount("Timing.ShowAccountsDialog", FedCmEntry::kEntryName, 1);
  ExpectUKMCount("Timing.ShowAccountsDialog", FedCmIdpEntry::kEntryName, 2);
  ExpectUKMPresenceInternal("NumRequestsPerDocument", FedCmEntry::kEntryName);

  ExpectUkmValue("NumIdpsRequested", 2);
  ExpectUkmValue("NumIdpsMismatch", 0);
  CheckAllFedCmSessionIDs();
}

// Test successful multi IDP FedCM request.
TEST_F(FederatedAuthRequestImplTest,
       AllSuccessfulMultiIdpRequestWithIdpReorder) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  RequestExpectations expectations = kExpectationSuccess;
  // Since the first IDP does not set the login state of the account but the
  // second IDP has one with state set to SignIn, selecting the first account
  // means that the second IDP is the one that is selected.
  expectations.selected_idp_config_url = kProviderTwoUrlFull;
  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations,
              kConfigurationMultiIdpValid);
  EXPECT_EQ(2u, NumFetched(FetchedEndpoint::ACCOUNTS));

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  ExpectUKMCount("Timing.ShowAccountsDialog", FedCmEntry::kEntryName, 1);
  ExpectUKMCount("Timing.ShowAccountsDialog", FedCmIdpEntry::kEntryName, 2);

  ExpectUkmValueInEntry(
      "Status.RequestIdToken", FedCmEntry::kEntryName,
      static_cast<int>(TokenStatus::kSuccessUsingTokenInHttpResponse));
  ExpectUkmValueInEntry(
      "Status.RequestIdToken", FedCmIdpEntry::kEntryName,
      static_cast<int>(TokenStatus::kSuccessUsingTokenInHttpResponse),
      /*other_values_allowed=*/true);
  ExpectUkmValueInEntry("Status.RequestIdToken", FedCmIdpEntry::kEntryName,
                        static_cast<int>(TokenStatus::kOtherIdpChosen),
                        /*other_values_allowed=*/true);
  ExpectUkmValue("NumIdpsRequested", 2);
  ExpectUkmValue("NumIdpsMismatch", 0);
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
      FederatedAuthRequestResult::kConfigNotInWellKnown,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/kProviderTwoUrlFull};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::CONFIG), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::TOKEN), 1u);

  ExpectUKMCount("AccountsRequestSent", FedCmEntry::kEntryName, 1);
  ExpectUKMCount("AccountsRequestSent", FedCmIdpEntry::kEntryName, 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 1);
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
      FederatedAuthRequestResult::kConfigNotInWellKnown,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/kProviderUrlFull};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::CONFIG), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::TOKEN), 1u);

  ExpectUKMCount("AccountsRequestSent", FedCmEntry::kEntryName, 1);
  ExpectUKMCount("AccountsRequestSent", FedCmIdpEntry::kEntryName, 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 1);
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
      FederatedAuthRequestResult::kConfigNotInWellKnown,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::CONFIG), 2u);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));

  ExpectNoUKMPresence("Timing.ShowAccountsDialog");
  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 0);
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

  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kError,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  RunAuthTest(request_parameters, expectations, kConfigurationMultiIdpValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that API can succeed with multiple IdPs, if one IdP is signed out but
// the other isn't.
TEST_F(FederatedAuthRequestImplTest, MultiIdpWithOneIdpSignedOut) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = false;

  RequestExpectations expectations = kExpectationSuccess;
  expectations.selected_idp_config_url = kProviderTwoUrlFull;

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations,
              kConfigurationMultiIdpValid);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that API shows all accounts if the user logs in to the IDP with the
// mismatch UI.
TEST_F(FederatedAuthRequestImplTest, MultiIdpLoginToOneIdp) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  url::Origin providerOrigin = OriginFromString(kProviderUrlFull);
  test_permission_delegate_->idp_signin_statuses_[providerOrigin] = true;

  MockConfiguration config = kConfigurationMultiIdpValid;
  // Second IDP has invalid accounts response.
  config.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  config.accounts_dialog_action = AccountsDialogAction::kNone;

  RunAuthDontWaitForCallback(kDefaultMultiIdpRequestParameters, config);

  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 2u);
  EXPECT_TRUE(did_show_accounts_dialog());
  // The second IDP has 3 accounts, so those should be showing up.
  EXPECT_EQ(all_accounts_for_display().size(), 3u);

  // First, simulate the user clicking on the sign in to IDP active.
  SimulateLoginToIdP();
  // Then, simulate user signing into IdP by updating the IdP signin status and
  // calling the observer.
  test_permission_delegate_->idp_signin_statuses_[providerOrigin] = true;
  // Second IDP will now correctly return its account.
  config.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kSuccess;
  SetConfig(config);
  federated_auth_request_impl_->OnIdpSigninStatusReceived(
      providerOrigin, /*idp_signin_status=*/true);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 3u);
  EXPECT_TRUE(did_show_accounts_dialog());
  // The first IDP only has a single account, and the total of accounts is
  // now 4.
  EXPECT_EQ(all_accounts_for_display().size(), 4u);
  EXPECT_EQ(new_accounts().size(), 1u);
}

// Test that API can succeed with multiple IdPs, if all IDPs have login status
// mismatch.
TEST_F(FederatedAuthRequestImplTest, MultiIdpWithAllIdpsMismatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderTwoUrlFull)] = true;

  // Set the config so that both accounts fetches result in failure.
  MockConfiguration config = kConfigurationMultiIdpValid;
  config.idp_info[kProviderUrlFull].accounts_response.parse_status =
      IdpNetworkRequestManager::ParseStatus::kEmptyListError;
  config.idp_info[kProviderTwoUrlFull].accounts_response.parse_status =
      IdpNetworkRequestManager::ParseStatus::kInvalidResponseError;
  // Need to change the accounts dialog action since we won't get any accounts.
  config.accounts_dialog_action = AccountsDialogAction::kClose;

  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kShouldEmbargo,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, config);

  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 2u);
  EXPECT_TRUE(all_accounts_for_display().empty());
  auto mismatch_idps = displayed_mismatch_idps();
  ASSERT_EQ(mismatch_idps.size(), 2u);
  EXPECT_EQ(mismatch_idps[0], "idp.example");
  EXPECT_EQ(mismatch_idps[1], "idp2.example");
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 1);

  ExpectNoUKMPresence("Timing.AccountsDialogShownDuration");

  ExpectUKMCount("Timing.MismatchDialogShownDuration", FedCmEntry::kEntryName,
                 1);
  ExpectUKMCount("Timing.MismatchDialogShownDuration",
                 FedCmIdpEntry::kEntryName, 2);
  CheckAllFedCmSessionIDs();

  ExpectUkmValue("Status.RequestIdToken",
                 static_cast<int>(TokenStatus::kShouldEmbargo));
  ExpectUkmValue("NumIdpsRequested", 2);
  ExpectUkmValue("NumIdpsMismatch", 2);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, MultiIdpWithOneIdpMismatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderTwoUrlFull)] = true;

  // Set the config so that both accounts fetches result in failure.
  MockConfiguration config = kConfigurationMultiIdpValid;
  config.idp_info[kProviderTwoUrlFull].accounts_response.parse_status =
      IdpNetworkRequestManager::ParseStatus::kEmptyListError;

  RunAuthTest(kDefaultMultiIdpRequestParameters, kExpectationSuccess, config);

  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 2u);
  EXPECT_TRUE(!all_accounts_for_display().empty());
  auto mismatch_idps = displayed_mismatch_idps();
  ASSERT_EQ(mismatch_idps.size(), 1u);
  EXPECT_EQ(mismatch_idps[0], "idp2.example");
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 1);
  ExpectUKMCount("Timing.AccountsDialogShownDuration", FedCmEntry::kEntryName,
                 1);
  ExpectUKMCount("Timing.AccountsDialogShownDuration",
                 FedCmIdpEntry::kEntryName, 1);

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 0);
  ExpectUKMCount("Timing.MismatchDialogShownDuration", FedCmEntry::kEntryName,
                 0);
  ExpectUKMCount("Timing.MismatchDialogShownDuration",
                 FedCmIdpEntry::kEntryName, 1);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  ExpectUKMCount("Timing.ShowAccountsDialog", FedCmEntry::kEntryName, 1);
  ExpectUKMCount("Timing.ShowAccountsDialog", FedCmIdpEntry::kEntryName, 1);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsDialogShown", 1);
  ExpectUKMCount("AccountsDialogShown", FedCmEntry::kEntryName, 1);
  ExpectUKMCount("AccountsDialogShown", FedCmIdpEntry::kEntryName, 1);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.MismatchDialogShown",
                                     0);
  ExpectUKMCount("MismatchDialogShown", FedCmEntry::kEntryName, 0);
  ExpectUKMCount("MismatchDialogShown", FedCmIdpEntry::kEntryName, 1);

  ExpectUkmValueInEntry(
      "Status.RequestIdToken", FedCmEntry::kEntryName,
      static_cast<int>(TokenStatus::kSuccessUsingTokenInHttpResponse));
  ExpectUkmValueInEntry(
      "Status.RequestIdToken", FedCmIdpEntry::kEntryName,
      static_cast<int>(TokenStatus::kSuccessUsingTokenInHttpResponse),
      /*other_values_allowed=*/true);
  ExpectUkmValueInEntry("Status.RequestIdToken", FedCmIdpEntry::kEntryName,
                        static_cast<int>(TokenStatus::kOtherIdpChosen),
                        /*other_values_allowed=*/true);
  ExpectUkmValue("NumIdpsRequested", 2);
  ExpectUkmValue("NumIdpsMismatch", 1);
  CheckAllFedCmSessionIDs();
}

// Test that API can succeed with multiple IdPs, if silent mediation is used but
// only one IdP has a returning account.
TEST_F(FederatedAuthRequestImplTest,
       MultiIdpWithSilentMediationAndReturningAccountInSecondIdp) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Pretend the sharing permission has not been granted for any account for the
  // first IdP.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(false));

  // Pretend the sharing permission has been granted for exactly one account for
  // the second IdP.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderTwoUrlFull)))
      .WillOnce(Return(true));
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderTwoUrlFull), kAccountIdPeter))
      .WillRepeatedly(Return(std::make_optional<base::Time>()));
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderTwoUrlFull), kAccountIdNicolas))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderTwoUrlFull), kAccountIdZach))
      .WillRepeatedly(Return(std::nullopt));

  // Ensure auto reauthn is not considered as disabled.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .Times(3)
      .WillRepeatedly(Return(false));

  RequestExpectations expectations = kExpectationSuccess;
  expectations.selected_idp_config_url = kProviderTwoUrlFull;
  expectations.is_auto_selected = true;
  // There will be a console message due to using mediation:silent and having an
  // IdP that does not have a returning account.
  expectations.standalone_console_message =
      "Silent mediation issue: the user has not used FedCM on this site with "
      "this identity provider.";

  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.mediation_requirement = MediationRequirement::kSilent;

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that API fails with multiple IdPs, if silent mediation is used and two
// IdPs have a single returning account.
TEST_F(FederatedAuthRequestImplTest,
       MultiIdpWithSilentMediationAndReturningAccountInTwoIdps) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Pretend the sharing permission has been granted for exactly one account for
  // the first IdP.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  // Pretend the sharing permission has been granted for exactly one account for
  // the second IdP.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderTwoUrlFull)))
      .WillOnce(Return(true));
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderTwoUrlFull), kAccountIdPeter))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderTwoUrlFull), kAccountIdNicolas))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderTwoUrlFull), kAccountIdZach))
      .WillRepeatedly(Return(std::nullopt));

  // Ensure auto reauthn is not considered as disabled.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillRepeatedly(Return(false));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kSilentMediationFailure,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.mediation_requirement = MediationRequirement::kSilent;

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);

  // Accounts still need to be fetched since there could have been a single
  // returning account.
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that when there are two IDPs with sharing permissions but the account
// fetch fails for one of them, mediation silent can still succeed.
TEST_F(FederatedAuthRequestImplTest,
       MultiIdpWithSilentMediationAndOneIdpFetchFailure) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);
  // Mark both IDPs as logged in.
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderTwoUrlFull)] = true;

  // Pretend the sharing permission has been granted for exactly one account for
  // the first IdP.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(true));

  // Pretend the sharing permission has been granted for exactly one account for
  // the second IdP.
  EXPECT_CALL(
      *test_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderTwoUrlFull)))
      .WillOnce(Return(true));
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderTwoUrlFull), kAccountIdPeter))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderTwoUrlFull), kAccountIdZach))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderTwoUrlFull), kAccountIdNicolas))
      .WillRepeatedly(Return(std::nullopt));

  // Ensure auto reauthn is not considered as disabled.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .Times(3)
      .WillRepeatedly(Return(false));

  RequestExpectations expectations = kExpectationSuccess;
  expectations.selected_idp_config_url = kProviderTwoUrlFull;
  expectations.is_auto_selected = true;
  expectations.standalone_console_message =
      "Silent mediation was requested, but the conditions to achieve it were "
      "not met.";

  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.mediation_requirement = MediationRequirement::kSilent;
  // Let the first IDP accounts fetch fail.
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      IdpNetworkRequestManager::ParseStatus::kNoResponseError;

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);

  // Accounts still need to be fetched since there could have been a single
  // returning account.
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

TEST_F(FederatedAuthRequestImplTest, MultiIdpLoggedOut) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Mark both IDPs as logged out so the request fails early.
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = false;
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderTwoUrlFull)] = false;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kNotSignedInWithIdp,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  request_remote_.set_disconnect_handler(auth_helper_.quit_closure());

  RunAuthDontWaitForCallback(kDefaultMultiIdpRequestParameters,
                             kConfigurationMultiIdpValid);
  base::RunLoop().RunUntilIdle();
  // The callback must be delayed.
  EXPECT_FALSE(auth_helper_.was_callback_called());
  WaitForCurrentAuthRequest();
  CheckAuthExpectations(kConfigurationMultiIdpValid, expectations);
}

TEST_F(FederatedAuthRequestImplTest, MultiIdpWithError) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  MockConfiguration configuration = kConfigurationMultiIdpValid;
  ErrorDialogType error_dialog_type =
      ErrorDialogType::kGenericNonEmptyWithoutUrl;
  TokenResponseType token_response_type =
      TokenResponseType::kTokenReceivedAndErrorReceivedAndContinueOnNotReceived;
  configuration.token_response.parse_status =
      ParseStatus::kInvalidResponseError;
  configuration.error_dialog_type = error_dialog_type;
  configuration.token_response_type = token_response_type;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(dialog_controller_state_.did_show_error_dialog);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.ErrorDialogType",
                                       error_dialog_type, 1);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Error.ErrorDialogResult",
      FedCmErrorDialogResult::kCloseWithoutMoreDetails, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.TokenResponseType",
                                       token_response_type, 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Error.ErrorUrlType", 0);

  ExpectUKMPresence("Error.ErrorDialogType");
  ExpectUKMPresence("Error.ErrorDialogResult");
  ExpectUKMPresence("Error.TokenResponseType");
  ExpectNoUKMPresence("Error.ErrorUrlType");
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, TooManyRequests) {
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;
  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());

  // Reset the network request manager so we can check that we fetch no
  // endpoints in the subsequent call.
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  SetNetworkRequestManager(std::make_unique<TestIdpNetworkRequestManager>());
  // The next FedCM request should fail since the initial request has not yet
  // been finalized.
  RequestExpectations expectations = {
      RequestTokenStatus::kErrorTooManyRequests,
      FederatedAuthRequestResult::kTooManyRequests,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  // Check that the appropriate metrics are recorded upon destruction.
  federated_auth_request_impl_->ResetAndDeleteThis();

  ukm_loop.Run();

  // Only count the first request, the second request that resulted in
  // RequestTokenStatus::kErrorTooManyRequests should not be counted.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.NumRequestsPerDocument", 1,
                                       1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.MultipleRequestsRpMode", 0,
                                       1);
  ExpectUkmValue(
      "MultipleRequestsRpMode",
      static_cast<int>(FedCmMultipleRequestsRpMode::kPassiveThenPassive));

  // Check for RP-keyed UKM presence.
  ExpectUKMPresenceInternal("NumRequestsPerDocument", FedCmEntry::kEntryName);
  CheckAllFedCmSessionIDs(2, /*check_request_id_token=*/true);
}

TEST_F(FederatedAuthRequestImplTest,
       ActiveModeTooManyRequestsWithNewPassiveFlow) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RunAuthDontWaitForCallback(parameters, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());

  // Reset the network request manager so we can check that we fetch no
  // endpoints in the subsequent call.
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  SetNetworkRequestManager(std::make_unique<TestIdpNetworkRequestManager>());
  // The next FedCM request should fail since a passive flow cannot replace
  // another active flow.
  RequestExpectations expectations = {
      RequestTokenStatus::kErrorTooManyRequests,
      FederatedAuthRequestResult::kTooManyRequests,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  // Check that the appropriate metrics are recorded upon destruction.
  federated_auth_request_impl_->ResetAndDeleteThis();

  ukm_loop.Run();

  // Only count the first request, the second request that resulted in
  // RequestTokenStatus::kErrorTooManyRequests should not be counted.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.NumRequestsPerDocument", 1,
                                       1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.MultipleRequestsRpMode", 2,
                                       1);
  ExpectUkmValue(
      "MultipleRequestsRpMode",
      static_cast<int>(FedCmMultipleRequestsRpMode::kActiveThenPassive));

  // Check for RP-keyed UKM presence.
  ExpectUKMPresenceInternal("NumRequestsPerDocument", FedCmEntry::kEntryName);
  CheckAllFedCmSessionIDs(2, /*check_request_id_token=*/true);
}

TEST_F(FederatedAuthRequestImplTest,
       ActiveModeTooManyRequestsWithNewActiveFlow) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RunAuthDontWaitForCallback(parameters, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());

  // Reset the network request manager so we can check that we fetch no
  // endpoints in the subsequent call.
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  SetNetworkRequestManager(std::make_unique<TestIdpNetworkRequestManager>());
  // The next FedCM request should fail since a active flow cannot replace
  // another active flow.
  RequestExpectations expectations = {
      RequestTokenStatus::kErrorTooManyRequests,
      FederatedAuthRequestResult::kTooManyRequests,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  // Check that the appropriate metrics are recorded upon destruction.
  federated_auth_request_impl_->ResetAndDeleteThis();

  ukm_loop.Run();

  // Only count the first request, the second request that resulted in
  // RequestTokenStatus::kErrorTooManyRequests should not be counted.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.NumRequestsPerDocument", 1,
                                       1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.MultipleRequestsRpMode", 3,
                                       1);
  ExpectUkmValue(
      "MultipleRequestsRpMode",
      static_cast<int>(FedCmMultipleRequestsRpMode::kActiveThenActive));

  // Check for RP-keyed UKM presence.
  ExpectUKMPresenceInternal("NumRequestsPerDocument", FedCmEntry::kEntryName);
  CheckAllFedCmSessionIDs(2, /*check_request_id_token=*/true);
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

  // Since the first IDP does not set the login state of the account but the
  // second IDP has one with state set to SignIn, selecting the first account
  // means that the second IDP is the one that is selected.
  RequestExpectations expectations = kExpectationSuccess;
  expectations.selected_idp_config_url = kProviderTwoUrlFull;
  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations,
              kConfigurationMultiIdpValid);
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_success(),
              ElementsAre("https://idp2.example/metrics"));
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_failure(),
              ElementsAre(kMetricsEndpoint));
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
      RequestTokenStatus::kError, FederatedAuthRequestResult::kShouldEmbargo,
      /*standalone_console_message=*/std::nullopt,
      /* selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());

  EXPECT_TRUE(
      metrics_recorder->get_metrics_endpoints_notified_success().empty());
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_failure(),
              ElementsAre(kMetricsEndpoint, "https://idp2.example/metrics"));
}

TEST_F(FederatedAuthRequestImplTest, AccountsSortedWithTimestamps) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;
  // First account is kSignUp so the fact that it has a last used timestamp
  // should not affect its relative ordering.
  EXPECT_CALL(*test_permission_delegate_,
              GetLastUsedTimestamp(
                  OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                  OriginFromString(kProviderUrlFull), kAccountIdNicolas))
      .WillRepeatedly(Return(std::make_optional<base::Time>(
          base::Time() + base::Microseconds(10))));
  // The second account is marked signed in but has no last used timestamp.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdPeter))
      .WillRepeatedly(Return(std::nullopt));
  // The third account is marked sign (as is the second), but since it has a
  // timestamp it should show first.
  configuration.idp_info[kProviderUrlFull].accounts[2]->login_state =
      LoginState::kSignIn;
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountIdZach))
      .WillRepeatedly(Return(std::make_optional<base::Time>(
          base::Time() + base::Microseconds(1))));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 3u);
  // Account order should be: accounts[2], accounts[1], accounts[0].
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdZach);
  EXPECT_EQ(all_accounts_for_display()[1]->id, kAccountIdPeter);
  EXPECT_EQ(all_accounts_for_display()[2]->id, kAccountIdNicolas);
}

TEST_F(FederatedAuthRequestImplTest, AccountLabelMultipleAccountsNoMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAuthz);

  RequestParameters parameters = kDefaultRequestParameters;
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsListEmpty,
      {kAccountLabelNoMatchMessage},
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.requested_label =
      "invalid_label";
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHintsAndDomains;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.AccountLabel.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kZero, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.Raw", 3, 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsSize.ReadyToShow", 0);
  ExpectUkmValueInEntry("AccountLabel.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 0);
  ExpectNoUKMPresence("DomainHint.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, AccountLabelMultipleAccountsOneMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAuthz);

  RequestParameters parameters = kDefaultRequestParameters;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.requested_label = "label";
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHintsAndDomains;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdPeter);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.AccountLabel.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kOne, 1);
  ExpectUkmValueInEntry("AccountLabel.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 1);
  ExpectNoUKMPresence("DomainHint.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountIdMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = kAccountId;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kSingleAccountWithHint;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountId);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.LoginHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kOne, 1);
  ExpectUkmValueInEntry("LoginHint.NumMatchingAccounts", FedCmEntry::kEntryName,
                        1);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("DomainHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountEmailMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = kEmail;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kSingleAccountWithHint;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->email, kEmail);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.LoginHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kOne, 1);
  ExpectUkmValueInEntry("LoginHint.NumMatchingAccounts", FedCmEntry::kEntryName,
                        1);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("DomainHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountNoMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = "incorrect_login_hint";
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsListEmpty,
      {kLoginHintNoMatchMessage},
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kSingleAccountWithHint;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.LoginHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kZero, 1);
  ExpectUkmValueInEntry("LoginHint.NumMatchingAccounts", FedCmEntry::kEntryName,
                        0);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("DomainHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, LoginHintFirstAccountMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = kAccountIdNicolas;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHintsAndDomains;

  RunAuthTest(parameters, kExpectationSuccess, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdNicolas);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.LoginHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kOne, 1);
  ExpectUkmValueInEntry("LoginHint.NumMatchingAccounts", FedCmEntry::kEntryName,
                        1);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("DomainHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, LoginHintLastAccountMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = kAccountIdZach;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHintsAndDomains;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdZach);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.LoginHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kOne, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.Raw", 3, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.ReadyToShow",
                                       1, 1);
  ExpectUkmValueInEntry("LoginHint.NumMatchingAccounts", FedCmEntry::kEntryName,
                        1);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("DomainHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, LoginHintMultipleAccountsNoMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = "incorrect_login_hint";
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsListEmpty,
      {kLoginHintNoMatchMessage},
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHintsAndDomains;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.LoginHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kZero, 1);
  ExpectUkmValueInEntry("LoginHint.NumMatchingAccounts", FedCmEntry::kEntryName,
                        0);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("DomainHint.NumMatchingAccounts");
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.Raw", 3, 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsSize.ReadyToShow", 0);
}


TEST_F(FederatedAuthRequestImplTest, DomainHintSingleAccountMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint = kDomainHint;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kSingleAccountWithDomainHint;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountId);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.DomainHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kOne, 1);
  ExpectUkmValueInEntry("DomainHint.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 1);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, DomainHintSingleAccountStarMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint =
      FederatedAuthRequestImpl::kWildcardDomainHint;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kSingleAccountWithDomainHint;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountId);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.DomainHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kOne, 1);
  ExpectUkmValueInEntry("DomainHint.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 1);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, DomainHintSingleAccountStarNoMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint =
      FederatedAuthRequestImpl::kWildcardDomainHint;

  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsListEmpty,
      {kDomainHintNoMatchMessage},
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationValid;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.DomainHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kZero, 1);
  ExpectUkmValueInEntry("DomainHint.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 0);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, DomainHintSingleAccountNoMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint = "incorrect_domain_hint";
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsListEmpty,
      {kDomainHintNoMatchMessage},
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kSingleAccountWithDomainHint;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.DomainHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kZero, 1);
  ExpectUkmValueInEntry("DomainHint.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 0);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.Raw", 1, 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsSize.ReadyToShow", 0);
}

TEST_F(FederatedAuthRequestImplTest, DomainHintNoMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint = kDomainHint;
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsListEmpty,
      {kDomainHintNoMatchMessage},
      /*selected_idp_config_url=*/std::nullopt};

  RunAuthTest(parameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.DomainHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kZero, 1);
  ExpectUkmValueInEntry("DomainHint.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 0);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, DomainHintMultipleAccountsSingleMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint = kOtherDomainHint;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHintsAndDomains;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdZach);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.DomainHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kOne, 1);
  ExpectUkmValueInEntry("DomainHint.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 1);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.Raw", 3, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.ReadyToShow",
                                       1, 1);
}

TEST_F(FederatedAuthRequestImplTest,
       DomainHintMultipleAccountsMultipleMatches) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint = kDomainHint;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHintsAndDomains;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 2u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdNicolas);
  EXPECT_EQ(all_accounts_for_display()[1]->id, kAccountIdZach);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.DomainHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kMultiple, 1);
  ExpectUkmValueInEntry("DomainHint.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 2);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.Raw", 3, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsSize.ReadyToShow",
                                       2, 1);
}

TEST_F(FederatedAuthRequestImplTest, DomainHintMultipleAccountsStar) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint =
      FederatedAuthRequestImpl::kWildcardDomainHint;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHintsAndDomains;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 2u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdNicolas);
  EXPECT_EQ(all_accounts_for_display()[1]->id, kAccountIdZach);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.DomainHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kMultiple, 1);
  ExpectUkmValueInEntry("DomainHint.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 2);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, DomainHintMultipleAccountsNoMatch) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint = "incorrect_domain_hint";
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsListEmpty,
      {kDomainHintNoMatchMessage},
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts =
      kMultipleAccountsWithHintsAndDomains;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.DomainHint.NumMatchingAccounts",
      FedCmMetrics::NumAccounts::kZero, 1);
  ExpectUkmValueInEntry("DomainHint.NumMatchingAccounts",
                        FedCmEntry::kEntryName, 0);
  ExpectNoUKMPresence("AccountLabel.NumMatchingAccounts");
  ExpectNoUKMPresence("LoginHint.NumMatchingAccounts");
}

TEST_F(FederatedAuthRequestImplTest, PictureFetch) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts[0]->picture =
      GURL(kAccountPicture);
  // This ensures we don't fetch client metadata, to test a different codepath.
  configuration.idp_info[kProviderUrlFull].accounts[0]->login_state =
      LoginState::kSignIn;

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountId);
  EXPECT_FALSE(all_accounts_for_display()[0]->decoded_picture.IsEmpty());
  EXPECT_EQ(kAccountPictureSize,
            all_accounts_for_display()[0]->decoded_picture.Width());
  EXPECT_EQ(kAccountPictureSize,
            all_accounts_for_display()[0]->decoded_picture.Height());
}

TEST_F(FederatedAuthRequestImplTest, PictureFetchMultipleAccounts) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;
  configuration.idp_info[kProviderUrlFull].accounts[0]->picture =
      GURL(kAccountPicture);
  configuration.idp_info[kProviderUrlFull].accounts[1]->picture =
      GURL(kAccountPicture);
  configuration.idp_info[kProviderUrlFull].accounts[2]->picture =
      GURL(kAccountPicture404);

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  ASSERT_EQ(all_accounts_for_display().size(), 3u);
  EXPECT_FALSE(all_accounts_for_display()[0]->decoded_picture.IsEmpty());
  EXPECT_EQ(kAccountPictureSize,
            all_accounts_for_display()[0]->decoded_picture.Width());
  EXPECT_EQ(kAccountPictureSize,
            all_accounts_for_display()[0]->decoded_picture.Height());
  EXPECT_FALSE(all_accounts_for_display()[1]->decoded_picture.IsEmpty());
  EXPECT_EQ(kAccountPictureSize,
            all_accounts_for_display()[1]->decoded_picture.Width());
  EXPECT_EQ(kAccountPictureSize,
            all_accounts_for_display()[1]->decoded_picture.Height());
  EXPECT_TRUE(all_accounts_for_display()[2]->decoded_picture.IsEmpty());
}

// Test that when FedCmRpContext flag is enabled and rp_context is specified,
// the FedCM request succeeds with the specified rp_context.
TEST_F(FederatedAuthRequestImplTest, RpContextIsSetToNonDefaultValue) {
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.rp_context = blink::mojom::RpContext::kContinue;
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  EXPECT_EQ(dialog_controller_state_.rp_context,
            blink::mojom::RpContext::kContinue);
}

TEST_F(FederatedAuthRequestImplTest, WellKnownInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull]
      .well_known.fetch_status.parse_status =
      ParseStatus::kInvalidContentTypeError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kWellKnownInvalidContentType,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  ukm_loop.Run();

  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  ExpectStatusMetrics(TokenStatus::kWellKnownInvalidContentType);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, ConfigInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.fetch_status.parse_status =
      ParseStatus::kInvalidContentTypeError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kConfigInvalidContentType,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  ukm_loop.Run();

  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  ExpectStatusMetrics(TokenStatus::kConfigInvalidContentType);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, ClientMetadataInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull]
      .client_metadata.fetch_status.parse_status =
      ParseStatus::kInvalidContentTypeError;

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  // The FedCM flow succeeds even if the client metadata fetch fails.
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  ukm_loop.Run();

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  ExpectStatusMetrics(TokenStatus::kSuccessUsingTokenInHttpResponse);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, AccountsInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidContentTypeError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kAccountsInvalidContentType,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  ukm_loop.Run();

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  ExpectStatusMetrics(TokenStatus::kAccountsInvalidContentType);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, IdTokenInvalidContentType) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.token_response.parse_status =
      ParseStatus::kInvalidContentTypeError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenInvalidContentType,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  ukm_loop.Run();

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  ExpectStatusMetrics(TokenStatus::kIdTokenInvalidContentType);
  CheckAllFedCmSessionIDs();
}

// Test that the implementation ignores the fields parameter when AuthZ is
// disabled.
TEST_F(FederatedAuthRequestImplTest, ScopeGetsIgnoredWhenAuthzIsDisabled) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].fields = {"non_default_field"};

  RunAuthTest(parameters, kExpectationSuccess, kConfigurationValid);

  // We expect the metadata file to be fetched when fields is []
  // but AuthZ is disabled.
  EXPECT_TRUE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
}

// Test successful AuthZ request that returns tokens without opening
// pop-up windows.
TEST_F(FederatedAuthRequestImplTest, SuccessfulAuthZRequestNoPopUpWindow) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAuthz);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].fields = {"non_default_field"};

  RunAuthTest(parameters, kExpectationSuccess, kConfigurationValid);

  // When the authorization is delegated and the feature is enabled
  // we don't fetch the client metadata endpoint (which is used to
  // mediate - but not to delegate - the authorization prompt).
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
  // Ensure that metrics were recorded.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.RpParametersAndScopeState",
                                       FedCmRpParameters::kHasNonDefaultScope,
                                       1);
}

// Test successful AuthZ request that request the opening of pop-up
// windows.
TEST_F(FederatedAuthRequestImplTest, SuccessfulAuthZRequestWithPopUpWindow) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures(
      /*enabled_features=*/{features::kFedCmAuthz,
                            features::kFedCmMetricsEndpoint},
      /*disabled_features=*/{});

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].fields = {"non_default_field"};

  MockConfiguration config = kConfigurationValid;
  // Expect an access token to be produced, rather the typical idtoken.
  config.token = "an-access-token";

  // Set up the network expectations to return a "continue_on" response
  // rather than the typical idtoken response.
  GURL continue_on = GURL(kProviderUrlFull).Resolve("/more-permissions.php");
  config.continue_on = std::move(continue_on);

  std::unique_ptr<IdpNetworkRequestMetricsRecorder> unique_metrics_recorder =
      std::make_unique<IdpNetworkRequestMetricsRecorder>();
  IdpNetworkRequestMetricsRecorder* metrics_recorder =
      unique_metrics_recorder.get();
  SetNetworkRequestManager(std::move(unique_metrics_recorder));

  // Set up the UI dialog controller to show a pop-up window, rather
  // than the typical mediated authorization prompt that generates
  // an idtoken.
  auto dialog_controller =
      std::make_unique<TestDialogController>(kConfigurationValid);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  // When the pop-up window is opened, resolve it immediately by
  // producing an access token.
  std::unique_ptr<WebContents> modal(CreateTestWebContents());
  auto impl = federated_auth_request_impl_;
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>([&modal, &impl](const GURL& url) {
        impl->OnResolve(GURL(kProviderUrlFull), std::nullopt,
                        "an-access-token");
        return modal.get();
      }));

  RunAuthTest(parameters, kExpectationSuccess, config);
  ExpectStatusMetrics(TokenStatus::kSuccessUsingIdentityProviderResolve);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.ContinueOn.PopupWindowStatus",
      FedCmContinueOnPopupStatus::kPopupOpened, 1);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.ContinueOn.PopupWindowResult",
      FedCmContinueOnPopupResult::kTokenReceived, 1);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOn.Response",
                                     1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ContinueOn.TurnaroundTime", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);

  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_success(),
              ElementsAre("https://idp.example/metrics"));
  EXPECT_TRUE(
      metrics_recorder->get_metrics_endpoints_notified_failure().empty());

  // When the authorization is delegated and the feature is enabled
  // we don't fetch the client metadata endpoint (which is used to
  // mediate - but not to delegate - the authorization prompt).
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
}

// Test the continuation popup calling close().
TEST_F(FederatedAuthRequestImplTest, ContinuationPopupCallingClose) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAuthz);

  RequestParameters parameters = kDefaultRequestParameters;

  MockConfiguration config = kConfigurationValid;
  // Expect an access token to be produced, rather the typical idtoken.
  config.token = "an-access-token";

  // Set up the network expectations to return a "continue_on" response
  // rather than the typical idtoken response.
  GURL continue_on = GURL(kProviderUrlFull).Resolve("/more-permissions.php");
  config.continue_on = std::move(continue_on);

  // Set up the UI dialog controller to show a pop-up window, rather
  // than the typical mediated authorization prompt that generates
  // an idtoken.
  auto dialog_controller =
      std::make_unique<TestDialogController>(kConfigurationValid);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  // When the pop-up window is opened, resolve it immediately by
  // producing an access token.
  std::unique_ptr<WebContents> modal(CreateTestWebContents());
  auto impl = federated_auth_request_impl_;
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>([&modal, &impl](const GURL& url) {
        impl->OnClose();
        return modal.get();
      }));

  RequestExpectations error = {RequestTokenStatus::kError,
                               FederatedAuthRequestResult::kError,
                               /*standalone_console_message=*/std::nullopt,
                               /*selected_idp_config_url=*/std::nullopt};

  RunAuthTest(parameters, error, config);
  ExpectStatusMetrics(
      TokenStatus::kContinuationPopupClosedByIdentityProviderClose);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.ContinueOn.PopupWindowStatus",
      FedCmContinueOnPopupStatus::kPopupOpened, 1);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.ContinueOn.PopupWindowResult",
      FedCmContinueOnPopupResult::kClosedByIdentityProviderClose, 1);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOn.Response",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ContinueOn.TurnaroundTime", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);
}

// Test successful AuthZ request that request the opening of pop-up
// windows.
TEST_F(FederatedAuthRequestImplTest,
       FailsLoadingAContinueOnForADifferentOrigin) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAuthz);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].fields = {"non_default_field"};

  MockConfiguration config = kConfigurationValid;

  // Set up the network expectations to return a "continue_on" response
  // rather than the typical idtoken response.
  GURL continue_on =
      GURL("https://another-origin.example").Resolve("/more-permissions.php");
  config.continue_on = std::move(continue_on);

  RequestExpectations error = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenInvalidResponse,
      // TODO(crbug.com/40262526): introduce a more granular error.
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  RunAuthTest(parameters, error, config);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.ContinueOn.PopupWindowStatus",
      FedCmContinueOnPopupStatus::kUrlNotSameOrigin, 1);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.ContinueOn.PopupWindowResult",
      FedCmContinueOnPopupResult::kTokenReceived, 0);

  // We only record timing on success.
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOn.Response",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.ContinueOn.TurnaroundTime", 0);
}

// Test metrics for a request with parameters.
TEST_F(FederatedAuthRequestImplTest, RequestWithParameters) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAuthz);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].params = {{"foo", "bar"}};

  RunAuthTest(parameters, kExpectationSuccess, kConfigurationValid);

  // Ensure that metrics were recorded.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.RpParametersAndScopeState",
                                       FedCmRpParameters::kHasParameters, 1);
}

// Test metrics for a request with parameters and scopes.
TEST_F(FederatedAuthRequestImplTest, RequestWithParametersAndScopes) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAuthz);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].fields = {"non_default_field"};
  parameters.identity_providers[0].params = {{"foo", "bar"}};

  RunAuthTest(parameters, kExpectationSuccess, kConfigurationValid);

  // When the authorization is delegated and the feature is enabled
  // we don't fetch the client metadata endpoint (which is used to
  // mediate - but not to delegate - the authorization prompt).
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
  // Ensure that metrics were recorded.
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.RpParametersAndScopeState",
      FedCmRpParameters::kHasParametersAndNonDefaultScope, 1);
}

// Test successfully signing-in users when they are signed-out on
// active flows.
TEST_F(FederatedAuthRequestImplTest,
       SignInWhenSignedOutOnActiveModeWithUserActivation) {
  ExpectSuccessfulActiveFlow();
}

// Test active flow failure outside of user activation.
TEST_F(FederatedAuthRequestImplTest, ActiveFlowRequiresUserActivation) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = false;

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  request_remote_.set_disconnect_handler(auth_helper_.quit_closure());

  RequestExpectations error = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kMissingTransientUserActivation,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  // Active flow request gets rejected without delay.
  RunAuthDontWaitForCallback(parameters, kConfigurationValid);
  CheckAuthExpectations(kConfigurationValid, error);

  EXPECT_FALSE(DidFetchAnyEndpoint());
}

// Test the active flow request fails without delay if IdP config is wrong.
TEST_F(FederatedAuthRequestImplTest, ActiveFlowWellKnownNotInList) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  RequestExpectations request_not_in_list = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kConfigNotInWellKnown,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  const char* idp_config_url =
      kDefaultRequestParameters.identity_providers[0].provider;
  const char* kWellKnownMismatchConfigUrl = "https://mismatch.example";
  EXPECT_NE(std::string(idp_config_url), kWellKnownMismatchConfigUrl);

  MockConfiguration config = kConfigurationValid;
  config.idp_info[idp_config_url].well_known = {
      {kWellKnownMismatchConfigUrl}, {ParseStatus::kSuccess, net::HTTP_OK}};

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  // Active flow request gets rejected without delay.
  RunAuthDontWaitForCallback(parameters, config);
  CheckAuthExpectations(config, request_not_in_list);

  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

TEST_F(FederatedAuthRequestImplTest, ActiveFlowWithUnknownLoginStatus) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;

  // Check that the LoginStatus is "unknown".
  EXPECT_EQ(test_permission_delegate_->idp_signin_statuses_.count(kIdpOrigin),
            0u);

  auto dialog_controller =
      std::make_unique<TestDialogController>(configuration);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  // Check that the pop-up window is displayed.
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(Return(nullptr));

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RunAuthDontWaitForCallback(parameters, configuration);
}

// Test that active flow can skip the mismatch UI.
TEST_F(FederatedAuthRequestImplTest, ActiveFlowSkipsMismatchUI) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;

  auto dialog_controller =
      std::make_unique<TestDialogController>(configuration);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));
  // Check that the pop-up window is displayed.
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(Return(nullptr));

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;
  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RunAuthDontWaitForCallback(parameters, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that active flow shows the loading dialog.
TEST_F(FederatedAuthRequestImplTest, ActiveFlowShowsLoadingUI) {
  ExpectSuccessfulActiveFlow();
  EXPECT_TRUE(dialog_controller_state_.did_show_loading_dialog);
}

// Test dismissing a active flow through the loading UI.
TEST_F(FederatedAuthRequestImplTest, ActiveFlowDismissLoadingUI) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kError,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.loading_dialog_action = LoadingDialogAction::kClose;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(dialog_controller_state_.did_show_loading_dialog);
}

TEST_F(FederatedAuthRequestImplTest, CloseModalDialogView) {
  // Test that IdentityRegistry is notified when modal dialog view is closed.
  EXPECT_FALSE(test_identity_registry_->notified_);
  federated_auth_request_impl_->CloseModalDialogView();
  EXPECT_TRUE(test_identity_registry_->notified_);
}

TEST_F(FederatedAuthRequestImplTest, GetDisclosureFieldsEmpty) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAuthz);
  // An unknown field is being requested.
  EXPECT_THAT(GetDisclosureFields({"phone"}), ElementsAre());
  // Nothing is requested.
  EXPECT_THAT(GetDisclosureFields({}), ElementsAre());
}

TEST_F(FederatedAuthRequestImplTest, GetDisclosureFields) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAuthz);
  // When no fields are passed, we use the default.
  EXPECT_THAT(federated_auth_request_impl_->GetDisclosureFields(
                  *NewIDPWithFields(std::nullopt)),
              ElementsAre(Field::kName, Field::kEmail, Field::kPicture));
  // When the default fields are explicitly passed, we should mediate them.
  EXPECT_THAT(GetDisclosureFields({"name", "email", "picture"}),
              ElementsAre(Field::kName, Field::kEmail, Field::kPicture));
  // When a superset of the default fields is passed, we should mediate the
  // default fields.
  EXPECT_THAT(
      GetDisclosureFields({"name", "email", "picture", "locale", "phone"}),
      ElementsAre(Field::kName, Field::kEmail, Field::kPicture));
}

TEST_F(FederatedAuthRequestImplTest, GetDisclosureFieldsSubsetOfDefault) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures({features::kFedCmAuthz, features::kFedCmFlexibleFields},
                        {});
  // Subsets of the default fields should work.
  EXPECT_THAT(GetDisclosureFields({"name", "locale"}),
              ElementsAre(Field::kName));
}

TEST_F(FederatedAuthRequestImplTest, GetDisclosureFieldsWithoutFeatureEnabled) {
  // Assert that we always mediate the default fields when the kFedCmAuthz flag
  // is not enabled.
  EXPECT_THAT(GetDisclosureFields({"locale"}),
              ElementsAre(Field::kName, Field::kEmail, Field::kPicture));
}

class FederatedAuthRequestImplNewTabTest : public FederatedAuthRequestImplTest {
 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    InitConstants();
    test_api_permission_delegate_ =
        std::make_unique<TestApiPermissionDelegate>();
    test_permission_delegate_ = std::make_unique<TestPermissionDelegate>();
    test_auto_reauthn_permission_delegate_ =
        std::make_unique<TestAutoReauthnPermissionDelegate>();
    test_identity_registry_ = std::make_unique<TestIdentityRegistry>(
        web_contents(), /*delegate=*/nullptr, GURL(kIdpUrl));

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL("chrome://newtab/"), ui::PAGE_TRANSITION_LINK);

    federated_auth_request_impl_ = &FederatedAuthRequestImpl::CreateForTesting(
        *main_test_rfh(), test_api_permission_delegate_.get(),
        test_auto_reauthn_permission_delegate_.get(),
        test_permission_delegate_.get(), test_identity_registry_.get(),
        request_remote_.BindNewPipeAndPassReceiver());

    std::unique_ptr<TestIdpNetworkRequestManager> network_request_manager =
        std::make_unique<TestIdpNetworkRequestManager>();
    SetNetworkRequestManager(std::move(network_request_manager));
  }
};

TEST_F(FederatedAuthRequestImplNewTabTest, SuccessfulFlow) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

class UserInfoCallbackHelper {
 public:
  UserInfoCallbackHelper() = default;
  ~UserInfoCallbackHelper() = default;

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

  void Complete(
      blink::mojom::RequestUserInfoStatus user_info_status,
      std::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info) {
    CHECK(!was_called_);
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

 private:
  bool was_called_{false};
  base::RunLoop wait_for_callback_loop_;
};

TEST_F(FederatedAuthRequestImplTest, RequestUserInfoFailure) {
  blink::mojom::IdentityProviderConfigPtr config =
      blink::mojom::IdentityProviderConfig::New();
  config->config_url = GURL(kIdpUrl);
  UserInfoCallbackHelper callback_helper;
  // This request will fail right away (not from IDP origin).
  federated_auth_request_impl_->RequestUserInfo(
      std::move(config), base::BindOnce(&UserInfoCallbackHelper::Complete,
                                        base::Unretained(&callback_helper)));
  // This is a regression test and it passes if the test does not crash.
  callback_helper.WaitForCallback();
}

// Tests that when an accounts dialog is shown, the appropriate metrics are
// recorded.
TEST_F(FederatedAuthRequestImplTest, AccountsDialogShownMetric) {
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  ukm_loop.Run();

  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsDialogShown", 1, 1);
  ExpectUKMPresence("AccountsDialogShown");
  ExpectNoUKMPresence("MismatchDialogShown");
  CheckAllFedCmSessionIDs();
}

// Tests that when a mismatch dialog is shown, the appropriate metrics are
// recorded.
TEST_F(FederatedAuthRequestImplTest, MismatchDialogShownMetric) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);
  MockConfiguration configuration = kConfigurationValid;

  // Setup IdP sign-in status mismatch.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);

  ukm_loop.Run();

  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.MismatchDialogShown", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.MismatchDialogType",
      FedCmMetrics::MismatchDialogType::kFirstWithoutHints, 1);
  ExpectUKMPresence("MismatchDialogShown");
  ExpectNoUKMPresence("AccountsDialogShown");
  CheckAllFedCmSessionIDs();
}

// Tests that a mismatch dialog is shown twice.
TEST_F(FederatedAuthRequestImplTest, DoubleMismatchDialog) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);
  MockConfiguration configuration = kConfigurationValid;

  // Setup IdP sign-in status mismatch.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kHttpNotFoundError;

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = "hint";

  RunAuthDontWaitForCallback(parameters, configuration);

  ukm_loop.Run();

  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.MismatchDialogShown", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.MismatchDialogType",
      FedCmMetrics::MismatchDialogType::kFirstWithHints, 1);
  CheckAllFedCmSessionIDs();

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;
  federated_auth_request_impl_->OnIdpSigninStatusReceived(
      OriginFromString(kProviderUrlFull), true);
  base::RunLoop().RunUntilIdle();

  // The additional mismatch should be recorded in the metrics.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.MismatchDialogShown", 1, 2);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.MismatchDialogType", 2);
  histogram_tester_.ExpectBucketCount(
      "Blink.FedCm.MismatchDialogType",
      FedCmMetrics::MismatchDialogType::kRepeatedWithHints, 1);
}

// Tests that when an accounts request is sent, the appropriate metrics are
// recorded.
TEST_F(FederatedAuthRequestImplTest, AccountsRequestSentMetric) {
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  ukm_loop.Run();

  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.AccountsRequestSent", 1, 1);
  ExpectUKMPresence("AccountsRequestSent");
}

// Tests that when an accounts dialog is aborted, the appropriate duration
// metrics are recorded.
TEST_F(FederatedAuthRequestImplTest, AbortedAccountsDialogShownDurationMetric) {
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;
  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  // Abort the request.
  federated_auth_request_impl_->CancelTokenRequest();

  WaitForCurrentAuthRequest();
  RequestExpectations expectations{RequestTokenStatus::kErrorCanceled,
                                   FederatedAuthRequestResult::kCanceled,
                                   /*standalone_console_message=*/std::nullopt,
                                   /*selected_idp_config_url=*/std::nullopt};
  CheckAuthExpectations(configuration, expectations);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 1);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 0);

  ExpectUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectNoUKMPresence("Timing.MismatchDialogShownDuration");
  CheckAllFedCmSessionIDs();
}

// Tests that when a mismatch dialog is aborted, the appropriate duration
// metrics are recorded.
TEST_F(FederatedAuthRequestImplTest, AbortedMismatchDialogShownDurationMetric) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  SetNetworkRequestManager(
      std::make_unique<ParseStatusOverrideIdpNetworkRequestManager>());
  auto* network_manager =
      static_cast<ParseStatusOverrideIdpNetworkRequestManager*>(
          test_network_request_manager_.get());

  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  // Setup IdP sign-in status mismatch.
  network_manager->accounts_parse_status_ = ParseStatus::kInvalidResponseError;
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;

  RunAuthDontWaitForCallback(kDefaultRequestParameters, kConfigurationValid);
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_FALSE(did_show_accounts_dialog());

  // Abort the request.
  federated_auth_request_impl_->CancelTokenRequest();

  RequestExpectations expectations{RequestTokenStatus::kErrorCanceled,
                                   FederatedAuthRequestResult::kCanceled,
                                   /*standalone_console_message=*/std::nullopt,
                                   /*selected_idp_config_url=*/std::nullopt};
  WaitForCurrentAuthRequest();
  CheckAuthExpectations(kConfigurationValid, expectations);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 0);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", 1);

  ExpectNoUKMPresence("Timing.AccountsDialogShownDuration");
  ExpectUKMPresence("Timing.MismatchDialogShownDuration");
  CheckAllFedCmSessionIDs();
}

// Tests that when requests are made to FedCM in succession, the appropriate
// metrics are recorded upon destruction.
TEST_F(FederatedAuthRequestImplTest, RecordNumRequestsPerDocumentMetric) {
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // First auth request.
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;
  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  // Abort the first auth request.
  federated_auth_request_impl_->CancelTokenRequest();

  WaitForCurrentAuthRequest();
  RequestExpectations expectations{RequestTokenStatus::kErrorCanceled,
                                   FederatedAuthRequestResult::kCanceled,
                                   /*standalone_console_message=*/std::nullopt,
                                   /*selected_idp_config_url=*/std::nullopt};
  CheckAuthExpectations(configuration, expectations);

  // Reset test classes for second auth request.
  SetNetworkRequestManager(std::make_unique<TestIdpNetworkRequestManager>());
  auth_helper_.Reset();

  // Second auth request.
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;
  expectations = {RequestTokenStatus::kError,
                  FederatedAuthRequestResult::kShouldEmbargo,
                  /*standalone_console_message=*/std::nullopt,
                  /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());

  // Check that the appropriate metrics are recorded upon destruction.
  federated_auth_request_impl_->ResetAndDeleteThis();

  ukm_loop.Run();

  // Both requests should have been counted.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.NumRequestsPerDocument", 2,
                                       1);

  // Check for RP-keyed UKM presence.
  ExpectUKMPresenceInternal("NumRequestsPerDocument", FedCmEntry::kEntryName);
  CheckAllFedCmSessionIDs(2, /*check_request_id_token=*/true);
}

// Test that an error dialog is shown when the token response is invalid.
TEST_F(FederatedAuthRequestImplTest, InvalidResponseErrorDialogShown) {
  MockConfiguration configuration = kConfigurationValid;
  ErrorDialogType error_dialog_type = ErrorDialogType::kGenericEmptyWithoutUrl;
  TokenResponseType token_response_type = TokenResponseType::
      kTokenNotReceivedAndErrorNotReceivedAndContinueOnNotReceived;
  configuration.token_response.parse_status =
      ParseStatus::kInvalidResponseError;
  configuration.error_dialog_type = error_dialog_type;
  configuration.token_response_type = token_response_type;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenInvalidResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(dialog_controller_state_.did_show_error_dialog);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.ErrorDialogType",
                                       error_dialog_type, 1);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Error.ErrorDialogResult",
      FedCmErrorDialogResult::kCloseWithoutMoreDetails, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.TokenResponseType",
                                       token_response_type, 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Error.ErrorUrlType", 0);

  ExpectUKMPresence("Error.ErrorDialogType");
  ExpectUKMPresence("Error.ErrorDialogResult");
  ExpectUKMPresence("Error.TokenResponseType");
  ExpectNoUKMPresence("Error.ErrorUrlType");
  CheckAllFedCmSessionIDs();
}

// Test that an error dialog is shown when the token response is missing.
TEST_F(FederatedAuthRequestImplTest, NoResponseErrorDialogShown) {
  MockConfiguration configuration = kConfigurationValid;
  ErrorDialogType error_dialog_type = ErrorDialogType::kGenericEmptyWithoutUrl;
  TokenResponseType token_response_type = TokenResponseType::
      kTokenNotReceivedAndErrorNotReceivedAndContinueOnNotReceived;
  configuration.token_response.parse_status = ParseStatus::kNoResponseError;
  configuration.error_dialog_type = error_dialog_type;
  configuration.token_response_type = token_response_type;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenNoResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(dialog_controller_state_.did_show_error_dialog);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.ErrorDialogType",
                                       error_dialog_type, 1);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Error.ErrorDialogResult",
      FedCmErrorDialogResult::kCloseWithoutMoreDetails, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.TokenResponseType",
                                       token_response_type, 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Error.ErrorUrlType", 0);

  ExpectUKMPresence("Error.ErrorDialogType");
  ExpectUKMPresence("Error.ErrorDialogResult");
  ExpectUKMPresence("Error.TokenResponseType");
  ExpectNoUKMPresence("Error.ErrorUrlType");
  CheckAllFedCmSessionIDs();
}

// Test that the error UI has proper url set.
TEST_F(FederatedAuthRequestImplTest, ErrorUrlDisplayedWithProperUrl) {
  MockConfiguration configuration = kConfigurationValid;
  ErrorDialogType error_dialog_type = ErrorDialogType::kGenericEmptyWithUrl;
  TokenResponseType token_response_type = TokenResponseType::
      kTokenNotReceivedAndErrorNotReceivedAndContinueOnNotReceived;
  ErrorUrlType error_url_type = ErrorUrlType::kCrossOriginSameSite;
  configuration.token_error =
      TokenError(/*code=*/"", GURL("https://foo.idp.example/error"));
  configuration.error_dialog_type = error_dialog_type;
  configuration.token_response_type = token_response_type;
  configuration.error_url_type = error_url_type;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenIdpErrorResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(dialog_controller_state_.did_show_error_dialog);
  EXPECT_EQ(dialog_controller_state_.token_error->url,
            GURL("https://foo.idp.example/error"));

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.ErrorDialogType",
                                       error_dialog_type, 1);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Error.ErrorDialogResult",
      FedCmErrorDialogResult::kCloseWithMoreDetails, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.TokenResponseType",
                                       token_response_type, 1);
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.ErrorUrlType",
                                       error_url_type, 1);

  ExpectUKMPresence("Error.ErrorDialogType");
  ExpectUKMPresence("Error.ErrorDialogResult");
  ExpectUKMPresence("Error.TokenResponseType");
  ExpectUKMPresenceInternal("Error.ErrorUrlType", FedCmIdpEntry::kEntryName);
  CheckAllFedCmSessionIDs();
}

// Test that permission is embargoed upon closing a mismatch dialog.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusCloseMismatchEmbargo) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  configuration.idp_signin_status_mismatch_dialog_action =
      IdpSigninStatusMismatchDialogAction::kClose;
  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kShouldEmbargo,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.count(
      main_test_rfh()->GetLastCommittedOrigin()));
}

// Test that permission is not embargoed upon closing an IDP sign-in flow
// pop-up.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusClosePopupEmbargo) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdpSigninStatusEnabled);

  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  configuration.idp_signin_status_mismatch_dialog_action =
      IdpSigninStatusMismatchDialogAction::kClosePopup;
  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kError,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.empty());
}

// Test that no registered IdP is added without a registry requested.
TEST_F(FederatedAuthRequestImplTest, MaybeAddRegisteredProvidersEmptyList) {
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> result =
      MaybeAddRegisteredProviders(providers);

  EXPECT_TRUE(result.empty());
}

// Test that no registered IdP with only named providers requested.
TEST_F(FederatedAuthRequestImplTest, MaybeAddRegisteredProvidersNamed) {
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;
  providers.emplace_back(NewNamedIdP(GURL("https://idp.example"), kClientId));

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> result =
      MaybeAddRegisteredProviders(providers);

  // Expects the vector to be the same.
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> expected;
  expected.emplace_back(NewNamedIdP(GURL("https://idp.example"), kClientId));

  EXPECT_EQ(expected, result);
}

// Test that a registered provider is added.
TEST_F(FederatedAuthRequestImplTest, MaybeAddRegisteredProvidersAdded) {
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;
  providers.emplace_back(NewRegisteredIdP(kClientId));

  std::vector<GURL> registry;
  registry.emplace_back("https://idp.example");

  EXPECT_CALL(*test_permission_delegate_, GetRegisteredIdPs())
      .WillOnce(Return(registry));

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> result =
      MaybeAddRegisteredProviders(providers);

  // Expects that the registered IdP gets replaced by a named IdP.
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> expected;
  expected.emplace_back(NewNamedIdP(GURL("https://idp.example"), kClientId));

  EXPECT_EQ(expected, result);
}

// Test that all registered IdPs are expanded.
TEST_F(FederatedAuthRequestImplTest,
       MaybeAddRegisteredProvidersAllRequestsForRegisteredIdPsAreExpanded) {
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;
  providers.emplace_back(NewRegisteredIdP(kClientId));
  providers.emplace_back(NewRegisteredIdP(kClientId));

  std::vector<GURL> registry;
  registry.emplace_back("https://idp.example");

  EXPECT_CALL(*test_permission_delegate_, GetRegisteredIdPs())
      .WillOnce(Return(registry));

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> result =
      MaybeAddRegisteredProviders(providers);

  // Expects that the registered IdP gets replaced by a named IdP.
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> expected;
  expected.emplace_back(NewNamedIdP(GURL("https://idp.example"), kClientId));
  expected.emplace_back(NewNamedIdP(GURL("https://idp.example"), kClientId));

  EXPECT_EQ(expected, result);
}

// Test that the registry can add two idps.
TEST_F(FederatedAuthRequestImplTest,
       MaybeAddRegisteredProvidersTwoRegisteredIdPs) {
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;
  providers.emplace_back(NewRegisteredIdP(kClientId));

  std::vector<GURL> registry;
  registry.emplace_back("https://idp1.example");
  registry.emplace_back("https://idp2.example");

  EXPECT_CALL(*test_permission_delegate_, GetRegisteredIdPs())
      .WillOnce(Return(registry));

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> result =
      MaybeAddRegisteredProviders(providers);

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> expected;
  expected.emplace_back(NewNamedIdP(GURL("https://idp2.example"), kClientId));
  expected.emplace_back(NewNamedIdP(GURL("https://idp1.example"), kClientId));

  EXPECT_EQ(expected, result);
}

// Test that registered idps are inserted inline.
TEST_F(FederatedAuthRequestImplTest,
       MaybeAddRegisteredProvidersInsertedInline) {
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;
  providers.emplace_back(NewNamedIdP(GURL("https://idp1.example"), kClientId));
  providers.emplace_back(NewRegisteredIdP(kClientId));
  providers.emplace_back(NewNamedIdP(GURL("https://idp2.example"), kClientId));

  std::vector<GURL> registry;
  registry.emplace_back("https://idp-registered1.example");
  registry.emplace_back("https://idp-registered2.example");

  EXPECT_CALL(*test_permission_delegate_, GetRegisteredIdPs())
      .WillOnce(Return(registry));

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> result =
      MaybeAddRegisteredProviders(providers);

  // Expects that the registered IdP gets replaced by a named IdP.
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> expected;
  expected.emplace_back(NewNamedIdP(GURL("https://idp1.example"), kClientId));
  expected.emplace_back(
      NewNamedIdP(GURL("https://idp-registered2.example"), kClientId));
  expected.emplace_back(
      NewNamedIdP(GURL("https://idp-registered1.example"), kClientId));
  expected.emplace_back(NewNamedIdP(GURL("https://idp2.example"), kClientId));

  EXPECT_EQ(expected, result);
}

// Test that error dialog type metrics are recorded.
TEST_F(FederatedAuthRequestImplTest, ErrorDialogTypeMetrics) {
  MockConfiguration configuration = kConfigurationValid;
  ErrorDialogType error_dialog_type = ErrorDialogType::kInvalidRequestWithUrl;
  configuration.token_error = TokenError(/*code=*/"invalid_request",
                                         GURL("https://foo.idp.example/error"));
  configuration.error_dialog_type = error_dialog_type;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenIdpErrorResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(dialog_controller_state_.did_show_error_dialog);

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.ErrorDialogType",
                                       error_dialog_type, 1);

  ExpectUKMPresence("Error.ErrorDialogType");
  CheckAllFedCmSessionIDs();
}

// Test that error dialog result metrics are recorded.
TEST_F(FederatedAuthRequestImplTest, ErrorDialogResultMetrics) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.token_error =
      TokenError(/*code=*/"", GURL("https://foo.idp.example/error"));
  configuration.error_dialog_action = ErrorDialogAction::kGotIt;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenIdpErrorResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(dialog_controller_state_.did_show_error_dialog);

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Error.ErrorDialogResult",
      FedCmErrorDialogResult::kGotItWithMoreDetails, 1);

  ExpectUKMPresence("Error.ErrorDialogResult");
  CheckAllFedCmSessionIDs();
}

// Test that token response type metrics are recorded.
TEST_F(FederatedAuthRequestImplTest, TokenResponseTypeMetrics) {
  MockConfiguration configuration = kConfigurationValid;
  TokenResponseType token_response_type = TokenResponseType::
      kTokenNotReceivedAndErrorReceivedAndContinueOnNotReceived;
  configuration.token_error = TokenError(/*code=*/"invalid_request",
                                         GURL("https://foo.idp.example/error"));
  configuration.token_response_type = token_response_type;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenIdpErrorResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(dialog_controller_state_.did_show_error_dialog);

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.TokenResponseType",
                                       token_response_type, 1);

  ExpectUKMPresence("Error.TokenResponseType");
  CheckAllFedCmSessionIDs();
}

// Test that error url type metrics are recorded.
TEST_F(FederatedAuthRequestImplTest, ErrorUrlTypeMetrics) {
  MockConfiguration configuration = kConfigurationValid;
  ErrorUrlType error_url_type = ErrorUrlType::kCrossOriginSameSite;
  configuration.token_error = TokenError(/*code=*/"invalid_request",
                                         GURL("https://foo.idp.example/error"));
  configuration.error_url_type = error_url_type;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenIdpErrorResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(dialog_controller_state_.did_show_error_dialog);

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Error.ErrorUrlType",
                                       error_url_type, 1);

  ExpectUKMPresenceInternal("Error.ErrorUrlType", FedCmIdpEntry::kEntryName);
  CheckAllFedCmSessionIDs();
}

// Test that cross-site URL fails the request with the appropriate devtools
// issue.
TEST_F(FederatedAuthRequestImplTest, CrossSiteErrorDialogDevtoolsIssue) {
  MockConfiguration configuration = kConfigurationValid;
  ErrorUrlType error_url_type = ErrorUrlType::kCrossSite;
  configuration.token_error = TokenError(
      /*code=*/"invalid_request", GURL("https://cross-site.example/error"));
  configuration.error_url_type = error_url_type;

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kIdTokenCrossSiteIdpErrorResponse,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(dialog_controller_state_.did_show_error_dialog);
}

// Test that the account UI is not displayed if FedCM is disabled after accounts
// fetch.
TEST_F(FederatedAuthRequestImplTest,
       AccountUiNotDisplayedIfFedCmDisabledAfterAccountsFetch) {
  test_api_permission_delegate_->permission_override_for_nth_ = std::make_pair(
      /*override the nth invocation=*/2,
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_EMBARGO));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      FederatedAuthRequestResult::kDisabledInSettings,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

TEST_F(FederatedAuthRequestImplTest, DomainHintInLoginUrl) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint = kDomainHint;

  // Need to have sign in status set to signed in to prompt login url
  // computations.
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;

  auto dialog_controller =
      std::make_unique<TestDialogController>(kConfigurationValid);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  std::unique_ptr<WebContents> modal(CreateTestWebContents());
  GURL login_url;
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>([&modal, &login_url](const GURL& url) {
        login_url = url;
        return modal.get();
      }));

  RunAuthDontWaitForCallback(parameters, kConfigurationValid);
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());

  SimulateLoginToIdP();

  std::string expected_url = kIdpLoginUrl;
  expected_url += "?domain_hint=domain%40corp.com";
  EXPECT_EQ(login_url, expected_url);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintInLoginUrl) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint = "hint";

  // Need to have sign in status set to signed in to prompt login url
  // computations.
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;

  auto dialog_controller =
      std::make_unique<TestDialogController>(kConfigurationValid);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  std::unique_ptr<WebContents> modal(CreateTestWebContents());
  GURL login_url;
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>([&modal, &login_url](const GURL& url) {
        login_url = url;
        return modal.get();
      }));

  RunAuthDontWaitForCallback(parameters, kConfigurationValid);
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());

  SimulateLoginToIdP();

  std::string expected_url = kIdpLoginUrl;
  expected_url += "?login_hint=hint";
  EXPECT_EQ(login_url, expected_url);
}

TEST_F(FederatedAuthRequestImplTest, DomainHintAndLoginHintInLoginUrl) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint = kDomainHint;
  parameters.identity_providers[0].login_hint = "hint";

  // Need to have sign in status set to signed in to prompt login url
  // computations.
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;

  auto dialog_controller =
      std::make_unique<TestDialogController>(kConfigurationValid);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  std::unique_ptr<WebContents> modal(CreateTestWebContents());
  GURL login_url;
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>([&modal, &login_url](const GURL& url) {
        login_url = url;
        return modal.get();
      }));

  RunAuthDontWaitForCallback(parameters, kConfigurationValid);
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());

  SimulateLoginToIdP();

  std::string expected_url = kIdpLoginUrl;
  expected_url += "?login_hint=hint&domain_hint=domain%40corp.com";
  EXPECT_EQ(login_url, expected_url);
}

TEST_F(FederatedAuthRequestImplTest,
       DomainHintAndLoginHintInLoginUrlWithQuery) {
  RequestParameters parameters = kDefaultRequestParameters;
  // Testing that spaces are transformed in the url.
  parameters.identity_providers[0].domain_hint = "domain hint";
  parameters.identity_providers[0].login_hint = "login hint";

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.idp_login_url += "?q=1&t=2";

  // Need to have sign in status set to signed in to prompt login url
  // computations.
  test_permission_delegate_
      ->idp_signin_statuses_[OriginFromString(kProviderUrlFull)] = true;

  auto dialog_controller =
      std::make_unique<TestDialogController>(kConfigurationValid);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  std::unique_ptr<WebContents> modal(CreateTestWebContents());
  GURL login_url;
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>([&modal, &login_url](const GURL& url) {
        login_url = url;
        return modal.get();
      }));

  RunAuthDontWaitForCallback(parameters, configuration);
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());

  SimulateLoginToIdP(
      configuration.idp_info[kProviderUrlFull].config.idp_login_url);

  std::string expected_url = kIdpLoginUrl;
  expected_url += "?q=1&t=2&login_hint=login%20hint&domain_hint=domain%20hint";
  EXPECT_EQ(login_url, expected_url);
}

TEST_F(FederatedAuthRequestImplTest, DomainHintAddAccount) {
  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].domain_hint = kDomainHint;

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kAddAccount;
  configuration.idp_info[kProviderUrlFull].accounts =
      kSingleAccountWithDomainHint;

  auto dialog_controller =
      std::make_unique<TestDialogController>(configuration);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  std::unique_ptr<WebContents> modal(CreateTestWebContents());
  GURL login_url;
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>([&modal, &login_url](const GURL& url) {
        login_url = url;
        return modal.get();
      }));

  RunAuthDontWaitForCallback(parameters, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());

  // The `login_url` used when invoking AddAccounts should not include hints.
  EXPECT_EQ(login_url, kIdpLoginUrl);
}

// Test that auto re-authn works in active mode.
TEST_F(FederatedAuthRequestImplTest, AutoReauthnInActiveMode) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  // The following checks work in active mode.
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnEmbargoed(OriginFromString(kRpUrl)))
      .WillOnce(Return(false));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              RequiresUserMediation(url::Origin::Create(GURL(kRpUrl))))
      .WillOnce(Return(false));
  EXPECT_CALL(*test_auto_reauthn_permission_delegate_,
              IsAutoReauthnSettingEnabled())
      .WillOnce(Return(true));

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  RunAuthDontWaitForCallback(parameters, kConfigurationValid);

  ASSERT_EQ(all_accounts_for_display().size(), 1u);
  EXPECT_EQ(all_accounts_for_display()[0]->browser_trusted_login_state,
            LoginState::kSignIn);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);
}

// Test that IdP claimed SignUp takes precedence over browser observed SignIn.
TEST_F(FederatedAuthRequestImplTest,
       IdPClaimedSignUpTakesPrecedenceOverBrowserObservedSignIn) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  // Pretend the sharing permission has been granted for all accounts.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), _))
      .WillRepeatedly(
          Return(std::make_optional<base::Time>(base::Time::Now())));

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;

  RunAuthDontWaitForCallback(parameters, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 3u);
  // Accounts are reordered to have sign-in users displayed first.
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignIn);
  EXPECT_EQ(all_accounts_for_display()[0]->browser_trusted_login_state,
            LoginState::kSignIn);
  EXPECT_EQ(all_accounts_for_display()[1]->login_state, LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[1]->browser_trusted_login_state,
            LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[2]->login_state, LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[2]->browser_trusted_login_state,
            LoginState::kSignUp);
}

// Test that IdP claimed SignIn does not affect browser observed SignUp.
TEST_F(FederatedAuthRequestImplTest,
       IdPClaimedSignInDoesNotAffectBrowserObservedSignUp) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  // Pretend the sharing permission has NOT been granted for any account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), _))
      .WillRepeatedly(Return(std::nullopt));

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;

  RunAuthDontWaitForCallback(parameters, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 3u);
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignIn);
  // This should be kSignUp regardless of IdP's claim.
  EXPECT_EQ(all_accounts_for_display()[0]->browser_trusted_login_state,
            LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[1]->login_state, LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[1]->browser_trusted_login_state,
            LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[2]->login_state, LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[2]->browser_trusted_login_state,
            LoginState::kSignUp);
}

// Test that IdP claimed SignIn can affect browser observed SignUp if they have
// third-party cookies access.
TEST_F(FederatedAuthRequestImplTest,
       IdPClaimedSignInAffectsBrowserObservedSignUpWith3PCAccess) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  // Pretend the sharing permission has NOT been granted for any account.
  EXPECT_CALL(
      *test_permission_delegate_,
      GetLastUsedTimestamp(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), _))
      .WillRepeatedly(Return(std::nullopt));

  // Pretend the IdP was given third-party cookies access.
  EXPECT_CALL(*test_api_permission_delegate_,
              HasThirdPartyCookiesAccess(_, GURL(kProviderUrlFull),
                                         OriginFromString(kRpUrl)))
      .WillRepeatedly(Return(true));

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;

  RunAuthDontWaitForCallback(parameters, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 3u);
  EXPECT_EQ(all_accounts_for_display()[0]->login_state, LoginState::kSignIn);
  // This should be kSignIn to match IdP's claim due to it has 3PC access.
  EXPECT_EQ(all_accounts_for_display()[0]->browser_trusted_login_state,
            LoginState::kSignIn);
  EXPECT_EQ(all_accounts_for_display()[1]->login_state, LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[1]->browser_trusted_login_state,
            LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[2]->login_state, LoginState::kSignUp);
  EXPECT_EQ(all_accounts_for_display()[2]->browser_trusted_login_state,
            LoginState::kSignUp);
}

// Test active flow is exempted if the FedCM is disabled in  settings.
TEST_F(FederatedAuthRequestImplTest, ActiveFlowNotAffectedBySettings) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_SETTINGS);
  ExpectSuccessfulActiveFlow();
}

// Test active flow is exempted if the FedCM is embargoed in the passive flow.
TEST_F(FederatedAuthRequestImplTest, ActiveFlowNotAffectedByEmbargo) {
  test_api_permission_delegate_->RecordDismissAndEmbargo(
      OriginFromString(kRpUrl));
  ExpectSuccessfulActiveFlow();
}

// Test dismissing UI in active flow does not trigger embargo.
TEST_F(FederatedAuthRequestImplTest, ActiveFlowNotAffectEmbargo) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmButtonMode);

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  static_cast<TestRenderFrameHost*>(web_contents()->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.rp_mode = blink::mojom::RpMode::kActive;

  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kError,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;

  RunAuthTest(parameters, expectations, configuration);

  ukm_loop.Run();
  ExpectUkmValue("RpMode", static_cast<int>(RpMode::kActive));

  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_FALSE(test_api_permission_delegate_->embargoed_origins_.count(
      main_test_rfh()->GetLastCommittedOrigin()));
}

// Tests that when background text is passed but no background color, the
// background text is ignored.
TEST_F(FederatedAuthRequestImplTest,
       BrandingWithTextColorAndNoBackgroundColor) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.brand_text_color =
      SkColorSetRGB(10, 10, 10);
  RequestExpectations expectations = kExpectationSuccess;
  expectations.standalone_console_message =
      "The FedCM text color is ignored because background color was not "
      "provided";
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_EQ(brand_background_color(), std::nullopt);
  EXPECT_EQ(brand_text_color(), std::nullopt);
}

// Tests that when background text does not contrast enough with the background
// color, the text color is ignored.
TEST_F(FederatedAuthRequestImplTest,
       BrandingWithInsufficientContrastTextColor) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.brand_background_color =
      SkColorSetRGB(0, 0, 0);
  configuration.idp_info[kProviderUrlFull].config.brand_text_color =
      SkColorSetRGB(1, 1, 1);
  RequestExpectations expectations = kExpectationSuccess;
  expectations.standalone_console_message =
      "The FedCM text color is ignored because it does not contrast enough "
      "with the provided background color";
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);

  EXPECT_EQ(brand_background_color(), SkColorSetRGB(0, 0, 0));
  EXPECT_EQ(brand_text_color(), std::nullopt);
}

class FederatedAuthRequestExampleOrgTest : public FederatedAuthRequestImplTest {
 public:
  FederatedAuthRequestExampleOrgTest()
      : FederatedAuthRequestImplTest("https://rp.example.org/") {}
};

TEST_F(FederatedAuthRequestExampleOrgTest, WellKnownSameSite) {
  static const char kExampleOrgProviderUrl[] =
      "https://idp.example.org/fedcm.json";

  MockIdpInfo idp_info = kDefaultIdentityProviderInfo;
  idp_info.well_known.fetch_status.parse_status =
      ParseStatus::kInvalidContentTypeError;
  idp_info.config.accounts_endpoint = "https://idp.example.org/accounts";
  idp_info.config.token_endpoint = "https://idp.example.org/token";
  idp_info.config.client_metadata_endpoint =
      "https://idp.example.org/client_metadata";
  idp_info.config.metrics_endpoint = "";
  idp_info.config.idp_login_url = "https://idp.example.org/login";
  idp_info.config.disconnect_endpoint = "";

  // We make the request from rp.example to idp.example, so it should
  // only succeed despite the well-known failure if the flag is on.
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info.clear();
  configuration.idp_info[kExampleOrgProviderUrl] = idp_info;

  RequestParameters request{kDefaultRequestParameters};
  request.identity_providers[0].provider = kExampleOrgProviderUrl;

  RequestExpectations expectation = kExpectationSuccess;
  expectation.selected_idp_config_url = kExampleOrgProviderUrl;

  RunAuthTest(request, expectation, configuration);
}

class TestDialogControllerWithImmediateDismiss : public TestDialogController {
 public:
  explicit TestDialogControllerWithImmediateDismiss(
      MockConfiguration configuration)
      : TestDialogController(configuration) {}

  ~TestDialogControllerWithImmediateDismiss() override = default;

  TestDialogControllerWithImmediateDismiss(
      const TestDialogControllerWithImmediateDismiss&) = delete;
  TestDialogControllerWithImmediateDismiss& operator=(
      TestDialogControllerWithImmediateDismiss&) = delete;

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
    std::move(dismiss_callback).Run(DismissReason::kOther);
    return false;
  }

  bool ShowFailureDialog(
      const std::string& rp_for_display,
      const std::string& idp_for_display,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const IdentityProviderMetadata& idp_metadata,
      IdentityRequestDialogController::DismissCallback dismiss_callback,
      IdentityRequestDialogController::LoginToIdPCallback
          identity_registry_callback) override {
    std::move(dismiss_callback).Run(DismissReason::kOther);
    return false;
  }
};

// Crash test for crbug.com/328945371.
TEST_F(FederatedAuthRequestImplTest, ImmediateDismiss) {
  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kError,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};

  SetDialogController(
      std::make_unique<TestDialogControllerWithImmediateDismiss>(
          kConfigurationValid));

  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  histogram_tester_.ExpectTotalCount(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", 0);
}

// Tests that dismissing during ShowFailureDialog() does not crash.
TEST_F(FederatedAuthRequestImplTest, FailureDialogImmediateDismiss) {
  url::Origin kIdpOrigin = OriginFromString(kProviderUrlFull);

  MockConfiguration configuration = kConfigurationValid;

  // Setup IdP sign-in status mismatch.
  test_permission_delegate_->idp_signin_statuses_[kIdpOrigin] = true;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;

  SetDialogController(
      std::make_unique<TestDialogControllerWithImmediateDismiss>(
          configuration));

  RequestExpectations expectations = {
      RequestTokenStatus::kError, FederatedAuthRequestResult::kError,
      /*standalone_console_message=*/std::nullopt,
      /*selected_idp_config_url=*/std::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
}

TEST_F(FederatedAuthRequestImplTest, UseOtherAccountAccountOrder) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmUseOtherAccount);

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kAddAccount;

  // User has accounts, kAccountIdNicolas and kAccountIdZach, in that order.
  configuration.idp_info[kProviderUrlFull].accounts = kTwoAccounts;

  auto dialog_controller =
      std::make_unique<TestDialogController>(configuration);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  std::unique_ptr<WebContents> modal(CreateTestWebContents());
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>([&modal, this](const GURL& url) {
        // The user signs in with account, kAccountIdPeter. User now has
        // accounts, kAccountIdNicolas, kAccountIdPeter and kAccountIdZach,
        // in that order.
        test_network_request_manager_->accounts_list_ = kMultipleAccounts;
        federated_auth_request_impl_->OnIdpSigninStatusReceived(
            OriginFromString(kProviderUrlFull), true);
        return modal.get();
      }));

  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 3u);
  ASSERT_EQ(new_accounts().size(), 1u);

  // Accounts are reordered to have the most recently signed in account,
  // kAccountIdPeter, displayed first.
  EXPECT_EQ(all_accounts_for_display()[0]->id, kAccountIdPeter);
  EXPECT_EQ(all_accounts_for_display()[1]->id, kAccountIdNicolas);
  EXPECT_EQ(all_accounts_for_display()[2]->id, kAccountIdZach);
  EXPECT_EQ(new_accounts()[0]->id, kAccountIdPeter);
}

// Tests that when use a different account is used and multiple accounts are
// logged in at once, all the new accounts are part of the new_accounts().
TEST_F(FederatedAuthRequestImplTest, UseOtherAccountMultipleNewAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmUseOtherAccount);

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kAddAccount;
  auto dialog_controller =
      std::make_unique<TestDialogController>(configuration);
  base::WeakPtr<TestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  std::unique_ptr<WebContents> modal(CreateTestWebContents());
  EXPECT_CALL(*weak_dialog_controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>([&modal, this](const GURL& url) {
        // The user signs in with kAccountIdNicolas and kAccountIdZach. User now
        // has accounts kAccountId, kAccountIdNicolas, and kAccountIdZach, in
        // that order.
        test_network_request_manager_->accounts_list_ = {
            kSingleAccount[0], kTwoAccounts[0], kTwoAccounts[1]};
        federated_auth_request_impl_->OnIdpSigninStatusReceived(
            OriginFromString(kProviderUrlFull), true);
        return modal.get();
      }));

  RunAuthDontWaitForCallback(kDefaultRequestParameters, configuration);

  ASSERT_EQ(all_accounts_for_display().size(), 3u);
  ASSERT_EQ(new_accounts().size(), 2u);

  // Accounts are reordered to have the most recently signed in accounts
  // displayed first.
  EXPECT_EQ(all_accounts_for_display()[0]->id, kTwoAccounts[0]->id);
  EXPECT_EQ(all_accounts_for_display()[1]->id, kTwoAccounts[1]->id);
  EXPECT_EQ(all_accounts_for_display()[2]->id, kSingleAccount[0]->id);
  EXPECT_EQ(new_accounts()[0]->id, kTwoAccounts[0]->id);
  EXPECT_EQ(new_accounts()[1]->id, kTwoAccounts[1]->id);
}

}  // namespace content
