// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/socket/websocket_transport_client_socket_pool.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/connect_job.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/transport_client_socket_pool_test_util.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;
const RequestPriority kDefaultPriority = LOW;

IPAddress ParseIP(const std::string& ip) {
  IPAddress address;
  CHECK(address.AssignFromIPLiteral(ip));
  return address;
}

// RunLoop doesn't support this natively but it is easy to emulate.
void RunLoopForTimePeriod(base::TimeDelta period) {
  base::RunLoop run_loop;
  base::OnceClosure quit_closure(run_loop.QuitClosure());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(quit_closure), period);
  run_loop.Run();
}

class WebSocketTransportClientSocketPoolTest : public TestWithTaskEnvironment {
 protected:
  WebSocketTransportClientSocketPoolTest()
      : group_id_(url::SchemeHostPort(url::kHttpScheme, "www.google.com", 80),
                  PrivacyMode::PRIVACY_MODE_DISABLED,
                  NetworkAnonymizationKey(),
                  SecureDnsPolicy::kAllow,
                  /*disable_cert_network_fetches=*/false),
        params_(ClientSocketPool::SocketParams::CreateForHttpForTesting()),
        host_resolver_(std::make_unique<
                       MockHostResolver>(/*default_result=*/
                                         MockHostResolverBase::RuleResolver::
                                             GetLocalhostResult())),
        client_socket_factory_(NetLog::Get()),
        common_connect_job_params_(
            &client_socket_factory_,
            host_resolver_.get(),
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
            /*net_log=*/nullptr,
            &websocket_endpoint_lock_manager_,
            /*http_server_properties=*/nullptr,
            /*alpn_protos=*/nullptr,
            /*application_settings=*/nullptr,
            /*ignore_certificate_errors=*/nullptr,
            /*early_data_enabled=*/nullptr),
        pool_(kMaxSockets,
              kMaxSocketsPerGroup,
              ProxyChain::Direct(),
              &common_connect_job_params_) {
    websocket_endpoint_lock_manager_.SetUnlockDelayForTesting(
        base::TimeDelta());
  }

  WebSocketTransportClientSocketPoolTest(
      const WebSocketTransportClientSocketPoolTest&) = delete;
  WebSocketTransportClientSocketPoolTest& operator=(
      const WebSocketTransportClientSocketPoolTest&) = delete;

  ~WebSocketTransportClientSocketPoolTest() override {
    RunUntilIdle();
    // ReleaseAllConnections() calls RunUntilIdle() after releasing each
    // connection.
    ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);
    EXPECT_TRUE(websocket_endpoint_lock_manager_.IsEmpty());
  }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  int StartRequest(RequestPriority priority) {
    return test_base_.StartRequestUsingPool(
        &pool_, group_id_, priority, ClientSocketPool::RespectLimits::ENABLED,
        params_);
  }

  int GetOrderOfRequest(size_t index) {
    return test_base_.GetOrderOfRequest(index);
  }

  bool ReleaseOneConnection(ClientSocketPoolTest::KeepAlive keep_alive) {
    return test_base_.ReleaseOneConnection(keep_alive);
  }

  void ReleaseAllConnections(ClientSocketPoolTest::KeepAlive keep_alive) {
    test_base_.ReleaseAllConnections(keep_alive);
  }

  TestSocketRequest* request(int i) { return test_base_.request(i); }

  std::vector<std::unique_ptr<TestSocketRequest>>* requests() {
    return test_base_.requests();
  }
  size_t completion_count() const { return test_base_.completion_count(); }

  // |group_id_| and |params_| correspond to the same socket parameters.
  const ClientSocketPool::GroupId group_id_;
  scoped_refptr<ClientSocketPool::SocketParams> params_;
  std::unique_ptr<MockHostResolver> host_resolver_;
  MockTransportClientSocketFactory client_socket_factory_;
  WebSocketEndpointLockManager websocket_endpoint_lock_manager_;
  const StaticHttpUserAgentSettings http_user_agent_settings_ = {"*",
                                                                 "test-ua"};
  const CommonConnectJobParams common_connect_job_params_;
  WebSocketTransportClientSocketPool pool_;
  ClientSocketPoolTest test_base_;
};

TEST_F(WebSocketTransportClientSocketPoolTest, Basic) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfoConnectedNotReused(handle);
}

// Make sure that the ConnectJob passes on its priority to its HostResolver
// request on Init.
TEST_F(WebSocketTransportClientSocketPoolTest, SetResolvePriorityOnInit) {
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    RequestPriority priority = static_cast<RequestPriority>(i);
    TestCompletionCallback callback;
    ClientSocketHandle handle;
    EXPECT_EQ(
        ERR_IO_PENDING,
        handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                    priority, SocketTag(),
                    ClientSocketPool::RespectLimits::ENABLED,
                    callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                    &pool_, NetLogWithSource()));
    EXPECT_EQ(priority, host_resolver_->last_request_priority());
  }
}

TEST_F(WebSocketTransportClientSocketPoolTest, InitHostResolutionFailure) {
  url::SchemeHostPort endpoint(url::kHttpScheme, "unresolvable.host.name", 80);
  host_resolver_->rules()->AddSimulatedTimeoutFailure(endpoint.host());
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(ClientSocketPool::GroupId(
                      std::move(endpoint), PRIVACY_MODE_DISABLED,
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_network_fetches=*/false),
                  ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                  std::nullopt /* proxy_annotation_tag */, kDefaultPriority,
                  SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource()));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(handle.resolve_error_info().error, IsError(ERR_DNS_TIMED_OUT));
  EXPECT_THAT(handle.connection_attempts(),
              testing::ElementsAre(
                  ConnectionAttempt(IPEndPoint(), ERR_NAME_NOT_RESOLVED)));
}

