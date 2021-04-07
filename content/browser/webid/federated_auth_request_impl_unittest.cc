// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "content/browser/webid/id_token_request_callback_data.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

using blink::mojom::RequestIdTokenStatus;
using blink::mojom::RequestMode;
using AccountsResponse = content::IdpNetworkRequestManager::AccountsResponse;
using TokenResponse = content::IdpNetworkRequestManager::TokenResponse;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using SigninResponse = content::IdpNetworkRequestManager::SigninResponse;
using UserApproval = content::IdentityRequestDialogController::UserApproval;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace content {

namespace {

constexpr char kIdpTestOrigin[] = "https://idp.example";
constexpr char kIdpEndpoint[] = "https://idp.example/webid";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kSigninUrl[] = "https://idp.example/signin";

// Values will be added here as token introspection is implemented.
constexpr char kAuthRequest[] = "";
constexpr char kToken[] = "[not a real token]";
constexpr char kEmptyToken[] = "";

static const std::initializer_list<IdentityRequestAccount> kAccounts{{
    "1234",                            // sub
    "ken@idp.example",                 // email
    "Ken R. Example",                  // name
    "Ken",                             // given_name
    "https://idp.example/profile/567"  // picture
}};

// Parameters for a call to RequestIdToken.
typedef struct {
  const char* provider;
  const char* request;
  RequestMode mode;
} RequestParameters;

// Expected return values from a call to RequestIdToken.
typedef struct {
  RequestIdTokenStatus return_status;
  const char* token;
} RequestExpectations;

// Mock configuration values for test.
typedef struct {
  base::Optional<SigninResponse> signin_response;
  const char* signin_url_or_token;
  base::Optional<UserApproval> token_permission;
} MockPermissionConfiguration;

typedef struct {
  base::Optional<AccountsResponse> accounts_response;
  IdpNetworkRequestManager::AccountList accounts;
  base::Optional<TokenResponse> token_response;
} MockMediatedConfiguration;

typedef struct {
  const char* token;
  base::Optional<UserApproval> initial_permission;
  base::Optional<FetchStatus> wellknown_fetch_status;
  const char* idp_endpoint;
  const char* accounts_endpoint;
  const char* token_endpoint;
  MockPermissionConfiguration Permission_conf;
  MockMediatedConfiguration Mediated_conf;
} MockConfiguration;

// base::Optional fields should be nullopt to prevent the corresponding
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

static const MockMediatedConfiguration kMediatedNoop{base::nullopt, kAccounts,
                                                     base::nullopt};
static const MockPermissionConfiguration kPermissionNoop{base::nullopt, "",
                                                         base::nullopt};

static const AuthRequestTestCase kPermissionTestCases[]{
    {"Successful run with the IdP page loaded",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
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
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
     {RequestIdTokenStatus::kSuccess, kToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kTokenGranted, kToken, base::nullopt},
      kMediatedNoop}},

    {"Initial user permission denied",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
     {RequestIdTokenStatus::kApprovalDeclined, kEmptyToken},
     {kToken, UserApproval::kDenied, base::nullopt, "", "", "", kPermissionNoop,
      kMediatedNoop}},

    {"Wellknown file not found",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorWebIdNotSupportedByProvider, kEmptyToken},
     {kToken, UserApproval::kApproved, FetchStatus::kWebIdNotSupported, "", "",
      "", kPermissionNoop, kMediatedNoop}},

    {"Wellknown fetch error",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorFetchingWellKnown, kEmptyToken},
     {kToken, UserApproval::kApproved, FetchStatus::kFetchError, "", "", "",
      kPermissionNoop, kMediatedNoop}},

    {"Error parsing wellknown for Permission mode",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorInvalidWellKnown, kEmptyToken},
     {kToken, UserApproval::kApproved, FetchStatus::kInvalidResponseError, "",
      kAccountsEndpoint, kTokenEndpoint, kPermissionNoop, kMediatedNoop}},

    {"Error reaching the idpendpoint",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorFetchingSignin, kEmptyToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kSigninError, "", base::nullopt},
      kMediatedNoop}},

    {"Error parsing the idpendpoint response",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
     {RequestIdTokenStatus::kErrorInvalidSigninResponse, kEmptyToken},
     {kToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kInvalidResponseError, "", base::nullopt},
      kMediatedNoop}},

    {"IdP window closed before token provision",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
     {RequestIdTokenStatus::kError, kEmptyToken},
     {kEmptyToken,
      UserApproval::kApproved,
      FetchStatus::kSuccess,
      kIdpEndpoint,
      "",
      "",
      {SigninResponse::kLoadIdp, kSigninUrl, base::nullopt},
      kMediatedNoop}},

    {"Token provision declined by user after IdP window closed",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kPermission},
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
     {kIdpTestOrigin, kAuthRequest, RequestMode::kMediated},
     {RequestIdTokenStatus::kErrorInvalidWellKnown, kEmptyToken},
     {kToken, base::nullopt, FetchStatus::kInvalidResponseError, kIdpEndpoint,
      kAccountsEndpoint, "", kPermissionNoop, kMediatedNoop}},

    {"Error parsing wellknown for Mediated mode missing accounts endpoint",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kMediated},
     {RequestIdTokenStatus::kErrorInvalidWellKnown, kEmptyToken},
     {kToken, base::nullopt, FetchStatus::kInvalidResponseError, kIdpEndpoint,
      "", kTokenEndpoint, kPermissionNoop, kMediatedNoop}},

    {"Error reaching Accounts endpoint",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kMediated},
     {RequestIdTokenStatus::kError, kEmptyToken},
     {kEmptyToken,
      base::nullopt,
      FetchStatus::kSuccess,
      "",
      kAccountsEndpoint,
      kTokenEndpoint,
      kPermissionNoop,
      {AccountsResponse::kNetError, kAccounts, base::nullopt}}},

    {"Error parsing Accounts response",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kMediated},
     {RequestIdTokenStatus::kErrorInvalidAccountsResponse, kEmptyToken},
     {kToken,
      base::nullopt,
      FetchStatus::kSuccess,
      "",
      kAccountsEndpoint,
      kTokenEndpoint,
      kPermissionNoop,
      {AccountsResponse::kInvalidResponseError, kAccounts, base::nullopt}}},

    {"Successful Mediated flow",
     {kIdpTestOrigin, kAuthRequest, RequestMode::kMediated},
     {RequestIdTokenStatus::kSuccess, kToken},
     {kToken,
      base::nullopt,
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
  base::Optional<std::string> token() const { return token_; }

  // This can only be called once per lifetime of this object.
  base::OnceCallback<void(RequestIdTokenStatus,
                          const base::Optional<std::string>&)>
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
                      const base::Optional<std::string>& token) {
    status_ = status;
    token_ = token;
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  RequestIdTokenStatus status_;
  base::Optional<std::string> token_;
};

}  // namespace

