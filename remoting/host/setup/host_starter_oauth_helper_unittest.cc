// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/host_starter_oauth_helper.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
constexpr char kAuthorizationCodeValue[] = "LEGIT_AUTHORIZATION_CODE";
constexpr char kAccessTokenValue[] = "LEGIT_ACCESS_TOKEN";
constexpr char kRefreshTokenValue[] = "LEGIT_REFRESH_TOKEN";
constexpr char kGetTokensResponse[] = R"({
            "refresh_token": "LEGIT_REFRESH_TOKEN",
            "access_token": "LEGIT_ACCESS_TOKEN",
            "expires_in": 3600,
            "token_type": "Bearer"
         })";
constexpr char kTestEmail[] = "user@test.com";
constexpr char kDifferentTestEmail[] = "different_user@test.com";
constexpr char kGetUserEmailResponse[] = R"({"email": "user@test.com"})";
}  // namespace

class HostStarterOAuthHelperTest : public testing::Test {
 public:
  HostStarterOAuthHelperTest();
  ~HostStarterOAuthHelperTest() override;

  void SetUp() override;

  void OnTokensRetrieved(const std::string& user_email,
                         const std::string& access_token,
                         const std::string& refresh_token,
                         const std::string& scopes);
  void HandleOAuthError(const std::string& error_message,
                        HostStarter::Result error_result);

 protected:
  void RunUntilQuit();
  void AddGetTokenResponse(const std::string& response);
  void AddGetUserEmailResponse(const std::string& response);
  void AddGetTokenErrorResponse(net::HttpStatusCode status);
  void AddGetUserEmailErrorResponse(net::HttpStatusCode status);

  HostStarterOAuthHelper& host_starter_oauth_helper() {
    return *host_starter_oauth_helper_;
  }
  std::string& access_token() { return access_token_; }
  std::string& refresh_token() { return refresh_token_; }
  std::string& user_email() { return user_email_; }
  std::string& scopes() { return scopes_; }
  std::string& error_message() { return error_message_; }
  std::optional<HostStarter::Result>& error_result() { return error_result_; }

 private:
  std::string access_token_;
  std::string refresh_token_;
  std::string user_email_;
  std::string scopes_;
  std::string error_message_;
  std::optional<HostStarter::Result> error_result_;

  base::RepeatingClosure quit_closure_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<HostStarterOAuthHelper> host_starter_oauth_helper_;
};

HostStarterOAuthHelperTest::HostStarterOAuthHelperTest() = default;

HostStarterOAuthHelperTest::~HostStarterOAuthHelperTest() = default;

void HostStarterOAuthHelperTest::SetUp() {
  access_token_.clear();
  refresh_token_.clear();
  user_email_.clear();
  scopes_.clear();
  error_message_.clear();
  error_result_.reset();

  shared_url_loader_factory_ = test_url_loader_factory_.GetSafeWeakWrapper();
  host_starter_oauth_helper_ =
      std::make_unique<HostStarterOAuthHelper>(shared_url_loader_factory_);
  quit_closure_ = task_environment_.QuitClosure();
}

void HostStarterOAuthHelperTest::OnTokensRetrieved(
    const std::string& user_email,
    const std::string& access_token,
    const std::string& refresh_token,
    const std::string& scopes) {
  access_token_ = access_token;
  refresh_token_ = refresh_token;
  user_email_ = user_email;
  scopes_ = scopes;
  quit_closure_.Run();
}

void HostStarterOAuthHelperTest::HandleOAuthError(
    const std::string& error_message,
    HostStarter::Result error_result) {
  error_message_ = error_message;
  error_result_ = error_result;
  quit_closure_.Run();
}

void HostStarterOAuthHelperTest::AddGetTokenResponse(
    const std::string& response) {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(), response);
}

void HostStarterOAuthHelperTest::AddGetUserEmailResponse(
    const std::string& response) {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth_user_info_url().spec(), response);
}

void HostStarterOAuthHelperTest::AddGetTokenErrorResponse(
    net::HttpStatusCode status) {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      /*content=*/std::string(), status);
}

void HostStarterOAuthHelperTest::AddGetUserEmailErrorResponse(
    net::HttpStatusCode status) {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth_user_info_url().spec(),
      /*content=*/std::string(), status);
}

void HostStarterOAuthHelperTest::RunUntilQuit() {
  task_environment_.RunUntilQuit();
}

