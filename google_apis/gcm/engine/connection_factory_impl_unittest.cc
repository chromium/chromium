// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/connection_factory_impl.h"

#include <cmath>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/fake_connection_handler.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/backoff_entry.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

class Policy;

namespace gcm {
namespace {

const char kMCSEndpoint[] = "http://my.server";
const char kMCSEndpoint2[] = "http://my.alt.server";

const int kBackoffDelayMs = 1;
const int kBackoffMultiplier = 2;

// A backoff policy with small enough delays that tests aren't burdened.
const net::BackoffEntry::Policy kTestBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  kBackoffDelayMs,

  // Factor by which the waiting time will be multiplied.
  kBackoffMultiplier,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0,

  // Maximum amount of time we are willing to delay our request in ms.
  10,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

std::vector<GURL> BuildEndpoints() {
  std::vector<GURL> endpoints;
  endpoints.push_back(GURL(kMCSEndpoint));
  endpoints.push_back(GURL(kMCSEndpoint2));
  return endpoints;
}

// Helper for calculating total expected exponential backoff delay given an
// arbitrary number of failed attempts. See BackoffEntry::CalculateReleaseTime.
double CalculateBackoff(int num_attempts) {
  double delay = kBackoffDelayMs;
  for (int i = 1; i < num_attempts; ++i) {
    delay += kBackoffDelayMs * pow(static_cast<double>(kBackoffMultiplier),
                                   i - 1);
  }
  DVLOG(1) << "Expected backoff " << delay << " milliseconds.";
  return delay;
}

void ReadContinuation(std::unique_ptr<google::protobuf::MessageLite> message) {}

void WriteContinuation() {
}

// A connection factory that stubs out network requests and overrides the
// backoff policy.
class TestConnectionFactoryImpl : public ConnectionFactoryImpl {
 public:
  TestConnectionFactoryImpl(
      GetProxyResolvingFactoryCallback get_socket_factory_callback,
      const base::Closure& finished_callback);
  ~TestConnectionFactoryImpl() override;

  void InitializeFactory();

  // Overridden stubs.
  void StartConnection() override;
  void InitHandler(mojo::ScopedDataPipeConsumerHandle receive_stream,
                   mojo::ScopedDataPipeProducerHandle send_stream) override;
  std::unique_ptr<net::BackoffEntry> CreateBackoffEntry(
      const net::BackoffEntry::Policy* const policy) override;
  std::unique_ptr<ConnectionHandler> CreateConnectionHandler(
      base::TimeDelta read_timeout,
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback,
      const ConnectionHandler::ConnectionChangedCallback& connection_callback)
      override;
  base::TimeTicks NowTicks() override;

  // Helpers for verifying connection attempts are made. Connection results
  // must be consumed.
  void SetConnectResult(int connect_result);
  void SetMultipleConnectResults(int connect_result, int num_expected_attempts);

  // Force a login handshake to be delayed.
  void SetDelayLogin(bool delay_login);

  // Simulate a socket error.
  void SetSocketError();

  base::SimpleTestTickClock* tick_clock() { return &tick_clock_; }

