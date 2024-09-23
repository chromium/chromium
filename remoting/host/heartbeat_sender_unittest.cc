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
#include "remoting/host/heartbeat_service_client.h"
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
constexpr char kScopes[] = "fake_scope";

constexpr char kFtlId[] = "fake_user@domain.com/chromoting_ftl_abc123";

constexpr int32_t kGoodIntervalSeconds = 300;

constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::Seconds(5.5);
constexpr base::TimeDelta kOfflineReasonTimeout = base::Seconds(123);
constexpr base::TimeDelta kTestHeartbeatDelay = base::Seconds(350);

struct ValidateLegacyHeartbeatOptions {
  // Request options.
  bool is_initial_heartbeat = false;
  std::string host_offline_reason = "";

  // Response options.
  bool use_lite_heartbeat = false;
  std::string host_owner = "";
  std::optional<bool> require_session_auth = std::nullopt;
};

decltype(auto) DoValidateLegacyHeartbeatAndRespondOk(
    const ValidateLegacyHeartbeatOptions& options) {
  return [=](bool is_initial_heartbeat, std::optional<std::string> signaling_id,
             std::optional<std::string> offline_reason,
             HeartbeatServiceClient::HeartbeatResponseCallback callback) {
    ASSERT_EQ(is_initial_heartbeat, options.is_initial_heartbeat);
    if (options.host_offline_reason.empty()) {
      ASSERT_FALSE(offline_reason);
    } else {
      ASSERT_EQ(options.host_offline_reason, *offline_reason);
    }

    base::TimeDelta wait_interval = base::Seconds(kGoodIntervalSeconds);
    std::move(callback).Run(ProtobufHttpStatus::OK(),
                            std::make_optional(wait_interval),
                            options.host_owner, options.require_session_auth,
                            std::make_optional(options.use_lite_heartbeat));
  };
}

decltype(auto) DoValidateSendHeartbeatAndRespondOk() {
  return [=](HeartbeatServiceClient::HeartbeatResponseCallback callback) {
    base::TimeDelta wait_interval = base::Seconds(kGoodIntervalSeconds);
    std::move(callback).Run(ProtobufHttpStatus::OK(),
                            std::make_optional(wait_interval), kUserEmail,
                            false, std::nullopt);
  };
}

class MockDelegate : public HeartbeatSender::Delegate {
 public:
  MOCK_METHOD0(OnFirstHeartbeatSuccessful, void());
  MOCK_METHOD1(OnUpdateHostOwner, void(const std::string& host_owner));
  MOCK_METHOD1(OnUpdateRequireSessionAuthorization, void(bool require));
  MOCK_METHOD0(OnHostNotFound, void());
  MOCK_METHOD0(OnAuthFailed, void());
};

class MockHeartbeatServiceClient : public HeartbeatServiceClient {
 public:
  MOCK_METHOD4(SendFullHeartbeat,
               void(bool is_initial_heartbeat,
                    std::optional<std::string> signaling_id,
                    std::optional<std::string> offline_reason,
                    HeartbeatResponseCallback callback));
  MOCK_METHOD1(SendLiteHeartbeat, void(HeartbeatResponseCallback callback));
  MOCK_METHOD0(CancelPendingRequests, void());
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

    auto mock_client = std::make_unique<MockHeartbeatServiceClient>();
    mock_client_ = mock_client.get();

    mock_observer_ = std::make_unique<MockObserver>();

