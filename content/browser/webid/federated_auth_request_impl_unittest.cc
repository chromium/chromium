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
#include "content/browser/webid/id_token_request_callback_data.h"
#include "content/browser/webid/test/mock_active_session_permission_delegate.h"
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
using blink::mojom::RequestMode;
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
constexpr char kIdpEndpoint[] = "https://idp.example/webid";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kCrossOriginAccountsEndpoint[] = "https://idp2.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kClientMetadataEndpoint[] =
    "https://idp.example/client_metadata";
constexpr char kRevokeEndpoint[] = "https://idp.example/revoke";
constexpr char kPrivacyPolicyUrl[] = "https://rp.example/pp";
constexpr char kTermsOfServiceUrl[] = "https://rp.example/tos";
constexpr char kSigninUrl[] = "https://idp.example/signin";
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
  RequestMode mode;
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
  absl::optional<SigninResponse> signin_response;
  const char* signin_url_or_token;
  absl::optional<UserApproval> token_permission;
} MockPermissionConfiguration;

typedef struct {
  absl::optional<FetchStatus> accounts_response;
  AccountList accounts;
  absl::optional<FetchStatus> token_response;
  absl::optional<bool> customized_dialog;
} MockMediatedConfiguration;

typedef struct {
  const char* token;
  absl::optional<UserApproval> initial_permission;
  absl::optional<FetchStatus> manifest_fetch_status;
  absl::optional<MockClientIdConfiguration> client_metadata;
  const char* idp_endpoint;
  const char* accounts_endpoint;
  const char* token_endpoint;
  const char* client_metadata_endpoint;
  MockPermissionConfiguration Permission_conf;
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
static const MockPermissionConfiguration kPermissionNoop{absl::nullopt, "",
                                                         absl::nullopt};
static const MockClientIdConfiguration kSuccessfulClientId{
    FetchStatus::kSuccess, kPrivacyPolicyUrl, kTermsOfServiceUrl};

static const AuthRequestTestCase kPermissionTestCases[]{
    {"Successful run with the IdP page loaded",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
      kToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      absl::nullopt,
      kIdpEndpoint,
      "",
      "",
      "",
      {SigninResponse::kLoadIdp, kSigninUrl, UserApproval::kApproved},
      kMediatedNoop}},

    {"Successful run with a token response from the idp_endpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
      kToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      absl::nullopt,
      kIdpEndpoint,
      "",
      "",
      "",
      {SigninResponse::kTokenGranted, kToken, absl::nullopt},
      kMediatedNoop}},

    {"Initial user permission denied",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kApprovalDeclined,
      FederatedAuthRequestResult::kApprovalDeclined, kEmptyToken},
     {kToken, UserApproval::kDenied, absl::nullopt, absl::nullopt, "", "", "",
      "", kPermissionNoop, kMediatedNoop}},

    {"FedCM manifest file not found",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound,
      kEmptyToken},
     {kToken, UserApproval::kApproved, FetchStatus::kHttpNotFoundError,
      absl::nullopt, "", "", "", "", kPermissionNoop, kMediatedNoop}},

    {"FedCM manifest fetch error",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestNoResponse,
      kEmptyToken},
     {kToken, UserApproval::kApproved, FetchStatus::kNoResponseError,
      absl::nullopt, "", "", "", "", kPermissionNoop, kMediatedNoop}},

    {"Error parsing FedCM manifest for Permission mode",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
      kEmptyToken},
     {kToken, UserApproval::kApproved, FetchStatus::kInvalidResponseError,
      absl::nullopt, "", kAccountsEndpoint, kTokenEndpoint, "", kPermissionNoop,
      kMediatedNoop}},

    {"Error reaching the idpendpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorFetchingSignin,
      FederatedAuthRequestResult::kErrorFetchingSignin, kEmptyToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      absl::nullopt,
      kIdpEndpoint,
      "",
      "",
      "",
      {SigninResponse::kSigninError, "", absl::nullopt},
      kMediatedNoop}},

    {"Error parsing the idpendpoint response",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorInvalidSigninResponse,
      FederatedAuthRequestResult::kErrorInvalidSigninResponse, kEmptyToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      absl::nullopt,
      kIdpEndpoint,
      "",
      "",
      "",
      {SigninResponse::kInvalidResponseError, "", absl::nullopt},
      kMediatedNoop}},

    {"IdP window closed before token provision",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kError, FederatedAuthRequestResult::kError,
      kEmptyToken},
     {kEmptyToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      absl::nullopt,
      kIdpEndpoint,
      "",
      "",
      "",
      {SigninResponse::kLoadIdp, kSigninUrl, absl::nullopt},
      kMediatedNoop}},

    {"Token provision declined by user after IdP window closed",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kApprovalDeclined,
      FederatedAuthRequestResult::kApprovalDeclined, kEmptyToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      absl::nullopt,
      kIdpEndpoint,
      "",
      "",
      "",
      {SigninResponse::kLoadIdp, kSigninUrl, UserApproval::kDenied},
      kMediatedNoop}}};