 private:
  // Clock for controlling delay.
  base::SimpleTestTickClock tick_clock_;
  // The result to return on the next connect attempt.
  int connect_result_;
  // The number of expected connection attempts;
  int num_expected_attempts_;
  // Whether all expected connection attempts have been fulfilled since an
  // expectation was last set.
  bool connections_fulfilled_;
  // Whether to delay a login handshake completion or not.
  bool delay_login_;
  // Callback to invoke when all connection attempts have been made.
  base::Closure finished_callback_;
  // A temporary scoped pointer to make sure we don't leak the handler in the
  // cases it's never consumed by the ConnectionFactory.
  std::unique_ptr<FakeConnectionHandler> scoped_handler_;
  // The current fake connection handler..
  FakeConnectionHandler* fake_handler_;
  // Dummy GCM Stats recorder.
  FakeGCMStatsRecorder dummy_recorder_;
  // Dummy mojo pipes.
  mojo::DataPipe receive_pipe_;
  mojo::DataPipe send_pipe_;
};

TestConnectionFactoryImpl::TestConnectionFactoryImpl(
    GetProxyResolvingFactoryCallback get_socket_factory_callback,
    const base::Closure& finished_callback)
    : ConnectionFactoryImpl(
          BuildEndpoints(),
          net::BackoffEntry::Policy(),
          get_socket_factory_callback,
          base::ThreadTaskRunnerHandle::Get(),
          &dummy_recorder_,
          network::TestNetworkConnectionTracker::GetInstance()),
      connect_result_(net::ERR_UNEXPECTED),
      num_expected_attempts_(0),
      connections_fulfilled_(true),
      delay_login_(false),
      finished_callback_(finished_callback),
      scoped_handler_(std::make_unique<FakeConnectionHandler>(
          base::Bind(&ReadContinuation),
          base::Bind(&WriteContinuation))),
      fake_handler_(scoped_handler_.get()) {
  // Set a non-null time.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
}

TestConnectionFactoryImpl::~TestConnectionFactoryImpl() {
  EXPECT_EQ(0, num_expected_attempts_);
}

void TestConnectionFactoryImpl::StartConnection() {
  ASSERT_GT(num_expected_attempts_, 0);
  ASSERT_FALSE(GetConnectionHandler()->CanSendMessage());
  std::unique_ptr<mcs_proto::LoginRequest> request(BuildLoginRequest(0, 0, ""));
  GetConnectionHandler()->Init(*request,
                               std::move(receive_pipe_.consumer_handle),
                               std::move(send_pipe_.producer_handle));
  OnConnectDone(connect_result_, net::IPEndPoint(), net::IPEndPoint(),
                mojo::ScopedDataPipeConsumerHandle(),
                mojo::ScopedDataPipeProducerHandle());
  if (!NextRetryAttempt().is_null()) {
    // Advance the time to the next retry time.
    base::TimeDelta time_till_retry =
        NextRetryAttempt() - tick_clock_.NowTicks();
    tick_clock_.Advance(time_till_retry);
  }
  --num_expected_attempts_;
  if (num_expected_attempts_ == 0) {
    connect_result_ = net::ERR_UNEXPECTED;
    connections_fulfilled_ = true;
    finished_callback_.Run();
  }
}

void TestConnectionFactoryImpl::InitHandler(
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  EXPECT_NE(connect_result_, net::ERR_UNEXPECTED);
  if (!delay_login_)
    ConnectionHandlerCallback(net::OK);
}

std::unique_ptr<net::BackoffEntry>
TestConnectionFactoryImpl::CreateBackoffEntry(
    const net::BackoffEntry::Policy* const policy) {
  return std::make_unique<net::BackoffEntry>(&kTestBackoffPolicy, &tick_clock_);
}

std::unique_ptr<ConnectionHandler>
TestConnectionFactoryImpl::CreateConnectionHandler(
    base::TimeDelta read_timeout,
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback,
    const ConnectionHandler::ConnectionChangedCallback& connection_callback) {
  return std::move(scoped_handler_);
}

base::TimeTicks TestConnectionFactoryImpl::NowTicks() {
  return tick_clock_.NowTicks();
}

void TestConnectionFactoryImpl::SetConnectResult(int connect_result) {
  DCHECK_NE(connect_result, net::ERR_UNEXPECTED);
  ASSERT_EQ(0, num_expected_attempts_);
  connections_fulfilled_ = false;
  connect_result_ = connect_result;
  num_expected_attempts_ = 1;
  fake_handler_->ExpectOutgoingMessage(
      MCSMessage(kLoginRequestTag, BuildLoginRequest(0, 0, "")));
}

void TestConnectionFactoryImpl::SetMultipleConnectResults(
    int connect_result,
    int num_expected_attempts) {
  DCHECK_NE(connect_result, net::ERR_UNEXPECTED);
  DCHECK_GT(num_expected_attempts, 0);
  ASSERT_EQ(0, num_expected_attempts_);
  connections_fulfilled_ = false;
  connect_result_ = connect_result;
  num_expected_attempts_ = num_expected_attempts;
  for (int i = 0 ; i < num_expected_attempts; ++i) {
    fake_handler_->ExpectOutgoingMessage(
        MCSMessage(kLoginRequestTag, BuildLoginRequest(0, 0, "")));
  }
}

void TestConnectionFactoryImpl::SetDelayLogin(bool delay_login) {
  delay_login_ = delay_login;
  fake_handler_->set_fail_login(delay_login_);
}

void TestConnectionFactoryImpl::SetSocketError() {
  fake_handler_->set_had_error(true);
}

}  // namespace

class ConnectionFactoryImplTest
    : public testing::Test,
      public ConnectionFactory::ConnectionListener {
 public:
  ConnectionFactoryImplTest();
  ~ConnectionFactoryImplTest() override;

  TestConnectionFactoryImpl* factory() { return &factory_; }
  GURL& connected_server() { return connected_server_; }

  void WaitForConnections();

  // ConnectionFactory::ConnectionListener
  void OnConnected(const GURL& current_server,
                   const net::IPEndPoint& ip_endpoint) override;
  void OnDisconnected() override;

  // Get the client events recorded by the event tracker.
  const google::protobuf::RepeatedPtrField<mcs_proto::ClientEvent>
  GetClientEvents() {
    mcs_proto::LoginRequest login_request;
    factory()->event_tracker_.WriteToLoginRequest(&login_request);
    return login_request.client_event();
  }

 private:
  void GetProxyResolvingSocketFactory(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver) {
    network_context_->CreateProxyResolvingSocketFactory(std::move(receiver));
  }
  void ConnectionsComplete();

  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  base::test::TaskEnvironment task_environment_;
  TestConnectionFactoryImpl factory_;
  std::unique_ptr<base::RunLoop> run_loop_;

  GURL connected_server_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<network::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<network::NetworkContext> network_context_;
};

ConnectionFactoryImplTest::ConnectionFactoryImplTest()
    : network_connection_tracker_(
          network::TestNetworkConnectionTracker::CreateInstance()),
      task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
      factory_(base::BindRepeating(
                   &ConnectionFactoryImplTest::GetProxyResolvingSocketFactory,
                   base::Unretained(this)),
               base::Bind(&ConnectionFactoryImplTest::ConnectionsComplete,
                          base::Unretained(this))),
      run_loop_(new base::RunLoop()),
      network_change_notifier_(
          net::NetworkChangeNotifier::CreateMockIfNeeded()),
      network_service_(network::NetworkService::CreateForTesting()) {
  network::mojom::NetworkContextParamsPtr params =
      network::mojom::NetworkContextParams::New();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  network_context_ = std::make_unique<network::NetworkContext>(
      network_service_.get(),
      network_context_remote_.BindNewPipeAndPassReceiver(), std::move(params));
  factory()->SetConnectionListener(this);
  factory()->Initialize(ConnectionFactory::BuildLoginRequestCallback(),
                        ConnectionHandler::ProtoReceivedCallback(),
                        ConnectionHandler::ProtoSentCallback());
}
ConnectionFactoryImplTest::~ConnectionFactoryImplTest() {}

void ConnectionFactoryImplTest::WaitForConnections() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void ConnectionFactoryImplTest::ConnectionsComplete() {
  if (!run_loop_)
    return;
  run_loop_->Quit();
}

void ConnectionFactoryImplTest::OnConnected(
    const GURL& current_server,
    const net::IPEndPoint& ip_endpoint) {
  connected_server_ = current_server;
}

void ConnectionFactoryImplTest::OnDisconnected() {
  connected_server_ = GURL();
}

// Verify building a connection handler works.
TEST_F(ConnectionFactoryImplTest, Initialize) {
  ASSERT_FALSE(factory()->GetConnectionHandler());
  EXPECT_FALSE(factory()->IsEndpointReachable());
  EXPECT_FALSE(connected_server().is_valid());
}

// An initial successful connection should not result in backoff.
TEST_F(ConnectionFactoryImplTest, ConnectSuccess) {
  factory()->SetConnectResult(net::OK);
  factory()->Connect();
  ASSERT_TRUE(factory()->GetConnectionHandler());
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());
  EXPECT_EQ(factory()->GetCurrentEndpoint(), BuildEndpoints()[0]);
  EXPECT_TRUE(factory()->IsEndpointReachable());
  EXPECT_TRUE(connected_server().is_valid());
}