TEST_F(WebSocketTransportClientSocketPoolTest, InitConnectionFailure) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kFailing);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), &pool_,
                  NetLogWithSource()));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_FAILED));
  EXPECT_THAT(
      handle.connection_attempts(),
      testing::ElementsAre(ConnectionAttempt(
          IPEndPoint(IPAddress::IPv4Localhost(), 80), ERR_CONNECTION_FAILED)));

  // Make the host resolutions complete synchronously this time.
  host_resolver_->set_synchronous_mode(true);
  EXPECT_EQ(
      ERR_CONNECTION_FAILED,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), &pool_,
                  NetLogWithSource()));
  EXPECT_THAT(
      handle.connection_attempts(),
      testing::ElementsAre(ConnectionAttempt(
          IPEndPoint(IPAddress::IPv4Localhost(), 80), ERR_CONNECTION_FAILED)));
}

TEST_F(WebSocketTransportClientSocketPoolTest, PendingRequestsFinishFifo) {
  // First request finishes asynchronously.
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(request(0)->WaitForResult(), IsOk());

  // Make all subsequent host resolutions complete synchronously.
  host_resolver_->set_synchronous_mode(true);

  // Rest of them wait for the first socket to be released.
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(6, client_socket_factory_.allocation_count());

  // One initial asynchronous request and then 5 pending requests.
  EXPECT_EQ(6U, completion_count());

  // The requests finish in FIFO order.
  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(7));
}

TEST_F(WebSocketTransportClientSocketPoolTest, PendingRequests_NoKeepAlive) {
  // First request finishes asynchronously.
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(request(0)->WaitForResult(), IsOk());

  // Make all subsequent host resolutions complete synchronously.
  host_resolver_->set_synchronous_mode(true);

  // Rest of them wait for the first socket to be released.
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  // The pending requests should finish successfully.
  EXPECT_THAT(request(1)->WaitForResult(), IsOk());
  EXPECT_THAT(request(2)->WaitForResult(), IsOk());
  EXPECT_THAT(request(3)->WaitForResult(), IsOk());
  EXPECT_THAT(request(4)->WaitForResult(), IsOk());
  EXPECT_THAT(request(5)->WaitForResult(), IsOk());

  EXPECT_EQ(static_cast<int>(requests()->size()),
            client_socket_factory_.allocation_count());

  // First asynchronous request, and then last 5 pending requests.
  EXPECT_EQ(6U, completion_count());
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending host resolution will eventually complete, and destroy the
// ClientSocketPool which will crash if the group was not cleared properly.
TEST_F(WebSocketTransportClientSocketPoolTest, CancelRequestClearGroup) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), &pool_,
                  NetLogWithSource()));
  handle.Reset();
}

TEST_F(WebSocketTransportClientSocketPoolTest, TwoRequestsCancelOne) {
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  ClientSocketHandle handle2;
  TestCompletionCallback callback2;

  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), &pool_,
                  NetLogWithSource()));
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle2.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                   kDefaultPriority, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &pool_, NetLogWithSource()));

  handle.Reset();

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  handle2.Reset();
}

TEST_F(WebSocketTransportClientSocketPoolTest, ConnectCancelConnect) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kPending);
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), &pool_,
                  NetLogWithSource()));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED,
                  callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource()));

  host_resolver_->set_synchronous_mode(true);
  // At this point, handle has two ConnectingSockets out for it.  Due to the
  // setting the mock resolver into synchronous mode, the host resolution for
  // both will return in the same loop of the MessageLoop.  The client socket
  // is a pending socket, so the Connect() will asynchronously complete on the
  // next loop of the MessageLoop.  That means that the first
  // ConnectingSocket will enter OnIOComplete, and then the second one will.
  // If the first one is not cancelled, it will advance the load state, and
  // then the second one will crash.

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(WebSocketTransportClientSocketPoolTest, CancelRequest) {
  // First request finishes asynchronously.
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(request(0)->WaitForResult(), IsOk());

  // Make all subsequent host resolutions complete synchronously.
  host_resolver_->set_synchronous_mode(true);

  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));

  // Cancel a request.
  const size_t index_to_cancel = 2;
  EXPECT_FALSE(request(index_to_cancel)->handle()->is_initialized());
  request(index_to_cancel)->handle()->Reset();

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(5, client_socket_factory_.allocation_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(ClientSocketPoolTest::kRequestNotFound,
            GetOrderOfRequest(3));  // Canceled request.
  EXPECT_EQ(3, GetOrderOfRequest(4));
  EXPECT_EQ(4, GetOrderOfRequest(5));
  EXPECT_EQ(5, GetOrderOfRequest(6));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(7));
}

