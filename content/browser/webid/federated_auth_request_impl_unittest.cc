// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/federated_auth_request_service.h"
#include "content/browser/webid/test/mock_active_session_permission_delegate.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_request_permission_delegate.h"
#include "content/browser/webid/test/mock_sharing_permission_delegate.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_features.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::LogoutRpsRequest;
using blink::mojom::LogoutRpsRequestPtr;
using blink::mojom::LogoutRpsStatus;
using blink::mojom::RequestIdTokenStatus;
using blink::mojom::RevokeStatus;
using Entry = ukm::builders::Blink_FedCm;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using LogoutResponse = content::IdpNetworkRequestManager::LogoutResponse;
using SigninResponse = content::IdpNetworkRequestManager::SigninResponse;
using RevokeResponse = content::IdpNetworkRequestManager::RevokeResponse;
using UserApproval = content::IdentityRequestDialogController::UserApproval;
using AccountList = content::IdpNetworkRequestManager::AccountList;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using IdTokenStatus = content::FedCmRequestIdTokenStatus;
using RevokeStatusForMetrics = content::FedCmRevokeStatus;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

constexpr bool kPreferAutoSignIn = true;
constexpr bool kNotPreferAutoSignIn = false;
constexpr char kRpTestOrigin[] = "https://rp.example";
constexpr char kIdpTestOrigin[] = "https://idp.example";
constexpr char kProviderUrl[] = "https://idp.example";
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

// Values will be added here as token introspection is implemented.
constexpr char kToken[] = "[not a real token]";
constexpr char kEmptyToken[] = "";

static const std::initializer_list<IdentityRequestAccount> kAccounts{{
    "1234",             // id
    "ken@idp.example",  // email
    "Ken R. Example",   // name
    "Ken",              // given_name
    GURL()              // picture
}};

// Parameters for a call to RequestIdToken.
typedef struct {
  const char* provider;
  const char* client_id;
  const char* nonce;
  bool prefer_auto_sign_in;
} RequestParameters;

// Expected return values from a call to RequestIdToken.
typedef struct {
  RequestIdTokenStatus return_status;
  FederatedAuthRequestResult devtools_issue_status;
  const char* token;
} RequestExpectations;

// Mock configuration values for test.
struct MockClientIdConfiguration {
  FetchStatus fetch_status;
  const char* privacy_policy_url;
  const char* terms_of_service_url;
};

typedef struct {
  absl::optional<FetchStatus> accounts_response;
  AccountList accounts;
  absl::optional<FetchStatus> token_response;
  absl::optional<bool> customized_dialog;
} MockMediatedConfiguration;

typedef struct {
  const char* token;
  absl::optional<FetchStatus> manifest_fetch_status;
  absl::optional<MockClientIdConfiguration> client_metadata;
  const char* accounts_endpoint;
  const char* token_endpoint;
  const char* client_metadata_endpoint;
  MockMediatedConfiguration Mediated_conf;
} MockConfiguration;

// absl::optional fields should be nullopt to prevent the corresponding
// methods from having EXPECT_CALL set on the mocks.
typedef struct {
  std::string test_name;
  RequestParameters inputs;
  RequestExpectations expected;
  MockConfiguration config;
} AuthRequestTestCase;

std::ostream& operator<<(std::ostream& os,
                         const AuthRequestTestCase& testcase) {
  std::string name;
  base::ReplaceChars(testcase.test_name, " ", "", &name);
  return os << name;
}

static const MockMediatedConfiguration kMediatedNoop{absl::nullopt, kAccounts,
                                                     absl::nullopt};
static const MockClientIdConfiguration kSuccessfulClientId{
    FetchStatus::kSuccess, kPrivacyPolicyUrl, kTermsOfServiceUrl};

static const MockClientIdConfiguration kClientMetadataHttpNotFound{
    FetchStatus::kHttpNotFoundError, "", ""};

static const MockClientIdConfiguration kClientMetadataNoResponse{
    FetchStatus::kNoResponseError, "", ""};

static const MockClientIdConfiguration kClientMetadataInvalidResponse{
    FetchStatus::kInvalidResponseError, "", ""};

static const MockClientIdConfiguration kClientMetadataNoPrivacyPolicyUrl{
    FetchStatus::kSuccess, "", ""};

static const AuthRequestTestCase kMissingTokenEndpoint{
    "Error parsing FedCM manifest for Mediated mode missing token endpoint",
    {kIdpTestOrigin, kClientId, kNonce},
    {RequestIdTokenStatus::kError,
     FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
     kEmptyToken},
    {kToken, FetchStatus::kSuccess, absl::nullopt, kAccountsEndpoint, "",
     kClientMetadataEndpoint, kMediatedNoop}};

static const AuthRequestTestCase kMissingAccountsEndpoint{
    "Error parsing FedCM manifest for Mediated mode missing accounts endpoint",
    {kIdpTestOrigin, kClientId, kNonce},
    {RequestIdTokenStatus::kError,
     FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
     kEmptyToken},
    {kToken, FetchStatus::kSuccess, absl::nullopt, "", kTokenEndpoint,
     kClientMetadataEndpoint, kMediatedNoop}};

