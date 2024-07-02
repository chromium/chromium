// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_stream_attempt.h"

#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_entry.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/transport_client_socket_pool_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

IPEndPoint MakeIPEndPoint(std::string_view ip_literal, uint16_t port = 80) {
  std::optional<IPAddress> ip = IPAddress::FromIPLiteral(std::move(ip_literal));
  return IPEndPoint(*ip, port);
}

class NetLogObserver : public NetLog::ThreadSafeObserver {
 public:
  explicit NetLogObserver(NetLog* net_log) {
    net_log->AddObserver(this, NetLogCaptureMode::kEverything);
  }

  ~NetLogObserver() override {
    if (net_log()) {
      net_log()->RemoveObserver(this);
    }
  }

  void OnAddEntry(const NetLogEntry& entry) override {
    entries_.emplace_back(entry.Clone());
  }

  const std::vector<NetLogEntry>& entries() const { return entries_; }

 private:
  std::vector<NetLogEntry> entries_;
};

class TestSocketPerformanceWatcher : public SocketPerformanceWatcher {
 public:
  ~TestSocketPerformanceWatcher() override = default;

  bool ShouldNotifyUpdatedRTT() const override { return false; }

  void OnUpdatedRTTAvailable(const base::TimeDelta& rtt) override {}

  void OnConnectionChanged() override {}
};

class TestSocketPerformanceWatcherFactory
    : public SocketPerformanceWatcherFactory {
 public:
  ~TestSocketPerformanceWatcherFactory() override = default;

  std::unique_ptr<SocketPerformanceWatcher> CreateSocketPerformanceWatcher(
      const Protocol protocol,
      const IPAddress& ip_address) override {
    return std::make_unique<TestSocketPerformanceWatcher>();
  }
};

class StreamAttemptHelper {
 public:
  StreamAttemptHelper(StreamAttemptParams* params, IPEndPoint ip_endpoint)
      : attempt_(std::make_unique<TcpStreamAttempt>(params, ip_endpoint)) {}

  int Start() {
    return attempt_->Start(base::BindOnce(&StreamAttemptHelper::OnComplete,
                                          base::Unretained(this)));
  }

  int WaitForCompletion() {
    if (result_.has_value()) {
      return *result_;
    }

    base::RunLoop loop;
    completion_closure_ = loop.QuitClosure();
    loop.Run();

    return *result_;
  }

  TcpStreamAttempt* attempt() { return attempt_.get(); }

 private:
  void OnComplete(int rv) {
    result_ = rv;
    if (completion_closure_) {
      std::move(completion_closure_).Run();
    }
  }

  std::unique_ptr<TcpStreamAttempt> attempt_;
  base::OnceClosure completion_closure_;
  std::optional<int> result_;
};

}  // namespace

class TcpStreamAttemptTest : public TestWithTaskEnvironment {
 public:
  TcpStreamAttemptTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        socket_factory_(NetLog::Get()),
        params_(&socket_factory_,
                /*ssl_client_context=*/nullptr,
                /*socket_performance_watcher_factory=*/nullptr,
                /*network_quality_estimator=*/nullptr,
                /*net_log=*/NetLog::Get()) {}

 protected:
  void EnableSocketPerformanceWatcher() {
    params_.socket_performance_watcher_factory =
        &socket_performance_watcher_factory_;
  }

  MockTransportClientSocketFactory& socket_factory() { return socket_factory_; }

  StreamAttemptParams* params() { return &params_; }

 private:
  MockTransportClientSocketFactory socket_factory_;
  TestSocketPerformanceWatcherFactory socket_performance_watcher_factory_;
  StreamAttemptParams params_;
};

TEST_F(TcpStreamAttemptTest, SuccessSync) {
  socket_factory().set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kSynchronous);
  StreamAttemptHelper helper(params(), MakeIPEndPoint("192.0.2.1"));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_TRUE(stream_socket);
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_start.is_null());
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_end.is_null());
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(TcpStreamAttemptTest, SuccessAsync) {
  socket_factory().set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kPending);
  StreamAttemptHelper helper(params(), MakeIPEndPoint("192.0.2.1"));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_CONNECTING);

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_TRUE(stream_socket);
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_start.is_null());
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_end.is_null());
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(TcpStreamAttemptTest, FailureSync) {
  socket_factory().set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kFailing);
  StreamAttemptHelper helper(params(), MakeIPEndPoint("192.0.2.1"));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_FAILED));
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(TcpStreamAttemptTest, FailureAsync) {
  socket_factory().set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kPendingFailing);
  StreamAttemptHelper helper(params(), MakeIPEndPoint("192.0.2.1"));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_FAILED));
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(TcpStreamAttemptTest, Timeout) {
  socket_factory().set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kStalled);
  StreamAttemptHelper helper(params(), MakeIPEndPoint("192.0.2.1"));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  FastForwardBy(TcpStreamAttempt::kTcpHandshakeTimeout);
  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_TIMED_OUT));
  ASSERT_FALSE(helper.attempt()->ReleaseStreamSocket());
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(TcpStreamAttemptTest, Abort) {
  socket_factory().set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kPending);
  auto helper = std::make_unique<StreamAttemptHelper>(
      params(), MakeIPEndPoint("192.0.2.1"));
  int rv = helper->Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  NetLogObserver observer(helper->attempt()->net_log().net_log());
  // Drop the helpr to abort the attempt.
  helper.reset();

  ASSERT_EQ(observer.entries().size(), 1u);
  std::optional<int> error =
      observer.entries().front().params.FindInt("net_error");
  ASSERT_TRUE(error.has_value());
  EXPECT_THAT(*error, IsError(ERR_ABORTED));
}

TEST_F(TcpStreamAttemptTest, SocketPerformanceWatcher) {
  EnableSocketPerformanceWatcher();

  socket_factory().set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kSynchronous);
  StreamAttemptHelper helper(params(), MakeIPEndPoint("192.0.2.1"));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_TRUE(stream_socket);
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_start.is_null());
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_end.is_null());
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_IDLE);
}

}  // namespace net
