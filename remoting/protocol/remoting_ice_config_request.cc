// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/remoting_ice_config_request.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/grpc_support/grpc_async_executor.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/network_traversal_service.grpc.pb.h"
#include "remoting/protocol/ice_config.h"

namespace remoting {
namespace protocol {

namespace {

using GetIceConfigCallback =
    base::OnceCallback<void(const grpc::Status&,
                            const apis::v1::GetIceConfigResponse&)>;

}  // namespace

class RemotingIceConfigRequest::NetworkTraversalClient {
 public:
  NetworkTraversalClient();
  explicit NetworkTraversalClient(GrpcChannelSharedPtr channel);
  ~NetworkTraversalClient();

  void GetIceConfig(GetIceConfigCallback callback);

 private:
  using NetworkTraversalService = apis::v1::RemotingNetworkTraversalService;

  GrpcAsyncExecutor executor_;
  std::unique_ptr<NetworkTraversalService::Stub> network_traversal_;
};

RemotingIceConfigRequest::NetworkTraversalClient::NetworkTraversalClient()
    : NetworkTraversalClient(CreateSslChannelForEndpoint(
          ServiceUrls::GetInstance()->remoting_server_endpoint())) {}

RemotingIceConfigRequest::NetworkTraversalClient::NetworkTraversalClient(
    GrpcChannelSharedPtr channel) {
  network_traversal_ = NetworkTraversalService::NewStub(channel);
}

RemotingIceConfigRequest::NetworkTraversalClient::~NetworkTraversalClient() =
    default;

void RemotingIceConfigRequest::NetworkTraversalClient::GetIceConfig(
    GetIceConfigCallback callback) {
  auto async_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&NetworkTraversalService::Stub::AsyncGetIceConfig,
                     base::Unretained(network_traversal_.get())),
      apis::v1::GetIceConfigRequest(), std::move(callback));
#if defined(OS_CHROMEOS)
  // Use the default Chrome API key for ChromeOS as the only host instance
  // which runs there is used for the ChromeOS Enterprise Kiosk mode
  // scenario.  If we decide to implement a remote access host for ChromeOS,
  // then we will need a way for the caller to provide an API key.
  async_request->context()->AddMetadata("x-goog-api-key",
                                        google_apis::GetAPIKey());
#else
  async_request->context()->AddMetadata("x-goog-api-key",
                                        google_apis::GetRemotingAPIKey());
#endif

  executor_.ExecuteRpc(std::move(async_request));
}

// End of RemotingIceConfigRequest::NetworkTraversalClient

RemotingIceConfigRequest::RemotingIceConfigRequest() {
  network_traversal_client_ = std::make_unique<NetworkTraversalClient>();
}

RemotingIceConfigRequest::~RemotingIceConfigRequest() = default;

void RemotingIceConfigRequest::Send(OnIceConfigCallback callback) {
  DCHECK(on_ice_config_callback_.is_null());
  DCHECK(!callback.is_null());

  on_ice_config_callback_ = std::move(callback);

  network_traversal_client_->GetIceConfig(base::BindOnce(
      &RemotingIceConfigRequest::OnResponse, base::Unretained(this)));
}

void RemotingIceConfigRequest::SetGrpcChannelForTest(
    GrpcChannelSharedPtr channel) {
  network_traversal_client_ = std::make_unique<NetworkTraversalClient>(channel);
}

void RemotingIceConfigRequest::OnResponse(
    const grpc::Status& status,
    const apis::v1::GetIceConfigResponse& response) {
  DCHECK(!on_ice_config_callback_.is_null());

  if (!status.ok()) {
    LOG(ERROR) << "Received error code: " << status.error_code()
               << ", message: " << status.error_message();
    std::move(on_ice_config_callback_).Run(IceConfig());
    return;
  }

  std::move(on_ice_config_callback_).Run(IceConfig::Parse(response));
}

}  // namespace protocol
}  // namespace remoting