// Function to be used as a callback on socket request completion.  It first
// disconnects the successfully connected socket from the first request, and
// then reuses the ClientSocketHandle to request another socket.  The second
// request is expected to succeed asynchronously.
//
// |nested_callback| is called with the result of the second socket request.
void RequestSocketOnComplete(const ClientSocketPool::GroupId& group_id,
                             ClientSocketHandle* handle,
                             WebSocketTransportClientSocketPool* pool,
                             TestCompletionCallback* nested_callback,
                             int first_request_result) {
  EXPECT_THAT(first_request_result, IsOk());

  // Don't allow reuse of the socket.  Disconnect it and then release it.
  handle->socket()->Disconnect();
  handle->Reset();

  int rv = handle->Init(
      group_id, ClientSocketPool::SocketParams::CreateForHttpForTesting(),
      std::nullopt /* proxy_annotation_tag */, LOWEST, SocketTag(),
      ClientSocketPool::RespectLimits::ENABLED, nested_callback->callback(),
      ClientSocketPool::ProxyAuthCallback(), pool, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  if (ERR_IO_PENDING != rv) {
    nested_callback->callback().Run(rv);
  }
}

// Tests the case where a second socket is requested in a completion callback,
// and the second socket connects asynchronously.  Reuses the same
// ClientSocketHandle for the second socket, after disconnecting the first.
TEST_F(WebSocketTransportClientSocketPoolTest, RequestTwice) {
  ClientSocketHandle handle;
  TestCompletionCallback second_result_callback;
  int rv = handle.Init(
      group_id_, ClientSocketPool::SocketParams::CreateForHttpForTesting(),
      std::nullopt /* proxy_annotation_tag */, LOWEST, SocketTag(),
      ClientSocketPool::RespectLimits::ENABLED,
      base::BindOnce(&RequestSocketOnComplete, group_id_, &handle, &pool_,
                     &second_result_callback),
      ClientSocketPool::ProxyAuthCallback(), &pool_, NetLogWithSource());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(second_result_callback.WaitForResult(), IsOk());

  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(WebSocketTransportClientSocketPoolTest,
       CancelActiveRequestWithPendingRequests) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kPending);

  // Queue up all the requests
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));

  // Now, kMaxSocketsPerGroup requests should be active.  Let's cancel them.
  ASSERT_LE(kMaxSocketsPerGroup, static_cast<int>(requests()->size()));
  for (int i = 0; i < kMaxSocketsPerGroup; i++) {
    request(i)->handle()->Reset();
  }

  // Let's wait for the rest to complete now.
  for (size_t i = kMaxSocketsPerGroup; i < requests()->size(); ++i) {
    EXPECT_THAT(request(i)->WaitForResult(), IsOk());
    request(i)->handle()->Reset();
  }

  EXPECT_EQ(requests()->size() - kMaxSocketsPerGroup, completion_count());
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(WebSocketTransportClientSocketPoolTest,
       FailingActiveRequestWithPendingRequests) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kPendingFailing);

  const int kNumRequests = 2 * kMaxSocketsPerGroup + 1;
  ASSERT_LE(kNumRequests, kMaxSockets);  // Otherwise the test will hang.

  // Queue up all the requests
  for (int i = 0; i < kNumRequests; i++) {
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }

  for (int i = 0; i < kNumRequests; i++) {
    EXPECT_THAT(request(i)->WaitForResult(), IsError(ERR_CONNECTION_FAILED));
  }
}

// The lock on the endpoint is released when a ClientSocketHandle is reset.
TEST_F(WebSocketTransportClientSocketPoolTest, LockReleasedOnHandleReset) {
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(request(0)->WaitForResult(), IsOk());
  EXPECT_FALSE(request(1)->handle()->is_initialized());
  request(0)->handle()->Reset();
  RunUntilIdle();
  EXPECT_TRUE(request(1)->handle()->is_initialized());
}

// The lock on the endpoint is released when a ClientSocketHandle is deleted.
TEST_F(WebSocketTransportClientSocketPoolTest, LockReleasedOnHandleDelete) {
  TestCompletionCallback callback;
  auto handle = std::make_unique<ClientSocketHandle>();
  int rv =
      handle->Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                   LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_FALSE(request(0)->handle()->is_initialized());
  handle.reset();
  RunUntilIdle();
  EXPECT_TRUE(request(0)->handle()->is_initialized());
}

// A new connection is performed when the lock on the previous connection is
// explicitly released.
TEST_F(WebSocketTransportClientSocketPoolTest,
       ConnectionProceedsOnExplicitRelease) {
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(request(0)->WaitForResult(), IsOk());
  EXPECT_FALSE(request(1)->handle()->is_initialized());
  WebSocketTransportClientSocketPool::UnlockEndpoint(
      request(0)->handle(), &websocket_endpoint_lock_manager_);
  RunUntilIdle();
  EXPECT_TRUE(request(1)->handle()->is_initialized());
}

// A connection which is cancelled before completion does not block subsequent
// connections.
TEST_F(WebSocketTransportClientSocketPoolTest,
       CancelDuringConnectionReleasesLock) {
  MockTransportClientSocketFactory::Rule rules[] = {
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalled),
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kPending)};

  client_socket_factory_.SetRules(rules);

  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  RunUntilIdle();
  pool_.CancelRequest(group_id_, request(0)->handle(),
                      false /* cancel_connect_job */);
  EXPECT_THAT(request(1)->WaitForResult(), IsOk());
}