    heartbeat_sender_ = std::make_unique<HeartbeatSender>(
        &mock_delegate_, kHostId, signal_strategy_.get(), &oauth_token_getter_,
        std::move(mock_client), mock_observer_.get(), nullptr, false);
  }

  ~HeartbeatSenderTest() override {
    heartbeat_sender_.reset();
    signal_strategy_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  HeartbeatSender* heartbeat_sender() { return heartbeat_sender_.get(); }

  const net::BackoffEntry& GetBackoff() const {
    return heartbeat_sender_->backoff_;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<MockHeartbeatServiceClient, DanglingUntriaged> mock_client_;
  std::unique_ptr<MockObserver> mock_observer_;

  std::unique_ptr<FakeSignalStrategy> signal_strategy_;

  MockDelegate mock_delegate_;

 private:
  // |heartbeat_sender_| must be deleted before |signal_strategy_|.
  std::unique_ptr<HeartbeatSender> heartbeat_sender_;

  FakeOAuthTokenGetter oauth_token_getter_{OAuthTokenGetter::Status::SUCCESS,
                                           kUserEmail, kOAuthAccessToken,
                                           kScopes};
};

TEST_F(HeartbeatSenderTest, SendHeartbeat) {
  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
  };

  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst));
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent());
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);
  EXPECT_CALL(mock_delegate_, OnUpdateHostOwner(_)).Times(0);
  EXPECT_CALL(mock_delegate_, OnUpdateRequireSessionAuthorization(_)).Times(0);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
}

TEST_F(HeartbeatSenderTest, SendHeartbeat_WithOwnerEmail) {
  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
      .host_owner = "email",
  };

  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst));
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent());
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);
  EXPECT_CALL(mock_delegate_, OnUpdateHostOwner(_)).Times(1);
  EXPECT_CALL(mock_delegate_, OnUpdateRequireSessionAuthorization(_)).Times(0);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
}

TEST_F(HeartbeatSenderTest, SendHeartbeat_RequireSessionAuth) {
  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
      .require_session_auth = true,
  };

  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst));
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent());
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);
  EXPECT_CALL(mock_delegate_, OnUpdateHostOwner(_)).Times(0);
  EXPECT_CALL(mock_delegate_, OnUpdateRequireSessionAuthorization(_)).Times(1);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
}

TEST_F(HeartbeatSenderTest, SignalingReconnect_NewHeartbeats) {
  base::RunLoop run_loop;

  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
  };
  ValidateLegacyHeartbeatOptions options;

  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(options))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(options));
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);
  EXPECT_CALL(mock_delegate_, OnUpdateHostOwner(_)).Times(0);
  EXPECT_CALL(mock_delegate_, OnUpdateRequireSessionAuthorization(_)).Times(0);

  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, SignalingReconnect_NewHeartbeats_Lite) {
  base::RunLoop run_loop;

  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
      .use_lite_heartbeat = true,
  };
  ValidateLegacyHeartbeatOptions options{
      .use_lite_heartbeat = true,
  };

  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(options))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(options));
  // SendHeartbeat is not called because host keeps reconnecting.
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);
  EXPECT_CALL(mock_delegate_, OnUpdateHostOwner(_)).Times(0);
  EXPECT_CALL(mock_delegate_, OnUpdateRequireSessionAuthorization(_)).Times(0);

  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, Signaling_MultipleHeartbeats) {
  base::RunLoop run_loop;

  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
  };
  ValidateLegacyHeartbeatOptions options;

  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(options))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(options));
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);
  EXPECT_CALL(mock_delegate_, OnUpdateHostOwner(_)).Times(0);
  EXPECT_CALL(mock_delegate_, OnUpdateRequireSessionAuthorization(_)).Times(0);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kTestHeartbeatDelay * 2);
}

TEST_F(HeartbeatSenderTest, Signaling_MultipleHeartbeats_Lite) {
  base::RunLoop run_loop;

  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
      .use_lite_heartbeat = true,
  };

  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst));
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_))
      .WillOnce(DoValidateSendHeartbeatAndRespondOk())
      .WillOnce(DoValidateSendHeartbeatAndRespondOk());
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).Times(3);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);
  EXPECT_CALL(mock_delegate_, OnUpdateHostOwner(_)).Times(0);
  EXPECT_CALL(mock_delegate_, OnUpdateRequireSessionAuthorization(_)).Times(0);

  signal_strategy_->Connect();
  task_environment_.FastForwardBy(kTestHeartbeatDelay * 2);
}