static const AuthRequestTestCase kMissingClientMetadata{
    "Error parsing FedCM manifest for Mediated mode missing client metadata "
    "endpoint",
    {kIdpTestOrigin, kClientId, kNonce},
    {RequestIdTokenStatus::kError,
     FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
     kEmptyToken},
    {kToken, FetchStatus::kSuccess, absl::nullopt, kAccountsEndpoint,
     kTokenEndpoint, "", kMediatedNoop}};

static const AuthRequestTestCase kMediatedTestCases[]{
    kMissingTokenEndpoint,
    kMissingAccountsEndpoint,
    kMissingClientMetadata,

    {"Error due to accounts endpoint in different origin than identity "
     "provider",
     {kIdpTestOrigin, kClientId, kNonce},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
      kEmptyToken},
     {kToken, FetchStatus::kSuccess, absl::nullopt,
      kCrossOriginAccountsEndpoint, kTokenEndpoint, kClientMetadataEndpoint,
      kMediatedNoop}},

    {"Error reaching Accounts endpoint",
     {kIdpTestOrigin, kClientId, kNonce},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse,
      kEmptyToken},
     {kEmptyToken,
      FetchStatus::kSuccess,
      kSuccessfulClientId,
      kAccountsEndpoint,
      kTokenEndpoint,
      kClientMetadataEndpoint,
      {FetchStatus::kNoResponseError, kAccounts, absl::nullopt}}},

    {"Error parsing Accounts response",
     {kIdpTestOrigin, kClientId, kNonce},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
      kEmptyToken},
     {kToken,
      FetchStatus::kSuccess,
      kSuccessfulClientId,
      kAccountsEndpoint,
      kTokenEndpoint,
      kClientMetadataEndpoint,
      {FetchStatus::kInvalidResponseError, kAccounts, absl::nullopt}}},

    {"Successful Mediated flow",
     {kIdpTestOrigin, kClientId, kNonce},
     {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
      kToken},
     {kToken,
      FetchStatus::kSuccess,
      kSuccessfulClientId,
      kAccountsEndpoint,
      kTokenEndpoint,
      kClientMetadataEndpoint,
      {FetchStatus::kSuccess, kAccounts, FetchStatus::kSuccess}}},

    {"Client metadata file not found",
     {kIdpTestOrigin, kClientId, kNonce},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound,
      kEmptyToken},
     {kToken, FetchStatus::kSuccess, kClientMetadataHttpNotFound,
      kAccountsEndpoint, kTokenEndpoint, kClientMetadataEndpoint,
      kMediatedNoop}},

    {"Client metadata empty response",
     {kIdpTestOrigin, kClientId, kNonce},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse,
      kEmptyToken},
     {kToken, FetchStatus::kSuccess, kClientMetadataNoResponse,
      kAccountsEndpoint, kTokenEndpoint, kClientMetadataEndpoint,
      kMediatedNoop}},

    {"Client metadata invalid response",
     {kIdpTestOrigin, kClientId, kNonce},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingClientMetadataInvalidResponse,
      kEmptyToken},
     {kToken, FetchStatus::kSuccess, kClientMetadataInvalidResponse,
      kAccountsEndpoint, kTokenEndpoint, kClientMetadataEndpoint,
      kMediatedNoop}},

    {"Client metadata has no privacy policy url",
     {kIdpTestOrigin, kClientId, kNonce},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorClientMetadataMissingPrivacyPolicyUrl,
      kEmptyToken},
     {kToken, FetchStatus::kSuccess, kClientMetadataNoPrivacyPolicyUrl,
      kAccountsEndpoint, kTokenEndpoint, kClientMetadataEndpoint,
      kMediatedNoop}},
};

// Helper class for receiving the mojo method callback.
class AuthRequestCallbackHelper {
 public:
  AuthRequestCallbackHelper() = default;
  ~AuthRequestCallbackHelper() = default;

  AuthRequestCallbackHelper(const AuthRequestCallbackHelper&) = delete;
  AuthRequestCallbackHelper& operator=(const AuthRequestCallbackHelper&) =
      delete;

  RequestIdTokenStatus status() const { return status_; }
  absl::optional<std::string> token() const { return token_; }

