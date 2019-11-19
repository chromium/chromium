// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/remoting_ice_config_request.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/grpc_test_support/grpc_test_server.h"
#include "remoting/proto/remoting/v1/network_traversal_service.grpc.pb.h"
#include "remoting/protocol/ice_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {
namespace protocol {

namespace {

using testing::_;
using testing::Invoke;

class MockNetworkTraversalService final
    : public apis::v1::RemotingNetworkTraversalService::Service {
 public:
  MOCK_METHOD3(GetIceConfig,
               grpc::Status(grpc::ServerContext*,
                            const apis::v1::GetIceConfigRequest*,
                            apis::v1::GetIceConfigResponse*));
};

}  // namespace

class RemotingIceConfigRequestTest : public testing::Test {
 public:
  RemotingIceConfigRequestTest() {
    request_.SetGrpcChannelForTest(test_server_.CreateInProcessChannel());
  }

 protected:
  IceConfig SendRequestAndReceiveConfig() {
    base::MockCallback<IceConfigRequest::OnIceConfigCallback> mock_on_result;
    IceConfig received_config;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_on_result, Run(_))
        .WillOnce(Invoke([&](const IceConfig& config) {
          received_config = config;
          run_loop.Quit();
        }));
    request_.Send(mock_on_result.Get());
    run_loop.Run();
    return received_config;
  }

  base::test::TaskEnvironment task_environment_;
  RemotingIceConfigRequest request_;
  test::GrpcTestServer<MockNetworkTraversalService> test_server_;
};

TEST_F(RemotingIceConfigRequestTest, SuccessfulRequest) {
  EXPECT_CALL(*test_server_, GetIceConfig(_, _, _))
      .WillOnce(Invoke([](grpc::ServerContext* context,
                          const apis::v1::GetIceConfigRequest* request,
                          apis::v1::GetIceConfigResponse* response) {
        // Verify API key is set.
        auto it = context->client_metadata().find("x-goog-api-key");
        EXPECT_NE(context->client_metadata().end(), it);
        EXPECT_FALSE(it->second.empty());

        // Fill out the response.
        response->mutable_lifetime_duration()->set_seconds(43200);
        apis::v1::IceServer* turn_server = response->add_servers();
        turn_server->add_urls("turns:the_server.com");
        turn_server->set_username("123");
        turn_server->set_credential("abc");
        apis::v1::IceServer* stun_server = response->add_servers();
        stun_server->add_urls("stun:stun_server.com:18344");

        return grpc::Status::OK;
      }));
  IceConfig received_config = SendRequestAndReceiveConfig();

  ASSERT_FALSE(received_config.is_null());

  EXPECT_EQ(1U, received_config.turn_servers.size());
  EXPECT_EQ(1U, received_config.stun_servers.size());
}

TEST_F(RemotingIceConfigRequestTest, FailedRequest) {
  EXPECT_CALL(*test_server_, GetIceConfig(_, _, _))
      .WillOnce(Invoke([](grpc::ServerContext* context,
                          const apis::v1::GetIceConfigRequest* request,
                          apis::v1::GetIceConfigResponse* response) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "");
      }));

  IceConfig received_config = SendRequestAndReceiveConfig();
  EXPECT_TRUE(received_config.is_null());
}

}  // namespace protocol
}  // namespace remoting
