// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_config_fetcher_cloud.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/proto/google/internal/remoting/cloud/v1alpha/network_traversal_service.pb.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/ice_config_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

namespace {

using testing::_;

using MockOnResultCallback =
    base::MockCallback<IceConfigFetcher::OnIceConfigCallback>;

using ::google::internal::remoting::cloud::v1alpha::GenerateIceConfigResponse;
using ::google::internal::remoting::cloud::v1alpha::StunServer;
using ::google::internal::remoting::cloud::v1alpha::TurnServer;

constexpr int kLifetimeDurationSeconds = 43200;

}  // namespace

class IceConfigFetcherCloudTest : public testing::Test {
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

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ProtobufHttpTestResponder test_responder_;
  PassthroughOAuthTokenGetter oauth_token_getter_{{"blergh", "blargh"}};
  IceConfigFetcherCloud fetcher_{test_responder_.GetUrlLoaderFactory(),
                                 &oauth_token_getter_};
};

TEST_F(IceConfigFetcherCloudTest, StunOnlyResponse) {
  base::RunLoop run_loop;
  std::optional<IceConfig> received_config;
  auto mock_on_result = SendRequest(&run_loop, received_config);

  auto now = base::Time::Now();
  GenerateIceConfigResponse response;
  response.mutable_lifetime_duration()->set_seconds(kLifetimeDurationSeconds);
  auto* stun_server = response.add_stun_servers();
  stun_server->add_urls("stun:stun_server.com:18344");
  test_responder_.AddResponseToMostRecentRequestUrl(response);
  run_loop.Run();

  ASSERT_TRUE(received_config.has_value());

  EXPECT_EQ(received_config->turn_servers.size(), 0U);
  EXPECT_EQ(received_config->stun_servers.size(), 1U);
  EXPECT_EQ(received_config->max_bitrate_kbps, 0);
  EXPECT_GE(received_config->expiration_time,
            now + base::Seconds(kLifetimeDurationSeconds));
}

TEST_F(IceConfigFetcherCloudTest, TurnOnlyResponse) {
  base::RunLoop run_loop;
  std::optional<IceConfig> received_config;
  auto mock_on_result = SendRequest(&run_loop, received_config);

  auto now = base::Time::Now();
  GenerateIceConfigResponse response;
  response.mutable_lifetime_duration()->set_seconds(kLifetimeDurationSeconds);
  auto* turn_server_1 = response.add_turn_servers();
  turn_server_1->add_urls("turn:the_server.com");
  turn_server_1->add_urls("turn:the_server_1.com");
  turn_server_1->set_username("123");
  turn_server_1->set_credential("abc");
  auto* turn_server_2 = response.add_turn_servers();
  turn_server_2->add_urls("turns:the_server.com");
  turn_server_2->add_urls("turns:the_server_1.com");
  turn_server_2->set_username("123");
  turn_server_2->set_credential("abc");
  turn_server_2->set_max_rate_kbps(42);
  auto* turn_server_3 = response.add_turn_servers();
  turn_server_3->add_urls("turn:my_server.com");
  turn_server_3->set_username("123");
  turn_server_3->set_credential("abc");
  test_responder_.AddResponseToMostRecentRequestUrl(response);
  run_loop.Run();

  ASSERT_TRUE(received_config.has_value());

  EXPECT_EQ(received_config->turn_servers.size(), 5U);
  EXPECT_EQ(received_config->stun_servers.size(), 0U);
  EXPECT_EQ(received_config->max_bitrate_kbps, 42);
  EXPECT_GE(received_config->expiration_time,
            now + base::Seconds(kLifetimeDurationSeconds));
}

TEST_F(IceConfigFetcherCloudTest, MaxBitrateTest) {
  base::RunLoop run_loop;
  std::optional<IceConfig> received_config;
  auto mock_on_result = SendRequest(&run_loop, received_config);

  GenerateIceConfigResponse response;
  response.mutable_lifetime_duration()->set_seconds(kLifetimeDurationSeconds);
  auto* turn_server_1 = response.add_turn_servers();
  turn_server_1->add_urls("turn:the_server.com");
  turn_server_1->set_username("123");
  turn_server_1->set_credential("abc");
  turn_server_1->set_max_rate_kbps(10);
  auto* turn_server_2 = response.add_turn_servers();
  turn_server_2->add_urls("turns:the_server.com");
  turn_server_2->set_username("123");
  turn_server_2->set_credential("abc");
  turn_server_2->set_max_rate_kbps(5);  // Lowest value should be chosen.
  auto* turn_server_3 = response.add_turn_servers();
  turn_server_3->add_urls("turn:my_server.com");
  turn_server_3->set_username("123");
  turn_server_3->set_credential("abc");
  turn_server_3->set_max_rate_kbps(7);
  test_responder_.AddResponseToMostRecentRequestUrl(response);
  run_loop.Run();

  ASSERT_TRUE(received_config.has_value());
  EXPECT_EQ(received_config->max_bitrate_kbps, 5);
}

TEST_F(IceConfigFetcherCloudTest, StunAndTurnServersInResponse) {
  base::RunLoop run_loop;
  std::optional<IceConfig> received_config;
  auto mock_on_result = SendRequest(&run_loop, received_config);

  auto now = base::Time::Now();
  GenerateIceConfigResponse response;
  response.mutable_lifetime_duration()->set_seconds(kLifetimeDurationSeconds);
  auto* turn_server = response.add_turn_servers();
  turn_server->add_urls("turns:the_server.com");
  turn_server->set_username("123");
  turn_server->set_credential("abc");
  auto* stun_server = response.add_stun_servers();
  stun_server->add_urls("stun:stun_server.com:18344");
  test_responder_.AddResponseToMostRecentRequestUrl(response);
  run_loop.Run();

  ASSERT_TRUE(received_config.has_value());

  EXPECT_EQ(received_config->turn_servers.size(), 1U);
  EXPECT_EQ(received_config->stun_servers.size(), 1U);
  EXPECT_GE(received_config->expiration_time,
            now + base::Seconds(kLifetimeDurationSeconds));
}

TEST_F(IceConfigFetcherCloudTest, EmptyResponse) {
  base::RunLoop run_loop;
  std::optional<IceConfig> received_config;
  auto mock_on_result = SendRequest(&run_loop, received_config);

  auto now = base::Time::Now();
  GenerateIceConfigResponse response;
  response.mutable_lifetime_duration()->set_seconds(kLifetimeDurationSeconds);
  test_responder_.AddResponseToMostRecentRequestUrl(response);
  run_loop.Run();

  ASSERT_TRUE(received_config.has_value());

  EXPECT_EQ(received_config->turn_servers.size(), 0U);
  EXPECT_EQ(received_config->stun_servers.size(), 0U);
  EXPECT_LT(received_config->expiration_time,
            now + base::Seconds(kLifetimeDurationSeconds));
}

TEST_F(IceConfigFetcherCloudTest, FailedRequest) {
  base::RunLoop run_loop;
  std::optional<IceConfig> received_config;
  auto mock_on_result = SendRequest(&run_loop, received_config);

  ASSERT_GT(test_responder_.GetNumPending(), 0);
  test_responder_.AddErrorToMostRecentRequestUrl(
      ProtobufHttpStatus(ProtobufHttpStatus::Code::INVALID_ARGUMENT, ""));
  run_loop.Run();

  EXPECT_FALSE(received_config.has_value());
}

}  // namespace remoting::protocol
