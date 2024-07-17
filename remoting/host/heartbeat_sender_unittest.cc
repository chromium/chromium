// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::AtMost;
using testing::InSequence;
using testing::Return;

using LegacyHeartbeatResponseCallback =
    base::OnceCallback<void(const ProtobufHttpStatus&,
                            std::unique_ptr<apis::v1::HeartbeatResponse>)>;
using SendHeartbeatResponseCallback =
    base::OnceCallback<void(const ProtobufHttpStatus&,
                            std::unique_ptr<apis::v1::SendHeartbeatResponse>)>;

constexpr char kOAuthAccessToken[] = "fake_access_token";
constexpr char kHostId[] = "fake_host_id";
constexpr char kUserEmail[] = "fake_user@domain.com";

constexpr char kFtlId[] = "fake_user@domain.com/chromoting_ftl_abc123";

constexpr int32_t kGoodIntervalSeconds = 300;

constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::Seconds(5.5);
constexpr base::TimeDelta kOfflineReasonTimeout = base::Seconds(123);
constexpr base::TimeDelta kTestHeartbeatDelay = base::Seconds(350);

void ValidateLegacyHeartbeat(
    std::unique_ptr<apis::v1::HeartbeatRequest> request,
    bool expected_is_initial_heartbeat = false,
    const std::string& expected_host_offline_reason = {},
    bool is_googler = false) {
  ASSERT_TRUE(request->has_host_version());
  if (expected_host_offline_reason.empty()) {
    ASSERT_FALSE(request->has_host_offline_reason());
  } else {
    ASSERT_EQ(expected_host_offline_reason, request->host_offline_reason());
  }
  ASSERT_EQ(kHostId, request->host_id());
  ASSERT_EQ(kFtlId, request->tachyon_id());
  ASSERT_TRUE(request->has_host_version());
  ASSERT_TRUE(request->has_host_os_version());
  ASSERT_TRUE(request->has_host_os_name());
  ASSERT_TRUE(request->has_host_cpu_type());
  ASSERT_EQ(expected_is_initial_heartbeat, request->is_initial_heartbeat());

  // We expect hostname (fqdn) to be populated for a Googler-owner host.
  ASSERT_EQ(is_googler, request->has_hostname());
}

decltype(auto) DoValidateLegacyHeartbeatAndRespondOk(
    bool expected_is_initial_heartbeat = false,
    const std::string& expected_host_offline_reason = {},
    bool is_googler = false,
    bool use_lite_heartbeat = false) {
  return [=](std::unique_ptr<apis::v1::HeartbeatRequest> request,
             LegacyHeartbeatResponseCallback callback) {
    ValidateLegacyHeartbeat(std::move(request), expected_is_initial_heartbeat,
                            expected_host_offline_reason, is_googler);
    auto response = std::make_unique<apis::v1::HeartbeatResponse>();
    response->set_set_interval_seconds(kGoodIntervalSeconds);
    response->set_use_lite_heartbeat(use_lite_heartbeat);
    std::move(callback).Run(ProtobufHttpStatus::OK(), std::move(response));
  };
}

decltype(auto) DoValidateSendHeartbeatAndRespondOk() {
  return [=](std::unique_ptr<apis::v1::SendHeartbeatRequest> request,
             SendHeartbeatResponseCallback callback) {
    ASSERT_EQ(kHostId, request->host_id());
    auto response = std::make_unique<apis::v1::SendHeartbeatResponse>();
    response->set_wait_interval_seconds(kGoodIntervalSeconds);
    std::move(callback).Run(ProtobufHttpStatus::OK(), std::move(response));
  };
}

class MockDelegate : public HeartbeatSender::Delegate {
 public:
  MOCK_METHOD0(OnFirstHeartbeatSuccessful, void());
  MOCK_METHOD0(OnHostNotFound, void());
  MOCK_METHOD0(OnAuthFailed, void());
};

class MockObserver : public HeartbeatSender::Observer {
 public:
  MOCK_METHOD0(OnHeartbeatSent, void());
};

}  // namespace