  // This can only be called once per lifetime of this object.
  base::OnceCallback<void(RequestIdTokenStatus,
                          const absl::optional<std::string>&)>
  callback() {
    return base::BindOnce(&AuthRequestCallbackHelper::ReceiverMethod,
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
  void ReceiverMethod(RequestIdTokenStatus status,
                      const absl::optional<std::string>& token) {
    status_ = status;
    token_ = token;
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  RequestIdTokenStatus status_;
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

// Helper class for receiving the Revoke method callback.
class RevokeRequestCallbackHelper {
 public:
  RevokeRequestCallbackHelper() = default;
  ~RevokeRequestCallbackHelper() = default;

  RevokeRequestCallbackHelper(const RevokeRequestCallbackHelper&) = delete;
  RevokeRequestCallbackHelper& operator=(const RevokeRequestCallbackHelper&) =
      delete;

  RevokeStatus status() const { return status_; }

  // This can only be called once per lifetime of this object.
  base::OnceCallback<void(RevokeStatus)> callback() {
    return base::BindOnce(&RevokeRequestCallbackHelper::ReceiverMethod,
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
  void ReceiverMethod(RevokeStatus status) {
    status_ = status;
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  RevokeStatus status_;
};

LogoutRpsRequestPtr MakeLogoutRequest(const std::string& endpoint,
                                      const std::string& account_id) {
  auto request = LogoutRpsRequest::New();
  request->url = GURL(endpoint);
  request->account_id = account_id;
  return request;
}

}  // namespace

class FederatedAuthRequestImplTest : public RenderViewHostImplTestHarness {
 protected:
  FederatedAuthRequestImplTest() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  ~FederatedAuthRequestImplTest() override = default;

  FederatedAuthRequestImpl& CreateAuthRequest(const GURL& provider) {
    provider_ = provider;
    // `FederatedAuthRequestService` derives from `DocumentService` and
    // controls its own lifetime.
    auth_request_service_ = new FederatedAuthRequestService(
        main_test_rfh(), request_remote_.BindNewPipeAndPassReceiver());
    auto mock_request_manager =
        std::make_unique<NiceMock<MockIdpNetworkRequestManager>>(
            provider, url::Origin::Create(GURL(kRpTestOrigin)));
    mock_request_manager_ = mock_request_manager.get();
    auth_request_service_->GetImplForTesting()->SetNetworkManagerForTests(
        std::move(mock_request_manager));
    auto mock_dialog_controller =
        std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
    mock_dialog_controller_ = mock_dialog_controller.get();
    auth_request_service_->GetImplForTesting()->SetDialogControllerForTests(
        std::move(mock_dialog_controller));

    mock_request_permission_delegate_ =
        std::make_unique<NiceMock<MockRequestPermissionDelegate>>();

    mock_active_session_permission_delegate_ =
        std::make_unique<NiceMock<MockActiveSessionPermissionDelegate>>();

    mock_sharing_permission_delegate_ =
        std::make_unique<NiceMock<MockSharingPermissionDelegate>>();

    auth_request_service_->GetImplForTesting()
        ->SetSharingPermissionDelegateForTests(
            mock_sharing_permission_delegate_.get());
    return *auth_request_service_->GetImplForTesting();
  }

  std::pair<RequestIdTokenStatus, absl::optional<std::string>>
  PerformAuthRequest(const std::string& client_id,
                     const std::string& nonce,
                     bool prefer_auto_sign_in) {
    AuthRequestCallbackHelper auth_helper;
    request_remote_->RequestIdToken(provider_, client_id, nonce,
                                    prefer_auto_sign_in,
                                    auth_helper.callback());
    auth_helper.WaitForCallback();
    return std::make_pair(auth_helper.status(), auth_helper.token());
  }

  LogoutRpsStatus PerformLogoutRequest(
      std::vector<LogoutRpsRequestPtr> logout_requests) {
    auth_request_service_->GetImplForTesting()
        ->SetActiveSessionPermissionDelegateForTests(
            mock_active_session_permission_delegate_.get());

    LogoutRpsRequestCallbackHelper logout_helper;
    request_remote_->LogoutRps(std::move(logout_requests),
                               logout_helper.callback());
    logout_helper.WaitForCallback();
    return logout_helper.status();
  }

  RevokeStatus PerformRevokeRequest(const char* account_id) {
    RevokeRequestCallbackHelper revoke_helper;
    request_remote_->Revoke(GURL(kProviderUrl), kClientId, account_id,
                            revoke_helper.callback());
    revoke_helper.WaitForCallback();
    return revoke_helper.status();
  }

  void SetMediatedMockExpectations(const MockMediatedConfiguration& conf,
                                   std::string token,
                                   bool prefer_auto_sign_in) {
    if (conf.accounts_response) {
      EXPECT_CALL(*mock_request_manager_, SendAccountsRequest(_, _, _))
          .WillOnce(Invoke(
              [&](const GURL&, const std::string&,
                  IdpNetworkRequestManager::AccountsRequestCallback callback) {
                std::move(callback).Run(*conf.accounts_response, conf.accounts);
              }));
    } else {
      EXPECT_CALL(*mock_request_manager_, SendAccountsRequest(_, _, _))
          .Times(0);
    }

    if (conf.accounts_response == FetchStatus::kSuccess) {
      if (!prefer_auto_sign_in && !conf.customized_dialog) {
        // Expects a dialog if prefer_auto_sign_in is not set by RP. However,
        // even though the bit is set we may not exercise the AutoSignIn flow.
        // e.g. for sign up flow, multiple accounts, user opt-out etc. In this
        // case, it's up to the test to expect this mock function call.
        EXPECT_CALL(*mock_dialog_controller_,
                    ShowAccountsDialog(_, _, _, _, _, _, _))
            .WillOnce(Invoke(
                [&](content::WebContents* rp_web_contents,
                    const GURL& idp_signin_url,
                    base::span<const content::IdentityRequestAccount> accounts,
                    const IdentityProviderMetadata& idp_metadata,
                    const ClientIdData& client_id_data, SignInMode sign_in_mode,
                    IdentityRequestDialogController::AccountSelectionCallback
                        on_selected) {
                  displayed_accounts_ =
                      AccountList(accounts.begin(), accounts.end());
                  std::move(on_selected)
                      .Run(accounts[0].id,
                           accounts[0].login_state == LoginState::kSignIn);
                }));
      }
    } else {
      EXPECT_CALL(*mock_dialog_controller_,
                  ShowAccountsDialog(_, _, _, _, _, _, _))
          .Times(0);
    }

    if (conf.token_response) {
      auto delivered_token =
          conf.token_response == FetchStatus::kSuccess ? token : std::string();
      EXPECT_CALL(*mock_request_manager_, SendTokenRequest(_, _, _, _))
          .WillOnce(Invoke(
              [=](const GURL& idp_signin_url, const std::string& account_id,
                  const std::string& request,
                  IdpNetworkRequestManager::TokenRequestCallback callback) {
                std::move(callback).Run(*conf.token_response, delivered_token);
              }));
      task_environment()->FastForwardBy(base::Seconds(3));
    } else {
      EXPECT_CALL(*mock_request_manager_, SendTokenRequest(_, _, _, _))
          .Times(0);
    }
  }

  void SetMockExpectations(const AuthRequestTestCase& test_case) {
    if (test_case.config.manifest_fetch_status) {
      EXPECT_CALL(*mock_request_manager_, FetchManifest(_, _, _))
          .WillOnce(Invoke(
              [&](absl::optional<int>, absl::optional<int>,
                  IdpNetworkRequestManager::FetchManifestCallback callback) {
                IdpNetworkRequestManager::Endpoints endpoints;
                endpoints.accounts = test_case.config.accounts_endpoint;
                endpoints.token = test_case.config.token_endpoint;
                endpoints.client_metadata =
                    test_case.config.client_metadata_endpoint;
                std::move(callback).Run(*test_case.config.manifest_fetch_status,
                                        endpoints, IdentityProviderMetadata());
              }));
    } else {
      EXPECT_CALL(*mock_request_manager_, FetchManifest(_, _, _)).Times(0);
    }

    if (test_case.config.client_metadata) {
      EXPECT_CALL(*mock_request_manager_, FetchClientMetadata(_, _, _))
          .WillOnce(
              Invoke([&](const GURL&, const std::string& client_id,
                         IdpNetworkRequestManager::FetchClientMetadataCallback
                             callback) {
                EXPECT_EQ(test_case.inputs.client_id, client_id);
                std::move(callback).Run(
                    test_case.config.client_metadata->fetch_status,
                    IdpNetworkRequestManager::ClientMetadata{
                        test_case.config.client_metadata->privacy_policy_url,
                        test_case.config.client_metadata
                            ->terms_of_service_url});
              }));
    } else {
      EXPECT_CALL(*mock_request_manager_, FetchClientMetadata(_, _, _))
          .Times(0);
    }

    SetMediatedMockExpectations(test_case.config.Mediated_conf,
                                test_case.config.token,
                                test_case.inputs.prefer_auto_sign_in);
  }

  // Expectations have to be set explicitly in advance using
  // logout_return_count() and logout_requests().
  void SetLogoutMockExpectations() {
    for (int i = logout_session_permissions_.size() - 1; i >= 0; i--) {
      auto single_session_permission = logout_session_permissions_[i];
      EXPECT_CALL(*mock_active_session_permission_delegate_,
                  HasActiveSession(_, _, _))
          .WillOnce(Return(single_session_permission))
          .RetiresOnSaturation();
    }

    for (int i = 0; i < logout_return_count_; i++) {
      EXPECT_CALL(*mock_request_manager_, SendLogout(_, _))
          .WillOnce(
              Invoke([](const GURL& logout_endpoint,
                        IdpNetworkRequestManager::LogoutCallback callback) {
                std::move(callback).Run();
              }))
          .RetiresOnSaturation();
    }
  }

  int& logout_return_count() { return logout_return_count_; }
  std::vector<LogoutRpsRequestPtr>& logout_requests() {
    return logout_requests_;
  }
  std::vector<bool>& logout_session_permissions() {
    return logout_session_permissions_;
  }

  base::span<const content::IdentityRequestAccount> displayed_accounts() const {
    return displayed_accounts_;
  }
  MockIdentityRequestDialogController* mock_dialog_controller() const {
    return mock_dialog_controller_;
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  void ExpectRequestIdTokenStatusUKM(IdTokenStatus status) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    if (entries.empty())
      FAIL() << "No RequestIdTokenStatus was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status_RequestIdToken");
      if (metric && *metric != static_cast<int>(status))
        FAIL() << "Unexpected status was recorded";
    }

    SUCCEED();
  }

  void ExpectRevokeStatusUKM(RevokeStatusForMetrics status) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    if (entries.empty())
      FAIL() << "No RevokeStatus was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status_Revoke");
      if (metric && *metric != static_cast<int>(status))
        FAIL() << "Unexpected status was recorded";
    }

    SUCCEED();
  }

  void ExpectTimingUKM(const std::string& metric_name) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

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
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    ASSERT_FALSE(entries.empty());

    for (const auto* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name))
        FAIL() << "Unexpected UKM was recorded";
    }
    SUCCEED();
  }

 protected:
  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  // Note: `auth_request_service_` owns itself, and will generally be deleted
  // with the TestRenderFrameHost is torn down at `TearDown()` time.
  raw_ptr<FederatedAuthRequestService> auth_request_service_;

  // Owned by `auth_request_service_`.
  raw_ptr<NiceMock<MockIdpNetworkRequestManager>> mock_request_manager_;
  raw_ptr<NiceMock<MockIdentityRequestDialogController>>
      mock_dialog_controller_;

  std::unique_ptr<NiceMock<MockRequestPermissionDelegate>>
      mock_request_permission_delegate_;
  std::unique_ptr<NiceMock<MockActiveSessionPermissionDelegate>>
      mock_active_session_permission_delegate_;
  std::unique_ptr<NiceMock<MockSharingPermissionDelegate>>
      mock_sharing_permission_delegate_;

  base::OnceClosure close_idp_window_callback_;

  // Test case storage for Logout tests.
  int logout_return_count_ = 0;
  std::vector<LogoutRpsRequestPtr> logout_requests_;
  std::vector<bool> logout_session_permissions_;

  // Storage for displayed accounts
  AccountList displayed_accounts_;

  GURL provider_;

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

