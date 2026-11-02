// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/isolated_http_network_session.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/net_buildflags.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/reporting/reporting_policy.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_http_transaction_factory_override.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using net::test::IsOk;

class IsolatedHttpNetworkSessionTest : public testing::Test,
                                       public WithTaskEnvironment {
 public:
  IsolatedHttpNetworkSessionTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    auto builder = CreateTestURLRequestContextBuilder();
    builder->set_client_socket_factory_for_testing(&primary_factory_);
    context_ = builder->Build();
  }

  URLRequestContext* context() { return context_.get(); }
  MockClientSocketFactory& primary_factory() { return primary_factory_; }

  // Separate HttpServerProperties instance for isolated sessions, as
  // required by CreateIsolatedHttpNetworkSession(). Must outlive the
  // sessions the tests create.
  HttpServerProperties* isolated_properties() { return &isolated_properties_; }

 private:
  MockClientSocketFactory primary_factory_;
  HttpServerProperties isolated_properties_;
  std::unique_ptr<URLRequestContext> context_;
};

TEST_F(IsolatedHttpNetworkSessionTest, CreatesSessionWithDifferentFactory) {
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  ASSERT_TRUE(isolated_session);

  // The isolated session should use our factory, not the default.
  // We can verify by checking the context's client_socket_factory field.
  EXPECT_EQ(isolated_session->context().client_socket_factory, &mock_factory);
}

TEST_F(IsolatedHttpNetworkSessionTest, SharesHostResolver) {
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  // Both sessions should share the same host resolver.
  EXPECT_EQ(isolated_session->context().host_resolver,
            context()->host_resolver());
}

TEST_F(IsolatedHttpNetworkSessionTest, SharesCertVerifier) {
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  EXPECT_EQ(isolated_session->context().cert_verifier,
            context()->cert_verifier());
}

TEST_F(IsolatedHttpNetworkSessionTest, SharesProxyService) {
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  EXPECT_EQ(isolated_session->context().proxy_resolution_service,
            context()->proxy_resolution_service());
}

TEST_F(IsolatedHttpNetworkSessionTest, SharesSSLConfigService) {
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  EXPECT_EQ(isolated_session->context().ssl_config_service,
            context()->ssl_config_service());
}

TEST_F(IsolatedHttpNetworkSessionTest, HasOwnHttpServerProperties) {
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  // The isolated session must use the caller-provided HttpServerProperties,
  // not the primary context's: simulation-induced failures (e.g. QUIC
  // timeouts under simulated delay) must not mark alternative services
  // broken for unthrottled requests.
  EXPECT_EQ(isolated_session->context().http_server_properties,
            isolated_properties());
  EXPECT_NE(isolated_session->context().http_server_properties,
            context()->http_server_properties());
}

TEST_F(IsolatedHttpNetworkSessionTest, CopiesDefaultParams) {
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  // Should copy params from the existing session.
  const auto* original_params = context()->GetNetworkSessionParams();
  ASSERT_TRUE(original_params);
  EXPECT_EQ(isolated_session->params().enable_quic,
            original_params->enable_quic);
  EXPECT_EQ(isolated_session->params().enable_http2,
            original_params->enable_http2);
}

TEST_F(IsolatedHttpNetworkSessionTest, CanOverrideParams) {
  MockClientSocketFactory mock_factory;

  HttpNetworkSessionParams custom_params =
      *context()->GetNetworkSessionParams();
  custom_params.enable_quic = false;

  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties(), &custom_params);

  EXPECT_FALSE(isolated_session->params().enable_quic);
}

TEST_F(IsolatedHttpNetworkSessionTest, HasIndependentSocketPools) {
  MockClientSocketFactory mock_factory1;
  MockClientSocketFactory mock_factory2;

  auto session1 = CreateIsolatedHttpNetworkSession(context(), &mock_factory1,
                                                   isolated_properties());
  auto session2 = CreateIsolatedHttpNetworkSession(context(), &mock_factory2,
                                                   isolated_properties());

  // Each session should have its own socket pool.
  ClientSocketPool* pool1 = session1->GetSocketPool(
      HttpNetworkSession::SocketPoolType::kNormal, ProxyChain::Direct());
  ClientSocketPool* pool2 = session2->GetSocketPool(
      HttpNetworkSession::SocketPoolType::kNormal, ProxyChain::Direct());

  EXPECT_NE(pool1, pool2);
}

