// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/corp_service_client.h"

#include "base/functional/bind.h"
#include "base/strings/stringize_macros.h"
#include "net/http/http_request_headers.h"
#include "remoting/base/corp_auth_util.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/service_urls.h"
#include "remoting/base/version.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(REMOTING_INTERNAL)
#include "remoting/internal/base/api_keys.h"
#endif

namespace remoting {

CorpServiceClient::CorpServiceClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : http_client_(ServiceUrls::GetInstance()->remoting_corp_endpoint(),
                   /*oauth_token_getter=*/nullptr,
                   url_loader_factory) {}

CorpServiceClient::CorpServiceClient(
    const std::string& refresh_token,
    const std::string& service_account_email,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : oauth_token_getter_(CreateCorpTokenGetter(url_loader_factory,
                                                service_account_email,
                                                refresh_token)),
      http_client_(ServiceUrls::GetInstance()->remoting_corp_endpoint(),
                   oauth_token_getter_.get(),
                   url_loader_factory) {}

CorpServiceClient::~CorpServiceClient() = default;

void CorpServiceClient::ProvisionCorpMachine(
    const std::string& owner_email,
    const std::string& fqdn,
    const std::string& public_key,
    const std::optional<std::string>& existing_host_id,
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
      net::HttpRequestHeaders::kPostMethod,
      /*unauthenticated=*/true,
      internal::GetMachineProvisioningRequest(
          owner_email, fqdn, public_key, STRINGIZE(VERSION), existing_host_id),
          std::move(callback));
}

void CorpServiceClient::ReportProvisioningError(
    const std::string& directory_id,
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
  constexpr auto* version = STRINGIZE(VERSION);
  ExecuteRequest(traffic_annotation,
                 internal::GetReportProvisioningErrorRequestPath(),
                 net::HttpRequestHeaders::kPostMethod,
                 /*unauthenticated=*/true,
                 internal::GetReportProvisioningErrorRequest(
                     directory_id, error_message, version),
                 std::move(callback));
}

void CorpServiceClient::SendHeartbeat(const std::string& directory_id,
                                      SendHeartbeatCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("remoting_corp_send_heartbeat",
                                          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Updates the last seen time in the Chrome Remote Desktop Directory "
            "service for a given remote access host instance."
          trigger:
            "Configuring a Google Corp machine for CRD remote access host."
          user_data {
            type: OTHER
          }
          data:
            "An internal UUID to identify the remote access host instance."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2024-09-23"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the CRD host is not configured to run on a Corp machine."
          policy_exception_justification:
            "Not implemented."
        })");
  ExecuteRequest(traffic_annotation, internal::GetSendHeartbeatRequestPath(),
                 net::HttpRequestHeaders::kPostMethod,
                 /*unauthenticated=*/false,
                 internal::GetSendHeartbeatRequest(directory_id),
                 std::move(callback));
}

void CorpServiceClient::UpdateRemoteAccessHost(
    const std::string& directory_id,
    std::optional<std::string> host_version,
    std::optional<std::string> signaling_id,
    std::optional<std::string> offline_reason,
    std::optional<std::string> os_name,
    std::optional<std::string> os_version,
    UpdateRemoteAccessHostCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "remoting_corp_update_remote_access_host",
          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Updates the Chrome Remote Desktop Directory service with "
            "environment details and signaling information for a given remote "
            "access host instance."
          trigger:
            "Configuring a Google Corp machine for CRD remote access host."
          user_data {
            type: OTHER
          }
          data:
            "Includes an internal UUID to identify the remote access host "
            "instance, the name and version of the operating system, the "
            "version of the CRD package installed, and a set of signaling ids "
            "which the CRD client can use to send the host messages."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2024-09-23"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the CRD host is not configured to run on a Corp machine."
          policy_exception_justification:
            "Not implemented."
        })");
  ExecuteRequest(
      traffic_annotation, internal::GetUpdateRemoteAccessHostRequestPath(),
      net::HttpRequestHeaders::kPatchMethod,
      /*unauthenticated=*/false,
      internal::GetUpdateRemoteAccessHostRequest(
          directory_id, std::move(host_version), std::move(signaling_id),
          std::move(offline_reason), std::move(os_name), std::move(os_version)),
      std::move(callback));
}

void CorpServiceClient::CancelPendingRequests() {
  http_client_.CancelPendingRequests();
}

template <typename CallbackType>
void CorpServiceClient::ExecuteRequest(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& path,
    const std::string& method,
    bool unauthenticated,
    std::unique_ptr<google::protobuf::MessageLite> request_message,
    CallbackType callback) {
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(traffic_annotation);
  request_config->path = path;
  request_config->method = method;
  if (unauthenticated) {
    request_config->api_key = internal::GetRemotingCorpApiKey();
    request_config->authenticated = false;
  } else {
    // Authenticated calls must provide an OAuthTokenGetter instance.
    CHECK(oauth_token_getter_);
    request_config->authenticated = true;
  }
  request_config->provide_certificate = true;
  request_config->request_message = std::move(request_message);
  auto request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  request->SetResponseCallback(std::move(callback));
  http_client_.ExecuteRequest(std::move(request));
}

}  // namespace remoting