class HeartbeatSenderTest : public testing::Test {
 public:
  HeartbeatSenderTest() {
    signal_strategy_ =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kFtlId));

    // Start in disconnected state.
    signal_strategy_->Disconnect();

    mock_observer_ = std::make_unique<MockObserver>();

    heartbeat_sender_ = std::make_unique<HeartbeatSender>(
        &mock_delegate_, kHostId, signal_strategy_.get(), &oauth_token_getter_,
        mock_observer_.get(), nullptr, false);
    auto heartbeat_client = std::make_unique<MockHeartbeatClient>();
    mock_client_ = heartbeat_client.get();
    heartbeat_sender_->client_ = std::move(heartbeat_client);
  }

  ~HeartbeatSenderTest() override {
    heartbeat_sender_.reset();
    signal_strategy_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  class MockHeartbeatClient : public HeartbeatSender::HeartbeatClient {
   public:
    MOCK_METHOD2(LegacyHeartbeat,
                 void(std::unique_ptr<apis::v1::HeartbeatRequest>,
                      LegacyHeartbeatResponseCallback));
    MOCK_METHOD2(SendHeartbeat,
                 void(std::unique_ptr<apis::v1::SendHeartbeatRequest>,
                      SendHeartbeatResponseCallback));

    void CancelPendingRequests() override {
      // We just don't care about this method being called.
    }
  };

  HeartbeatSender* heartbeat_sender() { return heartbeat_sender_.get(); }

  void set_is_googler() { heartbeat_sender()->is_googler_ = true; }

  const net::BackoffEntry& GetBackoff() const {
    return heartbeat_sender_->backoff_;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<MockHeartbeatClient, DanglingUntriaged> mock_client_;
  std::unique_ptr<MockObserver> mock_observer_;

  std::unique_ptr<FakeSignalStrategy> signal_strategy_;

  MockDelegate mock_delegate_;

 private:
  // |heartbeat_sender_| must be deleted before |signal_strategy_|.
  std::unique_ptr<HeartbeatSender> heartbeat_sender_;

  FakeOAuthTokenGetter oauth_token_getter_{OAuthTokenGetter::Status::SUCCESS,
                                           kUserEmail, kOAuthAccessToken};
};

TEST_F(HeartbeatSenderTest, SendHeartbeat) {
  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true));
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent());
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
}

TEST_F(HeartbeatSenderTest, SendHeartbeat_Googler) {
  set_is_googler();

  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true, "", true));
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent());
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
}

TEST_F(HeartbeatSenderTest, SignalingReconnect_NewHeartbeats) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk())
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk());
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);

  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, SignalingReconnect_NewHeartbeats_Lite) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true, "", false, true))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(false, "", false, true))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(false, "", false, true));
  // SendHeartbeat is not called because host keeps reconnecting.
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);

  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, SignalingReconnect_NewHeartbeats_Googler) {
  base::RunLoop run_loop;

  set_is_googler();

  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true, "", true))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(false, "", true))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(false, "", true));
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);

  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, Signaling_MultipleHeartbeats) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk())
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk());
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kTestHeartbeatDelay * 2);
}

TEST_F(HeartbeatSenderTest, Signaling_MultipleHeartbeats_Googler) {
  base::RunLoop run_loop;

  set_is_googler();

  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true, "", true))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(false, "", true))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(false, "", true));
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kTestHeartbeatDelay * 2);
}

TEST_F(HeartbeatSenderTest, Signaling_MultipleHeartbeats_Lite) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true, "", false, true));
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _))
      .WillOnce(DoValidateSendHeartbeatAndRespondOk())
      .WillOnce(DoValidateSendHeartbeatAndRespondOk());
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kTestHeartbeatDelay * 2);
}