TEST_F(IsolatedHttpNetworkSessionTest, HasIndependentSpdySessionPool) {
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  HttpNetworkSession* primary_session =
      context()->http_transaction_factory()->GetSession();

  // SPDY session pools should be independent.
  EXPECT_NE(isolated_session->spdy_session_pool(),
            primary_session->spdy_session_pool());
}

TEST_F(IsolatedHttpNetworkSessionTest, CloseConnectionsDoesNotAffectPrimary) {
  // Prime the primary session's socket pool with an idle keep-alive socket
  // by running one complete transaction through the primary context.
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"),
  };
  StaticSocketDataProvider socket_data(reads, base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  primary_factory().AddSocketDataProvider(&socket_data);

  HttpNetworkSession* primary_session =
      context()->http_transaction_factory()->GetSession();
  {
    auto transaction = context()->http_transaction_factory()->CreateTransaction(
        DEFAULT_PRIORITY);
    ASSERT_TRUE(transaction);
    HttpRequestInfo request_info;
    request_info.method = "GET";
    request_info.url = GURL("http://example.com/");
    request_info.traffic_annotation =
        MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    TestCompletionCallback callback;
    int rv = transaction->Start(&request_info, callback.callback(),
                                NetLogWithSource());
    ASSERT_THAT(callback.GetResult(rv), IsOk());
  }
  // The socket returns to the pool via posted tasks once the transaction is
  // destroyed.
  ClientSocketPool* primary_pool = primary_session->GetSocketPool(
      HttpNetworkSession::SocketPoolType::kNormal, ProxyChain::Direct());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return primary_pool->IdleSocketCount() == 1; }));

  // The isolated session has its own, distinct socket pools.
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());
  EXPECT_NE(
      isolated_session->GetSocketPool(
          HttpNetworkSession::SocketPoolType::kNormal, ProxyChain::Direct()),
      primary_pool);

  // Closing the isolated session's connections must not touch the primary
  // session's pools: its idle socket survives. CloseAllConnections acts on
  // the isolated session's pools synchronously.
  isolated_session->CloseAllConnections(ERR_ABORTED, "test");
  EXPECT_EQ(primary_pool->IdleSocketCount(), 1);
}

TEST_F(IsolatedHttpNetworkSessionTest, HasNoNetworkQualityEstimator) {
  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  // Isolated sessions simulate network conditions, so their socket-level
  // observations must not feed the shared network-quality estimate.
  EXPECT_EQ(isolated_session->context().network_quality_estimator, nullptr);
  EXPECT_EQ(isolated_session->context().socket_performance_watcher_factory,
            nullptr);
}