TEST_F(HostStarterOAuthHelperTest, NoUserEmail_TokensRetrievedSuccessfully) {
  AddGetTokenResponse(kGetTokensResponse);
  AddGetUserEmailResponse(kGetUserEmailResponse);

  host_starter_oauth_helper().FetchTokens(
      std::string(), kAuthorizationCodeValue, gaia::OAuthClientInfo(),
      base::BindOnce(&HostStarterOAuthHelperTest::OnTokensRetrieved,
                     base::Unretained(this)),
      base::BindOnce(&HostStarterOAuthHelperTest::HandleOAuthError,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_EQ(access_token(), kAccessTokenValue);
  ASSERT_EQ(refresh_token(), kRefreshTokenValue);
  ASSERT_EQ(user_email(), kTestEmail);
  ASSERT_FALSE(error_result().has_value());
}

TEST_F(HostStarterOAuthHelperTest, UserEmail_TokensRetrievedSuccessfully) {
  AddGetTokenResponse(kGetTokensResponse);
  AddGetUserEmailResponse(kGetUserEmailResponse);

  host_starter_oauth_helper().FetchTokens(
      kTestEmail, kAuthorizationCodeValue, gaia::OAuthClientInfo(),
      base::BindOnce(&HostStarterOAuthHelperTest::OnTokensRetrieved,
                     base::Unretained(this)),
      base::BindOnce(&HostStarterOAuthHelperTest::HandleOAuthError,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_EQ(access_token(), kAccessTokenValue);
  ASSERT_EQ(refresh_token(), kRefreshTokenValue);
  ASSERT_EQ(user_email(), kTestEmail);
  ASSERT_FALSE(error_result().has_value());
}

TEST_F(HostStarterOAuthHelperTest, DifferentUserEmail_RunsErrorCallback) {
  AddGetTokenResponse(kGetTokensResponse);
  AddGetUserEmailResponse(kGetUserEmailResponse);

  host_starter_oauth_helper().FetchTokens(
      kDifferentTestEmail, kAuthorizationCodeValue, gaia::OAuthClientInfo(),
      base::BindOnce(&HostStarterOAuthHelperTest::OnTokensRetrieved,
                     base::Unretained(this)),
      base::BindOnce(&HostStarterOAuthHelperTest::HandleOAuthError,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(access_token().empty());
  ASSERT_TRUE(refresh_token().empty());
  ASSERT_TRUE(user_email().empty());
  ASSERT_FALSE(error_message().empty());
  ASSERT_TRUE(error_result().has_value());
  ASSERT_EQ(*error_result(), HostStarter::Result::PERMISSION_DENIED);
}

TEST_F(HostStarterOAuthHelperTest, GetTokensNetworkError_RunsErrorCallback) {
  AddGetTokenErrorResponse(net::HttpStatusCode::HTTP_SERVICE_UNAVAILABLE);

  host_starter_oauth_helper().FetchTokens(
      kTestEmail, kAuthorizationCodeValue, gaia::OAuthClientInfo(),
      base::BindOnce(&HostStarterOAuthHelperTest::OnTokensRetrieved,
                     base::Unretained(this)),
      base::BindOnce(&HostStarterOAuthHelperTest::HandleOAuthError,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(access_token().empty());
  ASSERT_TRUE(refresh_token().empty());
  ASSERT_TRUE(user_email().empty());
  ASSERT_FALSE(error_message().empty());
  ASSERT_TRUE(error_result().has_value());
  ASSERT_EQ(*error_result(), HostStarter::Result::NETWORK_ERROR);
}

TEST_F(HostStarterOAuthHelperTest, GetTokensOAuthError_RunsErrorCallback) {
  // Gaia client calls OnOAuthError for BAD_REQUEST or UNAUTHORIZED.
  // UNAUTHORIZED means the authorization_code has expired so simulate that.
  AddGetTokenErrorResponse(net::HttpStatusCode::HTTP_UNAUTHORIZED);

  host_starter_oauth_helper().FetchTokens(
      kTestEmail, kAuthorizationCodeValue, gaia::OAuthClientInfo(),
      base::BindOnce(&HostStarterOAuthHelperTest::OnTokensRetrieved,
                     base::Unretained(this)),
      base::BindOnce(&HostStarterOAuthHelperTest::HandleOAuthError,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(access_token().empty());
  ASSERT_TRUE(refresh_token().empty());
  ASSERT_TRUE(user_email().empty());
  ASSERT_FALSE(error_message().empty());
  ASSERT_TRUE(error_result().has_value());
  ASSERT_EQ(*error_result(), HostStarter::Result::OAUTH_ERROR);
}

TEST_F(HostStarterOAuthHelperTest, GetUserEmailNetworkError_RunsErrorCallback) {
  AddGetTokenResponse(kGetTokensResponse);
  AddGetUserEmailErrorResponse(net::HttpStatusCode::HTTP_SERVICE_UNAVAILABLE);

  host_starter_oauth_helper().FetchTokens(
      kTestEmail, kAuthorizationCodeValue, gaia::OAuthClientInfo(),
      base::BindOnce(&HostStarterOAuthHelperTest::OnTokensRetrieved,
                     base::Unretained(this)),
      base::BindOnce(&HostStarterOAuthHelperTest::HandleOAuthError,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(access_token().empty());
  ASSERT_TRUE(refresh_token().empty());
  ASSERT_TRUE(user_email().empty());
  ASSERT_FALSE(error_message().empty());
  ASSERT_EQ(error_result(), HostStarter::Result::NETWORK_ERROR);
}

}  // namespace remoting