TEST_F(HeartbeatSenderTest, SetHostOfflineReason) {
  base::MockCallback<base::OnceCallback<void(bool success)>> mock_ack_callback;
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(0);

  heartbeat_sender()->SetHostOfflineReason("test_error", kOfflineReasonTimeout,
                                           mock_ack_callback.Get());

  testing::Mock::VerifyAndClearExpectations(&mock_ack_callback);

  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true, "test_error"));
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent());

  // Callback should run once, when we get response to offline-reason.
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(1);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);

  signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, UnknownHostId) {
  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillRepeatedly([](std::unique_ptr<apis::v1::HeartbeatRequest> request,
                         LegacyHeartbeatResponseCallback callback) {
        ValidateLegacyHeartbeat(std::move(request), true);
        std::move(callback).Run(
            ProtobufHttpStatus(ProtobufHttpStatus::Code::NOT_FOUND,
                               "not found"),
            nullptr);
      });

  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).WillRepeatedly(Return());

  EXPECT_CALL(mock_delegate_, OnHostNotFound()).Times(1);

  signal_strategy_->Connect();

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(HeartbeatSenderTest, FailedToHeartbeat_Backoff) {
  {
    InSequence sequence;

    EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
        .Times(2)
        .WillRepeatedly([&](std::unique_ptr<apis::v1::HeartbeatRequest> request,
                            LegacyHeartbeatResponseCallback callback) {
          ValidateLegacyHeartbeat(std::move(request), true);
          std::move(callback).Run(
              ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAVAILABLE,
                                 "unavailable"),
              nullptr);
        });

    EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
        .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true));
  }
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);

  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).WillRepeatedly(Return());

  ASSERT_EQ(0, GetBackoff().failure_count());
  signal_strategy_->Connect();
  ASSERT_EQ(1, GetBackoff().failure_count());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(2, GetBackoff().failure_count());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(0, GetBackoff().failure_count());
}

TEST_F(HeartbeatSenderTest, HostComesBackOnlineAfterServiceOutage) {
  // Each call will simulate ~10 minutes of time (at max backoff duration).
  // We want to simulate a long outage (~3 hours) so run through 20 iterations.
  int retry_attempts = 20;

  {
    InSequence sequence;

    EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
        .Times(retry_attempts)
        .WillRepeatedly([&](std::unique_ptr<apis::v1::HeartbeatRequest> request,
                            LegacyHeartbeatResponseCallback callback) {
          ValidateLegacyHeartbeat(std::move(request), true);
          std::move(callback).Run(
              ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAVAILABLE,
                                 "unavailable"),
              nullptr);
        });

    EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
        .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true));
  }
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);

  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).WillRepeatedly(Return());

  ASSERT_EQ(0, GetBackoff().failure_count());
  signal_strategy_->Connect();
  for (int i = 1; i <= retry_attempts; i++) {
    ASSERT_EQ(i, GetBackoff().failure_count());
    task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  }

  // Host successfully back online.
  ASSERT_EQ(0, GetBackoff().failure_count());
}

TEST_F(HeartbeatSenderTest, Unauthenticated) {
  int legacy_heartbeat_count = 0;
  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillRepeatedly([&](std::unique_ptr<apis::v1::HeartbeatRequest> request,
                          LegacyHeartbeatResponseCallback callback) {
        ValidateLegacyHeartbeat(std::move(request), true);
        legacy_heartbeat_count++;
        std::move(callback).Run(
            ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAUTHENTICATED,
                               "unauthenticated"),
            nullptr);
      });
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).WillRepeatedly(Return());
  EXPECT_CALL(mock_delegate_, OnAuthFailed()).Times(1);

  signal_strategy_->Connect();
  task_environment_.FastForwardUntilNoTasksRemain();

  // Should retry heartbeating at least once.
  ASSERT_LT(1, legacy_heartbeat_count);
}

TEST_F(HeartbeatSenderTest, GooglerHostname) {
  set_is_googler();
  EXPECT_CALL(*mock_client_, LegacyHeartbeat(_, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(true, "", true));
  EXPECT_CALL(*mock_client_, SendHeartbeat(_, _)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(1);
  signal_strategy_->Connect();
}

}  // namespace remoting