#if BUILDFLAG(ENABLE_REPORTING)
TEST_F(IsolatedHttpNetworkSessionTest, SharesReportingAndNelServices) {
  // Reporting / NEL header processing must keep working for throttled
  // requests: the isolated session shares the context-owned services.
  auto builder = CreateTestURLRequestContextBuilder();
  builder->set_reporting_policy(std::make_unique<ReportingPolicy>());
  builder->set_network_error_logging_enabled(true);
  auto reporting_context = builder->Build();
  ASSERT_TRUE(reporting_context->reporting_service());
  ASSERT_TRUE(reporting_context->network_error_logging_service());

  MockClientSocketFactory mock_factory;
  auto isolated_session = CreateIsolatedHttpNetworkSession(
      reporting_context.get(), &mock_factory, isolated_properties());

  EXPECT_EQ(isolated_session->context().reporting_service,
            reporting_context->reporting_service());
  EXPECT_EQ(isolated_session->context().network_error_logging_service,
            reporting_context->network_error_logging_service());
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

TEST(HttpTransactionFactoryHandleTest, DestructionCallbackSeesNullFactory) {
  // The destruction callback typically releases the last reference to the
  // factory's owner, so the handle must clear `factory_` *before* running
  // it - otherwise the raw_ptr dangles for the rest of the destructor,
  // which MiraclePtr's dangling-pointer detector treats as fatal.
  MockNetworkLayer factory;
  bool callback_ran = false;
  bool factory_was_null = false;
  HttpTransactionFactoryHandle* handle_ptr = nullptr;
  {
    auto handle = base::MakeRefCounted<HttpTransactionFactoryHandle>(
        &factory, base::BindLambdaForTesting([&]() {
          callback_ran = true;
          factory_was_null = handle_ptr->factory() == nullptr;
        }),
        /*disables_network_quality_estimation=*/false);
    handle_ptr = handle.get();
    ASSERT_EQ(handle->factory(), &factory);
  }
  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(factory_was_null);
}

// Counts RTT observations delivered to a NetworkQualityEstimator.
class CountingRttObserver : public NetworkQualityEstimator::RTTObserver {
 public:
  CountingRttObserver() = default;
  ~CountingRttObserver() override = default;

  void OnRTTObservation(int32_t rtt_ms,
                        const base::TimeTicks& timestamp,
                        NetworkQualityObservationSource source) override {
    ++count_;
  }

  int count() const { return count_; }

 private:
  int count_ = 0;
};

// Uses real time: NQE discards zero-duration RTT observations, which is all
// a MOCK_TIME environment would produce with mock sockets.
class IsolatedSessionNqeTest : public testing::Test,
                               public WithTaskEnvironment {
 protected:
  // Serves one canned, connection-close HTTP response for `url` from
  // `socket_factory` and runs a request for it to completion. When `handle`
  // is non-null, it is attached to the request as a transaction-factory
  // override.
  void RunRequest(URLRequestContext* context,
                  MockClientSocketFactory* socket_factory,
                  const GURL& url,
                  scoped_refptr<HttpTransactionFactoryHandle> handle) {
    const MockRead kReads[] = {
        MockRead(ASYNC,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Length: 5\r\n"
                 "Connection: close\r\n\r\n"
                 "hello"),
        MockRead(SYNCHRONOUS, OK),
    };
    StaticSocketDataProvider data(kReads, base::span<MockWrite>());
    data.set_connect_data(MockConnect(ASYNC, OK));
    socket_factory->AddSocketDataProvider(&data);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context->CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS,
        handles::kInvalidNetworkHandle);
    if (handle) {
      HttpTransactionFactoryOverride::Set(request.get(), std::move(handle));
    }
    request->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ(delegate.request_status(), OK);
  }
};

TEST_F(IsolatedSessionNqeTest, FlaggedOverrideSkipsNqeObservations) {
  // Requests routed through a factory override flagged as disabling network
  // quality estimation must produce NO NQE observations; a regular request
  // through the same context must.
  TestNetworkQualityEstimator network_quality_estimator(
      /*variation_params=*/{},
      /*allow_local_host_requests_for_tests=*/true,
      /*allow_smaller_responses_for_tests=*/true);
  CountingRttObserver rtt_observer;
  network_quality_estimator.AddRTTObserver(&rtt_observer);

  MockClientSocketFactory socket_factory;
  auto builder = CreateTestURLRequestContextBuilder();
  builder->set_client_socket_factory_for_testing(&socket_factory);
  builder->set_network_quality_estimator(&network_quality_estimator);
  auto context = builder->Build();

  // Deterministically drains the estimator's pending notification tasks:
  // they are plain (non-delayed) tasks posted while the request ran, so by
  // FIFO ordering they all execute before a sentinel task posted now.
  auto drain_nqe_tasks = [] {
    base::RunLoop drain;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, drain.QuitClosure());
    drain.Run();
  };

  // Control: a regular request feeds the estimator.
  RunRequest(context.get(), &socket_factory, GURL("http://a.test/"),
             /*handle=*/nullptr);
  drain_nqe_tasks();
  EXPECT_GT(rtt_observer.count(), 0);

  // A request under a flagged override adds no observations, even though it
  // uses the very same transaction factory underneath.
  const int baseline = rtt_observer.count();
  auto handle = base::MakeRefCounted<HttpTransactionFactoryHandle>(
      context->http_transaction_factory(), base::DoNothing(),
      /*disables_network_quality_estimation=*/true);
  RunRequest(context.get(), &socket_factory, GURL("http://b.test/"),
             std::move(handle));
  drain_nqe_tasks();
  EXPECT_EQ(rtt_observer.count(), baseline);
}