// Test the case of the IPv6 address stalling, and falling back to the IPv4
// socket which finishes first.
TEST_F(WebSocketTransportClientSocketPoolTest,
       IPv6FallbackSocketIPv4FinishesFirst) {
  MockTransportClientSocketFactory::Rule rules[] = {
      // This is the IPv6 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalled),
      // This is the IPv4 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kPending)};

  client_socket_factory_.SetRules(rules);

  // Resolve an AddressList with an IPv6 address first and then an IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule("*", "2:abcd::3:4:ff,2.2.2.2",
                                            std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv4());
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// Test the case of the IPv6 address being slow, thus falling back to trying to
// connect to the IPv4 address, but having the connect to the IPv6 address
// finish first.
TEST_F(WebSocketTransportClientSocketPoolTest,
       IPv6FallbackSocketIPv6FinishesFirst) {
  MockTransportClientSocketFactory::Rule rules[] = {
      // This is the IPv6 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kDelayed),
      // This is the IPv4 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalled)};

  client_socket_factory_.SetRules(rules);
  client_socket_factory_.set_delay(TransportConnectJob::kIPv6FallbackTime +
                                   base::Milliseconds(50));

  // Resolve an AddressList with an IPv6 address first and then an IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule("*", "2:abcd::3:4:ff,2.2.2.2",
                                            std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv6());
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

TEST_F(WebSocketTransportClientSocketPoolTest,
       IPv6NoIPv4AddressesToFallbackTo) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kDelayed);

  // Resolve an AddressList with only IPv6 addresses.
  host_resolver_->rules()->AddIPLiteralRule(
      "*", "2:abcd::3:4:ff,3:abcd::3:4:ff", std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv6());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

TEST_F(WebSocketTransportClientSocketPoolTest, IPv4HasNoFallback) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kDelayed);

  // Resolve an AddressList with only IPv4 addresses.
  host_resolver_->rules()->AddIPLiteralRule("*", "1.1.1.1", std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv4());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

// If all IPv6 addresses fail to connect synchronously, then IPv4 connections
// proceeed immediately.
TEST_F(WebSocketTransportClientSocketPoolTest, IPv6InstantFail) {
  MockTransportClientSocketFactory::Rule rules[] = {
      // First IPv6 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing),
      // Second IPv6 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing),
      // This is the IPv4 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous)};

  client_socket_factory_.SetRules(rules);

  // Resolve an AddressList with two IPv6 addresses and then an IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule(
      "*", "2:abcd::3:4:ff,2:abcd::3:5:ff,2.2.2.2", std::string());
  host_resolver_->set_synchronous_mode(true);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(handle.socket());

  IPEndPoint endpoint;
  handle.socket()->GetPeerAddress(&endpoint);
  EXPECT_EQ("2.2.2.2", endpoint.ToStringWithoutPort());
}

// If all IPv6 addresses fail before the IPv4 fallback timeout, then the IPv4
// connections proceed immediately.
TEST_F(WebSocketTransportClientSocketPoolTest, IPv6RapidFail) {
  MockTransportClientSocketFactory::Rule rules[] = {
      // First IPv6 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kPendingFailing),
      // Second IPv6 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kPendingFailing),
      // This is the IPv4 socket.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous)};

  client_socket_factory_.SetRules(rules);

  // Resolve an AddressList with two IPv6 addresses and then an IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule(
      "*", "2:abcd::3:4:ff,2:abcd::3:5:ff,2.2.2.2", std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.socket());

  base::TimeTicks start(base::TimeTicks::Now());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_LT(base::TimeTicks::Now() - start,
            TransportConnectJob::kIPv6FallbackTime);
  ASSERT_TRUE(handle.socket());

  IPEndPoint endpoint;
  handle.socket()->GetPeerAddress(&endpoint);
  EXPECT_EQ("2.2.2.2", endpoint.ToStringWithoutPort());
}

// If two sockets connect successfully, the one which connected first wins (this
// can only happen if the sockets are different types, since sockets of the same
// type do not race).
TEST_F(WebSocketTransportClientSocketPoolTest, FirstSuccessWins) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kTriggerable);

  // Resolve an AddressList with an IPv6 addresses and an IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule("*", "2:abcd::3:4:ff,2.2.2.2",
                                            std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_FALSE(handle.socket());

  base::OnceClosure ipv6_connect_trigger =
      client_socket_factory_.WaitForTriggerableSocketCreation();
  base::OnceClosure ipv4_connect_trigger =
      client_socket_factory_.WaitForTriggerableSocketCreation();

  std::move(ipv4_connect_trigger).Run();
  std::move(ipv6_connect_trigger).Run();

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  ASSERT_TRUE(handle.socket());

  IPEndPoint endpoint;
  handle.socket()->GetPeerAddress(&endpoint);
  EXPECT_EQ("2.2.2.2", endpoint.ToStringWithoutPort());
}

