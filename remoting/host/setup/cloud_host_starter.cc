// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/cloud_host_starter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/base/cloud_service_client.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/host/host_config.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/host_starter_base.h"
#include "remoting/proto/google/internal/remoting/cloud/v1alpha/remote_access_service.pb.h"
#include "remoting/proto/remoting/v1/cloud_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

using LegacyProvisionGceInstanceResponse =
    apis::v1::ProvisionGceInstanceResponse;
using ProvisionGceInstanceResponse =
    ::google::internal::remoting::cloud::v1alpha::ProvisionGceInstanceResponse;

// A helper class which provisions a cloud machine for Chrome Remote Desktop.
class CloudHostStarter : public HostStarterBase {
 public:
  explicit CloudHostStarter(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  CloudHostStarter(
      const std::string& api_key,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CloudHostStarter(const CloudHostStarter&) = delete;
  CloudHostStarter& operator=(const CloudHostStarter&) = delete;

  ~CloudHostStarter() override;

  // HostStarterBase implementation.
  void RegisterNewHost(const std::string& public_key,
                       std::optional<std::string> access_token) override;
  void RemoveOldHostFromDirectory(base::OnceClosure on_host_removed) override;
  void ApplyConfigValues(base::Value::Dict& config) override;

  // CloudServiceClient callback.
  void OnProvisionGceInstanceResponse(
      const ProtobufHttpStatus& status,
      std::unique_ptr<ProvisionGceInstanceResponse> response);
  void OnLegacyProvisionGceInstanceResponse(
      const ProtobufHttpStatus& status,
      std::unique_ptr<LegacyProvisionGceInstanceResponse> response);

 private:
  std::unique_ptr<CloudServiceClient> cloud_service_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CloudHostStarter> weak_ptr_factory_{this};
};

CloudHostStarter::CloudHostStarter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : HostStarterBase(url_loader_factory),
      cloud_service_client_(
          std::make_unique<CloudServiceClient>(url_loader_factory)) {}

CloudHostStarter::CloudHostStarter(
    const std::string& api_key,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : HostStarterBase(url_loader_factory),
      cloud_service_client_(
          std::make_unique<CloudServiceClient>(api_key,
                                               /*oauth_token_getter=*/nullptr,
                                               url_loader_factory)) {
  params().api_key = api_key;
}

CloudHostStarter::~CloudHostStarter() = default;

void CloudHostStarter::RegisterNewHost(
    const std::string& public_key,
    std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't expect |access_token| to be populated for this flow but
  // |public_key| is required.
  DCHECK(!public_key.empty());
  DCHECK(!access_token.has_value());

  // TODO: joedow - Remove the legacy code path once the new flow is supported.
  if (!params().api_key.empty()) {
    cloud_service_client_->ProvisionGceInstance(
        params().owner_email, params().name, public_key, existing_host_id(),
        base::BindOnce(&CloudHostStarter::OnProvisionGceInstanceResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    cloud_service_client_->LegacyProvisionGceInstance(
        params().owner_email, params().name, public_key, existing_host_id(),
        base::BindOnce(&CloudHostStarter::OnLegacyProvisionGceInstanceResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CloudHostStarter::OnProvisionGceInstanceResponse(
    const ProtobufHttpStatus& status,
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

void CloudHostStarter::OnLegacyProvisionGceInstanceResponse(
    const ProtobufHttpStatus& status,
    std::unique_ptr<LegacyProvisionGceInstanceResponse> response) {
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

  // The legacy code is taken when an API_KEY is not provided. In that case, the
  // host acts like a Me2Me host rather than a Cloud host. Note that is kept for
  // compatibility reasons but is not going to be supported long-term.
  // TODO: joedow - Remove this when all Cloud hosts are required to use the
  // Cloud API.
  if (params().api_key.empty()) {
    config.Set(kHostTypeHintPath, kMe2MeHostTypeHint);
    config.Set(kHostSecretHashConfigPath,
               MakeHostPinHash(params().id, params().pin));
  } else {
    config.Set(kHostTypeHintPath, kCloudHostTypeHint);
    config.Set(kRequireSessionAuthorizationPath, true);
  }
}

}  // namespace

std::unique_ptr<HostStarter> ProvisionCloudInstance(
    const std::string& api_key,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // TODO: joedow - Remove this bit when we no longer support the legacy
  // provisioning code path.
  if (api_key.empty()) {
    return std::make_unique<CloudHostStarter>(url_loader_factory);
  }

  return std::make_unique<CloudHostStarter>(api_key, url_loader_factory);
}

}  // namespace remoting
