// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "content/browser/webid/id_token_request_callback_data.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_request_permission_delegate.h"
#include "content/browser/webid/test/mock_sharing_permission_delegate.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::LogoutStatus;
using blink::mojom::RequestIdTokenStatus;
using blink::mojom::RequestMode;
using AccountsResponse = content::IdpNetworkRequestManager::AccountsResponse;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using LogoutResponse = content::IdpNetworkRequestManager::LogoutResponse;
using SigninResponse = content::IdpNetworkRequestManager::SigninResponse;
using TokenResponse = content::IdpNetworkRequestManager::TokenResponse;
using UserApproval = content::IdentityRequestDialogController::UserApproval;
using AccountList = content::IdentityRequestDialogController::AccountList;
using LoginState = content::IdentityRequestAccount::LoginState;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

constexpr char kRpTestOrigin[] = "https://rp.example";
constexpr char kIdpTestOrigin[] = "https://idp.example";
constexpr char kIdpEndpoint[] = "https://idp.example/webid";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kSigninUrl[] = "https://idp.example/signin";
constexpr char kClientId[] = "client_id_123";
constexpr char kNonce[] = "nonce123";

// Values will be added here as token introspection is implemented.
constexpr char kToken[] = "[not a real token]";
constexpr char kEmptyToken[] = "";

static const std::initializer_list<IdentityRequestAccount> kAccounts{{
    "1234",             // sub
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
} RequestParameters;

// Expected return values from a call to RequestIdToken.
typedef struct {
  RequestIdTokenStatus return_status;
  const char* token;
} RequestExpectations;

// Mock configuration values for test.
typedef struct {
  absl::optional<SigninResponse> signin_response;
  const char* signin_url_or_token;
  absl::optional<UserApproval> token_permission;
} MockPermissionConfiguration;

typedef struct {
  absl::optional<AccountsResponse> accounts_response;
  AccountList accounts;
  absl::optional<TokenResponse> token_response;
} MockMediatedConfiguration;

typedef struct {
  const char* token;
  absl::optional<UserApproval> initial_permission;
  absl::optional<FetchStatus> wellknown_fetch_status;
  const char* idp_endpoint;
  const char* accounts_endpoint;
  const char* token_endpoint;
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

static const AuthRequestTestCase kPermissionTestCases[]{
    {"Successful run with the IdP page loaded",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kSuccess, kToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kLoadIdp, kSigninUrl, UserApproval::kApproved},
      kMediatedNoop}},

    {"Successful run with a token response from the idp_endpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kSuccess, kToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kTokenGranted, kToken, absl::nullopt},
      kMediatedNoop}},

    {"Initial user permission denied",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kApprovalDeclined, kEmptyToken},
     {kToken, UserApproval::kDenied, absl::nullopt, "", "", "", kPermissionNoop,
      kMediatedNoop}},

    {"Wellknown file not found",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorWebIdNotSupportedByProvider, kEmptyToken},
     {kToken, UserApproval::kApproved, FetchStatus::kWebIdNotSupported, "", "",
      "", kPermissionNoop, kMediatedNoop}},

    {"Wellknown fetch error",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorFetchingWellKnown, kEmptyToken},
     {kToken, UserApproval::kApproved, FetchStatus::kFetchError, "", "", "",
      kPermissionNoop, kMediatedNoop}},

    {"Error parsing wellknown for Permission mode",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorInvalidWellKnown, kEmptyToken},
     {kToken, UserApproval::kApproved, FetchStatus::kInvalidResponseError, "",
      kAccountsEndpoint, kTokenEndpoint, kPermissionNoop, kMediatedNoop}},

    {"Error reaching the idpendpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorFetchingSignin, kEmptyToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kSigninError, "", absl::nullopt},
      kMediatedNoop}},

    {"Error parsing the idpendpoint response",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorInvalidSigninResponse, kEmptyToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kInvalidResponseError, "", absl::nullopt},
      kMediatedNoop}},

    {"IdP window closed before token provision",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kError, kEmptyToken},
     {kEmptyToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kLoadIdp, kSigninUrl, absl::nullopt},
      kMediatedNoop}},

    {"Token provision declined by user after IdP window closed",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kPermission},
     {RequestIdTokenStatus::kApprovalDeclined, kEmptyToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kLoadIdp, kSigninUrl, UserApproval::kDenied},
      kMediatedNoop}}};

