// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/connection_factory_impl.h"

#include <cmath>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "google_apis/gcm/base/gcm_features.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/connection_factory.h"
#include "google_apis/gcm/engine/fake_connection_handler.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/backoff_entry.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
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
  endpoints.emplace_back(GURL(kMCSEndpoint));
  endpoints.emplace_back(GURL(kMCSEndpoint2));
  return endpoints;
}

// Used as a builder for test login requests.
void FillLoginRequest(mcs_proto::LoginRequest* login_request) {
  std::unique_ptr<mcs_proto::LoginRequest> request =
      BuildLoginRequest(0, 0, "");
  login_request->CopyFrom(*request);
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
      base::RepeatingClosure finished_callback);
  ~TestConnectionFactoryImpl() override;

  void InitializeFactory();

  // Overridden stubs.
  void StartConnection(bool ignore_connection_failure) override;
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
  int connect_result_ = net::ERR_UNEXPECTED;
  // The number of expected connection attempts;
  int num_expected_attempts_ = 0;
  // Callback to invoke when all connection attempts have been made.
  base::RepeatingClosure finished_callback_;
  // A temporary scoped pointer to make sure we don't leak the handler in the
  // cases it's never consumed by the ConnectionFactory.
  std::unique_ptr<FakeConnectionHandler> scoped_handler_;
  // The current fake connection handler..
  raw_ptr<FakeConnectionHandler> fake_handler_;
  // Dummy GCM Stats recorder.
  FakeGCMStatsRecorder dummy_recorder_;
  // Dummy mojo pipes.
  mojo::ScopedDataPipeProducerHandle receive_pipe_producer_;
  mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_;
  mojo::ScopedDataPipeProducerHandle send_pipe_producer_;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_;
};

TestConnectionFactoryImpl::TestConnectionFactoryImpl(
    GetProxyResolvingFactoryCallback get_socket_factory_callback,
    base::RepeatingClosure finished_callback)
    : ConnectionFactoryImpl(
          BuildEndpoints(),
          net::BackoffEntry::Policy(),
          get_socket_factory_callback,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          &dummy_recorder_,
          network::TestNetworkConnectionTracker::GetInstance()),
      finished_callback_(std::move(finished_callback)),
      scoped_handler_(std::make_unique<FakeConnectionHandler>(
          base::BindRepeating(&ReadContinuation),
          base::BindRepeating(&WriteContinuation),
          base::BindRepeating(
              &TestConnectionFactoryImpl::ConnectionHandlerCallback,
              base::Unretained(this)))),
      fake_handler_(scoped_handler_.get()) {
  // Set a non-null time.
  tick_clock_.Advance(base::Milliseconds(1));

  EXPECT_EQ(mojo::CreateDataPipe(nullptr, receive_pipe_producer_,
                                 receive_pipe_consumer_),
            MOJO_RESULT_OK);
  EXPECT_EQ(
      mojo::CreateDataPipe(nullptr, send_pipe_producer_, send_pipe_consumer_),
      MOJO_RESULT_OK);
}

TestConnectionFactoryImpl::~TestConnectionFactoryImpl() {
  EXPECT_EQ(0, num_expected_attempts_);
}

void TestConnectionFactoryImpl::StartConnection(
    bool ignore_connection_failure) {
  ASSERT_GT(num_expected_attempts_, 0) << "Unexpected connection attempt";
  ASSERT_FALSE(GetConnectionHandler()->CanSendMessage());

  // Update the number of apptempts before calling OnConnectDone() because it
  // can internally call StartConnection() again, e.g. during a network change.
  --num_expected_attempts_;
  OnConnectDone(ignore_connection_failure, connect_result_, net::IPEndPoint(),
                net::IPEndPoint(), mojo::ScopedDataPipeConsumerHandle(),
                mojo::ScopedDataPipeProducerHandle());

  if (!NextRetryAttempt().is_null()) {
    // Advance the time to the next retry time.
    base::TimeDelta time_till_retry =
        NextRetryAttempt() - tick_clock_.NowTicks();
    tick_clock_.Advance(time_till_retry);
  }
  if (num_expected_attempts_ == 0) {
    connect_result_ = net::ERR_UNEXPECTED;
    finished_callback_.Run();
  }
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
  connect_result_ = connect_result;
  num_expected_attempts_ = num_expected_attempts;
  for (int i = 0 ; i < num_expected_attempts; ++i) {
    fake_handler_->ExpectOutgoingMessage(
        MCSMessage(kLoginRequestTag, BuildLoginRequest(0, 0, "")));
  }
}