class BasicFederatedAuthRequestImplTest
    : public FederatedAuthRequestImplTest,
      public ::testing::WithParamInterface<AuthRequestTestCase> {};

INSTANTIATE_TEST_SUITE_P(MediatedTests,
                         BasicFederatedAuthRequestImplTest,
                         ::testing::ValuesIn(kMediatedTestCases),
                         ::testing::PrintToStringParamName());

// Exercise the auth test case give the configuration.
TEST_P(BasicFederatedAuthRequestImplTest, FederatedAuthRequests) {
  AuthRequestTestCase test_case = GetParam();
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(auth_response.first, test_case.expected.return_status);
  EXPECT_EQ(auth_response.second, test_case.expected.token);
}

TEST_P(BasicFederatedAuthRequestImplTest, FederatedAuthRequestIssue) {
  AuthRequestTestCase test_case = GetParam();
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(main_test_rfh()->GetFederatedAuthRequestIssueCount(
                test_case.expected.devtools_issue_status),
            auth_response.first == RequestIdTokenStatus::kSuccess ? 0 : 1);
  static std::unordered_map<FederatedAuthRequestResult,
                            absl::optional<std::string>>
      status_to_message = {
          {FederatedAuthRequestResult::kSuccess, absl::nullopt},
          {FederatedAuthRequestResult::kApprovalDeclined,
           "User declined the sign-in attempt."},
          {FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound,
           "The provider's FedCM manifest configuration cannot be found."},
          {FederatedAuthRequestResult::kErrorFetchingManifestNoResponse,
           "The provider's FedCM manifest configuration fetch resulted in an "
           "error response code."},
          {FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
           "Provider's FedCM manifest configuration is invalid."},
          {FederatedAuthRequestResult::kErrorFetchingSignin,
           "Error attempting to reach the provider's sign-in endpoint."},
          {FederatedAuthRequestResult::kErrorInvalidSigninResponse,
           "Provider's sign-in response is invalid."},
          {FederatedAuthRequestResult::kError, "Error retrieving an id token."},
          {FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse,
           "The provider's accounts list fetch resulted in an error response "
           "code."},
          {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
           "Provider's accounts list is invalid. Should have received an "
           "\"accounts\" list, where each account must "
           "have at least \"id\", \"name\", and \"email\"."},
          {FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound,
           "The provider's client metadata endpoint cannot be found."},
          {FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse,
           "The provider's client metadata fetch resulted in an error response "
           "code."},
          {FederatedAuthRequestResult::
               kErrorFetchingClientMetadataInvalidResponse,
           "Provider's client metadata is invalid."},
          {FederatedAuthRequestResult::
               kErrorClientMetadataMissingPrivacyPolicyUrl,
           "Provider's client metadata is missing or has an invalid privacy "
           "policy url."}};
  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  absl::optional<std::string> expected_message =
      status_to_message[test_case.expected.devtools_issue_status];
  if (!expected_message) {
    EXPECT_EQ(0u, messages.size());
  } else {
    ASSERT_LE(1u, messages.size());
    EXPECT_EQ(expected_message.value(), messages[messages.size() - 1]);
  }
}

