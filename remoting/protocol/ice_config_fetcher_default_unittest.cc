// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_config_fetcher_default.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/proto/remoting/v1/network_traversal_messages.pb.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/ice_config_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

namespace {

using testing::_;

using MockOnResultCallback =
    base::MockCallback<IceConfigFetcher::OnIceConfigCallback>;

}  // namespace

class IceConfigFetcherDefaultTest : public testing::Test {
 protected:
  std::unique_ptr<MockOnResultCallback> SendRequest(
      base::RunLoop* run_loop_to_quit,
      std::optional<IceConfig>& out_config) {
    auto mock_on_result = std::make_unique<MockOnResultCallback>();
    EXPECT_CALL(*mock_on_result, Run(_))
        .WillOnce([=, &out_config](std::optional<IceConfig> ice_config) {
          out_config = std::move(ice_config);
          run_loop_to_quit->Quit();
        });
    fetcher_.GetIceConfig(mock_on_result->Get());
    return mock_on_result;
  }

  base::test::TaskEnvironment task_environment_;
  ProtobufHttpTestResponder test_responder_;
  IceConfigFetcherDefault fetcher_{test_responder_.GetUrlLoaderFactory(),
                                   nullptr};
};

TEST_F(IceConfigFetcherDefaultTest, SuccessfulRequest) {
  base::RunLoop run_loop;
  std::optional<IceConfig> received_config;
  auto mock_on_result = SendRequest(&run_loop, received_config);

  EXPECT_THAT(
      test_responder_.GetMostRecentPendingRequest().request.headers.GetHeader(
          "x-goog-api-key"),
      testing::Optional(testing::Not(testing::IsEmpty())));

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

  ASSERT_TRUE(received_config.has_value());

  EXPECT_EQ(1U, received_config->turn_servers.size());
  EXPECT_EQ(1U, received_config->stun_servers.size());
}

TEST_F(IceConfigFetcherDefaultTest, FailedRequest) {
  base::RunLoop run_loop;
  std::optional<IceConfig> received_config;
  auto mock_on_result = SendRequest(&run_loop, received_config);

  ASSERT_LT(0, test_responder_.GetNumPending());
  test_responder_.AddErrorToMostRecentRequestUrl(
      ProtobufHttpStatus(ProtobufHttpStatus::Code::INVALID_ARGUMENT, ""));
  run_loop.Run();

  EXPECT_FALSE(received_config.has_value());
}

}  // namespace remoting::protocol