static const AuthRequestTestCase kMediatedTestCases[]{
    {"Error parsing FedCM manifest for Mediated mode missing token endpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
      kEmptyToken},
     {kToken, absl::nullopt, FetchStatus::kInvalidResponseError, absl::nullopt,
      kIdpEndpoint, kAccountsEndpoint, "", kClientMetadataEndpoint,
      kPermissionNoop, kMediatedNoop}},

    {"Error parsing FedCM manifest for Mediated mode missing accounts endpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
      kEmptyToken},
     {kToken, absl::nullopt, FetchStatus::kSuccess, absl::nullopt, kIdpEndpoint,
      "", kTokenEndpoint, kClientMetadataEndpoint, kPermissionNoop,
      kMediatedNoop}},
    {"Error due to accounts endpoint in different origin than identity "
     "provider",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
      kEmptyToken},
     {kToken, absl::nullopt, FetchStatus::kSuccess, absl::nullopt, kIdpEndpoint,
      kCrossOriginAccountsEndpoint, kTokenEndpoint, kClientMetadataEndpoint,
      kPermissionNoop, kMediatedNoop}},

    {"Error reaching Accounts endpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse,
      kEmptyToken},
     {kEmptyToken,
      absl::nullopt,
      FetchStatus::kSuccess,
      kSuccessfulClientId,
      "",
      kAccountsEndpoint,
      kTokenEndpoint,
      kClientMetadataEndpoint,
      kPermissionNoop,
      {FetchStatus::kNoResponseError, kAccounts, absl::nullopt}}},

    {"Error parsing Accounts response",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kError,
      FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
      kEmptyToken},
     {kToken,
      absl::nullopt,
      FetchStatus::kSuccess,
      kSuccessfulClientId,
      "",
      kAccountsEndpoint,
      kTokenEndpoint,
      kClientMetadataEndpoint,
      kPermissionNoop,
      {FetchStatus::kInvalidResponseError, kAccounts, absl::nullopt}}},

    {"Successful Mediated flow",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
      kToken},
     {kToken,
      absl::nullopt,
      FetchStatus::kSuccess,
      kSuccessfulClientId,
      "",
      kAccountsEndpoint,
      kTokenEndpoint,
      kClientMetadataEndpoint,
      kPermissionNoop,
      {FetchStatus::kSuccess, kAccounts, FetchStatus::kSuccess}}},
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

    return *auth_request_service_->GetImplForTesting();
  }

  std::pair<RequestIdTokenStatus, absl::optional<std::string>>
  PerformAuthRequest(const std::string& client_id,
                     const std::string& nonce,
                     blink::mojom::RequestMode mode,
                     bool prefer_auto_sign_in) {
    AuthRequestCallbackHelper auth_helper;
    request_remote_->RequestIdToken(provider_, client_id, nonce, mode,
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
    request_remote_->Revoke(GURL(kIdpEndpoint), kClientId, account_id,
                            revoke_helper.callback());
    revoke_helper.WaitForCallback();
    return revoke_helper.status();
  }

  void SetPermissionMockExpectations(const MockPermissionConfiguration& conf,
                                     std::string token) {
    if (conf.signin_response) {
      EXPECT_CALL(*mock_request_manager_, SendSigninRequest(_, _, _))
          .WillOnce(Invoke(
              [&](const GURL&, const std::string&,
                  IdpNetworkRequestManager::SigninRequestCallback callback) {
                std::move(callback).Run(*conf.signin_response,
                                        conf.signin_url_or_token);
              }));
    }

    // The IdP dialog only shows when kLoadIdP is the return code from the
    // signin request.
    if (conf.signin_response == SigninResponse::kLoadIdp) {
      EXPECT_CALL(*mock_dialog_controller_, ShowIdProviderWindow(_, _, _, _))
          .WillOnce(Invoke([=](WebContents*, WebContents* idp_web_contents,
                               const GURL&,
                               IdentityRequestDialogController::
                                   IdProviderWindowClosedCallback callback) {
            close_idp_window_callback_ = std::move(callback);
            auto* request_callback_data =
                IdTokenRequestCallbackData::Get(idp_web_contents);
            EXPECT_TRUE(request_callback_data);
            auto rp_done_callback = request_callback_data->TakeDoneCallback();
            IdTokenRequestCallbackData::Remove(idp_web_contents);
            EXPECT_TRUE(rp_done_callback);
            std::move(rp_done_callback).Run(token);
          }));

      EXPECT_CALL(*mock_dialog_controller_, CloseIdProviderWindow())
          .WillOnce(
              Invoke([&]() { std::move(close_idp_window_callback_).Run(); }));
    }

    if (conf.token_permission) {
      EXPECT_CALL(*mock_dialog_controller_,
                  ShowTokenExchangePermissionDialog(_, _, _))
          .WillOnce(Invoke(
              [&](content::WebContents* idp_web_contents, const GURL& idp_url,
                  IdentityRequestDialogController::TokenExchangeApprovalCallback
                      callback) {
                std::move(callback).Run(*conf.token_permission);
              }));
    }
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
                    ShowAccountsDialog(_, _, _, _, _, _, _, _))
            .WillOnce(Invoke(
                [&](content::WebContents* rp_web_contents,
                    content::WebContents* idp_web_contents,
                    const GURL& idp_signin_url,
                    base::span<const content::IdentityRequestAccount> accounts,
                    const IdentityProviderMetadata& idp_metadata,
                    const ClientIdData& client_id_data, SignInMode sign_in_mode,
                    IdentityRequestDialogController::AccountSelectionCallback
                        on_selected) {
                  displayed_accounts_ =
                      AccountList(accounts.begin(), accounts.end());
                  std::move(on_selected)
                      .Run(accounts[0].id, /*is_sign_in=*/false);
                }));
      }
    } else {
      EXPECT_CALL(*mock_dialog_controller_,
                  ShowAccountsDialog(_, _, _, _, _, _, _, _))
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
    if (test_case.config.initial_permission) {
      EXPECT_CALL(*mock_dialog_controller_,
                  ShowInitialPermissionDialog(_, _, _, _))
          .WillOnce(Invoke(
              [&](WebContents*, const GURL&,
                  IdentityRequestDialogController::PermissionDialogMode,
                  IdentityRequestDialogController::InitialApprovalCallback
                      callback) {
                std::move(callback).Run(*test_case.config.initial_permission);
              }));
    } else {
      EXPECT_CALL(*mock_dialog_controller_,
                  ShowInitialPermissionDialog(_, _, _, _))
          .Times(0);
    }

    if (test_case.config.manifest_fetch_status) {
      EXPECT_CALL(*mock_request_manager_, FetchManifest(_, _, _))
          .WillOnce(Invoke(
              [&](absl::optional<int>, absl::optional<int>,
                  IdpNetworkRequestManager::FetchManifestCallback callback) {
                IdpNetworkRequestManager::Endpoints endpoints;
                endpoints.idp = test_case.config.idp_endpoint;
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

    SetPermissionMockExpectations(test_case.config.Permission_conf,
                                  test_case.config.token);
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

INSTANTIATE_TEST_SUITE_P(PermissionTests,
                         BasicFederatedAuthRequestImplTest,
                         ::testing::ValuesIn(kPermissionTestCases),
                         ::testing::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(MediatedTests,
                         BasicFederatedAuthRequestImplTest,
                         ::testing::ValuesIn(kMediatedTestCases),
                         ::testing::PrintToStringParamName());

// Exercise the auth test case give the configuration.
TEST_P(BasicFederatedAuthRequestImplTest, FederatedAuthRequests) {
  AuthRequestTestCase test_case = GetParam();
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
      test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(auth_response.first, test_case.expected.return_status);
  EXPECT_EQ(auth_response.second, test_case.expected.token);
}

TEST_P(BasicFederatedAuthRequestImplTest, FederatedAuthRequestIssue) {
  AuthRequestTestCase test_case = GetParam();
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
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
           "The response body is empty when fetching the provider's "
           "FedCM manifest configuration."},
          {FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
           "Provider's FedCM manifest configuration is invalid."},
          {FederatedAuthRequestResult::kErrorFetchingSignin,
           "Error attempting to reach the provider's sign-in endpoint."},
          {FederatedAuthRequestResult::kErrorInvalidSigninResponse,
           "Provider's sign-in response is invalid."},
          {FederatedAuthRequestResult::kError, "Error retrieving an id token."},
          {FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse,
           "The response body is empty when fetching the provider's accounts "
           "list."},
          {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
           "Provider's accounts list is invalid."}};
  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  absl::optional<std::string> expected_message =
      status_to_message[test_case.expected.devtools_issue_status];
  if (!expected_message) {
    EXPECT_EQ(0u, messages.size());
  } else {
    ASSERT_EQ(1u, messages.size());
    EXPECT_EQ(expected_message.value(), messages[0]);
  }
}

// Test Logout method success with multiple relying parties.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutSuccessMultiple) {
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
    {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated,
     kNotPreferAutoSignIn},
    {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
     kToken},
    {kToken,
     absl::nullopt,
     FetchStatus::kSuccess,
     kSuccessfulClientId,
     "",
     kAccountsEndpoint,
     kTokenEndpoint,
     kClientMetadataEndpoint,
     kPermissionNoop,
     {FetchStatus::kSuccess, kAccounts, FetchStatus::kSuccess}}};

static const AuthRequestTestCase kFailedMediatedSignUpTestCase{
    "Failed mediated flow with one account",
    {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated,
     kNotPreferAutoSignIn},
    {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
     kToken},
    {kToken,
     absl::nullopt,
     FetchStatus::kSuccess,
     kSuccessfulClientId,
     "",
     kAccountsEndpoint,
     kTokenEndpoint,
     kClientMetadataEndpoint,
     kPermissionNoop,
     {FetchStatus::kSuccess, kAccounts, FetchStatus::kInvalidResponseError}}};

static const AuthRequestTestCase kSuccessfulMediatedAutoSignInTestCase{
    "Successful mediated flow with one account",
    {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated,
     kPreferAutoSignIn},
    {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
     kToken},
    {kToken,
     absl::nullopt,
     FetchStatus::kSuccess,
     kSuccessfulClientId,
     "",
     kAccountsEndpoint,
     kTokenEndpoint,
     kClientMetadataEndpoint,
     kPermissionNoop,
     {FetchStatus::kSuccess, kAccounts, FetchStatus::kSuccess}}};

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateShouldBeSignUpForFirstTimeUser) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
      test_case.inputs.prefer_auto_sign_in);

  EXPECT_EQ(LoginState::kSignUp, displayed_accounts()[0].login_state);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateShouldBeSignInForReturningUser) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  auto& auth_request = CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  // Set specific expectations for sharing permission:
  NiceMock<MockSharingPermissionDelegate> mock_sharing_permission_delegate;
  auth_request.SetSharingPermissionDelegateForTests(
      &mock_sharing_permission_delegate);

  // Pretend the sharing permission has been granted for this account.
  //
  // TODO(majidvp): Ideally we would use the kRpTestOrigin for second argument
  // but web contents has not navigated to that URL so origin() is null in
  // tests. We should fix this.
  EXPECT_CALL(mock_sharing_permission_delegate,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
      test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateSuccessfulSignUpGrantsSharingPermission) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  auto& auth_request = CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  // Set specific expectations for sharing permission.
  NiceMock<MockSharingPermissionDelegate> mock_sharing_permission_delegate;
  auth_request.SetSharingPermissionDelegateForTests(
      &mock_sharing_permission_delegate);

  EXPECT_CALL(mock_sharing_permission_delegate,
              HasSharingPermissionForAccount(_, _, _))
      .WillOnce(Return(false));
  // TODO(majidvp): Ideally we would use the kRpTestOrigin for second argument
  // but web contents has not navigated to that URL so origin() is null in
  // tests. We should fix this.
  EXPECT_CALL(mock_sharing_permission_delegate,
              GrantSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .Times(1);

  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
      test_case.inputs.prefer_auto_sign_in);
}

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateFailedSignUpNotGrantSharingPermission) {
  const auto& test_case = kFailedMediatedSignUpTestCase;
  auto& auth_request = CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  // Set specific expectations for sharing permission.
  NiceMock<MockSharingPermissionDelegate> mock_sharing_permission_delegate;
  auth_request.SetSharingPermissionDelegateForTests(
      &mock_sharing_permission_delegate);

  EXPECT_CALL(mock_sharing_permission_delegate,
              HasSharingPermissionForAccount(_, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_sharing_permission_delegate,
              GrantSharingPermissionForAccount(_, _, _))
      .Times(0);

  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
      test_case.inputs.prefer_auto_sign_in);
}