// A connection failure should result in backoff, and attempting the fallback
// endpoint next.
TEST_F(ConnectionFactoryImplTest, ConnectFail) {
  factory()->SetConnectResult(net::ERR_CONNECTION_FAILED);
  factory()->Connect();
  EXPECT_FALSE(factory()->NextRetryAttempt().is_null());
  EXPECT_EQ(factory()->GetCurrentEndpoint(), BuildEndpoints()[1]);
  EXPECT_FALSE(factory()->IsEndpointReachable());
  EXPECT_FALSE(connected_server().is_valid());
}

// A connection success after a failure should reset backoff.
TEST_F(ConnectionFactoryImplTest, FailThenSucceed) {
  factory()->SetConnectResult(net::ERR_CONNECTION_FAILED);
  base::TimeTicks connect_time = factory()->tick_clock()->NowTicks();
  factory()->Connect();
  WaitForConnections();
  EXPECT_FALSE(factory()->IsEndpointReachable());
  EXPECT_FALSE(connected_server().is_valid());
  base::TimeTicks retry_time = factory()->NextRetryAttempt();
  EXPECT_FALSE(retry_time.is_null());
  EXPECT_GE((retry_time - connect_time).InMilliseconds(), CalculateBackoff(1));
  factory()->SetConnectResult(net::OK);
  WaitForConnections();
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());
  EXPECT_TRUE(factory()->IsEndpointReachable());
  EXPECT_TRUE(connected_server().is_valid());
}

