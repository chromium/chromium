// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/corp_service_client.h"

#include "base/functional/bind.h"
#include "base/strings/stringize_macros.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/service_urls.h"
#include "remoting/base/version.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(REMOTING_INTERNAL)
#include "remoting/internal/base/api_keys.h"
#endif

namespace remoting {

namespace {

std::string GetRemotingCorpApiKey() {
#if BUILDFLAG(REMOTING_INTERNAL)
  return internal::GetRemotingCorpApiKey();
#else
  return "UNKNOWN API KEY";
#endif
}

}  // namespace

CorpServiceClient::CorpServiceClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : http_client_(ServiceUrls::GetInstance()->remoting_corp_endpoint(),
                   /*token_getter=*/nullptr,
                   url_loader_factory) {}

CorpServiceClient::~CorpServiceClient() = default;

void CorpServiceClient::ProvisionCorpMachine(
    const std::string& owner_email,
    const std::string& fqdn,
    const std::string& public_key,
    std::optional<std::string> existing_host_id,
    ProvisionCorpMachineCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("remoting_provision_corp_machine",
                                          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Creates a new remote access host instance for a corp user in the "
            "Chrome Remote Desktop directory server."
          trigger:
            "User runs the start-host tool with the corp-user flag. Note that "
            "this functionality is not available outside of the corp network "
            "so external users will never need to make this service request."
          user_data {
            type: EMAIL
            type: OTHER
          }
          data:
            "The email address of the account to configure CRD for and the "
            "fully-qualified domain name of the machine being configured for "
            "remote access."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2023-10-17"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the start-host utility is not run with the corp-user flag."
          policy_exception_justification:
            "Not implemented."
        })");
  ExecuteRequest(
      traffic_annotation, internal::GetMachineProvisioningRequestPath(),
      internal::GetMachineProvisioningRequest(
          owner_email, fqdn, public_key, STRINGIZE(VERSION), existing_host_id),
          std::move(callback));
}

void CorpServiceClient::ReportProvisioningError(
    const std::string& host_id,
    const std::string& error_message,
    ReportProvisioningErrorCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("remoting_report_provisioning_error",
                                          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Reports an error during the machine provisioning process to the "
            "Chrome Remote Desktop directory server."
          trigger:
            "User runs the start-host tool with the corp-user flag and an "
            "error occurs which prevents the machine from coming online. Note "
            "that this functionality is not available outside of the corp "
            "network so external users will never see this request being made."
          user_data {
            type: OTHER
          }
          data:
            "The host id and an error message/reason why provisioning failed."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2023-10-27"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the start-host utility is not run with the corp-user flag."
          policy_exception_justification:
            "Not implemented."
        })");
  ExecuteRequest(
      traffic_annotation, internal::GetReportProvisioningErrorRequestPath(),
      internal::GetReportProvisioningErrorRequest(
          host_id, error_message, STRINGIZE(VERSION)), std::move(callback));
}

void CorpServiceClient::CancelPendingRequests() {
  http_client_.CancelPendingRequests();
}

template <typename CallbackType>
void CorpServiceClient::ExecuteRequest(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& path,
    std::unique_ptr<google::protobuf::MessageLite> request_message,
    CallbackType callback) {
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(traffic_annotation);
  request_config->path = path;
  request_config->api_key = GetRemotingCorpApiKey();
  request_config->authenticated = false;
  request_config->provide_certificate = true;
  request_config->request_message = std::move(request_message);
  auto request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  request->SetResponseCallback(std::move(callback));
  http_client_.ExecuteRequest(std::move(request));
}

}  // namespace remoting
