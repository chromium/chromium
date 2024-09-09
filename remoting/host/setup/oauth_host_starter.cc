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
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/host/host_config.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/host_starter_base.h"
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

  // HostStarterBase implementation.
  void RegisterNewHost(const std::string& public_key,
                       std::optional<std::string> access_token) override;
  void RemoveOldHostFromDirectory(base::OnceClosure on_host_removed) override;
  void ApplyConfigValues(base::Value::Dict& config) override;

  // DirectoryServiceClient callbacks.
  void OnDeleteHostResponse(
      const ProtobufHttpStatus& status,
      std::unique_ptr<apis::v1::DeleteHostResponse> response);
  void OnRegisterHostResponse(
      const ProtobufHttpStatus& status,
      std::unique_ptr<apis::v1::RegisterHostResponse> response);

 private:
  base::OnceClosure on_host_removed_;
  PassthroughOAuthTokenGetter token_getter_;
  DirectoryServiceClient directory_service_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OAuthHostStarter> weak_ptr_factory_{this};
};

OAuthHostStarter::OAuthHostStarter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : HostStarterBase(url_loader_factory),
      directory_service_client_(&token_getter_, url_loader_factory) {}

OAuthHostStarter::~OAuthHostStarter() = default;

void OAuthHostStarter::RegisterNewHost(
    const std::string& public_key,
    std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(access_token.has_value());
  DCHECK(!access_token->empty());
  DCHECK(!public_key.empty());

  token_getter_.set_access_token(*access_token);

  directory_service_client_.RegisterHost(
      params().id, params().name, public_key,
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
    const ProtobufHttpStatus& status,
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
    const ProtobufHttpStatus& status,
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