TEST_F(HeartbeatSenderTest, SetHostOfflineReason) {
  base::MockCallback<base::OnceCallback<void(bool success)>> mock_ack_callback;
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(0);

  heartbeat_sender()->SetHostOfflineReason("test_error", kOfflineReasonTimeout,
                                           mock_ack_callback.Get());

  testing::Mock::VerifyAndClearExpectations(&mock_ack_callback);

  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
      .host_offline_reason = "test_error",
  };

  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst));
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent());

  // Callback should run once, when we get response to offline-reason.
  EXPECT_CALL(mock_ack_callback, Run(_)).Times(1);
  EXPECT_CALL(mock_delegate_, OnFirstHeartbeatSuccessful()).Times(1);
  EXPECT_CALL(mock_delegate_, OnUpdateHostOwner(_)).Times(0);
  EXPECT_CALL(mock_delegate_, OnUpdateRequireSessionAuthorization(_)).Times(0);

  signal_strategy_->Connect();
}

TEST_F(HeartbeatSenderTest, UnknownHostId) {
  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillRepeatedly(
          [](bool is_initial_heartbeat, std::optional<std::string> signaling_id,
             std::optional<std::string> offline_reason,
             HeartbeatServiceClient::HeartbeatResponseCallback callback) {
            std::move(callback).Run(
                ProtobufHttpStatus(ProtobufHttpStatus::Code::NOT_FOUND,
                                   "not found"),
                std::nullopt, "", false, std::nullopt);
          });

  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).WillRepeatedly(Return());

  EXPECT_CALL(mock_delegate_, OnHostNotFound()).Times(1);

  signal_strategy_->Connect();

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(HeartbeatSenderTest, FailedToHeartbeat_Backoff) {
  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
  };

  {
    InSequence sequence;

    EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
        .Times(2)
        .WillRepeatedly(
            [&](bool is_initial_heartbeat,
                std::optional<std::string> signaling_id,
                std::optional<std::string> offline_reason,
                HeartbeatServiceClient::HeartbeatResponseCallback callback) {
              std::move(callback).Run(
                  ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAVAILABLE,
                                     "unavailable"),
                  std::nullopt, "", false, std::nullopt);
            });

    EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
        .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst));
  }
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);

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
  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
  };

  // Each call will simulate ~10 minutes of time (at max backoff duration).
  // We want to simulate a long outage (~3 hours) so run through 20 iterations.
  int retry_attempts = 20;

  {
    InSequence sequence;

    EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
        .Times(retry_attempts)
        .WillRepeatedly(
            [&](bool is_initial_heartbeat,
                std::optional<std::string> signaling_id,
                std::optional<std::string> offline_reason,
                HeartbeatServiceClient::HeartbeatResponseCallback callback) {
              std::move(callback).Run(
                  ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAVAILABLE,
                                     "unavailable"),
                  std::nullopt, "", false, std::nullopt);
            });

    EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
        .WillOnce(DoValidateLegacyHeartbeatAndRespondOk(optionsFirst));
  }
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);

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
  ValidateLegacyHeartbeatOptions optionsFirst{
      .is_initial_heartbeat = true,
  };

  int legacy_heartbeat_count = 0;
  EXPECT_CALL(*mock_client_, SendFullHeartbeat(_, _, _, _))
      .WillRepeatedly(
          [&](bool is_initial_heartbeat,
              std::optional<std::string> signaling_id,
              std::optional<std::string> offline_reason,
              HeartbeatServiceClient::HeartbeatResponseCallback callback) {
            legacy_heartbeat_count++;
            std::move(callback).Run(
                ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAUTHENTICATED,
                                   "unauthenticated"),
                std::nullopt, "", false, std::nullopt);
          });
  EXPECT_CALL(*mock_client_, SendLiteHeartbeat(_)).Times(0);
  EXPECT_CALL(*mock_observer_, OnHeartbeatSent()).WillRepeatedly(Return());
  EXPECT_CALL(mock_delegate_, OnAuthFailed()).Times(1);

  signal_strategy_->Connect();
  task_environment_.FastForwardUntilNoTasksRemain();

  // Should retry heartbeating at least once.
  ASSERT_LT(1, legacy_heartbeat_count);
}

}  // namespace remoting
