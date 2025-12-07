// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remoting_register_support_host_request.h"

#include "base/logging.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/http_status.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/chrome_os_enterprise_options.pb.h"
#include "remoting/proto/remoting/v1/remote_support_host_messages.pb.h"
#include "remoting/signaling/signaling_address.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "remoting_register_support_host_request",
        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Request used by Chrome Remote Desktop to register a new remote "
            "support host."
          trigger:
            "User requests for remote assistance using Chrome Remote Desktop."
          user_data {
            type: CREDENTIALS
            type: ACCESS_TOKEN
          }
          data:
            "The user's OAuth token for Chrome Remote Desktop (CRD) and CRD "
            "host information such as CRD host public key, host version, and "
            "OS version."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { email: "garykac@chromium.org" }
            contacts { email: "jamiewalch@chromium.org" }
            contacts { email: "joedow@chromium.org" }
            contacts { email: "lambroslambrou@chromium.org" }
            contacts { email: "rkjnsn@chromium.org" }
            contacts { email: "yuweih@chromium.org" }
          }
          last_reviewed: "2023-07-07"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          chrome_policy {
            RemoteAccessHostAllowRemoteSupportConnections {
              RemoteAccessHostAllowRemoteSupportConnections: false
            }
            RemoteAccessHostAllowEnterpriseRemoteSupportConnections {
              RemoteAccessHostAllowEnterpriseRemoteSupportConnections: false
            }
          }
        })");

constexpr char kRegisterSupportHostPath[] =
    "/v1/remotesupport:registersupporthost";

}  // namespace

class RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl final
    : public RegisterSupportHostClient {
 public:
  RegisterSupportHostClientImpl(
      OAuthTokenGetter* token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  RegisterSupportHostClientImpl(const RegisterSupportHostClientImpl&) = delete;
  RegisterSupportHostClientImpl& operator=(
      const RegisterSupportHostClientImpl&) = delete;

  ~RegisterSupportHostClientImpl() override;

  void RegisterSupportHost(
      std::unique_ptr<apis::v1::RegisterSupportHostRequest> request,
      RegisterSupportHostResponseCallback callback) override;
  void CancelPendingRequests() override;

 private:
  ProtobufHttpClient http_client_;
};

RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl::
    RegisterSupportHostClientImpl(
        OAuthTokenGetter* token_getter,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : http_client_(ServiceUrls::GetInstance()->remoting_server_endpoint(),
                   token_getter,
                   url_loader_factory) {}

RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl::
    ~RegisterSupportHostClientImpl() = default;

void RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl::
    RegisterSupportHost(
        std::unique_ptr<apis::v1::RegisterSupportHostRequest> request,
        RegisterSupportHostResponseCallback callback) {
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(kTrafficAnnotation);
  request_config->path = kRegisterSupportHostPath;
  request_config->request_message = std::move(request);
  auto http_request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  http_request->SetResponseCallback(std::move(callback));
  http_client_.ExecuteRequest(std::move(http_request));
}

void RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl::
    CancelPendingRequests() {
  http_client_.CancelPendingRequests();
}

// End of RegisterSupportHostClientImpl.

RemotingRegisterSupportHostRequest::RemotingRegisterSupportHostRequest(
    std::unique_ptr<OAuthTokenGetter> token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : token_getter_(std::move(token_getter)),
      register_host_client_(std::make_unique<RegisterSupportHostClientImpl>(
          token_getter_.get(),
          url_loader_factory)) {}

RemotingRegisterSupportHostRequest::~RemotingRegisterSupportHostRequest() =
    default;

void RemotingRegisterSupportHostRequest::Initialize(
    std::unique_ptr<net::ClientCertStore> client_cert_store) {}

void RemotingRegisterSupportHostRequest::RegisterHost(
    const internal::RemoteSupportHostStruct& host,
    const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
    RegisterHostCallback callback) {
  auto request = std::make_unique<apis::v1::RegisterSupportHostRequest>();
  request->set_public_key(host.public_key);
  std::string tachyon_id = SignalingAddress::CreateFtlSignalingAddress(
                               host.tachyon_account_info.account_id,
                               host.tachyon_account_info.registration_id)
                               .id();
  request->set_tachyon_id(tachyon_id);
  request->set_host_version(host.version);
  request->set_host_os_name(host.operating_system_info.name);
  request->set_host_os_version(host.operating_system_info.version);

  if (enterprise_params.has_value()) {
    apis::v1::ChromeOsEnterpriseOptions* enterprise_options =
        request->mutable_chrome_os_enterprise_options();
    enterprise_options->set_allow_troubleshooting_tools(
        enterprise_params->allow_troubleshooting_tools);
    enterprise_options->set_show_troubleshooting_tools(
        enterprise_params->show_troubleshooting_tools);
    enterprise_options->set_allow_reconnections(
        enterprise_params->allow_reconnections);
    enterprise_options->set_allow_file_transfer(
        enterprise_params->allow_file_transfer);
    enterprise_options->set_connection_dialog_required(
        enterprise_params->connection_dialog_required);
    enterprise_options->set_allow_remote_input(
        enterprise_params->allow_remote_input);
    enterprise_options->set_allow_clipboard_sync(
        enterprise_params->allow_clipboard_sync);
    if (!enterprise_params->connection_auto_accept_timeout.is_zero()) {
      enterprise_options->mutable_connection_auto_accept_timeout()->set_seconds(
          enterprise_params->connection_auto_accept_timeout.InSeconds());
    }
  }

  if (!host.authorized_helper_email.empty()) {
    request->set_authorized_helper(host.authorized_helper_email);
  }

  register_host_client_->RegisterSupportHost(
      std::move(request),
      base::BindOnce(
          [](RegisterHostCallback callback, const HttpStatus& status,
             std::unique_ptr<apis::v1::RegisterSupportHostResponse> response) {
            if (response) {
              std::move(callback).Run(
                  status, response->support_id(),
                  base::Seconds(response->support_id_lifetime_seconds()));
            } else {
              std::move(callback).Run(status, {}, {});
            }
          },
          std::move(callback)));
}

void RemotingRegisterSupportHostRequest::CancelPendingRequests() {
  register_host_client_->CancelPendingRequests();
}

}  // namespace remoting