TEST_F(IsolatedHttpNetworkSessionTest,
       OverrideDisablesNetworkQualityEstimationFlag) {
  // The "disables network quality estimation" bit is an explicit property
  // of the handle (not inferred from the override's presence) and must
  // survive Reset(): a request that ran under simulated conditions must
  // never feed the estimator, even after its session was retired.
  TestDelegate delegate;
  auto request = context()->CreateRequest(
      GURL("https://example.com/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS, handles::kInvalidNetworkHandle);
  EXPECT_FALSE(HttpTransactionFactoryOverride::DisablesNetworkQualityEstimation(
      request.get()));

  auto flagged_handle = base::MakeRefCounted<HttpTransactionFactoryHandle>(
      context()->http_transaction_factory(), base::DoNothing(),
      /*disables_network_quality_estimation=*/true);
  HttpTransactionFactoryOverride::Set(request.get(), flagged_handle);
  EXPECT_TRUE(HttpTransactionFactoryOverride::DisablesNetworkQualityEstimation(
      request.get()));

  flagged_handle->Reset();
  EXPECT_FALSE(HttpTransactionFactoryOverride::Get(request.get()));
  EXPECT_TRUE(HttpTransactionFactoryOverride::DisablesNetworkQualityEstimation(
      request.get()));

  // An unflagged override (some other use of the mechanism) does not
  // disable estimation.
  auto request2 = context()->CreateRequest(
      GURL("https://example.com/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS, handles::kInvalidNetworkHandle);
  auto unflagged_handle = base::MakeRefCounted<HttpTransactionFactoryHandle>(
      context()->http_transaction_factory(), base::DoNothing(),
      /*disables_network_quality_estimation=*/false);
  HttpTransactionFactoryOverride::Set(request2.get(), unflagged_handle);
  EXPECT_TRUE(HttpTransactionFactoryOverride::Get(request2.get()));
  EXPECT_FALSE(HttpTransactionFactoryOverride::DisablesNetworkQualityEstimation(
      request2.get()));
}

TEST_F(IsolatedHttpNetworkSessionTest, TransactionUsesIsolatedFactory) {
  // Set up a mock socket factory with a canned HTTP response.
  MockClientSocketFactory mock_factory;
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Content-Length: 5\r\n\r\n"
               "hello"),
  };
  StaticSocketDataProvider socket_data(reads, base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  mock_factory.AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  mock_factory.AddSSLSocketDataProvider(&ssl);

  auto isolated_session = CreateIsolatedHttpNetworkSession(
      context(), &mock_factory, isolated_properties());

  // Create a transaction through the isolated session's network layer.
  HttpNetworkLayer network_layer(isolated_session.get());
  auto transaction = network_layer.CreateTransaction(DEFAULT_PRIORITY);
  ASSERT_TRUE(transaction);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://example.com/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  TestCompletionCallback callback;
  int rv = transaction->Start(&request_info, callback.callback(),
                              NetLogWithSource());
  rv = callback.GetResult(rv);
  EXPECT_THAT(rv, IsOk());

  // Verify we got the response from our mock factory.
  const HttpResponseInfo* response = transaction->GetResponseInfo();
  ASSERT_TRUE(response);
  EXPECT_EQ(200, response->headers->response_code());

  // The mock factory should have consumed the socket data.
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
}

}  // namespace
}  // namespace net
