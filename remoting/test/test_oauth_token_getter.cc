// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_oauth_token_getter.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/test/cli_util.h"
#include "remoting/test/test_token_storage.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {
namespace test {

namespace {

constexpr char kChromotingAuthScopeValues[] =
    "https://www.googleapis.com/auth/chromoting.me2me.host "
    "https://www.googleapis.com/auth/chromoting.remote.support "
    "https://www.googleapis.com/auth/userinfo.email "
    "https://www.googleapis.com/auth/tachyon";

constexpr char kOauthRedirectUrl[] =
    "https://remotedesktop.google.com/_/oauthredirect";

std::string GetAuthorizationCodeUri(bool show_consent_page) {
  // Replace space characters with a '+' sign when formatting.
  bool use_plus = true;
  std::string uri = base::StringPrintf(
      "https://accounts.google.com/o/oauth2/auth"
      "?scope=%s"
      "&redirect_uri=https://remotedesktop.google.com/_/oauthredirect"
      "&response_type=code"
      "&client_id=%s"
      "&access_type=offline",
      net::EscapeUrlEncodedData(kChromotingAuthScopeValues, use_plus).c_str(),
      net::EscapeUrlEncodedData(
          google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
          use_plus)
          .c_str());
  if (show_consent_page) {
    uri += "&approval_prompt=force";
  }
  return uri;
}

}  // namespace

constexpr char TestOAuthTokenGetter::kSwitchNameAuthCode[];

// static
bool TestOAuthTokenGetter::IsServiceAccount(const std::string& email) {
  return email.find("@chromoting.gserviceaccount.com") != std::string::npos;
}

TestOAuthTokenGetter::TestOAuthTokenGetter(TestTokenStorage* token_storage) {
  DCHECK(token_storage);
  token_storage_ = token_storage;
  auto url_request_context_getter =
      base::MakeRefCounted<URLRequestContextGetter>(
          base::ThreadTaskRunnerHandle::Get());
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_request_context_getter);
}

TestOAuthTokenGetter::~TestOAuthTokenGetter() = default;

void TestOAuthTokenGetter::Initialize(base::OnceClosure on_done) {
  std::string user_email = token_storage_->FetchUserEmail();
  std::string access_token = token_storage_->FetchAccessToken();
  std::string refresh_token = token_storage_->FetchRefreshToken();
  if (user_email.empty() || (access_token.empty() && refresh_token.empty())) {
    ResetWithAuthenticationFlow(std::move(on_done));
    return;
  }
  VLOG(0) << "Reusing user_email: " << user_email;
  if (!refresh_token.empty()) {
    VLOG(0) << "Reusing refresh_token: " << refresh_token;
    token_getter_ = CreateWithRefreshToken(refresh_token, user_email);
  } else {
    VLOG(0) << "Reusing access token: " << access_token;
    token_getter_ = std::make_unique<FakeOAuthTokenGetter>(
        OAuthTokenGetter::Status::SUCCESS, user_email, access_token);
  }
  std::move(on_done).Run();
}

void TestOAuthTokenGetter::ResetWithAuthenticationFlow(
    base::OnceClosure on_done) {
  on_authentication_done_.push(std::move(on_done));
  InvalidateCache();
}

void TestOAuthTokenGetter::CallWithToken(TokenCallback on_access_token) {
  token_getter_->CallWithToken(std::move(on_access_token));
}

void TestOAuthTokenGetter::InvalidateCache() {
  if (is_authenticating_) {
    return;
  }

  is_authenticating_ = true;

  printf(
      "Is your account whitelisted to use 1P scope in consent page? [Y/n]: ");
  bool show_consent_page = test::ReadYNBool(true);

  static const std::string read_auth_code_prompt = base::StringPrintf(
      "Please authenticate at:\n\n"
      "  %s\n\n"
      "Enter the auth code: ",
      GetAuthorizationCodeUri(show_consent_page).c_str());
  std::string auth_code = test::ReadStringFromCommandLineOrStdin(
      kSwitchNameAuthCode, read_auth_code_prompt);

  // Make sure we don't try to reuse an auth code.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(kSwitchNameAuthCode);

  token_getter_ = CreateFromIntermediateCredentials(
      auth_code, base::BindRepeating(&TestOAuthTokenGetter::OnCredentialsUpdate,
                                     weak_factory_.GetWeakPtr()));

  // This triggers |OnCredentialsUpdate| to be called.
  token_getter_->CallWithToken(base::DoNothing());
}

base::WeakPtr<TestOAuthTokenGetter> TestOAuthTokenGetter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<OAuthTokenGetter>
TestOAuthTokenGetter::CreateFromIntermediateCredentials(
    const std::string& auth_code,
    const OAuthTokenGetter::CredentialsUpdatedCallback& on_credentials_update) {
  auto oauth_credentials =
      std::make_unique<OAuthTokenGetter::OAuthIntermediateCredentials>(
          auth_code, /* is_service_account */ false);
  oauth_credentials->oauth_redirect_uri = kOauthRedirectUrl;

  return std::make_unique<OAuthTokenGetterImpl>(
      std::move(oauth_credentials), on_credentials_update,
      url_loader_factory_owner_->GetURLLoaderFactory(),
      /* auto_refresh */ true);
}

std::unique_ptr<OAuthTokenGetter> TestOAuthTokenGetter::CreateWithRefreshToken(
    const std::string& refresh_token,
    const std::string& email) {
  bool is_service_account = IsServiceAccount(email);
  auto oauth_credentials =
      std::make_unique<OAuthTokenGetter::OAuthAuthorizationCredentials>(
          email, refresh_token, is_service_account);

  return std::make_unique<OAuthTokenGetterImpl>(
      std::move(oauth_credentials), base::DoNothing(),
      url_loader_factory_owner_->GetURLLoaderFactory(),
      /*auto_refresh=*/true);
}

void TestOAuthTokenGetter::OnCredentialsUpdate(
    const std::string& user_email,
    const std::string& refresh_token) {
  VLOG(0) << "Received user_email: " << user_email
          << ", refresh_token: " << refresh_token;
  token_storage_->StoreUserEmail(user_email);
  if (refresh_token.empty()) {
    VLOG(0)
        << "No refresh token is returned. Will cache the access token instead.";
    token_getter_->CallWithToken(base::BindOnce(
        &TestOAuthTokenGetter::OnAccessToken, weak_factory_.GetWeakPtr()));
    return;
  }
  is_authenticating_ = false;
  token_storage_->StoreRefreshToken(refresh_token);
  RunAuthenticationDoneCallbacks();
}

void TestOAuthTokenGetter::OnAccessToken(OAuthTokenGetter::Status status,
                                         const std::string& user_email,
                                         const std::string& access_token) {
  is_authenticating_ = false;
  if (status != OAuthTokenGetter::Status::SUCCESS) {
    fprintf(stderr,
            "Failed to authenticate. Please check if your access  token is "
            "correct.\n");
    InvalidateCache();
    return;
  }
  VLOG(0) << "Received access_token: " << access_token;
  token_storage_->StoreAccessToken(access_token);
  RunAuthenticationDoneCallbacks();
}

void TestOAuthTokenGetter::RunAuthenticationDoneCallbacks() {
  while (!on_authentication_done_.empty()) {
    std::move(on_authentication_done_.front()).Run();
    on_authentication_done_.pop();
  }
}

}  // namespace test
}  // namespace remoting