TEST_F(BasicFederatedAuthRequestImplTest, MissingTokenEndpoint) {
  const auto& test_case = kMissingTokenEndpoint;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
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

TEST_F(BasicFederatedAuthRequestImplTest, MissingAccountsEndpoint) {
  const auto& test_case = kMissingAccountsEndpoint;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
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

TEST_F(BasicFederatedAuthRequestImplTest, MissingClientMetadataEndpoint) {
  const auto& test_case = kMissingClientMetadata;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Manifest is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"client_metadata_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM manifest configuration is invalid.", messages[1]);
}

TEST_F(BasicFederatedAuthRequestImplTest, AllInvalidEndpoints) {
  // Both an empty url and cross origin urls are invalid endpoints.
  AuthRequestTestCase test_case = {
      "FedCM manifest missing all endpoints",
      {kIdpTestOrigin, kClientId, kNonce},
      {RequestIdTokenStatus::kError,
       FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
       kEmptyToken},
      {kToken, FetchStatus::kSuccess, absl::nullopt,
       "https://cross-origin-1.com", "", "https://cross-origin-2.com",
       kMediatedNoop}};
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Manifest is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"id_token_endpoint\"\n"
      "\"accounts_endpoint\"\n"
      "\"client_metadata_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM manifest configuration is invalid.", messages[1]);
}