// Multiple connection failures should retry with an exponentially increasing
// backoff, then reset on success.
TEST_F(ConnectionFactoryImplTest, MultipleFailuresThenSucceed) {
  const int kNumAttempts = 5;
  factory()->SetMultipleConnectResults(net::ERR_CONNECTION_FAILED,
                                       kNumAttempts);

  base::TimeTicks connect_time = factory()->tick_clock()->NowTicks();
  factory()->Connect();
  WaitForConnections();
  EXPECT_FALSE(factory()->IsEndpointReachable());
  EXPECT_FALSE(connected_server().is_valid());
  base::TimeTicks retry_time = factory()->NextRetryAttempt();
  EXPECT_FALSE(retry_time.is_null());
  EXPECT_GE((retry_time - connect_time).InMilliseconds(),
            CalculateBackoff(kNumAttempts));

  // There should be one failed client event for each failed connection.
  const auto client_events = GetClientEvents();
  ASSERT_EQ(kNumAttempts, client_events.size());

  for (const auto& client_event : client_events) {
    EXPECT_EQ(mcs_proto::ClientEvent::FAILED_CONNECTION, client_event.type());
    EXPECT_EQ(net::ERR_CONNECTION_FAILED, client_event.error_code());
  }

  factory()->SetConnectResult(net::OK);
  WaitForConnections();
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());
  EXPECT_TRUE(factory()->IsEndpointReachable());
  EXPECT_TRUE(connected_server().is_valid());

  // Old client events should have been reset after the successful connection.
  const auto new_client_events = GetClientEvents();
  ASSERT_EQ(0, new_client_events.size());
}

// Network change events should trigger canary connections.
TEST_F(ConnectionFactoryImplTest, FailThenNetworkChangeEvent) {
  factory()->SetConnectResult(net::ERR_CONNECTION_FAILED);
  factory()->Connect();
  WaitForConnections();
  base::TimeTicks initial_backoff = factory()->NextRetryAttempt();
  EXPECT_FALSE(initial_backoff.is_null());

  factory()->SetConnectResult(net::ERR_FAILED);
  factory()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  WaitForConnections();

  // Backoff should increase.
  base::TimeTicks next_backoff = factory()->NextRetryAttempt();
  EXPECT_GT(next_backoff, initial_backoff);
  EXPECT_FALSE(factory()->IsEndpointReachable());
}

// Verify that we reconnect even if a canary succeeded then disconnected while
// a backoff was pending.
TEST_F(ConnectionFactoryImplTest, CanarySucceedsThenDisconnects) {
  factory()->SetConnectResult(net::ERR_CONNECTION_FAILED);
  factory()->Connect();
  WaitForConnections();
  base::TimeTicks initial_backoff = factory()->NextRetryAttempt();
  EXPECT_FALSE(initial_backoff.is_null());

  factory()->SetConnectResult(net::OK);
  factory()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  WaitForConnections();
  EXPECT_TRUE(factory()->IsEndpointReachable());
  EXPECT_TRUE(connected_server().is_valid());

  factory()->SetConnectResult(net::OK);
  factory()->SignalConnectionReset(ConnectionFactory::SOCKET_FAILURE);
  EXPECT_FALSE(factory()->IsEndpointReachable());
  EXPECT_FALSE(connected_server().is_valid());
  WaitForConnections();
  EXPECT_TRUE(factory()->IsEndpointReachable());
  EXPECT_TRUE(connected_server().is_valid());
}

