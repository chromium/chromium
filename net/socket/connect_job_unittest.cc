// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class TestConnectJob : public ConnectJob {
 public:
  enum class JobType {
    kSyncSuccess,
    kAsyncSuccess,
    kHung,
  };

  TestConnectJob(JobType job_type,
                 base::TimeDelta timeout_duration,
                 const CommonConnectJobParams* common_connect_job_params,
                 ConnectJob::Delegate* delegate)
      : ConnectJob(DEFAULT_PRIORITY,
                   SocketTag(),
                   timeout_duration,
                   common_connect_job_params,
                   delegate,
                   nullptr /* net_log */,
                   NetLogSourceType::TRANSPORT_CONNECT_JOB,
                   NetLogEventType::TRANSPORT_CONNECT_JOB_CONNECT),
        job_type_(job_type) {
    switch (job_type_) {
      case JobType::kSyncSuccess:
        socket_data_provider_.set_connect_data(MockConnect(SYNCHRONOUS, OK));
        return;
      case JobType::kAsyncSuccess:
        socket_data_provider_.set_connect_data(MockConnect(ASYNC, OK));
        return;
      case JobType::kHung:
        socket_data_provider_.set_connect_data(
            MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
        return;
    }
  }

  TestConnectJob(const TestConnectJob&) = delete;
  TestConnectJob& operator=(const TestConnectJob&) = delete;

  // From ConnectJob:
  LoadState GetLoadState() const override { return LOAD_STATE_IDLE; }
  bool HasEstablishedConnection() const override { return false; }
  ResolveErrorInfo GetResolveErrorInfo() const override {
    return ResolveErrorInfo(net::OK);
  }
  int ConnectInternal() override {
    SetSocket(std::make_unique<MockTCPClientSocket>(
                  AddressList(), net_log().net_log(), &socket_data_provider_),
              std::nullopt /* dns_aliases */);
    return socket()->Connect(base::BindOnce(
        &TestConnectJob::NotifyDelegateOfCompletion, base::Unretained(this)));
  }
  void ChangePriorityInternal(RequestPriority priority) override {
    last_seen_priority_ = priority;
  }

  using ConnectJob::ResetTimer;

  // The priority seen during the most recent call to ChangePriorityInternal().
  RequestPriority last_seen_priority() const { return last_seen_priority_; }

 protected:
  const JobType job_type_;
  StaticSocketDataProvider socket_data_provider_;
  RequestPriority last_seen_priority_ = DEFAULT_PRIORITY;
};

class ConnectJobTest : public testing::Test {
 public:
  ConnectJobTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        common_connect_job_params_(
            /*client_socket_factory=*/nullptr,
            /*host_resolver=*/nullptr,
            /*http_auth_cache=*/nullptr,
            /*http_auth_handler_factory=*/nullptr,
            /*spdy_session_pool=*/nullptr,
            /*quic_supported_versions=*/nullptr,
            /*quic_session_pool=*/nullptr,
            /*proxy_delegate=*/nullptr,
            &http_user_agent_settings_,
            /*ssl_client_context=*/nullptr,
            /*socket_performance_watcher_factory=*/nullptr,
            /*network_quality_estimator=*/nullptr,
            NetLog::Get(),
            /*websocket_endpoint_lock_manager=*/nullptr,
            /*http_server_properties*/ nullptr,
            /*alpn_protos=*/nullptr,
            /*application_settings=*/nullptr,
            /*ignore_certificate_errors=*/nullptr,
            /*early_data_enabled=*/nullptr) {}
  ~ConnectJobTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  RecordingNetLogObserver net_log_observer_;
  const StaticHttpUserAgentSettings http_user_agent_settings_ = {"*",
                                                                 "test-ua"};
  const CommonConnectJobParams common_connect_job_params_;
  TestConnectJobDelegate delegate_;
};

// Even though a timeout is specified, it doesn't time out on a synchronous
// completion.
TEST_F(ConnectJobTest, NoTimeoutOnSyncCompletion) {
  TestConnectJob job(TestConnectJob::JobType::kSyncSuccess,
                     base::Microseconds(1), &common_connect_job_params_,
                     &delegate_);
  EXPECT_THAT(job.Connect(), test::IsOk());
}