// We should not report failure until all connections have failed.
TEST_F(WebSocketTransportClientSocketPoolTest, LastFailureWins) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kDelayedFailing);
  base::TimeDelta delay = TransportConnectJob::kIPv6FallbackTime / 3;
  client_socket_factory_.set_delay(delay);

  // Resolve an AddressList with 4 IPv6 addresses and 2 IPv4 addresses.
  host_resolver_->rules()->AddIPLiteralRule("*",
                                            "1:abcd::3:4:ff,2:abcd::3:4:ff,"
                                            "3:abcd::3:4:ff,4:abcd::3:4:ff,"
                                            "1.1.1.1,2.2.2.2",
                                            std::string());

  // Expected order of events:
  // After 100ms: Connect to 1:abcd::3:4:ff times out
  // After 200ms: Connect to 2:abcd::3:4:ff times out
  // After 300ms: Connect to 3:abcd::3:4:ff times out, IPv4 fallback starts
  // After 400ms: Connect to 4:abcd::3:4:ff and 1.1.1.1 time out
  // After 500ms: Connect to 2.2.2.2 times out

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  base::TimeTicks start(base::TimeTicks::Now());
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_FAILED));

  EXPECT_GE(base::TimeTicks::Now() - start, delay * 5);

  // The order is slightly timing-dependent, so don't assert on the order.
  EXPECT_THAT(handle.connection_attempts(),
              testing::UnorderedElementsAre(
                  ConnectionAttempt(IPEndPoint(ParseIP("1:abcd::3:4:ff"), 80),
                                    ERR_CONNECTION_FAILED),
                  ConnectionAttempt(IPEndPoint(ParseIP("2:abcd::3:4:ff"), 80),
                                    ERR_CONNECTION_FAILED),
                  ConnectionAttempt(IPEndPoint(ParseIP("3:abcd::3:4:ff"), 80),
                                    ERR_CONNECTION_FAILED),
                  ConnectionAttempt(IPEndPoint(ParseIP("4:abcd::3:4:ff"), 80),
                                    ERR_CONNECTION_FAILED),
                  ConnectionAttempt(IPEndPoint(ParseIP("1.1.1.1"), 80),
                                    ERR_CONNECTION_FAILED),
                  ConnectionAttempt(IPEndPoint(ParseIP("2.2.2.2"), 80),
                                    ERR_CONNECTION_FAILED)));
}

// Test that, if an address fails due to `ERR_NETWORK_IO_SUSPENDED`, we do not
// try subsequent addresses.
TEST_F(WebSocketTransportClientSocketPoolTest, Suspend) {
  // Resolve an AddressList with 4 IPv6 addresses and 2 IPv4 addresses.
  host_resolver_->rules()->AddIPLiteralRule("*",
                                            "1:abcd::3:4:ff,2:abcd::3:4:ff,"
                                            "3:abcd::3:4:ff,4:abcd::3:4:ff,"
                                            "1.1.1.1,2.2.2.2",
                                            std::string());

  // The first connection attempt will fail, after which no more will be
  // attempted.
  MockTransportClientSocketFactory::Rule rule(
      MockTransportClientSocketFactory::Type::kFailing,
      std::vector{IPEndPoint(ParseIP("1:abcd::3:4:ff"), 80)},
      ERR_NETWORK_IO_SUSPENDED);
  client_socket_factory_.SetRules(base::span_from_ref(rule));

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(callback.GetResult(rv), IsError(ERR_NETWORK_IO_SUSPENDED));
  EXPECT_THAT(handle.connection_attempts(),
              testing::ElementsAre(
                  ConnectionAttempt(IPEndPoint(ParseIP("1:abcd::3:4:ff"), 80),
                                    ERR_NETWORK_IO_SUSPENDED)));
}

// Same as above, but with a asynchronous failure.
TEST_F(WebSocketTransportClientSocketPoolTest, SuspendAsync) {
  // Resolve an AddressList with 4 IPv6 addresses and 2 IPv4 addresses.
  host_resolver_->rules()->AddIPLiteralRule("*",
                                            "1:abcd::3:4:ff,2:abcd::3:4:ff,"
                                            "3:abcd::3:4:ff,4:abcd::3:4:ff,"
                                            "1.1.1.1,2.2.2.2",
                                            std::string());

  // The first connection attempt will fail, after which no more will be
  // attempted.
  MockTransportClientSocketFactory::Rule rule(
      MockTransportClientSocketFactory::Type::kPendingFailing,
      std::vector{IPEndPoint(ParseIP("1:abcd::3:4:ff"), 80)},
      ERR_NETWORK_IO_SUSPENDED);
  client_socket_factory_.SetRules(base::span_from_ref(rule));

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(callback.GetResult(rv), IsError(ERR_NETWORK_IO_SUSPENDED));
  EXPECT_THAT(handle.connection_attempts(),
              testing::ElementsAre(
                  ConnectionAttempt(IPEndPoint(ParseIP("1:abcd::3:4:ff"), 80),
                                    ERR_NETWORK_IO_SUSPENDED)));
}

// Global timeout for all connects applies. This test is disabled by default
// because it takes 4 minutes. Run with --gtest_also_run_disabled_tests if you
// want to run it.
TEST_F(WebSocketTransportClientSocketPoolTest, DISABLED_OverallTimeoutApplies) {
  const base::TimeDelta connect_job_timeout =
      TransportConnectJob::ConnectionTimeout();

  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kDelayedFailing);
  client_socket_factory_.set_delay(base::Seconds(1) + connect_job_timeout / 6);

  // Resolve an AddressList with 6 IPv6 addresses and 6 IPv4 addresses.
  host_resolver_->rules()->AddIPLiteralRule("*",
                                            "1:abcd::3:4:ff,2:abcd::3:4:ff,"
                                            "3:abcd::3:4:ff,4:abcd::3:4:ff,"
                                            "5:abcd::3:4:ff,6:abcd::3:4:ff,"
                                            "1.1.1.1,2.2.2.2,3.3.3.3,"
                                            "4.4.4.4,5.5.5.5,6.6.6.6",
                                            std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;

  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_TIMED_OUT));
}