void TestConnectionFactoryImpl::SetDelayLogin(bool delay_login) {
  fake_handler_->set_fail_login(delay_login);
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

  base::RunLoop* GetRunLoop() { return run_loop_.get(); }

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
      factory_(
          base::BindRepeating(
              &ConnectionFactoryImplTest::GetProxyResolvingSocketFactory,
              base::Unretained(this)),
          base::BindRepeating(&ConnectionFactoryImplTest::ConnectionsComplete,
                              base::Unretained(this))),
      run_loop_(new base::RunLoop()),
      network_change_notifier_(
          net::NetworkChangeNotifier::CreateMockIfNeeded()),
      network_service_(network::NetworkService::CreateForTesting()) {
  network::mojom::NetworkContextParamsPtr params =
      network::mojom::NetworkContextParams::New();
  // Use a dummy CertVerifier that always passes cert verification, since
  // these unittests don't need to test CertVerifier behavior.
  params->cert_verifier_params =
      network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  network_context_ = std::make_unique<network::NetworkContext>(
      network_service_.get(),
      network_context_remote_.BindNewPipeAndPassReceiver(), std::move(params));
  factory()->SetConnectionListener(this);
  factory()->Initialize(base::BindRepeating(&FillLoginRequest),
                        ConnectionHandler::ProtoReceivedCallback(),
                        ConnectionHandler::ProtoSentCallback());
}

ConnectionFactoryImplTest::~ConnectionFactoryImplTest() = default;

void ConnectionFactoryImplTest::WaitForConnections() {
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
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

  // Backoff should not change because of network change.
  base::TimeTicks next_backoff = factory()->NextRetryAttempt();
  EXPECT_EQ(next_backoff, initial_backoff);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, GetRunLoop()->QuitWhenIdleClosure(), base::Milliseconds(1));
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

TEST_F(ConnectionFactoryImplTest,
       ShouldNotIncreaseBackoffDelayOnNetworkChange) {
  base::test::ScopedFeatureList feature_override(
      gcm::features::kGCMDoNotIncreaseBackoffDelayOnNetworkChange);

  factory()->SetConnectResult(net::ERR_NAME_NOT_RESOLVED);
  factory()->Connect();
  WaitForConnections();
  base::TimeTicks retry_time = factory()->NextRetryAttempt();
  ASSERT_FALSE(retry_time.is_null());

  // Mimic a network change with `ERR_NAME_NOT_RESOLVED` error.
  factory()->SetConnectResult(net::ERR_NAME_NOT_RESOLVED);
  factory()->OnConnectionChanged(network::mojom::ConnectionType::CONNECTION_4G);
  WaitForConnections();
  ASSERT_FALSE(factory()->IsEndpointReachable());

  // Verify that the next retry time hasn't changed despite error.
  EXPECT_EQ(retry_time, factory()->NextRetryAttempt());
}

// Tests that if there are a lot of network changes resulting in net error
// (could happen in some cases with VPN connection), backoff delay does not
// increase. This is needed to avoid huge delays while there is no network
// connection.
TEST_F(ConnectionFactoryImplTest,
       ShouldRetryWithSmallDelayAfterManyNetworkChanges) {
  base::test::ScopedFeatureList feature_override(
      gcm::features::kGCMDoNotIncreaseBackoffDelayOnNetworkChange);

  factory()->SetConnectResult(net::ERR_NAME_NOT_RESOLVED);
  base::TimeTicks connect_time = factory()->tick_clock()->NowTicks();
  factory()->Connect();
  WaitForConnections();
  ASSERT_FALSE(factory()->NextRetryAttempt().is_null());

  // Mimic several network changes with `ERR_NAME_NOT_RESOLVED` error.
  const size_t kNumAttempts = 10;
  for (size_t i = 0; i < kNumAttempts; ++i) {
    factory()->SetConnectResult(net::ERR_NAME_NOT_RESOLVED);
    factory()->OnConnectionChanged(
        network::mojom::ConnectionType::CONNECTION_4G);
    WaitForConnections();
    ASSERT_FALSE(factory()->IsEndpointReachable());
  }

  // There must be at most 1 failed attempt affecting backoff policy (others are
  // ignored due to network changes). Calculate backoff for 2 attempts to avoid
  // flakiness.
  base::TimeTicks retry_time = factory()->NextRetryAttempt();
  EXPECT_LE((retry_time - connect_time).InMilliseconds(),
            CalculateBackoff(/*num_attempts=*/2));
}

// When the network is disconnected, close the socket and suppress further
// connection attempts until the network returns.
TEST_F(ConnectionFactoryImplTest, SuppressConnectWhenNoNetwork) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitAndEnableFeature(
      gcm::features::kGCMAvoidConnectionWhenNetworkUnavailable);

  factory()->SetConnectResult(net::OK);
  factory()->Connect();
  EXPECT_TRUE(factory()->NextRetryAttempt().is_null());
  EXPECT_TRUE(factory()->IsEndpointReachable());

  // Advance clock so the login window reset isn't encountered.
  factory()->tick_clock()->Advance(base::Seconds(11));

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

TEST_F(ConnectionFactoryImplTest,
       ShouldConnectWhenNetworkChangedDuringHandshake) {
  // SetDelayLogin() will keep handshake in progress.
  factory()->SetDelayLogin(true);
  factory()->SetConnectResult(net::OK);
  factory()->Connect();
  WaitForConnections();
  ASSERT_FALSE(factory()->IsEndpointReachable());

  // Mimic a network change during handshake, and fail connection request.
  factory()->SetDelayLogin(false);
  factory()->SetMultipleConnectResults(net::ERR_CONNECTION_FAILED,
                                       /*num_expected_attempts=*/2);
  factory()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  WaitForConnections();
  ASSERT_FALSE(factory()->IsEndpointReachable());
  ASSERT_FALSE(factory()->NextRetryAttempt().is_null());

  // After backoff, connection should be established.
  factory()->SetConnectResult(net::OK);
  factory()->tick_clock()->Advance(factory()->NextRetryAttempt() -
                                   factory()->tick_clock()->NowTicks());
  WaitForConnections();

  EXPECT_TRUE(factory()->IsEndpointReachable());
}

}  // namespace gcm
