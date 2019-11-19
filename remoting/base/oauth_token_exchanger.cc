// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_exchanger.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/grpc_support/grpc_async_executor.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/grpc/src/include/grpcpp/security/credentials.h"

namespace remoting {

namespace {

// Maximum number of retries on network/500 errors.
const int kMaxRetries = 3;

const char API_TACHYON[] = "https://www.googleapis.com/auth/tachyon";

const char TOKENINFO_SCOPE_KEY[] = "scope";

// Returns whether the "scopes" part of the OAuth tokeninfo response
// contains all the needed scopes.
bool HasNeededScopes(const std::string& scopes) {
  std::vector<base::StringPiece> scopes_list =
      base::SplitStringPiece(scopes, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return base::Contains(scopes_list, API_TACHYON);
}

}  // namespace

class OAuthTokenExchanger::DirectoryServiceClient {
 public:
  using UpdateRobotTokenCallback =
      base::OnceCallback<void(const grpc::Status&,
                              const apis::v1::UpdateRobotTokenResponse&)>;

  DirectoryServiceClient();
  ~DirectoryServiceClient();

  void UpdateRobotToken(const std::string& access_token,
                        bool offline,
                        UpdateRobotTokenCallback callback);

 private:
  using RemotingDirectoryService = apis::v1::RemotingDirectoryService;

  GrpcAsyncExecutor grpc_executor_;
  std::unique_ptr<RemotingDirectoryService::Stub> stub_;
};

OAuthTokenExchanger::DirectoryServiceClient::DirectoryServiceClient() {
  GrpcChannelSharedPtr channel = CreateSslChannelForEndpoint(
      ServiceUrls::GetInstance()->remoting_server_endpoint());
  stub_ = RemotingDirectoryService::NewStub(channel);
}

OAuthTokenExchanger::DirectoryServiceClient::~DirectoryServiceClient() =
    default;

void OAuthTokenExchanger::DirectoryServiceClient::UpdateRobotToken(
    const std::string& access_token,
    bool offline,
    UpdateRobotTokenCallback callback) {
  auto update_robot_token_request = apis::v1::UpdateRobotTokenRequest();
  update_robot_token_request.set_client_id(
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST));
  update_robot_token_request.set_offline(offline);

  auto async_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&RemotingDirectoryService::Stub::AsyncUpdateRobotToken,
                     base::Unretained(stub_.get())),
      update_robot_token_request, std::move(callback));
  async_request->context()->set_credentials(
      grpc::AccessTokenCredentials(access_token));
  grpc_executor_.ExecuteRpc(std::move(async_request));
}

OAuthTokenExchanger::OAuthTokenExchanger(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : gaia_oauth_client_(std::make_unique<gaia::GaiaOAuthClient>(
          std::move(url_loader_factory))),
      directory_service_client_(std::make_unique<DirectoryServiceClient>()) {}

OAuthTokenExchanger::~OAuthTokenExchanger() = default;

void OAuthTokenExchanger::ExchangeToken(const std::string& access_token,
                                        TokenCallback on_new_token) {
  oauth_access_token_ = access_token;
  pending_callbacks_.push(std::move(on_new_token));

  if (!need_token_exchange_.has_value()) {
    gaia_oauth_client_->GetTokenInfo(access_token, kMaxRetries, this);
    return;
  }

  if (need_token_exchange_.value()) {
    RequestNewToken();
    return;
  }

  // Return the original token, as it already has required scopes.
  NotifyCallbacks(OAuthTokenGetter::SUCCESS, std::string() /* refresh_token */,
                  oauth_access_token_);
}

void OAuthTokenExchanger::OnGetTokensResponse(const std::string& refresh_token,
                                              const std::string& access_token,
                                              int expires_in_seconds) {
  // |expires_in_seconds| is unused - the exchanged token is assumed to be
  // valid for at least as long as the original access token.
  NotifyCallbacks(OAuthTokenGetter::SUCCESS, refresh_token, access_token);
}

void OAuthTokenExchanger::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  base::Value* scopes_value = token_info->FindKey(TOKENINFO_SCOPE_KEY);
  if (!scopes_value || !scopes_value->is_string()) {
    NotifyCallbacks(OAuthTokenGetter::AUTH_ERROR, std::string(), std::string());
    return;
  }
  std::string scopes = scopes_value->GetString();
  VLOG(1) << "Token scopes: " << scopes;
  need_token_exchange_ = !HasNeededScopes(scopes);
  VLOG(1) << "Need exchange: " << (need_token_exchange_.value() ? "yes" : "no");

  if (need_token_exchange_.value()) {
    RequestNewToken();
  } else {
    NotifyCallbacks(OAuthTokenGetter::SUCCESS,
                    std::string() /* refresh_token */, oauth_access_token_);
  }
}

void OAuthTokenExchanger::OnOAuthError() {
  LOG(ERROR) << "OAuth error.";
  NotifyCallbacks(OAuthTokenGetter::AUTH_ERROR, std::string(), std::string());
}

void OAuthTokenExchanger::OnNetworkError(int response_code) {
  LOG(ERROR) << "Network error: " << response_code;
  NotifyCallbacks(OAuthTokenGetter::NETWORK_ERROR, std::string(),
                  std::string());
}

void OAuthTokenExchanger::NotifyCallbacks(OAuthTokenGetter::Status status,
                                          const std::string& refresh_token,
                                          const std::string& access_token) {
  // Protect against recursion by moving the callbacks into a temporary list.
  base::queue<TokenCallback> callbacks;
  callbacks.swap(pending_callbacks_);
  while (!callbacks.empty()) {
    std::move(callbacks.front()).Run(status, refresh_token, access_token);
    callbacks.pop();
  }
}

void OAuthTokenExchanger::RequestNewToken() {
  directory_service_client_->UpdateRobotToken(
      oauth_access_token_, offline_mode_,
      base::BindOnce(&OAuthTokenExchanger::OnRobotTokenResponse,
                     base::Unretained(this)));
}

void OAuthTokenExchanger::OnRobotTokenResponse(
    const grpc::Status& status,
    const apis::v1::UpdateRobotTokenResponse& response) {
  if (!status.ok()) {
    LOG(ERROR) << "Received error code: " << status.error_code()
               << ", message: " << status.error_message();
    NotifyCallbacks(OAuthTokenGetter::AUTH_ERROR, std::string(), std::string());
    return;
  }

  if (!response.has_auth_code()) {
    LOG(ERROR) << "Received response without auth_code.";
    NotifyCallbacks(OAuthTokenGetter::AUTH_ERROR, std::string(), std::string());
    return;
  }

  // When offline mode is used, this class will return a new refresh token
  // as well as an access token. It doesn't make sense to continue exchanging
  // tokens every hour in this case. OAuthTokenGetterImpl remembers the new
  // refresh token and uses that for fetching new access tokens every hour. So
  // there's no need for further token-exchanges after the first successful one.
  if (offline_mode_) {
    need_token_exchange_ = false;
  }

  // The redirect_uri parameter is required for GetTokensFromAuthCode(), but
  // "oob" (out of band) can be used for robot accounts.
  gaia::OAuthClientInfo client_info = {
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST),
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST),
      "oob"};

  gaia_oauth_client_->GetTokensFromAuthCode(client_info, response.auth_code(),
                                            kMaxRetries, this);
}

}  // namespace remoting