TEST_F(WebSocketTransportClientSocketPoolTest, MaxSocketsEnforced) {
  host_resolver_->set_synchronous_mode(true);
  for (int i = 0; i < kMaxSockets; ++i) {
    ASSERT_THAT(StartRequest(kDefaultPriority), IsOk());
    WebSocketTransportClientSocketPool::UnlockEndpoint(
        request(i)->handle(), &websocket_endpoint_lock_manager_);
    RunUntilIdle();
  }
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
}

TEST_F(WebSocketTransportClientSocketPoolTest, MaxSocketsEnforcedWhenPending) {
  for (int i = 0; i < kMaxSockets + 1; ++i) {
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }
  // Now there are 32 sockets waiting to connect, and one stalled.
  for (int i = 0; i < kMaxSockets; ++i) {
    RunUntilIdle();
    EXPECT_TRUE(request(i)->handle()->is_initialized());
    EXPECT_TRUE(request(i)->handle()->socket());
    WebSocketTransportClientSocketPool::UnlockEndpoint(
        request(i)->handle(), &websocket_endpoint_lock_manager_);
  }
  // Now there are 32 sockets connected, and one stalled.
  RunUntilIdle();
  EXPECT_FALSE(request(kMaxSockets)->handle()->is_initialized());
  EXPECT_FALSE(request(kMaxSockets)->handle()->socket());
}

TEST_F(WebSocketTransportClientSocketPoolTest, StalledSocketReleased) {
  host_resolver_->set_synchronous_mode(true);
  for (int i = 0; i < kMaxSockets; ++i) {
    ASSERT_THAT(StartRequest(kDefaultPriority), IsOk());
    WebSocketTransportClientSocketPool::UnlockEndpoint(
        request(i)->handle(), &websocket_endpoint_lock_manager_);
    RunUntilIdle();
  }

  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  ReleaseOneConnection(ClientSocketPoolTest::NO_KEEP_ALIVE);
  EXPECT_TRUE(request(kMaxSockets)->handle()->is_initialized());
  EXPECT_TRUE(request(kMaxSockets)->handle()->socket());
}

TEST_F(WebSocketTransportClientSocketPoolTest, IsStalledTrueWhenStalled) {
  for (int i = 0; i < kMaxSockets + 1; ++i) {
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }
  EXPECT_THAT(request(0)->WaitForResult(), IsOk());
  EXPECT_TRUE(pool_.IsStalled());
}

TEST_F(WebSocketTransportClientSocketPoolTest,
       CancellingPendingSocketUnstallsStalledSocket) {
  for (int i = 0; i < kMaxSockets + 1; ++i) {
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }
  EXPECT_THAT(request(0)->WaitForResult(), IsOk());
  request(1)->handle()->Reset();
  RunUntilIdle();
  EXPECT_FALSE(pool_.IsStalled());
}

TEST_F(WebSocketTransportClientSocketPoolTest,
       LoadStateOfStalledSocketIsWaitingForAvailableSocket) {
  for (int i = 0; i < kMaxSockets + 1; ++i) {
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET,
            pool_.GetLoadState(group_id_, request(kMaxSockets)->handle()));
}

TEST_F(WebSocketTransportClientSocketPoolTest,
       CancellingStalledSocketUnstallsPool) {
  for (int i = 0; i < kMaxSockets + 1; ++i) {
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }
  request(kMaxSockets)->handle()->Reset();
  RunUntilIdle();
  EXPECT_FALSE(pool_.IsStalled());
}

TEST_F(WebSocketTransportClientSocketPoolTest,
       FlushWithErrorFlushesPendingConnections) {
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  pool_.FlushWithError(ERR_FAILED, "Very good reason");
  EXPECT_THAT(request(0)->WaitForResult(), IsError(ERR_FAILED));
}

TEST_F(WebSocketTransportClientSocketPoolTest,
       FlushWithErrorFlushesStalledConnections) {
  for (int i = 0; i < kMaxSockets + 1; ++i) {
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }
  pool_.FlushWithError(ERR_FAILED, "Very good reason");
  EXPECT_THAT(request(kMaxSockets)->WaitForResult(), IsError(ERR_FAILED));
}

TEST_F(WebSocketTransportClientSocketPoolTest,
       AfterFlushWithErrorCanMakeNewConnections) {
  for (int i = 0; i < kMaxSockets + 1; ++i) {
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }
  pool_.FlushWithError(ERR_FAILED, "Very good reason");
  host_resolver_->set_synchronous_mode(true);
  EXPECT_THAT(StartRequest(kDefaultPriority), IsOk());
}

// Deleting pending connections can release the lock on the endpoint, which can
// in principle lead to other pending connections succeeding. However, when we
// call FlushWithError(), everything should fail.
TEST_F(WebSocketTransportClientSocketPoolTest,
       FlushWithErrorDoesNotCauseSuccessfulConnections) {
  host_resolver_->set_synchronous_mode(true);
  MockTransportClientSocketFactory::Rule first_rule[] = {
      // First socket
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kPending),
  };
  client_socket_factory_.SetRules(first_rule);
  // The rest of the sockets will connect synchronously.
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kSynchronous);
  for (int i = 0; i < kMaxSockets; ++i) {
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }
  // Now we have one socket in STATE_TRANSPORT_CONNECT and the rest in
  // STATE_OBTAIN_LOCK. If any of the sockets in STATE_OBTAIN_LOCK is given the
  // lock, they will synchronously connect.
  pool_.FlushWithError(ERR_FAILED, "Very good reason");
  for (int i = 0; i < kMaxSockets; ++i) {
    EXPECT_THAT(request(i)->WaitForResult(), IsError(ERR_FAILED));
  }
}

