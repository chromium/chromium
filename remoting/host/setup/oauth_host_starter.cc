// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/oauth_host_starter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/directory_service_client.h"
#include "remoting/base/http_status.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/host/host_config.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/host_starter_base.h"
#include "remoting/host/setup/host_starter_oauth_helper.h"
#include "remoting/proto/remoting/v1/directory_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

// A helper class that registers and starts a host using OAuth.
class OAuthHostStarter : public HostStarterBase {
 public:
  explicit OAuthHostStarter(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  OAuthHostStarter(const OAuthHostStarter&) = delete;
  OAuthHostStarter& operator=(const OAuthHostStarter&) = delete;

  ~OAuthHostStarter() override;

  void OnUserTokensRetrieved(const std::string& user_email,
                             const std::string& access_token,
                             const std::string& refresh_token,
                             const std::string& scopes);

  // HostStarterBase implementation.
  void RetrieveApiAccessToken() override;
  void RegisterNewHost(std::optional<std::string> access_token) override;
  void RemoveOldHostFromDirectory(base::OnceClosure on_host_removed) override;
  void ApplyConfigValues(base::Value::Dict& config) override;

  // DirectoryServiceClient callbacks.
  void OnDeleteHostResponse(
      const HttpStatus& status,
      std::unique_ptr<apis::v1::DeleteHostResponse> response);
  void OnRegisterHostResponse(
      const HttpStatus& status,
      std::unique_ptr<apis::v1::RegisterHostResponse> response);

 private:
  base::OnceClosure on_host_removed_;
  PassthroughOAuthTokenGetter token_getter_;
  DirectoryServiceClient directory_service_client_;
  HostStarterOAuthHelper oauth_helper_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<OAuthHostStarter> weak_ptr_;
  base::WeakPtrFactory<OAuthHostStarter> weak_ptr_factory_{this};
};

OAuthHostStarter::OAuthHostStarter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : HostStarterBase(url_loader_factory),
      directory_service_client_(&token_getter_, url_loader_factory),
      oauth_helper_(url_loader_factory) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

OAuthHostStarter::~OAuthHostStarter() = default;

void OAuthHostStarter::RetrieveApiAccessToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!params().auth_code.empty());

  oauth_helper_.FetchTokens(
      params().owner_email, params().auth_code,
      {
          .client_id =
              google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
          .client_secret =
              google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING),
          .redirect_uri = params().redirect_url,
      },
      base::BindOnce(&OAuthHostStarter::OnUserTokensRetrieved, weak_ptr_),
      base::BindOnce(&OAuthHostStarter::HandleError, weak_ptr_));
}

void OAuthHostStarter::OnUserTokensRetrieved(const std::string& user_email,
                                             const std::string& access_token,
                                             const std::string& refresh_token,
                                             const std::string& scope_str) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If an owner email was not provided, then use the account which created the
  // authorization code.
  if (params().owner_email.empty()) {
    params().owner_email = base::ToLowerASCII(user_email);
  }

  // We don't need a `refresh_token` for the user so ignore it even if the
  // authorization_code was created with the offline param.
  RegisterNewHost(access_token);
}

void OAuthHostStarter::RegisterNewHost(
    std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(access_token.has_value());
  DCHECK(!access_token->empty());

  token_getter_.set_access_token(*access_token);

  directory_service_client_.RegisterHost(
      params().id, params().name, key_pair().GetPublicKey(),
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST),
      base::BindOnce(&OAuthHostStarter::OnRegisterHostResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OAuthHostStarter::RemoveOldHostFromDirectory(
    base::OnceClosure on_host_removed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_host_removed_ = std::move(on_host_removed);

  if (existing_host_id().has_value()) {
    directory_service_client_.DeleteHost(
        *existing_host_id(),
        base::BindOnce(&OAuthHostStarter::OnDeleteHostResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    std::move(on_host_removed_).Run();
  }
}

void OAuthHostStarter::ApplyConfigValues(base::Value::Dict& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  config.Set(kHostTypeHintPath, kMe2MeHostTypeHint);
  config.Set(kHostSecretHashConfigPath,
             MakeHostPinHash(params().id, params().pin));
}

void OAuthHostStarter::OnRegisterHostResponse(
    const HttpStatus& status,
    std::unique_ptr<apis::v1::RegisterHostResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.ok()) {
    HandleHttpStatusError(status);
    return;
  }

  OnNewHostRegistered(base::ToLowerASCII(response->host_info().host_id()),
                      /*owner_account_email=*/std::string(),
                      /*service_account_email=*/std::string(),
                      response->auth_code());
}

void OAuthHostStarter::OnDeleteHostResponse(
    const HttpStatus& status,
    std::unique_ptr<apis::v1::DeleteHostResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.ok()) {
    LOG(ERROR) << "Error occurred when unregistering the existing host.";
  }

  std::move(on_host_removed_).Run();
}

}  // namespace

std::unique_ptr<HostStarter> CreateOAuthHostStarter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<OAuthHostStarter>(url_loader_factory);
}

}  // namespace remoting
