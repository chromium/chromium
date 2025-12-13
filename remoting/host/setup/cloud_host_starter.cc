// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/cloud_host_starter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "remoting/base/cloud_service_client.h"
#include "remoting/base/compute_engine_service_client.h"
#include "remoting/base/http_status.h"
#include "remoting/base/instance_identity_token.h"
#include "remoting/base/logging.h"
#include "remoting/base/oauth_token_info.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/base/service_urls.h"
#include "remoting/host/host_config.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/host_starter_base.h"
#include "remoting/proto/google/remoting/cloud/v1/provisioning_service.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

using ProvisionGceInstanceResponse =
    ::google::remoting::cloud::v1::ProvisionGceInstanceResponse;

// A helper class which provisions a cloud machine for Chrome Remote Desktop.
class CloudHostStarter : public HostStarterBase {
 public:
  explicit CloudHostStarter(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CloudHostStarter(const CloudHostStarter&) = delete;
  CloudHostStarter& operator=(const CloudHostStarter&) = delete;

  ~CloudHostStarter() override;

  // ComputeEngineServiceClient callbacks.
  void OnApiAccessTokenRetrieved(const HttpStatus& status);
  void OnIdentityTokenRetrieved(const HttpStatus& status);

  // HostStarterBase implementation.
  void RetrieveApiAccessToken() override;
  void RegisterNewHost(std::optional<std::string> access_token) override;
  void RemoveOldHostFromDirectory(base::OnceClosure on_host_removed) override;
  void ApplyConfigValues(base::Value::Dict& config) override;

  // CloudServiceClient callback.
  void OnProvisionGceInstanceResponse(
      const HttpStatus& status,
      std::unique_ptr<ProvisionGceInstanceResponse> response);

 private:
  std::unique_ptr<CloudServiceClient> cloud_service_client_;
  std::unique_ptr<ComputeEngineServiceClient> compute_engine_service_client_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<PassthroughOAuthTokenGetter> api_access_token_getter_;

  std::optional<std::string> instance_identity_token_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CloudHostStarter> weak_ptr_factory_{this};
};

CloudHostStarter::CloudHostStarter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : HostStarterBase(url_loader_factory),
      compute_engine_service_client_(
          std::make_unique<ComputeEngineServiceClient>(url_loader_factory)),
      url_loader_factory_(url_loader_factory) {}

CloudHostStarter::~CloudHostStarter() = default;

void CloudHostStarter::RetrieveApiAccessToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Try to retrieve an Instance Identity Token before the access token. Making
  // this query first simplifies the logic a bit as we may end up skipping the
  // the access token request if an API_KEY is provided but we want to try to
  // get the identity token for both scenarios.
  compute_engine_service_client_->GetInstanceIdentityToken(
      base::StringPrintf(
          "https://%s",
          ServiceUrls::GetInstance()->remoting_cloud_public_endpoint()),
      base::BindOnce(&CloudHostStarter::OnIdentityTokenRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CloudHostStarter::OnIdentityTokenRetrieved(const HttpStatus& status) {
  if (status.ok()) {
    auto jwt = status.response_body();
    auto validated_token = InstanceIdentityToken::Create(jwt);
    if (validated_token.has_value()) {
      HOST_LOG << "Retrieved instance identity token:\n" << *validated_token;
      instance_identity_token_ = std::move(jwt);
    }
  } else {
    int error_code = static_cast<int>(status.error_code());
    LOG(WARNING) << "Failed to retrieve an Instance Identity token.\n"
                 << "  Error code: " << error_code << "\n"
                 << "  Message: " << status.error_message() << "\n"
                 << "  Body: " << status.response_body();
  }

  // The two modes to configure a Cloud host are to generate an API_KEY and use
  // that to access the provisioning RPC or to generate an access token using
  // the default service account. If an API_KEY is being used, we can skip the
  // access token request since it won't be used.
  if (params().api_key.empty()) {
    compute_engine_service_client_->GetServiceAccountAccessToken(
        base::BindOnce(&CloudHostStarter::OnApiAccessTokenRetrieved,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    RegisterNewHost(/*access_token=*/std::nullopt);
  }
}

void CloudHostStarter::OnApiAccessTokenRetrieved(const HttpStatus& status) {
  if (!status.ok()) {
    HandleHttpStatusError(status);
    return;
  }
  if (status.response_body().empty()) {
    HandleError("Token response is empty.", Result::OAUTH_ERROR);
    return;
  }
  auto token_payload = base::JSONReader::Read(
      status.response_body(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!token_payload.has_value()) {
    HandleError("Token response was not valid JSON.", Result::OAUTH_ERROR);
    return;
  }
  auto* access_token = token_payload->GetDict().FindString("access_token");
  if (!access_token) {
    HandleError("Token response did not include an access token field.",
                Result::OAUTH_ERROR);
    return;
  }

  RegisterNewHost(*access_token);
}

void CloudHostStarter::RegisterNewHost(
    std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (access_token.has_value() && !access_token->empty()) {
    CHECK(params().api_key.empty());
    OAuthTokenInfo token_info{*access_token};
    api_access_token_getter_ =
        std::make_unique<PassthroughOAuthTokenGetter>(token_info);
    cloud_service_client_ =
        CloudServiceClient::CreateForGceDefaultServiceAccount(
            api_access_token_getter_.get(), url_loader_factory_);
  } else {
    CHECK(!params().api_key.empty());
    cloud_service_client_ = CloudServiceClient::CreateForGcpProject(
        params().api_key, url_loader_factory_);
  }

  cloud_service_client_->ProvisionGceInstance(
      params().owner_email, params().name, key_pair().GetPublicKey(),
      existing_host_id(), std::move(instance_identity_token_),
      base::BindOnce(&CloudHostStarter::OnProvisionGceInstanceResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CloudHostStarter::OnProvisionGceInstanceResponse(
    const HttpStatus& status,
    std::unique_ptr<ProvisionGceInstanceResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.ok()) {
    HandleHttpStatusError(status);
    return;
  }

  OnNewHostRegistered(
      base::ToLowerASCII(response->directory_id()),
      /*owner_account_email=*/std::string(),
      base::ToLowerASCII(response->service_account_info().email()),
      response->service_account_info().authorization_code());
}

void CloudHostStarter::RemoveOldHostFromDirectory(
    base::OnceClosure on_host_removed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This workflow removes the existing host as part of the provisioning service
  // call so we don't need to make an additional service request here.
  std::move(on_host_removed).Run();
}

void CloudHostStarter::ApplyConfigValues(base::Value::Dict& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  config.Set(kHostTypeHintPath, kCloudHostTypeHint);
  config.Set(kRequireSessionAuthorizationPath, true);
}

}  // namespace

std::unique_ptr<HostStarter> ProvisionCloudInstance(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<CloudHostStarter>(url_loader_factory);
}

}  // namespace remoting