static const AuthRequestTestCase kMediatedTestCases[]{
    {"Error parsing wellknown for Mediated mode missing token endpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kErrorInvalidWellKnown, kEmptyToken},
     {kToken, absl::nullopt, FetchStatus::kInvalidResponseError, kIdpEndpoint,
      kAccountsEndpoint, "", kPermissionNoop, kMediatedNoop}},

    {"Error parsing wellknown for Mediated mode missing accounts endpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kErrorInvalidWellKnown, kEmptyToken},
     {kToken, absl::nullopt, FetchStatus::kInvalidResponseError, kIdpEndpoint,
      "", kTokenEndpoint, kPermissionNoop, kMediatedNoop}},

    {"Error reaching Accounts endpoint",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kError, kEmptyToken},
     {kEmptyToken,
      absl::nullopt,
      FetchStatus::kSuccess,
      "",
      kAccountsEndpoint,
      kTokenEndpoint,
      kPermissionNoop,
      {AccountsResponse::kNetError, kAccounts, absl::nullopt}}},

    {"Error parsing Accounts response",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kErrorInvalidAccountsResponse, kEmptyToken},
     {kToken,
      absl::nullopt,
      FetchStatus::kSuccess,
      "",
      kAccountsEndpoint,
      kTokenEndpoint,
      kPermissionNoop,
      {AccountsResponse::kInvalidResponseError, kAccounts, absl::nullopt}}},

    {"Successful Mediated flow",
     {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
     {RequestIdTokenStatus::kSuccess, kToken},
     {kToken,
      absl::nullopt,
      FetchStatus::kSuccess,
      "",
      kAccountsEndpoint,
      kTokenEndpoint,
      kPermissionNoop,
      {AccountsResponse::kSuccess, kAccounts, TokenResponse::kSuccess}}},
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
class LogoutRequestCallbackHelper {
 public:
  LogoutRequestCallbackHelper() = default;
  ~LogoutRequestCallbackHelper() = default;

  LogoutRequestCallbackHelper(const LogoutRequestCallbackHelper&) = delete;
  LogoutRequestCallbackHelper& operator=(const LogoutRequestCallbackHelper&) =
      delete;

  LogoutStatus status() const { return status_; }

  // This can only be called once per lifetime of this object.
  base::OnceCallback<void(LogoutStatus)> callback() {
    return base::BindOnce(&LogoutRequestCallbackHelper::ReceiverMethod,
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
  void ReceiverMethod(LogoutStatus status) {
    status_ = status;
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  LogoutStatus status_;
};

}  // namespace

class FederatedAuthRequestImplTest : public RenderViewHostTestHarness {
 protected:
  FederatedAuthRequestImplTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FederatedAuthRequestImplTest() override = default;

  FederatedAuthRequestImpl& CreateAuthRequest(const GURL& provider) {
    provider_ = provider;
    auth_request_impl_ = std::make_unique<FederatedAuthRequestImpl>(
        main_rfh(), request_remote_.BindNewPipeAndPassReceiver());
    mock_request_manager_ =
        std::make_unique<NiceMock<MockIdpNetworkRequestManager>>(
            provider, url::Origin::Create(GURL(kRpTestOrigin)));
    mock_dialog_controller_ =
        std::make_unique<NiceMock<MockIdentityRequestDialogController>>();

    mock_request_permission_delegate_ =
        std::make_unique<NiceMock<MockRequestPermissionDelegate>>();

    return *auth_request_impl_;
  }

  std::pair<RequestIdTokenStatus, absl::optional<std::string>>
  PerformAuthRequest(const std::string& client_id,
                     const std::string& nonce,
                     blink::mojom::RequestMode mode) {
    auth_request_impl_->SetNetworkManagerForTests(
        std::move(mock_request_manager_));
    auth_request_impl_->SetDialogControllerForTests(
        std::move(mock_dialog_controller_));

    AuthRequestCallbackHelper auth_helper;
    request_remote_->RequestIdToken(provider_, client_id, nonce, mode,
                                    auth_helper.callback());
    auth_helper.WaitForCallback();
    return std::make_pair(auth_helper.status(), auth_helper.token());
  }

  LogoutStatus PerformLogoutRequest(
      const std::vector<std::string>& logout_endpoints) {
    auth_request_impl_->SetNetworkManagerForTests(
        std::move(mock_request_manager_));
    auth_request_impl_->SetRequestPermissionDelegateForTests(
        mock_request_permission_delegate_.get());

    LogoutRequestCallbackHelper logout_helper;
    request_remote_->Logout(logout_endpoints, logout_helper.callback());
    logout_helper.WaitForCallback();
    return logout_helper.status();
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
                                   std::string token) {
    if (conf.accounts_response) {
      EXPECT_CALL(*mock_request_manager_, SendAccountsRequest(_, _))
          .WillOnce(Invoke(
              [&](const GURL&,
                  IdpNetworkRequestManager::AccountsRequestCallback callback) {
                std::move(callback).Run(*conf.accounts_response, conf.accounts);
              }));
    }

    if (conf.accounts_response == AccountsResponse::kSuccess) {
      EXPECT_CALL(*mock_dialog_controller_, ShowAccountsDialog(_, _, _, _, _))
          .WillOnce(Invoke(
              [&](content::WebContents* rp_web_contents,
                  content::WebContents* idp_web_contents,
                  const GURL& idp_signin_url, AccountList accounts,
                  IdentityRequestDialogController::AccountSelectionCallback
                      on_selected) {
                displayed_accounts_ = accounts;
                std::move(on_selected).Run(accounts[0].sub);
              }));
    }

    if (conf.token_response) {
      auto delivered_token = conf.token_response == TokenResponse::kSuccess
                                 ? token
                                 : std::string();
      EXPECT_CALL(*mock_request_manager_, SendTokenRequest(_, _, _, _))
          .WillOnce(Invoke(
              [=](const GURL& idp_signin_url, const std::string& account_id,
                  const std::string& request,
                  IdpNetworkRequestManager::TokenRequestCallback callback) {
                std::move(callback).Run(*conf.token_response, delivered_token);
              }));
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
    }

    if (test_case.config.wellknown_fetch_status) {
      EXPECT_CALL(*mock_request_manager_, FetchIdpWellKnown(_))
          .WillOnce(Invoke(
              [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
                std::move(callback).Run(
                    *test_case.config.wellknown_fetch_status,
                    {test_case.config.idp_endpoint,
                     test_case.config.accounts_endpoint,
                     test_case.config.token_endpoint});
              }));
    }

    SetPermissionMockExpectations(test_case.config.Permission_conf,
                                  test_case.config.token);
    SetMediatedMockExpectations(test_case.config.Mediated_conf,
                                test_case.config.token);
  }

  // Expectations have to be set explicitly in advance using
  // logout_return_status() and logout_endpoints().
  void SetLogoutMockExpectations() {
    if (logout_request_permissions_.size() == 0) {
      EXPECT_CALL(*mock_request_permission_delegate_,
                  HasRequestPermission(_, _))
          .Times(0);
    } else {
      for (int i = logout_request_permissions_.size() - 1; i >= 0; i--) {
        auto single_logout_request_permission = logout_request_permissions_[i];
        EXPECT_CALL(*mock_request_permission_delegate_,
                    HasRequestPermission(_, _))
            .WillOnce(Return(single_logout_request_permission))
            .RetiresOnSaturation();
      }
    }

    if (logout_return_status_.size() == 0) {
      EXPECT_CALL(*mock_request_manager_, SendLogout(_, _)).Times(0);
    } else {
      for (int i = logout_return_status_.size() - 1; i >= 0; i--) {
        auto single_logout_return_status = logout_return_status_[i];
        EXPECT_CALL(*mock_request_manager_, SendLogout(_, _))
            .WillOnce(
                Invoke([single_logout_return_status](
                           const GURL& logout_endpoint,
                           IdpNetworkRequestManager::LogoutCallback callback) {
                  std::move(callback).Run(single_logout_return_status);
                }))
            .RetiresOnSaturation();
      }
    }
  }

  std::vector<LogoutResponse>& logout_return_status() {
    return logout_return_status_;
  }
  std::vector<std::string>& logout_endpoints() { return logout_endpoints_; }
  std::vector<bool>& logout_request_permissions() {
    return logout_request_permissions_;
  }

  const AccountList& displayed_accounts() const { return displayed_accounts_; }

 private:
  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  std::unique_ptr<FederatedAuthRequestImpl> auth_request_impl_;

  std::unique_ptr<NiceMock<MockIdpNetworkRequestManager>> mock_request_manager_;
  std::unique_ptr<NiceMock<MockIdentityRequestDialogController>>
      mock_dialog_controller_;
  std::unique_ptr<NiceMock<MockRequestPermissionDelegate>>
      mock_request_permission_delegate_;

  base::OnceClosure close_idp_window_callback_;

  // Test case storage for Logout tests.
  std::vector<LogoutResponse> logout_return_status_;
  std::vector<std::string> logout_endpoints_;
  std::vector<bool> logout_request_permissions_;

  // Storage for displayed accounts
  AccountList displayed_accounts_;

  GURL provider_;
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
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.mode);
  EXPECT_EQ(auth_response.first, test_case.expected.return_status);
  EXPECT_EQ(auth_response.second, test_case.expected.token);
}

// Test Logout method success with multiple relying parties.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutSuccessMultiple) {
  CreateAuthRequest(GURL(kIdpTestOrigin));

  logout_endpoints().push_back("https://rp1.example");
  logout_return_status().push_back(LogoutResponse::kSuccess);
  logout_request_permissions().push_back(true);
  logout_endpoints().push_back("https://rp2.example");
  logout_return_status().push_back(LogoutResponse::kSuccess);
  logout_request_permissions().push_back(true);
  logout_endpoints().push_back("https://rp3.example");
  logout_return_status().push_back(LogoutResponse::kSuccess);
  logout_request_permissions().push_back(true);

  SetLogoutMockExpectations();
  auto logout_response = PerformLogoutRequest(logout_endpoints());
  EXPECT_EQ(logout_response, LogoutStatus::kSuccess);
}

// Test Logout method with an invalid endpoint URL.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutInvalidEndpoint) {
  CreateAuthRequest(GURL(kIdpTestOrigin));

  logout_endpoints().push_back("Invalid string");

  SetLogoutMockExpectations();
  auto logout_response = PerformLogoutRequest(logout_endpoints());
  EXPECT_EQ(logout_response, LogoutStatus::kError);
}

