// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/optional.h"
#include "base/run_loop.h"
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

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace content {

namespace {

constexpr char kIdpTestOrigin[] = "https://idp.example";
constexpr char kIdpEndpoint[] = "https://idp.example/webid";
constexpr char kSigninUrl[] = "https://idp.example/signin";

// Values will be added here as token introspection is implemented.
constexpr char kAuthRequest[] = "";
constexpr char kToken[] = "[not a real token]";
constexpr char kEmptyToken[] = "";

// Parameters for a call to RequestIdToken.
typedef struct {
  const char* provider;
  const char* request;
} RequestParameters;

// Expected return values from a call to RequestIdToken.
typedef struct {
  RequestIdTokenStatus return_status;
  const char* token;
} RequestExpectations;

// Mock configuration values for test.
typedef struct {
  const char* token;
  IdentityRequestDialogController::UserApproval initial_permission;
  base::Optional<IdpNetworkRequestManager::FetchStatus> wellknown_fetch_status;
  const char* idp_endpoint;
  base::Optional<IdpNetworkRequestManager::SigninResponse> signin_response;
  const char* signin_url_or_token;
  base::Optional<IdentityRequestDialogController::UserApproval>
      token_permission;
} MockConfiguration;

// base::Optional fields should be nullopt to prevent the corresponding
// methods from having EXPECT_CALL set on the mocks.
typedef struct {
  RequestParameters inputs;
  RequestExpectations expected;
  MockConfiguration config;
} AuthRequestTestCase;

constexpr AuthRequestTestCase kAllTestCases[]{
    // Successful run with the IdP page loaded.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kSuccess, kToken},
     {kToken, IdentityRequestDialogController::UserApproval::kApproved,
      IdpNetworkRequestManager::FetchStatus::kSuccess, kIdpEndpoint,
      IdpNetworkRequestManager::SigninResponse::kLoadIdp, kSigninUrl,
      IdentityRequestDialogController::UserApproval::kApproved}},

    // Successful run with a token response from the idp_endpoint.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kSuccess, kToken},
     {kToken, IdentityRequestDialogController::UserApproval::kApproved,
      IdpNetworkRequestManager::FetchStatus::kSuccess, kIdpEndpoint,
      IdpNetworkRequestManager::SigninResponse::kTokenGranted, kToken,
      base::nullopt}},

    // Initial user permission denied.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kApprovalDeclined, kEmptyToken},
     {kToken, IdentityRequestDialogController::UserApproval::kDenied,
      base::nullopt, "", base::nullopt, "", base::nullopt}},

    // Well-known file not found.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kErrorWebIdNotSupportedByProvider, kEmptyToken},
     {kToken, IdentityRequestDialogController::UserApproval::kApproved,
      IdpNetworkRequestManager::FetchStatus::kWebIdNotSupported, "",
      base::nullopt, "", base::nullopt}},

    // Well-known fetch error.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kErrorFetchingWellKnown, kEmptyToken},
     {kToken, IdentityRequestDialogController::UserApproval::kApproved,
      IdpNetworkRequestManager::FetchStatus::kFetchError, "", base::nullopt, "",
      base::nullopt}},

    // Error parsing well-known.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kErrorInvalidWellKnown, kEmptyToken},
     {kToken, IdentityRequestDialogController::UserApproval::kApproved,
      IdpNetworkRequestManager::FetchStatus::kInvalidResponseError, "",
      base::nullopt, "", base::nullopt}},

    // Error reaching the idp_endpoint.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kErrorFetchingSignin, kEmptyToken},
     {kToken, IdentityRequestDialogController::UserApproval::kApproved,
      IdpNetworkRequestManager::FetchStatus::kSuccess, kIdpEndpoint,
      IdpNetworkRequestManager::SigninResponse::kSigninError, "",
      base::nullopt}},

    // Error parsing the idp_endpoint response.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kErrorInvalidSigninResponse, kEmptyToken},
     {kToken, IdentityRequestDialogController::UserApproval::kApproved,
      IdpNetworkRequestManager::FetchStatus::kSuccess, kIdpEndpoint,
      IdpNetworkRequestManager::SigninResponse::kInvalidResponseError, "",
      base::nullopt}},

    // IdP window closed before token provision.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kError, kEmptyToken},
     {kEmptyToken, IdentityRequestDialogController::UserApproval::kApproved,
      IdpNetworkRequestManager::FetchStatus::kSuccess, kIdpEndpoint,
      IdpNetworkRequestManager::SigninResponse::kLoadIdp, kSigninUrl,
      base::nullopt}},

    // Token provision declined by user after IdP window closed.
    {{kIdpTestOrigin, kAuthRequest},
     {RequestIdTokenStatus::kApprovalDeclined, kEmptyToken},
     {kToken, IdentityRequestDialogController::UserApproval::kApproved,
      IdpNetworkRequestManager::FetchStatus::kSuccess, kIdpEndpoint,
      IdpNetworkRequestManager::SigninResponse::kLoadIdp, kSigninUrl,
      IdentityRequestDialogController::UserApproval::kDenied}},

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
  PerformAuthRequest(const std::string& request) {
    auth_request_impl_->SetNetworkManagerForTests(
        std::move(mock_request_manager_));
    auth_request_impl_->SetDialogControllerForTests(
        std::move(mock_dialog_controller_));

    AuthRequestCallbackHelper auth_helper;
    request_remote_->RequestIdToken(provider_, request, auth_helper.callback());
    auth_helper.WaitForCallback();
    return std::make_pair(auth_helper.status(), auth_helper.token());
  }

  void SetMockExpectations(const AuthRequestTestCase& test_case) {
    EXPECT_CALL(*mock_dialog_controller_, ShowInitialPermissionDialog(_, _, _))
        .WillOnce(
            Invoke([&](WebContents*, const GURL&,
                       IdentityRequestDialogController::InitialApprovalCallback
                           callback) {
              std::move(callback).Run(test_case.config.initial_permission);
            }));

    if (test_case.config.wellknown_fetch_status) {
      EXPECT_CALL(*mock_request_manager_, FetchIdpWellKnown(_))
          .WillOnce(Invoke(
              [&](IdpNetworkRequestManager::FetchWellKnownCallback callback) {
                std::move(callback).Run(
                    *test_case.config.wellknown_fetch_status,
                    test_case.config.idp_endpoint);
              }));
    }

    if (test_case.config.signin_response) {
      EXPECT_CALL(*mock_request_manager_, SendSigninRequest(_, _, _))
          .WillOnce(Invoke(
              [&](const GURL&, const std::string&,
                  IdpNetworkRequestManager::SigninRequestCallback callback) {
                std::move(callback).Run(*test_case.config.signin_response,
                                        test_case.config.signin_url_or_token);
              }));
    }

    // The IdP dialog only shows when kLoadIdP is the return code from the
    // signin request.
    if (test_case.config.signin_response ==
        IdpNetworkRequestManager::SigninResponse::kLoadIdp) {
      EXPECT_CALL(*mock_dialog_controller_, ShowIdProviderWindow(_, _, _, _))
          .WillOnce(Invoke([&](WebContents*, WebContents* idp_web_contents,
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
            std::move(rp_done_callback).Run(test_case.config.token);
          }));

      EXPECT_CALL(*mock_dialog_controller_, CloseIdProviderWindow())
          .WillOnce(
              Invoke([&]() { std::move(close_idp_window_callback_).Run(); }));
    }

    if (test_case.config.token_permission) {
      EXPECT_CALL(*mock_dialog_controller_,
                  ShowTokenExchangePermissionDialog(_, _, _))
          .WillOnce(Invoke(
              [&](content::WebContents* idp_web_contents, const GURL& idp_url,
                  IdentityRequestDialogController::TokenExchangeApprovalCallback
                      callback) {
                std::move(callback).Run(*test_case.config.token_permission);
              }));
    }
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

INSTANTIATE_TEST_SUITE_P(FederatedAuthRequestImplTests,
                         BasicFederatedAuthRequestImplTest,
                         ::testing::ValuesIn(kAllTestCases));

// Exercise all configured test cases in kAllTestCases.
TEST_P(BasicFederatedAuthRequestImplTest, FederatedAuthRequests) {
  AuthRequestTestCase test_case = GetParam();
  CreateAuthRequest(GURL(test_case.inputs.provider));
  SetMockExpectations(test_case);
  auto auth_response = PerformAuthRequest(test_case.inputs.request);
  EXPECT_EQ(auth_response.first, test_case.expected.return_status);
  EXPECT_EQ(auth_response.second, test_case.expected.token);
}

}  // namespace content
