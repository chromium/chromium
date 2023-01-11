// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/remoting_ice_config_request.h"

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/proto/remoting/v1/network_traversal_messages.pb.h"
#include "remoting/protocol/ice_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

namespace {

using testing::_;

using MockOnResultCallback =
    base::MockCallback<IceConfigRequest::OnIceConfigCallback>;

}  // namespace

class RemotingIceConfigRequestTest : public testing::Test {
 protected:
  std::unique_ptr<MockOnResultCallback> SendRequest(
      base::RunLoop* run_loop_to_quit,
      IceConfig* out_config) {
    auto mock_on_result = std::make_unique<MockOnResultCallback>();
    EXPECT_CALL(*mock_on_result, Run(_)).WillOnce([=](const IceConfig& config) {
      *out_config = config;
      run_loop_to_quit->Quit();
    });
    request_.Send(mock_on_result->Get());
    return mock_on_result;
  }

  base::test::TaskEnvironment task_environment_;
  ProtobufHttpTestResponder test_responder_;
  RemotingIceConfigRequest request_{test_responder_.GetUrlLoaderFactory(),
                                    nullptr};
};

TEST_F(RemotingIceConfigRequestTest, SuccessfulRequest) {
  base::RunLoop run_loop;
  IceConfig received_config;
  auto mock_on_result = SendRequest(&run_loop, &received_config);

  std::string api_key;
  ASSERT_TRUE(
      test_responder_.GetMostRecentPendingRequest().request.headers.GetHeader(
          "x-goog-api-key", &api_key));
  EXPECT_FALSE(api_key.empty());

  // Fill out the response.
  apis::v1::GetIceConfigResponse response;
  response.mutable_lifetime_duration()->set_seconds(43200);
  apis::v1::IceServer* turn_server = response.add_servers();
  turn_server->add_urls("turns:the_server.com");
  turn_server->set_username("123");
  turn_server->set_credential("abc");
  apis::v1::IceServer* stun_server = response.add_servers();
  stun_server->add_urls("stun:stun_server.com:18344");
  test_responder_.AddResponseToMostRecentRequestUrl(response);
  run_loop.Run();

  ASSERT_FALSE(received_config.is_null());

  EXPECT_EQ(1U, received_config.turn_servers.size());
  EXPECT_EQ(1U, received_config.stun_servers.size());
}

TEST_F(RemotingIceConfigRequestTest, FailedRequest) {
  base::RunLoop run_loop;
  IceConfig received_config;
  auto mock_on_result = SendRequest(&run_loop, &received_config);

  ASSERT_LT(0, test_responder_.GetNumPending());
  test_responder_.AddErrorToMostRecentRequestUrl(
      ProtobufHttpStatus(ProtobufHttpStatus::Code::INVALID_ARGUMENT, ""));
  run_loop.Run();

  EXPECT_TRUE(received_config.is_null());
}

}  // namespace remoting::protocol