// Test Logout method success with multiple relying parties.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutSuccessMultiple) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSignoutFieldTrialParamName, "true"}});

  CreateAuthRequest(GURL(kIdpTestOrigin));

  auto request1 = MakeLogoutRequest("https://rp1.example", "user123");
  logout_requests().push_back(std::move(request1));
  logout_session_permissions().push_back(true);
  auto request2 = MakeLogoutRequest("https://rp2.example", "user456");
  logout_requests().push_back(std::move(request2));
  logout_session_permissions().push_back(true);
  auto request3 = MakeLogoutRequest("https://rp3.example", "user789");
  logout_requests().push_back(std::move(request3));
  logout_session_permissions().push_back(true);
  logout_return_count() = 3;

  SetLogoutMockExpectations();
  auto logout_response = PerformLogoutRequest(std::move(logout_requests()));
  EXPECT_EQ(logout_response, LogoutRpsStatus::kSuccess);
}

// Test Logout without session permission granted.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutWithoutPermission) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSignoutFieldTrialParamName, "true"}});

  CreateAuthRequest(GURL(kIdpTestOrigin));

  // logout_return_count is not set here because there should be no
  // attempt at dispatch.
  auto request = MakeLogoutRequest("https://rp1.example", "user123");
  logout_requests().push_back(std::move(request));
  logout_session_permissions().push_back(false);

  SetLogoutMockExpectations();
  auto logout_response = PerformLogoutRequest(std::move(logout_requests()));
  EXPECT_EQ(logout_response, LogoutRpsStatus::kSuccess);
}

// Test Logout method with an empty endpoint vector.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutNoEndpoints) {
  CreateAuthRequest(GURL(kIdpTestOrigin));

  SetLogoutMockExpectations();
  auto logout_response = PerformLogoutRequest(std::move(logout_requests()));
  EXPECT_EQ(logout_response, LogoutRpsStatus::kError);
}

// Tests for Login State

static const AuthRequestTestCase kSuccessfulMediatedSignUpTestCase{
    "Successful mediated flow with one account",
    {kIdpTestOrigin, kClientId, kNonce, kNotPreferAutoSignIn},
    {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
     kToken},
    {kToken,
     FetchStatus::kSuccess,
     kSuccessfulClientId,
     kAccountsEndpoint,
     kTokenEndpoint,
     kClientMetadataEndpoint,
     {FetchStatus::kSuccess, kAccounts, FetchStatus::kSuccess}}};

static const AuthRequestTestCase kFailedMediatedSignUpTestCase{
    "Failed mediated flow with one account",
    {kIdpTestOrigin, kClientId, kNonce, kNotPreferAutoSignIn},
    {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
     kToken},
    {kToken,
     FetchStatus::kSuccess,
     kSuccessfulClientId,
     kAccountsEndpoint,
     kTokenEndpoint,
     kClientMetadataEndpoint,
     {FetchStatus::kSuccess, kAccounts, FetchStatus::kInvalidResponseError}}};

static const AuthRequestTestCase kSuccessfulMediatedAutoSignInTestCase{
    "Successful mediated flow with one account",
    {kIdpTestOrigin, kClientId, kNonce, kPreferAutoSignIn},
    {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
     kToken},
    {kToken,
     FetchStatus::kSuccess,
     kSuccessfulClientId,
     kAccountsEndpoint,
     kTokenEndpoint,
     kClientMetadataEndpoint,
     {FetchStatus::kSuccess, kAccounts, FetchStatus::kSuccess}}};

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateShouldBeSignUpForFirstTimeUser) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);

  EXPECT_EQ(LoginState::kSignUp, displayed_accounts()[0].login_state);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateShouldBeSignInForReturningUser) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  // Pretend the sharing permission has been granted for this account.
  //
  // TODO(majidvp): Ideally we would use the kRpTestOrigin for second argument
  // but web contents has not navigated to that URL so origin() is null in
  // tests. We should fix this.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateSuccessfulSignUpGrantsSharingPermission) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermissionForAccount(_, _, _))
      .WillOnce(Return(false));
  // TODO(majidvp): Ideally we would use the kRpTestOrigin for second argument
  // but web contents has not navigated to that URL so origin() is null in
  // tests. We should fix this.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              GrantSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .Times(1);

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateFailedSignUpNotGrantSharingPermission) {
  const auto& test_case = kFailedMediatedSignUpTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermissionForAccount(_, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              GrantSharingPermissionForAccount(_, _, _))
      .Times(0);

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
}

