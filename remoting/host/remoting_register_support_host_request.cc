// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remoting_register_support_host_request.h"

#include "base/strings/stringize_macros.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/service_urls.h"
#include "remoting/host/host_details.h"
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

protocol::ErrorCode MapError(ProtobufHttpStatus::Code status_code) {
  switch (status_code) {
    case ProtobufHttpStatus::Code::OK:
      return protocol::ErrorCode::OK;
    case ProtobufHttpStatus::Code::DEADLINE_EXCEEDED:
      return protocol::ErrorCode::SIGNALING_TIMEOUT;
    case ProtobufHttpStatus::Code::PERMISSION_DENIED:
    case ProtobufHttpStatus::Code::UNAUTHENTICATED:
      return protocol::ErrorCode::AUTHENTICATION_FAILED;
    default:
      return protocol::ErrorCode::SIGNALING_ERROR;
  }
}

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

RemotingRegisterSupportHostRequest::~RemotingRegisterSupportHostRequest() {
  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
  }
}

void RemotingRegisterSupportHostRequest::StartRequest(
    SignalStrategy* signal_strategy,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& authorized_helper,
    std::optional<ChromeOsEnterpriseParams> params,
    RegisterCallback callback) {
  DCHECK_EQ(State::NOT_STARTED, state_);
  DCHECK(signal_strategy);
  DCHECK(key_pair);
  DCHECK(callback);
  signal_strategy_ = signal_strategy;
  key_pair_ = key_pair;
  callback_ = std::move(callback);
  enterprise_params_ = std::move(params);
  authorized_helper_ = authorized_helper;

  signal_strategy_->AddListener(this);
}

void RemotingRegisterSupportHostRequest::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  switch (state) {
    case SignalStrategy::State::CONNECTED:
      RegisterHost();
      break;
    case SignalStrategy::State::DISCONNECTED:
      RunCallback({}, {}, protocol::ErrorCode::SIGNALING_ERROR);
      break;
    default:
      // Do nothing.
      break;
  }
}

bool RemotingRegisterSupportHostRequest::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void RemotingRegisterSupportHostRequest::RegisterHost() {
  DCHECK_EQ(SignalStrategy::CONNECTED, signal_strategy_->GetState());
  if (state_ != State::NOT_STARTED) {
    return;
  }
  state_ = State::REGISTERING;

  auto request = std::make_unique<apis::v1::RegisterSupportHostRequest>();
  request->set_public_key(key_pair_->GetPublicKey());
  request->set_tachyon_id(signal_strategy_->GetLocalAddress().id());
  request->set_host_version(STRINGIZE(VERSION));
  request->set_host_os_name(GetHostOperatingSystemName());
  request->set_host_os_version(GetHostOperatingSystemVersion());

  if (enterprise_params_.has_value()) {
    apis::v1::ChromeOsEnterpriseOptions* enterprise_options =
        request->mutable_chrome_os_enterprise_options();
    enterprise_options->set_allow_troubleshooting_tools(
        enterprise_params_->allow_troubleshooting_tools);
    enterprise_options->set_show_troubleshooting_tools(
        enterprise_params_->show_troubleshooting_tools);
    enterprise_options->set_allow_reconnections(
        enterprise_params_->allow_reconnections);
    enterprise_options->set_allow_file_transfer(
        enterprise_params_->allow_file_transfer);
  }

  if (!authorized_helper_.empty()) {
    request->set_authorized_helper(authorized_helper_);
  }

  register_host_client_->RegisterSupportHost(
      std::move(request),
      base::BindOnce(&RemotingRegisterSupportHostRequest::OnRegisterHostResult,
                     base::Unretained(this)));
}

void RemotingRegisterSupportHostRequest::OnRegisterHostResult(
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::RegisterSupportHostResponse> response) {
  if (!status.ok()) {
    state_ = State::NOT_STARTED;
    RunCallback({}, {}, MapError(status.error_code()));
    return;
  }
  state_ = State::REGISTERED;
  base::TimeDelta lifetime =
      base::Seconds(response->support_id_lifetime_seconds());
  RunCallback(response->support_id(), lifetime, protocol::ErrorCode::OK);
}

void RemotingRegisterSupportHostRequest::RunCallback(
    const std::string& support_id,
    base::TimeDelta lifetime,
    protocol::ErrorCode error_code) {
  if (!callback_) {
    // Callback has already been run, so just return.
    return;
  }

  // Cleanup state before calling the callback.
  register_host_client_->CancelPendingRequests();
  signal_strategy_->RemoveListener(this);
  signal_strategy_ = nullptr;

  std::move(callback_).Run(support_id, lifetime, error_code);
}

}  // namespace remoting