TEST_F(BasicFederatedAuthRequestImplTest, AutoSignInForReturningUser) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmAutoSigninFieldTrialParamName, "true"}});

  AccountList displayed_accounts;
  const auto& test_case = kSuccessfulMediatedAutoSignInTestCase;
  auto& auth_request = CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  // Set specific expectations for sharing permission:
  NiceMock<MockSharingPermissionDelegate> mock_sharing_permission_delegate;
  auth_request.SetSharingPermissionDelegateForTests(
      &mock_sharing_permission_delegate);

  // Pretend the sharing permission has been granted for this account.
  //
  // TODO(majidvp): Ideally we would use the kRpTestOrigin for second argument
  // but web contents has not navigated to that URL so origin() is null in
  // tests. We should fix this.
  EXPECT_CALL(mock_sharing_permission_delegate,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              content::WebContents* idp_web_contents,
              const GURL& idp_signin_url,
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
  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
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
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              content::WebContents* idp_web_contents,
              const GURL& idp_signin_url,
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
  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
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
  auto& auth_request = CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  // Set specific expectations for sharing permission:
  NiceMock<MockSharingPermissionDelegate> mock_sharing_permission_delegate;
  auth_request.SetSharingPermissionDelegateForTests(
      &mock_sharing_permission_delegate);

  // Pretend the sharing permission has been granted for this account.
  //
  // TODO(majidvp): Ideally we would use the kRpTestOrigin for second argument
  // but web contents has not navigated to that URL so origin() is null in
  // tests. We should fix this.
  EXPECT_CALL(mock_sharing_permission_delegate,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              content::WebContents* idp_web_contents,
              const GURL& idp_signin_url,
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
  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
      test_case.inputs.prefer_auto_sign_in);

  ASSERT_FALSE(displayed_accounts.empty());
  EXPECT_EQ(displayed_accounts[0].login_state, LoginState::kSignIn);
  EXPECT_EQ(auth_response.second.value(), kToken);
}