TEST_F(BasicFederatedAuthRequestImplTest, AutoSignInForReturningUser) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  AccountList displayed_accounts;
  const auto& test_case = kSuccessfulMediatedAutoSignInTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  // Pretend the sharing permission has been granted for this account.
  //
  // TODO(majidvp): Ideally we would use the kRpTestOrigin for second argument
  // but web contents has not navigated to that URL so origin() is null in
  // tests. We should fix this.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected) {
            EXPECT_EQ(sign_in_mode, SignInMode::kAuto);
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            std::move(on_selected).Run(accounts[0].id, /*is_sign_in=*/true);
          }));

  EXPECT_EQ(test_case.config.Mediated_conf.accounts.size(), 1u);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignIn);
  EXPECT_EQ(auth_response.second.value(), kToken);
}

TEST_F(BasicFederatedAuthRequestImplTest, AutoSignInForFirstTimeUser) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  AccountList displayed_accounts;
  const auto& test_case = kSuccessfulMediatedAutoSignInTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected) {
            EXPECT_EQ(sign_in_mode, SignInMode::kExplicit);
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            std::move(on_selected).Run(accounts[0].id, /*is_sign_in=*/true);
          }));

  SetMockExpectations(test_case);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignUp);
  EXPECT_EQ(auth_response.second.value(), kToken);
}

TEST_F(BasicFederatedAuthRequestImplTest, AutoSignInWithScreenReader) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  content::BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::AXMode::kScreenReader);

  AccountList displayed_accounts;
  const auto& test_case = kSuccessfulMediatedAutoSignInTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  // Pretend the sharing permission has been granted for this account.
  //
  // TODO(majidvp): Ideally we would use the kRpTestOrigin for second argument
  // but web contents has not navigated to that URL so origin() is null in
  // tests. We should fix this.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected) {
            // Auto sign in replaced by explicit sign in if screen reader is on.
            EXPECT_EQ(sign_in_mode, SignInMode::kExplicit);
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            std::move(on_selected).Run(accounts[0].id, /*is_sign_in=*/true);
          }));

  EXPECT_EQ(test_case.config.Mediated_conf.accounts.size(), 1u);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignIn);
  EXPECT_EQ(auth_response.second.value(), kToken);
}

TEST_F(FederatedAuthRequestImplTest, Revoke) {
  constexpr char kHint[] = "foo@bar.com";

  auto& auth_request = CreateAuthRequest(GURL(kProviderUrl));
  auth_request.SetRequestPermissionDelegateForTests(
      mock_request_permission_delegate_.get());

  // Pretend the request permission has been granted for this account.
  EXPECT_CALL(
      *mock_request_permission_delegate_,
      HasRequestPermission(_, url::Origin::Create(GURL(kIdpTestOrigin))))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *mock_request_permission_delegate_,
      RevokeRequestPermission(_, url::Origin::Create(GURL(kIdpTestOrigin))));

  EXPECT_CALL(*mock_request_manager_, FetchManifest(_, _, _))
      .WillOnce(
          Invoke([&](absl::optional<int>, absl::optional<int>,
                     IdpNetworkRequestManager::FetchManifestCallback callback) {
            IdpNetworkRequestManager::Endpoints endpoints;
            endpoints.revocation = kRevocationEndpoint;
            std::move(callback).Run(FetchStatus::kSuccess, endpoints,
                                    IdentityProviderMetadata());
          }));
  EXPECT_CALL(*mock_request_manager_, SendRevokeRequest(_, _, _, _))
      .WillOnce(Invoke([&](const GURL& revoke_url, const std::string& client_id,
                           const std::string& hint,
                           IdpNetworkRequestManager::RevokeCallback callback) {
        EXPECT_EQ(kRevocationEndpoint, revoke_url.spec());
        EXPECT_EQ(kClientId, client_id);
        EXPECT_EQ(kHint, hint);
        std::move(callback).Run(RevokeResponse::kSuccess);
      }));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  auto status = PerformRevokeRequest(kHint);
  EXPECT_EQ(RevokeStatus::kSuccess, status);

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.Revoke",
                                       RevokeStatusForMetrics::kSuccess, 1);

  ExpectRevokeStatusUKM(RevokeStatusForMetrics::kSuccess);
}

