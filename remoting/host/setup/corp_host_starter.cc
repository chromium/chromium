// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/corp_host_starter.h"

#include <memory>
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
#include "remoting/base/corp_service_client.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/host/host_config.h"
#include "remoting/host/setup/buildflags.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/host_starter_base.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

// A helper class which provisions a corp machine for Chrome Remote Desktop.
class CorpHostStarter : public HostStarterBase {
 public:
  explicit CorpHostStarter(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CorpHostStarter(const CorpHostStarter&) = delete;
  CorpHostStarter& operator=(const CorpHostStarter&) = delete;

  ~CorpHostStarter() override;

  // HostStarterBase implementation.
  void RegisterNewHost(const std::string& public_key,
                       std::optional<std::string> access_token) override;
  void RemoveOldHostFromDirectory(base::OnceClosure on_host_removed) override;
  void ApplyConfigValues(base::Value::Dict& config) override;
  void ReportError(const std::string& error_message,
                   base::OnceClosure on_error_reported) override;

  // CorpServiceClient callback.
  void OnProvisionCorpMachineResponse(
      const ProtobufHttpStatus& status,
      std::unique_ptr<internal::ProvisionCorpMachineResponse> response);

 private:
  std::unique_ptr<CorpServiceClient> corp_service_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CorpHostStarter> weak_ptr_factory_{this};
};

CorpHostStarter::CorpHostStarter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : HostStarterBase(url_loader_factory),
      corp_service_client_(std::make_unique<CorpServiceClient>(
          url_loader_factory)) {}

CorpHostStarter::~CorpHostStarter() = default;

void CorpHostStarter::RegisterNewHost(const std::string& public_key,
                                      std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't expect |access_token| to be populated for this flow but
  // |public_key| is required.
  DCHECK(!public_key.empty());
  DCHECK(!access_token.has_value());

  corp_service_client_->ProvisionCorpMachine(
      params().username, params().name, public_key, existing_host_id(),
      base::BindOnce(&CorpHostStarter::OnProvisionCorpMachineResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CorpHostStarter::OnProvisionCorpMachineResponse(
    const ProtobufHttpStatus& status,
    std::unique_ptr<internal::ProvisionCorpMachineResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.ok()) {
    HandleHttpStatusError(status);
    return;
  }

  OnNewHostRegistered(
      base::ToLowerASCII(internal::GetHostId(*response)),
      base::ToLowerASCII(internal::GetOwnerEmail(*response)),
      base::ToLowerASCII(internal::GetServiceAccount(*response)),
      internal::GetAuthorizationCode(*response));
}

void CorpHostStarter::RemoveOldHostFromDirectory(
    base::OnceClosure on_host_removed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This workflow removes the existing host as part of the provisioning service
  // call so we don't need to make an additional service request here.
  std::move(on_host_removed).Run();
}

void CorpHostStarter::ApplyConfigValues(base::Value::Dict& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  config.Set(kRequireSessionAuthorizationPath, true);
  config.Set(kHostTypeHintPath, kCorpHostTypeHint);
}

void CorpHostStarter::ReportError(const std::string& message,
                                  base::OnceClosure on_error_reported) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string& host_id = params().id;
  LOG(ERROR) << "\n  Reporting provisioning error for host id `" << host_id
             << "`:\n    " << message;
  corp_service_client_->ReportProvisioningError(
      host_id, message,
      base::BindOnce(
          [](base::OnceClosure on_error_reported,
             const ProtobufHttpStatus& status, std::unique_ptr<Empty>) {
            if (!status.ok()) {
              LOG(ERROR) << "Failed to report provisioning error: "
                         << static_cast<int>(status.error_code());
            }
            std::move(on_error_reported).Run();
          },
          std::move(on_error_reported)));
}

}  // namespace

std::unique_ptr<HostStarter> ProvisionCorpMachine(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<CorpHostStarter>(url_loader_factory);
}

}  // namespace remoting