TEST_F(FederatedAuthRequestImplTest, Revoke) {
  constexpr char kAccountId[] = "foo@bar.com";

  auto& auth_request = CreateAuthRequest(GURL(kIdpEndpoint));
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
            endpoints.revoke = kRevokeEndpoint;
            std::move(callback).Run(FetchStatus::kSuccess, endpoints,
                                    IdentityProviderMetadata());
          }));
  EXPECT_CALL(*mock_request_manager_, SendRevokeRequest(_, _, _, _))
      .WillOnce(Invoke([&](const GURL& revoke_url, const std::string& client_id,
                           const std::string& account_id,
                           IdpNetworkRequestManager::RevokeCallback callback) {
        EXPECT_EQ(kRevokeEndpoint, revoke_url.spec());
        EXPECT_EQ(kClientId, client_id);
        EXPECT_EQ(kAccountId, account_id);
        std::move(callback).Run(RevokeResponse::kSuccess);
      }));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  auto status = PerformRevokeRequest(kAccountId);
  EXPECT_EQ(RevokeStatus::kSuccess, status);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Status.Revoke", 1);
  histogram_tester_.ExpectBucketCount("Blink.FedCm.Status.Revoke",
                                      RevokeStatusForMetrics::kSuccess, 1);

  ExpectRevokeStatusUKM(RevokeStatusForMetrics::kSuccess);
}