// Test an error is returned if one logout request fails.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutSingleFailure) {
  CreateAuthRequest(GURL(kIdpTestOrigin));

  logout_endpoints().push_back("https://rp1.example");
  logout_return_status().push_back(LogoutResponse::kSuccess);
  logout_request_permissions().push_back(true);
  logout_endpoints().push_back("https://rp2.example");
  logout_return_status().push_back(LogoutResponse::kError);
  logout_request_permissions().push_back(true);

  SetLogoutMockExpectations();
  auto logout_response = PerformLogoutRequest(logout_endpoints());
  EXPECT_EQ(logout_response, LogoutStatus::kSuccess);
}

// Test Logout method with an empty endpoint vector.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutNoEndpoints) {
  CreateAuthRequest(GURL(kIdpTestOrigin));

  SetLogoutMockExpectations();
  auto logout_response = PerformLogoutRequest(logout_endpoints());
  EXPECT_EQ(logout_response, LogoutStatus::kError);
}

// Test Logout without request permission granted.
TEST_F(BasicFederatedAuthRequestImplTest, LogoutWithoutPermission) {
  CreateAuthRequest(GURL(kIdpTestOrigin));

  // logout_return_status() not set because there should be no
  // attempt at dispatch.
  logout_endpoints().push_back("https://rp1.example");
  logout_request_permissions().push_back(false);

  SetLogoutMockExpectations();
  auto logout_response = PerformLogoutRequest(logout_endpoints());
  EXPECT_EQ(logout_response, LogoutStatus::kError);
}