TEST_F(FederatedAuthRequestImplTest, RevokeNoPermission) {
  constexpr char kHint[] = "foo@bar.com";

  auto& auth_request = CreateAuthRequest(GURL(kProviderUrl));
  auth_request.SetRequestPermissionDelegateForTests(
      mock_request_permission_delegate_.get());

  // Pretend the request permission has been denied for this account.
  EXPECT_CALL(
      *mock_request_permission_delegate_,
      HasRequestPermission(_, url::Origin::Create(GURL(kIdpTestOrigin))))
      .WillOnce(Return(false));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  auto status = PerformRevokeRequest(kHint);
  EXPECT_EQ(RevokeStatus::kError, status);

  ukm_loop.Run();
  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.Revoke", RevokeStatusForMetrics::kNoAccountToRevoke,
      1);

  ExpectRevokeStatusUKM(RevokeStatusForMetrics::kNoAccountToRevoke);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForSuccessfulSignUpCase) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  EXPECT_EQ(test_case.config.Mediated_conf.accounts.size(), 1u);

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(auth_response.second.value(), kToken);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       IdTokenStatus::kSuccess, 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.IsSignInUser", 0, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.ContinueOnDialog");
  ExpectTimingUKM("Timing.IdTokenResponse");
  ExpectTimingUKM("Timing.TurnaroundTime");
  ExpectNoTimingUKM("Timing.CancelOnDialog");

  ExpectRequestIdTokenStatusUKM(IdTokenStatus::kSuccess);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForSuccessfulSignInCase) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       IdTokenStatus::kSuccess, 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.IsSignInUser", 1, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.ContinueOnDialog");
  ExpectTimingUKM("Timing.IdTokenResponse");
  ExpectTimingUKM("Timing.TurnaroundTime");
  ExpectNoTimingUKM("Timing.CancelOnDialog");

  ExpectRequestIdTokenStatusUKM(IdTokenStatus::kSuccess);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForNotSelectingAccount) {
  base::HistogramTester histogram_tester_;

  AccountList displayed_accounts;
  const AuthRequestTestCase test_case = {
      "Failed mediated flow due to user not selecting an account",
      {kIdpTestOrigin, kClientId, kNonce, kNotPreferAutoSignIn},
      {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
       kToken},
      {kToken,
       FetchStatus::kSuccess,
       kSuccessfulClientId,
       kAccountsEndpoint,
       kTokenEndpoint,
       kClientMetadataEndpoint,
       {FetchStatus::kSuccess, kAccounts, absl::nullopt,
        /*customized_dialog=*/true}}};
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents, const GURL& idp_signin_url,
              base::span<const content::IdentityRequestAccount> accounts,
              const IdentityProviderMetadata& idp_metadata,
              const ClientIdData& client_id_data, SignInMode sign_in_mode,
              IdentityRequestDialogController::AccountSelectionCallback
                  on_selected) {
            displayed_accounts = AccountList(accounts.begin(), accounts.end());
            // Pretends that the user did not select any account.
            std::move(on_selected).Run("", /*is_sign_in=*/false);
          }));

  EXPECT_EQ(test_case.config.Mediated_conf.accounts.size(), 1u);

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);

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
                                       IdTokenStatus::kNotSelectAccount, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.CancelOnDialog");
  ExpectNoTimingUKM("Timing.ContinueOnDialog");
  ExpectNoTimingUKM("Timing.IdTokenResponse");
  ExpectNoTimingUKM("Timing.TurnaroundTime");

  ExpectRequestIdTokenStatusUKM(IdTokenStatus::kNotSelectAccount);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForWebContentsVisible) {
  base::HistogramTester histogram_tester;
  // Sets the WebContents to visible
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->UpdateWebContentsVisibility(Visibility::VISIBLE);
  ASSERT_EQ(web_contents_impl->GetVisibility(), Visibility::VISIBLE);

  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_sharing_permission_delegate_,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 1, 1);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForWebContentsInvisible) {
  base::HistogramTester histogram_tester;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->UpdateWebContentsVisibility(Visibility::VISIBLE);
  ASSERT_EQ(web_contents_impl->GetVisibility(), Visibility::VISIBLE);

  const AuthRequestTestCase test_case = {
      "Failed mediated flow due to user leaving the page",
      {kIdpTestOrigin, kClientId, kNonce, kNotPreferAutoSignIn},
      {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
       kToken},
      {kToken,
       FetchStatus::kSuccess,
       kSuccessfulClientId,
       kAccountsEndpoint,
       kTokenEndpoint,
       kClientMetadataEndpoint,
       {FetchStatus::kSuccess, kAccounts, absl::nullopt,
        /*customized_dialog=*/true}}};
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  // Sets the WebContents to invisible
  web_contents_impl->UpdateWebContentsVisibility(Visibility::HIDDEN);
  ASSERT_NE(web_contents_impl->GetVisibility(), Visibility::VISIBLE);

  PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                     test_case.inputs.prefer_auto_sign_in);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 0, 1);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       DisabledWhenThirdPartyCookiesBlocked) {
  const auto& test_case = kSuccessfulMediatedAutoSignInTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  NiceMock<MockApiPermissionDelegate> mock_api_permission_delegate;
  auth_request_service_->GetImplForTesting()->SetApiPermissionDelegateForTests(
      &mock_api_permission_delegate);
  // Not calling SetMockExpectations(test_case) because the testcase is rejected
  // early on, before the network manager is actually created.
  EXPECT_CALL(mock_api_permission_delegate, AreThirdPartyCookiesBlocked())
      .WillOnce(Return(true));
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(auth_response.first, RequestIdTokenStatus::kError);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       IdTokenStatus::kThirdPartyCookiesBlocked,
                                       1);
  ExpectRequestIdTokenStatusUKM(IdTokenStatus::kThirdPartyCookiesBlocked);
}

}  // namespace content