// Verify that if a canary connects, but hasn't finished the handshake, a
// pending backoff attempt doesn't interrupt the connection.
TEST_F(ConnectionFactoryImplTest, CanarySucceedsRetryDuringLogin) {
  factory()->SetConnectResult(net::ERR_CONNECTION_FAILED);
  factory()->Connect();
  WaitForConnections();
  base::TimeTicks initial_backoff = factory()->NextRetryAttempt();
  EXPECT_FALSE(initial_backoff.is_null());

  factory()->SetDelayLogin(true);
  factory()->SetConnectResult(net::OK);
  factory()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  WaitForConnections();
  EXPECT_FALSE(factory()->IsEndpointReachable());

  // Pump the loop, to ensure the pending backoff retry has no effect.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated(),
      base::TimeDelta::FromMilliseconds(1));
  WaitForConnections();
}

// Fail after successful connection via signal reset.
TEST_F(ConnectionFactoryImplTest, FailViaSignalReset) {
  factory()->SetConnectResult(net::OK);
  factory()->Connect();
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());

  factory()->SignalConnectionReset(ConnectionFactory::SOCKET_FAILURE);
  EXPECT_FALSE(factory()->NextRetryAttempt().is_null());
  EXPECT_FALSE(factory()->IsEndpointReachable());
}

TEST_F(ConnectionFactoryImplTest, IgnoreResetWhileConnecting) {
  factory()->SetConnectResult(net::OK);
  factory()->Connect();
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());

  factory()->SignalConnectionReset(ConnectionFactory::SOCKET_FAILURE);
  base::TimeTicks retry_time = factory()->NextRetryAttempt();
  EXPECT_FALSE(retry_time.is_null());
  EXPECT_FALSE(factory()->IsEndpointReachable());

  const int kNumAttempts = 5;
  for (int i = 0; i < kNumAttempts; ++i)
    factory()->SignalConnectionReset(ConnectionFactory::SOCKET_FAILURE);
  EXPECT_EQ(retry_time, factory()->NextRetryAttempt());
  EXPECT_FALSE(factory()->IsEndpointReachable());
}

// Go into backoff due to connection failure. On successful connection, receive
// a signal reset. The original backoff should be restored and extended, rather
// than a new backoff starting from scratch.
TEST_F(ConnectionFactoryImplTest, SignalResetRestoresBackoff) {
  factory()->SetConnectResult(net::ERR_CONNECTION_FAILED);
  base::TimeTicks connect_time = factory()->tick_clock()->NowTicks();
  factory()->Connect();
  WaitForConnections();
  base::TimeTicks retry_time = factory()->NextRetryAttempt();
  EXPECT_FALSE(retry_time.is_null());

  factory()->SetConnectResult(net::OK);
  connect_time = factory()->tick_clock()->NowTicks();
  WaitForConnections();
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());

  factory()->SignalConnectionReset(ConnectionFactory::SOCKET_FAILURE);
  EXPECT_FALSE(factory()->IsEndpointReachable());
  EXPECT_FALSE(connected_server().is_valid());
  EXPECT_NE(retry_time, factory()->NextRetryAttempt());
  retry_time = factory()->NextRetryAttempt();
  EXPECT_FALSE(retry_time.is_null());
  EXPECT_GE((retry_time - connect_time).InMilliseconds(),
            CalculateBackoff(2));

  factory()->SetConnectResult(net::OK);
  connect_time = factory()->tick_clock()->NowTicks();
  factory()->tick_clock()->Advance(
      factory()->NextRetryAttempt() - connect_time);
  WaitForConnections();
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());
  EXPECT_TRUE(factory()->IsEndpointReachable());
  EXPECT_TRUE(connected_server().is_valid());

  factory()->SignalConnectionReset(ConnectionFactory::SOCKET_FAILURE);
  EXPECT_NE(retry_time, factory()->NextRetryAttempt());
  retry_time = factory()->NextRetryAttempt();
  EXPECT_FALSE(retry_time.is_null());
  EXPECT_GE((retry_time - connect_time).InMilliseconds(),
            CalculateBackoff(3));
  EXPECT_FALSE(factory()->IsEndpointReachable());
  EXPECT_FALSE(connected_server().is_valid());
}