TEST_F(FederatedAuthRequestImplTest, RevokeNoPermission) {
  constexpr char kAccountId[] = "foo@bar.com";

  auto& auth_request = CreateAuthRequest(GURL(kIdpEndpoint));
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

  auto status = PerformRevokeRequest(kAccountId);
  EXPECT_EQ(RevokeStatus::kError, status);

  ukm_loop.Run();
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Status.Revoke", 1);
  histogram_tester_.ExpectBucketCount(
      "Blink.FedCm.Status.Revoke", RevokeStatusForMetrics::kNoAccountToRevoke,
      1);

  ExpectRevokeStatusUKM(RevokeStatusForMetrics::kNoAccountToRevoke);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForSuccessfulSignUpCase) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  auto& auth_request = CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  // Sets specific expectations for sharing permission.
  NiceMock<MockSharingPermissionDelegate> mock_sharing_permission_delegate;
  auth_request.SetSharingPermissionDelegateForTests(
      &mock_sharing_permission_delegate);

  EXPECT_EQ(test_case.config.Mediated_conf.accounts.size(), 1u);

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
      test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(auth_response.second.value(), kToken);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 1);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Status.RequestIdToken", 1);
  histogram_tester_.ExpectBucketCount("Blink.FedCm.Status.RequestIdToken",
                                      IdTokenStatus::kSuccess, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.ContinueOnDialog");
  ExpectTimingUKM("Timing.IdTokenResponse");
  ExpectTimingUKM("Timing.TurnaroundTime");
  ExpectNoTimingUKM("Timing.CancelOnDialog");

  ExpectRequestIdTokenStatusUKM(IdTokenStatus::kSuccess);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForSuccessfulSignInCase) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  auto& auth_request = CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  // Sets specific expectations for sharing permission.
  NiceMock<MockSharingPermissionDelegate> mock_sharing_permission_delegate;
  auth_request.SetSharingPermissionDelegateForTests(
      &mock_sharing_permission_delegate);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(mock_sharing_permission_delegate,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
      test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 1);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Status.RequestIdToken", 1);
  histogram_tester_.ExpectBucketCount("Blink.FedCm.Status.RequestIdToken",
                                      IdTokenStatus::kSuccess, 1);

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
      {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated,
       kNotPreferAutoSignIn},
      {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
       kToken},
      {kToken,
       absl::nullopt,
       FetchStatus::kSuccess,
       kSuccessfulClientId,
       "",
       kAccountsEndpoint,
       kTokenEndpoint,
       kClientMetadataEndpoint,
       kPermissionNoop,
       {FetchStatus::kSuccess, kAccounts, absl::nullopt,
        /*customized_dialog=*/true}}};
  auto& auth_request = CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  // Sets specific expectations for sharing permission:
  NiceMock<MockSharingPermissionDelegate> mock_sharing_permission_delegate;
  auth_request.SetSharingPermissionDelegateForTests(
      &mock_sharing_permission_delegate);

  EXPECT_CALL(*mock_dialog_controller(),
              ShowAccountsDialog(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](content::WebContents* rp_web_contents,
              content::WebContents* idp_web_contents,
              const GURL& idp_signin_url,
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

  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
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

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Status.RequestIdToken", 1);
  histogram_tester_.ExpectBucketCount("Blink.FedCm.Status.RequestIdToken",
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
  auto& auth_request = CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  // Sets specific expectations for sharing permission.
  NiceMock<MockSharingPermissionDelegate> mock_sharing_permission_delegate;
  auth_request.SetSharingPermissionDelegateForTests(
      &mock_sharing_permission_delegate);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(mock_sharing_permission_delegate,
              HasSharingPermissionForAccount(
                  url::Origin::Create(GURL(kIdpTestOrigin)), _, "1234"))
      .WillOnce(Return(true));

  auto auth_response = PerformAuthRequest(
      test_case.inputs.client_id, test_case.inputs.nonce, test_case.inputs.mode,
      test_case.inputs.prefer_auto_sign_in);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  histogram_tester.ExpectBucketCount("Blink.FedCm.WebContentsVisible", 1, 1);
  histogram_tester.ExpectTotalCount("Blink.FedCm.WebContentsVisible", 1);
}

TEST_F(BasicFederatedAuthRequestImplTest, MetricsForWebContentsInvisible) {
  base::HistogramTester histogram_tester;
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->UpdateWebContentsVisibility(Visibility::VISIBLE);
  ASSERT_EQ(web_contents_impl->GetVisibility(), Visibility::VISIBLE);

  const AuthRequestTestCase test_case = {
      "Failed mediated flow due to user leaving the page",
      {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated,
       kNotPreferAutoSignIn},
      {RequestIdTokenStatus::kSuccess, FederatedAuthRequestResult::kSuccess,
       kToken},
      {kToken,
       absl::nullopt,
       FetchStatus::kSuccess,
       kSuccessfulClientId,
       "",
       kAccountsEndpoint,
       kTokenEndpoint,
       kClientMetadataEndpoint,
       kPermissionNoop,
       {FetchStatus::kSuccess, kAccounts, absl::nullopt,
        /*customized_dialog=*/true}}};
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);

  // Sets the WebContents to invisible
  web_contents_impl->UpdateWebContentsVisibility(Visibility::HIDDEN);
  ASSERT_NE(web_contents_impl->GetVisibility(), Visibility::VISIBLE);

  PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                     test_case.inputs.mode,
                     test_case.inputs.prefer_auto_sign_in);

  histogram_tester.ExpectBucketCount("Blink.FedCm.WebContentsVisible", 0, 1);
  histogram_tester.ExpectTotalCount("Blink.FedCm.WebContentsVisible", 1);
}

}  // namespace content