// This is a regression test for the first attempted fix for
// FlushWithErrorDoesNotCauseSuccessfulConnections. Because a ConnectJob can
// have both IPv4 and IPv6 subjobs, it can be both connecting and waiting for
// the lock at the same time.
TEST_F(WebSocketTransportClientSocketPoolTest,
       FlushWithErrorDoesNotCauseSuccessfulConnectionsMultipleAddressTypes) {
  host_resolver_->set_synchronous_mode(true);
  // The first |kMaxSockets| sockets to connect will be IPv6. Then we will have
  // one IPv4.
  std::vector<MockTransportClientSocketFactory::Rule> rules(
      kMaxSockets + 1, MockTransportClientSocketFactory::Rule(
                           MockTransportClientSocketFactory::Type::kStalled));
  client_socket_factory_.SetRules(rules);
  // The rest of the sockets will connect synchronously.
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kSynchronous);
  for (int i = 0; i < kMaxSockets; ++i) {
    host_resolver_->rules()->ClearRules();
    // Each connect job has a different IPv6 address but the same IPv4 address.
    // So the IPv6 connections happen in parallel but the IPv4 ones are
    // serialised.
    host_resolver_->rules()->AddIPLiteralRule(
        "*",
        base::StringPrintf("%x:abcd::3:4:ff,"
                           "1.1.1.1",
                           i + 1),
        std::string());
    EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  }
  // Now we have |kMaxSockets| IPv6 sockets stalled in connect. No IPv4 sockets
  // are started yet.
  RunLoopForTimePeriod(TransportConnectJob::kIPv6FallbackTime);
  // Now we have |kMaxSockets| IPv6 sockets and one IPv4 socket stalled in
  // connect, and |kMaxSockets - 1| IPv4 sockets waiting for the endpoint lock.
  pool_.FlushWithError(ERR_FAILED, "Very good reason");
  for (int i = 0; i < kMaxSockets; ++i) {
    EXPECT_THAT(request(i)->WaitForResult(), IsError(ERR_FAILED));
  }
}

// Sockets that have had ownership transferred to a ClientSocketHandle should
// not be affected by FlushWithError.
TEST_F(WebSocketTransportClientSocketPoolTest,
       FlushWithErrorDoesNotAffectHandedOutSockets) {
  host_resolver_->set_synchronous_mode(true);
  MockTransportClientSocketFactory::Rule rules[] = {
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous),
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalled)};
  client_socket_factory_.SetRules(rules);
  EXPECT_THAT(StartRequest(kDefaultPriority), IsOk());
  // Socket has been "handed out".
  EXPECT_TRUE(request(0)->handle()->socket());

  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  // Now we have one socket handed out, and one pending.
  pool_.FlushWithError(ERR_FAILED, "Very good reason");
  EXPECT_THAT(request(1)->WaitForResult(), IsError(ERR_FAILED));
  // Socket owned by ClientSocketHandle is unaffected:
  EXPECT_TRUE(request(0)->handle()->socket());
  // Return it to the pool (which deletes it).
  request(0)->handle()->Reset();
}

// Sockets should not be leaked if CancelRequest() is called in between
// SetSocket() being called on the ClientSocketHandle and InvokeUserCallback().
TEST_F(WebSocketTransportClientSocketPoolTest, CancelRequestReclaimsSockets) {
  host_resolver_->set_synchronous_mode(true);
  MockTransportClientSocketFactory::Rule rules[] = {
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kTriggerable),
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous)};

  client_socket_factory_.SetRules(rules);

  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));

  base::OnceClosure connect_trigger =
      client_socket_factory_.WaitForTriggerableSocketCreation();

  std::move(connect_trigger).Run();  // Calls InvokeUserCallbackLater()

  request(0)->handle()->Reset();  // calls CancelRequest()

  RunUntilIdle();
  // We should now be able to create a new connection without blocking on the
  // endpoint lock.
  EXPECT_THAT(StartRequest(kDefaultPriority), IsOk());
}

// A handshake completing and then the WebSocket closing should only release one
// Endpoint, not two.
TEST_F(WebSocketTransportClientSocketPoolTest, EndpointLockIsOnlyReleasedOnce) {
  host_resolver_->set_synchronous_mode(true);
  ASSERT_THAT(StartRequest(kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest(kDefaultPriority), IsError(ERR_IO_PENDING));
  // First socket completes handshake.
  WebSocketTransportClientSocketPool::UnlockEndpoint(
      request(0)->handle(), &websocket_endpoint_lock_manager_);
  RunUntilIdle();
  // First socket is closed.
  request(0)->handle()->Reset();
  // Second socket should have been released.
  EXPECT_THAT(request(1)->WaitForResult(), IsOk());
  // Third socket should still be waiting for endpoint.
  ASSERT_FALSE(request(2)->handle()->is_initialized());
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET,
            request(2)->handle()->GetLoadState());
}