class FederatedAuthRequestImplTest : public RenderViewHostTestHarness {
 protected:
  FederatedAuthRequestImplTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FederatedAuthRequestImplTest() override = default;

  void CreateAuthRequest(const GURL& provider) {
    provider_ = provider;
    auth_request_impl_ = std::make_unique<FederatedAuthRequestImpl>(
        main_rfh(), request_remote_.BindNewPipeAndPassReceiver());
    mock_request_manager_ =
        std::make_unique<NiceMock<MockIdpNetworkRequestManager>>(provider,
                                                                 main_rfh());
    mock_dialog_controller_ =
        std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
  }

  std::pair<RequestIdTokenStatus, base::Optional<std::string>>
  PerformAuthRequest(const std::string& request,
                     blink::mojom::RequestMode mode) {
    auth_request_impl_->SetNetworkManagerForTests(
        std::move(mock_request_manager_));
    auth_request_impl_->SetDialogControllerForTests(
        std::move(mock_dialog_controller_));

    AuthRequestCallbackHelper auth_helper;
    request_remote_->RequestIdToken(provider_, request, mode,
                                    auth_helper.callback());
    auth_helper.WaitForCallback();
    return std::make_pair(auth_helper.status(), auth_helper.token());
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
                  const GURL& idp_signin_url,
                  IdpNetworkRequestManager::AccountList accounts,
                  IdentityRequestDialogController::AccountSelectionCallback
                      on_selected) {
                std::move(on_selected).Run(accounts[0].sub);
              }));
    }

    if (conf.token_response == TokenResponse::kSuccess) {
      EXPECT_CALL(*mock_request_manager_, SendTokenRequest(_, _, _, _))
          .WillOnce(Invoke(
              [=](const GURL& idp_signin_url, const std::string& account_id,
                  const std::string& request,
                  IdpNetworkRequestManager::TokenRequestCallback callback) {
                std::move(callback).Run(*conf.token_response, token);
              }));
    }
  }

  void SetMockExpectations(const AuthRequestTestCase& test_case) {
    if (test_case.config.initial_permission) {
      EXPECT_CALL(*mock_dialog_controller_,
                  ShowInitialPermissionDialog(_, _, _))
          .WillOnce(Invoke(
              [&](WebContents*, const GURL&,
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

 private:
  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  std::unique_ptr<FederatedAuthRequestImpl> auth_request_impl_;

  std::unique_ptr<NiceMock<MockIdpNetworkRequestManager>> mock_request_manager_;
  std::unique_ptr<NiceMock<MockIdentityRequestDialogController>>
      mock_dialog_controller_;

  base::OnceClosure close_idp_window_callback_;

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
      PerformAuthRequest(test_case.inputs.request, test_case.inputs.mode);
  EXPECT_EQ(auth_response.first, test_case.expected.return_status);
  EXPECT_EQ(auth_response.second, test_case.expected.token);
}

}  // namespace content