// Even though a timeout is specified, it doesn't time out on an asynchronous
// completion.
TEST_F(ConnectJobTest, NoTimeoutOnAsyncCompletion) {
  TestConnectJob job(TestConnectJob::JobType::kAsyncSuccess, base::Minutes(1),
                     &common_connect_job_params_, &delegate_);
  ASSERT_THAT(job.Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(delegate_.WaitForResult(), test::IsOk());
}

// Job shouldn't timeout when passed a TimeDelta of zero.
TEST_F(ConnectJobTest, NoTimeoutWithNoTimeDelta) {
  TestConnectJob job(TestConnectJob::JobType::kHung, base::TimeDelta(),
                     &common_connect_job_params_, &delegate_);
  ASSERT_THAT(job.Connect(), test::IsError(ERR_IO_PENDING));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(delegate_.has_result());
}

// Make sure that ChangePriority() works, and new priority is visible to
// subclasses during the SetPriorityInternal call.
TEST_F(ConnectJobTest, SetPriority) {
  TestConnectJob job(TestConnectJob::JobType::kAsyncSuccess,
                     base::Microseconds(1), &common_connect_job_params_,
                     &delegate_);
  ASSERT_THAT(job.Connect(), test::IsError(ERR_IO_PENDING));

  job.ChangePriority(HIGHEST);
  EXPECT_EQ(HIGHEST, job.priority());
  EXPECT_EQ(HIGHEST, job.last_seen_priority());

  job.ChangePriority(MEDIUM);
  EXPECT_EQ(MEDIUM, job.priority());
  EXPECT_EQ(MEDIUM, job.last_seen_priority());

  EXPECT_THAT(delegate_.WaitForResult(), test::IsOk());
}

TEST_F(ConnectJobTest, TimedOut) {
  const base::TimeDelta kTimeout = base::Hours(1);

  std::unique_ptr<TestConnectJob> job =
      std::make_unique<TestConnectJob>(TestConnectJob::JobType::kHung, kTimeout,
                                       &common_connect_job_params_, &delegate_);
  ASSERT_THAT(job->Connect(), test::IsError(ERR_IO_PENDING));

  // Nothing should happen before the specified time.
  task_environment_.FastForwardBy(kTimeout - base::Milliseconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate_.has_result());

  // At which point the job should time out.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(delegate_.WaitForResult(), test::IsError(ERR_TIMED_OUT));

  // Have to delete the job for it to log the end event.
  job.reset();

  auto entries = net_log_observer_.GetEntries();

  EXPECT_EQ(6u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(entries, 0, NetLogEventType::CONNECT_JOB));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 1, NetLogEventType::TRANSPORT_CONNECT_JOB_CONNECT));
  EXPECT_TRUE(LogContainsEvent(entries, 2,
                               NetLogEventType::CONNECT_JOB_SET_SOCKET,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(entries, 3,
                               NetLogEventType::CONNECT_JOB_TIMED_OUT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 4, NetLogEventType::TRANSPORT_CONNECT_JOB_CONNECT));
  EXPECT_TRUE(LogContainsEndEvent(entries, 5, NetLogEventType::CONNECT_JOB));
}

TEST_F(ConnectJobTest, TimedOutWithRestartedTimer) {
  const base::TimeDelta kTimeout = base::Hours(1);

  TestConnectJob job(TestConnectJob::JobType::kHung, kTimeout,
                     &common_connect_job_params_, &delegate_);
  ASSERT_THAT(job.Connect(), test::IsError(ERR_IO_PENDING));

  // Nothing should happen before the specified time.
  task_environment_.FastForwardBy(kTimeout - base::Milliseconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate_.has_result());

  // Make sure restarting the timer is respected.
  job.ResetTimer(kTimeout);
  task_environment_.FastForwardBy(kTimeout - base::Milliseconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate_.has_result());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(delegate_.WaitForResult(), test::IsError(ERR_TIMED_OUT));
}

}  // namespace
}  // namespace net