// Tests for Login State

static const AuthRequestTestCase kSuccessfulMediatedSignUpTestCase{
    "Successful mediated flow with one account",
    {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
    {RequestIdTokenStatus::kSuccess, kToken},
    {kToken,
     absl::nullopt,
     FetchStatus::kSuccess,
     "",
     kAccountsEndpoint,
     kTokenEndpoint,
     kPermissionNoop,
     {AccountsResponse::kSuccess, kAccounts, TokenResponse::kSuccess}}};

static const AuthRequestTestCase kFailedMediatedSignUpTestCase{
    "Failed mediated flow with one account",
    {kIdpTestOrigin, kClientId, kNonce, RequestMode::kMediated},
    {RequestIdTokenStatus::kSuccess, kToken},
    {kToken,
     absl::nullopt,
     FetchStatus::kSuccess,
     "",
     kAccountsEndpoint,
     kTokenEndpoint,
     kPermissionNoop,
     {AccountsResponse::kSuccess, kAccounts,
      TokenResponse::kInvalidResponseError}}};

TEST_F(BasicFederatedAuthRequestImplTest,
       LoginStateShouldBeSignUpForFirstTimeUser) {
  const auto& test_case = kSuccessfulMediatedSignUpTestCase;
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.mode);

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

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.mode);
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

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.mode);
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

  auto auth_response =
      PerformAuthRequest(test_case.inputs.client_id, test_case.inputs.nonce,
                         test_case.inputs.mode);
}

}  // namespace content
