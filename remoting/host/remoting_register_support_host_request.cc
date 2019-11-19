// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remoting_register_support_host_request.h"

#include "base/strings/stringize_macros.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/grpc_support/grpc_util.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/service_urls.h"
#include "remoting/host/host_details.h"
#include "remoting/proto/remoting/v1/remote_support_host_service.grpc.pb.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

namespace {

protocol::ErrorCode MapError(grpc::StatusCode status_code) {
  switch (status_code) {
    case grpc::StatusCode::OK:
      return protocol::ErrorCode::OK;
    case grpc::StatusCode::DEADLINE_EXCEEDED:
      return protocol::ErrorCode::SIGNALING_TIMEOUT;
    case grpc::StatusCode::PERMISSION_DENIED:
    case grpc::StatusCode::UNAUTHENTICATED:
      return protocol::ErrorCode::AUTHENTICATION_FAILED;
    default:
      return protocol::ErrorCode::SIGNALING_ERROR;
  }
}

}  // namespace

class RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl final
    : public RegisterSupportHostClient {
 public:
  explicit RegisterSupportHostClientImpl(OAuthTokenGetter* token_getter);
  ~RegisterSupportHostClientImpl() override;

  void RegisterSupportHost(
      const apis::v1::RegisterSupportHostRequest& request,
      RegisterSupportHostResponseCallback callback) override;
  void CancelPendingRequests() override;

 private:
  using RemoteSupportService = apis::v1::RemoteSupportService;

  GrpcAuthenticatedExecutor grpc_executor_;
  std::unique_ptr<RemoteSupportService::Stub> remote_support_;

  DISALLOW_COPY_AND_ASSIGN(RegisterSupportHostClientImpl);
};

RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl::
    RegisterSupportHostClientImpl(OAuthTokenGetter* token_getter)
    : grpc_executor_(token_getter),
      remote_support_(RemoteSupportService::NewStub(CreateSslChannelForEndpoint(
          ServiceUrls::GetInstance()->remoting_server_endpoint()))) {}

RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl::
    ~RegisterSupportHostClientImpl() = default;

void RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl::
    RegisterSupportHost(const apis::v1::RegisterSupportHostRequest& request,
                        RegisterSupportHostResponseCallback callback) {
  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&RemoteSupportService::Stub::AsyncRegisterSupportHost,
                     base::Unretained(remote_support_.get())),
      request, std::move(callback));
  grpc_executor_.ExecuteRpc(std::move(grpc_request));
}

void RemotingRegisterSupportHostRequest::RegisterSupportHostClientImpl::
    CancelPendingRequests() {
  grpc_executor_.CancelPendingRequests();
}

// End of RegisterSupportHostClientImpl.

RemotingRegisterSupportHostRequest::RemotingRegisterSupportHostRequest(
    std::unique_ptr<OAuthTokenGetter> token_getter)
    : token_getter_(std::move(token_getter)),
      register_host_client_(std::make_unique<RegisterSupportHostClientImpl>(
          token_getter_.get())) {}

RemotingRegisterSupportHostRequest::~RemotingRegisterSupportHostRequest() {
  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
  }
}

void RemotingRegisterSupportHostRequest::StartRequest(
    SignalStrategy* signal_strategy,
    scoped_refptr<RsaKeyPair> key_pair,
    RegisterCallback callback) {
  DCHECK_EQ(State::NOT_STARTED, state_);
  DCHECK(signal_strategy);
  DCHECK(key_pair);
  DCHECK(callback);
  signal_strategy_ = signal_strategy;
  key_pair_ = key_pair;
  callback_ = std::move(callback);

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

  apis::v1::RegisterSupportHostRequest request;
  request.set_public_key(key_pair_->GetPublicKey());
  request.set_tachyon_id(signal_strategy_->GetLocalAddress().id());
  request.set_host_version(STRINGIZE(VERSION));
  request.set_host_os_name(GetHostOperatingSystemName());
  request.set_host_os_version(GetHostOperatingSystemVersion());

  register_host_client_->RegisterSupportHost(
      request,
      base::BindOnce(&RemotingRegisterSupportHostRequest::OnRegisterHostResult,
                     base::Unretained(this)));
}

void RemotingRegisterSupportHostRequest::OnRegisterHostResult(
    const grpc::Status& status,
    const apis::v1::RegisterSupportHostResponse& response) {
  if (!status.ok()) {
    state_ = State::NOT_STARTED;
    RunCallback({}, {}, MapError(status.error_code()));
    return;
  }
  state_ = State::REGISTERED;
  base::TimeDelta lifetime =
      base::TimeDelta::FromSeconds(response.support_id_lifetime_seconds());
  RunCallback(response.support_id(), lifetime, protocol::ErrorCode::OK);
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