// Make sure that WebSocket requests use the correct NetworkAnonymizationKey.
TEST_F(WebSocketTransportClientSocketPoolTest, NetworkAnonymizationKey) {
  const SchemefulSite kSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  host_resolver_->set_ondemand_mode(true);

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  ClientSocketPool::GroupId group_id(
      url::SchemeHostPort(url::kHttpScheme, "www.google.com", 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  EXPECT_THAT(
      handle.Init(group_id, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), &pool_,
                  NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, host_resolver_->last_id());
  EXPECT_EQ(kNetworkAnonymizationKey,
            host_resolver_->request_network_anonymization_key(1));
}

TEST_F(WebSocketTransportClientSocketPoolTest,
       TransportConnectJobWithDnsAliases) {
  host_resolver_->set_synchronous_mode(true);
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kSynchronous);

  // Resolve an AddressList with DNS aliases.
  std::string kHostName("host");
  std::vector<std::string> aliases({"alias1", "alias2", kHostName});
  host_resolver_->rules()->AddIPLiteralRuleWithDnsAliases(kHostName, "2.2.2.2",
                                                          std::move(aliases));

  TestConnectJobDelegate test_delegate;
  scoped_refptr<TransportSocketParams> params =
      base::MakeRefCounted<TransportSocketParams>(
          HostPortPair(kHostName, 80), NetworkAnonymizationKey(),
          SecureDnsPolicy::kAllow, OnHostResolutionCallback(),
          /*supported_alpns=*/base::flat_set<std::string>());

  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_, params,
      &test_delegate, nullptr /* net_log */);

  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        true /* expect_sync_result */);

  // Verify that the elements of the alias list are those from the
  // parameter vector.
  EXPECT_THAT(test_delegate.socket()->GetDnsAliases(),
              testing::ElementsAre("alias1", "alias2", kHostName));
}

TEST_F(WebSocketTransportClientSocketPoolTest,
       TransportConnectJobWithNoAdditionalDnsAliases) {
  host_resolver_->set_synchronous_mode(true);
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kSynchronous);

  // Resolve an AddressList without additional DNS aliases. (The parameter
  // is an empty vector.)
  std::string kHostName("host");
  std::vector<std::string> aliases;
  host_resolver_->rules()->AddIPLiteralRuleWithDnsAliases(kHostName, "2.2.2.2",
                                                          std::move(aliases));

  TestConnectJobDelegate test_delegate;
  scoped_refptr<TransportSocketParams> params =
      base::MakeRefCounted<TransportSocketParams>(
          HostPortPair(kHostName, 80), NetworkAnonymizationKey(),
          SecureDnsPolicy::kAllow, OnHostResolutionCallback(),
          /*supported_alpns=*/base::flat_set<std::string>());

  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_, params,
      &test_delegate, nullptr /* net_log */);

  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        true /* expect_sync_result */);

  // Verify that the alias list only contains kHostName.
  EXPECT_THAT(test_delegate.socket()->GetDnsAliases(),
              testing::ElementsAre(kHostName));
}

TEST_F(WebSocketTransportClientSocketPoolTest, LoadState) {
  host_resolver_->rules()->AddRule("v6-only.test", "1:abcd::3:4:ff");
  host_resolver_->rules()->AddRule("v6-and-v4.test", "1:abcd::3:4:ff,2.2.2.2");
  host_resolver_->set_ondemand_mode(true);

  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kDelayedFailing);

  auto params_v6_only = base::MakeRefCounted<TransportSocketParams>(
      HostPortPair("v6-only.test", 80), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, OnHostResolutionCallback(),
      /*supported_alpns=*/base::flat_set<std::string>());
  auto params_v6_and_v4 = base::MakeRefCounted<TransportSocketParams>(
      HostPortPair("v6-and-v4.test", 80), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, OnHostResolutionCallback(),
      /*supported_alpns=*/base::flat_set<std::string>());

  // v6-only.test will first block on DNS.
  TestConnectJobDelegate test_delegate_v6_only;
  TransportConnectJob connect_job_v6_only(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      params_v6_only, &test_delegate_v6_only, /*net_log=*/nullptr);
  EXPECT_THAT(connect_job_v6_only.Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(connect_job_v6_only.GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  // When DNS is resolved, it should block on making a connection.
  host_resolver_->ResolveOnlyRequestNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(connect_job_v6_only.GetLoadState(), LOAD_STATE_CONNECTING);

  // v6-and-v4.test will also first block on DNS.
  TestConnectJobDelegate test_delegate_v6_and_v4;
  TransportConnectJob connect_job_v6_and_v4(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      params_v6_and_v4, &test_delegate_v6_and_v4, /*net_log=*/nullptr);
  EXPECT_THAT(connect_job_v6_and_v4.Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(connect_job_v6_and_v4.GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  // When DNS is resolved, it should attempt to connect to the IPv6 address, but
  // `connect_job_v6_only` holds the lock.
  host_resolver_->ResolveOnlyRequestNow();
  RunUntilIdle();
  EXPECT_THAT(connect_job_v6_and_v4.GetLoadState(),
              LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET);

  // After the IPv6 fallback timeout, it should attempt to connect to the IPv4
  // address. This lock is available, so `GetLoadState` should report it is now
  // actively connecting.
  RunLoopForTimePeriod(TransportConnectJob::kIPv6FallbackTime +
                       base::Milliseconds(50));
  EXPECT_THAT(connect_job_v6_and_v4.GetLoadState(), LOAD_STATE_CONNECTING);
}

}  // namespace

}  // namespace net