// When the network is disconnected, close the socket and suppress further
// connection attempts until the network returns.
// Disabled while crbug.com/396687 is being investigated.
TEST_F(ConnectionFactoryImplTest, DISABLED_SuppressConnectWhenNoNetwork) {
  factory()->SetConnectResult(net::OK);
  factory()->Connect();
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());
  EXPECT_TRUE(factory()->IsEndpointReachable());

  // Advance clock so the login window reset isn't encountered.
  factory()->tick_clock()->Advance(base::TimeDelta::FromSeconds(11));

  // Will trigger reset, but will not attempt a new connection.
  factory()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(factory()->IsEndpointReachable());
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());

  // When the network returns, attempt to connect.
  factory()->SetConnectResult(net::OK);
  factory()->OnConnectionChanged(network::mojom::ConnectionType::CONNECTION_4G);
  WaitForConnections();

  EXPECT_TRUE(factory()->IsEndpointReachable());
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());
}

// Receiving a network change event before the initial connection should have
// no effect.
TEST_F(ConnectionFactoryImplTest, NetworkChangeBeforeFirstConnection) {
  factory()->OnConnectionChanged(network::mojom::ConnectionType::CONNECTION_4G);
  factory()->SetConnectResult(net::OK);
  factory()->Connect();
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());
  EXPECT_TRUE(factory()->IsEndpointReachable());
}

// Test that if the client attempts to reconnect while a connection is already
// open, we don't crash.
TEST_F(ConnectionFactoryImplTest, ConnectionResetRace) {
  // Initial successful connection.
  factory()->SetConnectResult(net::OK);
  factory()->Connect();
  WaitForConnections();
  EXPECT_TRUE(factory()->IsEndpointReachable());

  // Trigger a connection error under the hood.
  factory()->SetSocketError();
  EXPECT_FALSE(factory()->IsEndpointReachable());

  // Now trigger force a re-connection.
  factory()->SetConnectResult(net::OK);
  factory()->Connect();
  WaitForConnections();

  // Re-connection should succeed.
  EXPECT_TRUE(factory()->IsEndpointReachable());
}

TEST_F(ConnectionFactoryImplTest, MultipleFailuresWrapClientEvents) {
  const int kNumAttempts = 50;
  factory()->SetMultipleConnectResults(net::ERR_CONNECTION_FAILED,
                                       kNumAttempts);

  factory()->Connect();
  WaitForConnections();

  // There should be one failed client event for each failed connection, but
  // there is a maximum cap of kMaxClientEvents, which is 30. There should also
  // be a single event which records the events which were discarded.
  auto client_events = GetClientEvents();
  ASSERT_EQ(31, client_events.size());

  bool found_discarded_events = false;
  for (const auto& client_event : client_events) {
    if (client_event.type() == mcs_proto::ClientEvent::DISCARDED_EVENTS) {
      // There should only be one event for discarded events.
      EXPECT_FALSE(found_discarded_events);
      found_discarded_events = true;
      // There should be 50-30=20 discarded events.
      EXPECT_EQ(20U, client_event.number_discarded_events());
    } else {
      EXPECT_EQ(mcs_proto::ClientEvent::FAILED_CONNECTION, client_event.type());
      EXPECT_EQ(net::ERR_CONNECTION_FAILED, client_event.error_code());
    }
  }
  EXPECT_TRUE(found_discarded_events);

  factory()->SetConnectResult(net::OK);
  WaitForConnections();
  EXPECT_TRUE(factory()->IsEndpointReachable());
  EXPECT_TRUE(connected_server().is_valid());

  // Old client events should have been reset after the successful connection.
  client_events = GetClientEvents();
  ASSERT_EQ(0, client_events.size());

  // Test that EndConnectionAttempt doesn't write empty events to the tracker.
  // There should be 2 events: 1) the successful connection which was previously
  // established. 2) the unsuccessful connection triggered as a result of the
  // SOCKET_FAILURE signal. The NETWORK_CHANGE signal should not cause an
  // additional event since there is no in progress event.
  factory()->SetConnectResult(net::ERR_CONNECTION_FAILED);
  factory()->SignalConnectionReset(ConnectionFactory::SOCKET_FAILURE);
  factory()->SignalConnectionReset(ConnectionFactory::NETWORK_CHANGE);
  WaitForConnections();

  client_events = GetClientEvents();
  ASSERT_EQ(2, client_events.size());
}

}  // namespace gcm
