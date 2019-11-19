// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_REMOTING_ICE_CONFIG_REQUEST_H_
#define REMOTING_PROTOCOL_REMOTING_ICE_CONFIG_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/url_request.h"
#include "remoting/protocol/ice_config_request.h"

namespace grpc {
class Status;
}  // namespace grpc

namespace remoting {

namespace apis {
namespace v1 {
class GetIceConfigResponse;
}  // namespace v1
}  // namespace apis

namespace protocol {

// IceConfigRequest that fetches IceConfig from the remoting NetworkTraversal
// service.
class RemotingIceConfigRequest final : public IceConfigRequest {
 public:
  RemotingIceConfigRequest();
  ~RemotingIceConfigRequest() override;

  // IceConfigRequest implementation.
  void Send(OnIceConfigCallback callback) override;

 private:
  friend class RemotingIceConfigRequestTest;
  class NetworkTraversalClient;

  void SetGrpcChannelForTest(GrpcChannelSharedPtr channel);

  void OnResponse(const grpc::Status& status,
                  const apis::v1::GetIceConfigResponse& response);

  OnIceConfigCallback on_ice_config_callback_;
  std::unique_ptr<NetworkTraversalClient> network_traversal_client_;

  DISALLOW_COPY_AND_ASSIGN(RemotingIceConfigRequest);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_REMOTING_ICE_CONFIG_REQUEST_H_
