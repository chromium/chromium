// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/socket/transport_client_socket_pool.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/stream_socket.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool_test_util.h"
#include "net/socket/transport_connect_job.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
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
constexpr base::TimeDelta kUnusedIdleSocketTimeout = base::Seconds(10);
const RequestPriority kDefaultPriority = LOW;

class SOCKS5MockData {
 public:
  explicit SOCKS5MockData(IoMode mode) {
    writes_ = std::make_unique<MockWrite[]>(2);
    writes_[0] =
        MockWrite(mode, kSOCKS5GreetRequest, kSOCKS5GreetRequestLength);
    writes_[1] = MockWrite(mode, kSOCKS5OkRequest, kSOCKS5OkRequestLength);

    reads_ = std::make_unique<MockRead[]>(2);
    reads_[0] =
        MockRead(mode, kSOCKS5GreetResponse, kSOCKS5GreetResponseLength);
    reads_[1] = MockRead(mode, kSOCKS5OkResponse, kSOCKS5OkResponseLength);

    data_ = std::make_unique<StaticSocketDataProvider>(
        base::make_span(reads_.get(), 2u), base::make_span(writes_.get(), 2u));
  }

  SocketDataProvider* data_provider() { return data_.get(); }

 private:
  std::unique_ptr<StaticSocketDataProvider> data_;
  std::unique_ptr<MockWrite[]> writes_;
  std::unique_ptr<MockRead[]> reads_;
};

class TransportClientSocketPoolTest : public ::testing::Test,
                                      public WithTaskEnvironment {
 public:
  TransportClientSocketPoolTest(const TransportClientSocketPoolTest&) = delete;
  TransportClientSocketPoolTest& operator=(
      const TransportClientSocketPoolTest&) = delete;

 protected:
  // Constructor that allows mocking of the time.
  explicit TransportClientSocketPoolTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : WithTaskEnvironment(time_source),
        connect_backup_jobs_enabled_(
            TransportClientSocketPool::set_connect_backup_jobs_enabled(true)),
        group_id_(url::SchemeHostPort(url::kHttpScheme, "www.google.com", 80),
                  PrivacyMode::PRIVACY_MODE_DISABLED,
                  NetworkAnonymizationKey(),
                  SecureDnsPolicy::kAllow,
                  /*disable_cert_network_fetches=*/false),
        params_(ClientSocketPool::SocketParams::CreateForHttpForTesting()),
        client_socket_factory_(NetLog::Get()) {
    std::unique_ptr<MockCertVerifier> cert_verifier =
        std::make_unique<MockCertVerifier>();
    cert_verifier->set_default_result(OK);
    session_deps_.cert_verifier = std::move(cert_verifier);

    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(&session_deps_);

    common_connect_job_params_ = std::make_unique<CommonConnectJobParams>(
        http_network_session_->CreateCommonConnectJobParams());
    common_connect_job_params_->client_socket_factory = &client_socket_factory_;
    pool_ = std::make_unique<TransportClientSocketPool>(
        kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout,
        ProxyChain::Direct(), /*is_for_websockets=*/false,
        common_connect_job_params_.get());

    tagging_common_connect_job_params_ =
        std::make_unique<CommonConnectJobParams>(
            http_network_session_->CreateCommonConnectJobParams());
    tagging_common_connect_job_params_->client_socket_factory =
        &tagging_client_socket_factory_;
    tagging_pool_ = std::make_unique<TransportClientSocketPool>(
        kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout,
        ProxyChain::Direct(), /*is_for_websockets=*/false,
        tagging_common_connect_job_params_.get());

    common_connect_job_params_for_real_sockets_ =
        std::make_unique<CommonConnectJobParams>(
            http_network_session_->CreateCommonConnectJobParams());
    common_connect_job_params_for_real_sockets_->client_socket_factory =
        ClientSocketFactory::GetDefaultFactory();
    pool_for_real_sockets_ = std::make_unique<TransportClientSocketPool>(
        kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout,
        ProxyChain::Direct(), /*is_for_websockets=*/false,
        common_connect_job_params_for_real_sockets_.get());
  }

  ~TransportClientSocketPoolTest() override {
    TransportClientSocketPool::set_connect_backup_jobs_enabled(
        connect_backup_jobs_enabled_);
  }

  int StartRequest(const std::string& host_name, RequestPriority priority) {
    ClientSocketPool::GroupId group_id(
        url::SchemeHostPort(url::kHttpScheme, host_name, 80),
        PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
        SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
    return test_base_.StartRequestUsingPool(
        pool_.get(), group_id, priority,
        ClientSocketPool::RespectLimits::ENABLED,
        ClientSocketPool::SocketParams::CreateForHttpForTesting());
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

  std::vector<std::unique_ptr<TestSocketRequest>>* requests() {
    return test_base_.requests();
  }
  size_t completion_count() const { return test_base_.completion_count(); }

  bool connect_backup_jobs_enabled_;

  // |group_id_| and |params_| correspond to the same group.
  const ClientSocketPool::GroupId group_id_;
  scoped_refptr<ClientSocketPool::SocketParams> params_;

  MockTransportClientSocketFactory client_socket_factory_;
  MockTaggingClientSocketFactory tagging_client_socket_factory_;

  // None of these tests check SPDY behavior, but this is a convenient way to
  // create most objects needed by the socket pools, as well as a SpdySession
  // pool, which is required by HttpProxyConnectJobs when using an HTTPS proxy.
  SpdySessionDependencies session_deps_;
  // As with |session_deps_|, this is a convenient way to construct objects
  // these tests depend on.
  std::unique_ptr<HttpNetworkSession> http_network_session_;

  std::unique_ptr<CommonConnectJobParams> common_connect_job_params_;
  std::unique_ptr<TransportClientSocketPool> pool_;

  // Just like |pool_|, except it uses a real MockTaggingClientSocketFactory
  // instead of MockTransportClientSocketFactory.
  std::unique_ptr<CommonConnectJobParams> tagging_common_connect_job_params_;
  std::unique_ptr<TransportClientSocketPool> tagging_pool_;

  // Just like |pool_|, except it uses a real ClientSocketFactory instead of
  // |client_socket_factory_|.
  std::unique_ptr<CommonConnectJobParams>
      common_connect_job_params_for_real_sockets_;
  std::unique_ptr<TransportClientSocketPool> pool_for_real_sockets_;

  ClientSocketPoolTest test_base_;
};

TEST_F(TransportClientSocketPoolTest, Basic) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfoConnectedNotReused(handle);
  EXPECT_EQ(0u, handle.connection_attempts().size());
}

// Make sure that TransportConnectJob passes on its priority to its
// HostResolver request on Init.
TEST_F(TransportClientSocketPoolTest, SetResolvePriorityOnInit) {
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
                    pool_.get(), NetLogWithSource()));
    EXPECT_EQ(priority, session_deps_.host_resolver->last_request_priority());
  }
}

TEST_F(TransportClientSocketPoolTest, SetSecureDnsPolicy) {
  for (auto secure_dns_policy :
       {SecureDnsPolicy::kAllow, SecureDnsPolicy::kDisable}) {
    TestCompletionCallback callback;
    ClientSocketHandle handle;
    ClientSocketPool::GroupId group_id(
        url::SchemeHostPort(url::kHttpScheme, "www.google.com", 80),
        PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
        secure_dns_policy, /*disable_cert_network_fetches=*/false);
    EXPECT_EQ(
        ERR_IO_PENDING,
        handle.Init(group_id, params_, std::nullopt /* proxy_annotation_tag */,
                    LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                    callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                    pool_.get(), NetLogWithSource()));
    EXPECT_EQ(secure_dns_policy,
              session_deps_.host_resolver->last_secure_dns_policy());
  }
}

TEST_F(TransportClientSocketPoolTest, ReprioritizeRequests) {
  session_deps_.host_resolver->set_ondemand_mode(true);

  TestCompletionCallback callback1;
  ClientSocketHandle handle1;
  int rv1 =
      handle1.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                   LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                   callback1.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv1, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback2;
  ClientSocketHandle handle2;
  int rv2 = handle2.Init(
      group_id_, params_, std::nullopt /* proxy_annotation_tag */, HIGHEST,
      SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
      callback2.callback(), ClientSocketPool::ProxyAuthCallback(), pool_.get(),
      NetLogWithSource());
  EXPECT_THAT(rv2, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback3;
  ClientSocketHandle handle3;
  int rv3 = handle3.Init(
      group_id_, params_, std::nullopt /* proxy_annotation_tag */, LOWEST,
      SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
      callback3.callback(), ClientSocketPool::ProxyAuthCallback(), pool_.get(),
      NetLogWithSource());
  EXPECT_THAT(rv3, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback4;
  ClientSocketHandle handle4;
  int rv4 = handle4.Init(
      group_id_, params_, std::nullopt /* proxy_annotation_tag */, MEDIUM,
      SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
      callback4.callback(), ClientSocketPool::ProxyAuthCallback(), pool_.get(),
      NetLogWithSource());
  EXPECT_THAT(rv4, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback5;
  ClientSocketHandle handle5;
  int rv5 = handle5.Init(
      group_id_, params_, std::nullopt /* proxy_annotation_tag */, HIGHEST,
      SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
      callback5.callback(), ClientSocketPool::ProxyAuthCallback(), pool_.get(),
      NetLogWithSource());
  EXPECT_THAT(rv5, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback6;
  ClientSocketHandle handle6;
  int rv6 =
      handle6.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                   LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                   callback6.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv6, IsError(ERR_IO_PENDING));

  // New jobs are created for each of the first 6 requests with the
  // corresponding priority.
  //
  // Queue of pending requests:
  // Request  Job  Priority
  // =======  ===  ========
  //    2      2   HIGHEST
  //    5      5   HIGHEST
  //    4      4   MEDIUM
  //    1      1   LOW
  //    6      6   LOW
  //    3      3   LOWEST
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(1));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(2));
  EXPECT_EQ(LOWEST, session_deps_.host_resolver->request_priority(3));
  EXPECT_EQ(MEDIUM, session_deps_.host_resolver->request_priority(4));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(5));
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(6));

  // Inserting a highest-priority request steals the job from the lowest
  // priority request and reprioritizes it to match the new request.
  TestCompletionCallback callback7;
  ClientSocketHandle handle7;
  int rv7 = handle7.Init(
      group_id_, params_, std::nullopt /* proxy_annotation_tag */, HIGHEST,
      SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
      callback7.callback(), ClientSocketPool::ProxyAuthCallback(), pool_.get(),
      NetLogWithSource());
  EXPECT_THAT(rv7, IsError(ERR_IO_PENDING));
  // Request  Job  Priority
  // =======  ===  ========
  //    2      2   HIGHEST
  //    5      5   HIGHEST
  //    7      3   HIGHEST
  //    4      4   MEDIUM
  //    1      1   LOW
  //    6      6   LOW
  //    3          LOWEST
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(1));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(2));
  EXPECT_EQ(HIGHEST,
            session_deps_.host_resolver->request_priority(3));  // reprioritized
  EXPECT_EQ(MEDIUM, session_deps_.host_resolver->request_priority(4));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(5));
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(6));

  TestCompletionCallback callback8;
  ClientSocketHandle handle8;
  int rv8 = handle8.Init(
      group_id_, params_, std::nullopt /* proxy_annotation_tag */, HIGHEST,
      SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
      callback8.callback(), ClientSocketPool::ProxyAuthCallback(), pool_.get(),
      NetLogWithSource());
  EXPECT_THAT(rv8, IsError(ERR_IO_PENDING));
  // Request  Job  Priority
  // =======  ===  ========
  //    2      2   HIGHEST
  //    5      5   HIGHEST
  //    7      3   HIGHEST
  //    8      6   HIGHEST
  //    4      4   MEDIUM
  //    1      1   LOW
  //    6          LOW
  //    3          LOWEST
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(1));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(2));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(3));
  EXPECT_EQ(MEDIUM, session_deps_.host_resolver->request_priority(4));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(5));
  EXPECT_EQ(HIGHEST,
            session_deps_.host_resolver->request_priority(6));  // reprioritized

  // A request completes, then the socket is returned to the socket pool and
  // goes to the highest remaining request. The job from the highest request
  // should then be reassigned to the first request without a job.
  session_deps_.host_resolver->ResolveNow(2);
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_TRUE(handle2.is_initialized());
  EXPECT_TRUE(handle2.socket());
  handle2.Reset();
  EXPECT_THAT(callback5.WaitForResult(), IsOk());
  EXPECT_TRUE(handle5.is_initialized());
  EXPECT_TRUE(handle5.socket());
  // Request  Job  Priority
  // =======  ===  ========
  //    7      3   HIGHEST
  //    8      6   HIGHEST
  //    4      4   MEDIUM
  //    1      1   LOW
  //    6      5   LOW
  //    3          LOWEST
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(1));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(3));
  EXPECT_EQ(MEDIUM, session_deps_.host_resolver->request_priority(4));
  EXPECT_EQ(LOW,
            session_deps_.host_resolver->request_priority(5));  // reprioritized
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(6));

  // Cancelling a request with a job reassigns the job to a lower request.
  handle7.Reset();
  // Request  Job  Priority
  // =======  ===  ========
  //    8      6   HIGHEST
  //    4      4   MEDIUM
  //    1      1   LOW
  //    6      5   LOW
  //    3      3   LOWEST
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(1));
  EXPECT_EQ(LOWEST,
            session_deps_.host_resolver->request_priority(3));  // reprioritized
  EXPECT_EQ(MEDIUM, session_deps_.host_resolver->request_priority(4));
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(5));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(6));

  // Reprioritizing a request changes its job's priority.
  pool_->SetPriority(group_id_, &handle4, LOWEST);
  // Request  Job  Priority
  // =======  ===  ========
  //    8      6   HIGHEST
  //    1      1   LOW
  //    6      5   LOW
  //    3      3   LOWEST
  //    4      4   LOWEST
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(1));
  EXPECT_EQ(LOWEST, session_deps_.host_resolver->request_priority(3));
  EXPECT_EQ(LOWEST,
            session_deps_.host_resolver->request_priority(4));  // reprioritized
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(5));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(6));

  pool_->SetPriority(group_id_, &handle3, MEDIUM);
  // Request  Job  Priority
  // =======  ===  ========
  //    8      6   HIGHEST
  //    3      3   MEDIUM
  //    1      1   LOW
  //    6      5   LOW
  //    4      4   LOWEST
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(1));
  EXPECT_EQ(MEDIUM,
            session_deps_.host_resolver->request_priority(3));  // reprioritized
  EXPECT_EQ(LOWEST, session_deps_.host_resolver->request_priority(4));
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(5));
  EXPECT_EQ(HIGHEST, session_deps_.host_resolver->request_priority(6));

  // Host resolution finishes for a lower-down request. The highest request
  // should get the socket and its job should be reassigned to the lower
  // request.
  session_deps_.host_resolver->ResolveNow(1);
  EXPECT_THAT(callback8.WaitForResult(), IsOk());
  EXPECT_TRUE(handle8.is_initialized());
  EXPECT_TRUE(handle8.socket());
  // Request  Job  Priority
  // =======  ===  ========
  //    3      3   MEDIUM
  //    1      6   LOW
  //    6      5   LOW
  //    4      4   LOWEST
  EXPECT_EQ(MEDIUM, session_deps_.host_resolver->request_priority(3));
  EXPECT_EQ(LOWEST, session_deps_.host_resolver->request_priority(4));
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(5));
  EXPECT_EQ(LOW,
            session_deps_.host_resolver->request_priority(6));  // reprioritized

  // Host resolution finishes for the highest request. Nothing gets
  // reprioritized.
  session_deps_.host_resolver->ResolveNow(3);
  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_TRUE(handle3.is_initialized());
  EXPECT_TRUE(handle3.socket());
  // Request  Job  Priority
  // =======  ===  ========
  //    1      6   LOW
  //    6      5   LOW
  //    4      4   LOWEST
  EXPECT_EQ(LOWEST, session_deps_.host_resolver->request_priority(4));
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(5));
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(6));

  session_deps_.host_resolver->ResolveAllPending();
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_TRUE(handle1.is_initialized());
  EXPECT_TRUE(handle1.socket());
  EXPECT_THAT(callback4.WaitForResult(), IsOk());
  EXPECT_TRUE(handle4.is_initialized());
  EXPECT_TRUE(handle4.socket());
  EXPECT_THAT(callback6.WaitForResult(), IsOk());
  EXPECT_TRUE(handle6.is_initialized());
  EXPECT_TRUE(handle6.socket());
}

TEST_F(TransportClientSocketPoolTest, RequestIgnoringLimitsIsReprioritized) {
  TransportClientSocketPool pool(
      kMaxSockets, 1, kUnusedIdleSocketTimeout, ProxyChain::Direct(),
      /*is_for_websockets=*/false, common_connect_job_params_.get());

  // Creates a job which ignores limits whose priority is MAXIMUM_PRIORITY.
  TestCompletionCallback callback1;
  ClientSocketHandle handle1;
  int rv1 = handle1.Init(
      group_id_, params_, std::nullopt /* proxy_annotation_tag */,
      MAXIMUM_PRIORITY, SocketTag(), ClientSocketPool::RespectLimits::DISABLED,
      callback1.callback(), ClientSocketPool::ProxyAuthCallback(), &pool,
      NetLogWithSource());
  EXPECT_THAT(rv1, IsError(ERR_IO_PENDING));

  EXPECT_EQ(MAXIMUM_PRIORITY, session_deps_.host_resolver->request_priority(1));

  TestCompletionCallback callback2;
  ClientSocketHandle handle2;
  int rv2 =
      handle2.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                   LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &pool, NetLogWithSource());
  EXPECT_THAT(rv2, IsError(ERR_IO_PENDING));

  // |handle2| gets assigned the job, which is reprioritized.
  handle1.Reset();
  EXPECT_EQ(LOW, session_deps_.host_resolver->request_priority(1));
}

TEST_F(TransportClientSocketPoolTest, InitHostResolutionFailure) {
  session_deps_.host_resolver->rules()->AddSimulatedTimeoutFailure(
      group_id_.destination().host());
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), pool_.get(),
                  NetLogWithSource()));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(handle.resolve_error_info().error, IsError(ERR_DNS_TIMED_OUT));
  ASSERT_EQ(1u, handle.connection_attempts().size());
  EXPECT_TRUE(handle.connection_attempts()[0].endpoint.address().empty());
  EXPECT_THAT(handle.connection_attempts()[0].result,
              IsError(ERR_NAME_NOT_RESOLVED));
}

TEST_F(TransportClientSocketPoolTest, InitConnectionFailure) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kFailing);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), pool_.get(),
                  NetLogWithSource()));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_FAILED));
  ASSERT_EQ(1u, handle.connection_attempts().size());
  EXPECT_EQ("127.0.0.1:80",
            handle.connection_attempts()[0].endpoint.ToString());
  EXPECT_THAT(handle.connection_attempts()[0].result,
              IsError(ERR_CONNECTION_FAILED));

  // Make the host resolutions complete synchronously this time.
  session_deps_.host_resolver->set_synchronous_mode(true);
  EXPECT_EQ(
      ERR_CONNECTION_FAILED,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), pool_.get(),
                  NetLogWithSource()));
  ASSERT_EQ(1u, handle.connection_attempts().size());
  EXPECT_EQ("127.0.0.1:80",
            handle.connection_attempts()[0].endpoint.ToString());
  EXPECT_THAT(handle.connection_attempts()[0].result,
              IsError(ERR_CONNECTION_FAILED));
}

TEST_F(TransportClientSocketPoolTest, PendingRequests) {
  // First request finishes asynchronously.
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT((*requests())[0]->WaitForResult(), IsOk());

  // Make all subsequent host resolutions complete synchronously.
  session_deps_.host_resolver->set_synchronous_mode(true);

  // Rest of them finish synchronously, until we reach the per-group limit.
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());

  // The rest are pending since we've used all active sockets.
  EXPECT_THAT(StartRequest("a", HIGHEST), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", LOWEST), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", LOWEST), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", MEDIUM), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", LOW), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", HIGHEST), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", LOWEST), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", MEDIUM), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", MEDIUM), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", HIGHEST), IsError(ERR_IO_PENDING));

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(kMaxSocketsPerGroup, client_socket_factory_.allocation_count());

  // One initial asynchronous request and then 10 pending requests.
  EXPECT_EQ(11U, completion_count());

  // First part of requests, all with the same priority, finishes in FIFO order.
  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));

  // Make sure that rest of the requests complete in the order of priority.
  EXPECT_EQ(7, GetOrderOfRequest(7));
  EXPECT_EQ(14, GetOrderOfRequest(8));
  EXPECT_EQ(15, GetOrderOfRequest(9));
  EXPECT_EQ(10, GetOrderOfRequest(10));
  EXPECT_EQ(13, GetOrderOfRequest(11));
  EXPECT_EQ(8, GetOrderOfRequest(12));
  EXPECT_EQ(16, GetOrderOfRequest(13));
  EXPECT_EQ(11, GetOrderOfRequest(14));
  EXPECT_EQ(12, GetOrderOfRequest(15));
  EXPECT_EQ(9, GetOrderOfRequest(16));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(17));
}

TEST_F(TransportClientSocketPoolTest, PendingRequests_NoKeepAlive) {
  // First request finishes asynchronously.
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT((*requests())[0]->WaitForResult(), IsOk());

  // Make all subsequent host resolutions complete synchronously.
  session_deps_.host_resolver->set_synchronous_mode(true);

  // Rest of them finish synchronously, until we reach the per-group limit.
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());

  // The rest are pending since we've used all active sockets.
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  // The pending requests should finish successfully.
  EXPECT_THAT((*requests())[6]->WaitForResult(), IsOk());
  EXPECT_THAT((*requests())[7]->WaitForResult(), IsOk());
  EXPECT_THAT((*requests())[8]->WaitForResult(), IsOk());
  EXPECT_THAT((*requests())[9]->WaitForResult(), IsOk());
  EXPECT_THAT((*requests())[10]->WaitForResult(), IsOk());

  EXPECT_EQ(static_cast<int>(requests()->size()),
            client_socket_factory_.allocation_count());

  // First asynchronous request, and then last 5 pending requests.
  EXPECT_EQ(6U, completion_count());
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending host resolution will eventually complete, and destroy the
// ClientSocketPool which will crash if the group was not cleared properly.
TEST_F(TransportClientSocketPoolTest, CancelRequestClearGroup) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), pool_.get(),
                  NetLogWithSource()));
  handle.Reset();
}

TEST_F(TransportClientSocketPoolTest, TwoRequestsCancelOne) {
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  ClientSocketHandle handle2;
  TestCompletionCallback callback2;

  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), pool_.get(),
                  NetLogWithSource()));
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle2.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                   kDefaultPriority, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_.get(), NetLogWithSource()));

  handle.Reset();

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  handle2.Reset();
}

TEST_F(TransportClientSocketPoolTest, ConnectCancelConnect) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kPending);
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), pool_.get(),
                  NetLogWithSource()));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(
      ERR_IO_PENDING,
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  kDefaultPriority, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED,
                  callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_.get(), NetLogWithSource()));

  session_deps_.host_resolver->set_synchronous_mode(true);
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

TEST_F(TransportClientSocketPoolTest, CancelRequest) {
  // First request finishes asynchronously.
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT((*requests())[0]->WaitForResult(), IsOk());

  // Make all subsequent host resolutions complete synchronously.
  session_deps_.host_resolver->set_synchronous_mode(true);

  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsOk());

  // Reached per-group limit, queue up requests.
  EXPECT_THAT(StartRequest("a", LOWEST), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", HIGHEST), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", HIGHEST), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", MEDIUM), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", MEDIUM), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", LOW), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", HIGHEST), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", LOW), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", LOW), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", LOWEST), IsError(ERR_IO_PENDING));

  // Cancel a request.
  size_t index_to_cancel = kMaxSocketsPerGroup + 2;
  EXPECT_FALSE((*requests())[index_to_cancel]->handle()->is_initialized());
  (*requests())[index_to_cancel]->handle()->Reset();

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(kMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests()->size() - kMaxSocketsPerGroup, completion_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));
  EXPECT_EQ(14, GetOrderOfRequest(7));
  EXPECT_EQ(7, GetOrderOfRequest(8));
  EXPECT_EQ(ClientSocketPoolTest::kRequestNotFound,
            GetOrderOfRequest(9));  // Canceled request.
  EXPECT_EQ(9, GetOrderOfRequest(10));
  EXPECT_EQ(10, GetOrderOfRequest(11));
  EXPECT_EQ(11, GetOrderOfRequest(12));
  EXPECT_EQ(8, GetOrderOfRequest(13));
  EXPECT_EQ(12, GetOrderOfRequest(14));
  EXPECT_EQ(13, GetOrderOfRequest(15));
  EXPECT_EQ(15, GetOrderOfRequest(16));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(17));
}

class RequestSocketCallback : public TestCompletionCallbackBase {
 public:
  RequestSocketCallback(
      const ClientSocketPool::GroupId& group_id,
      scoped_refptr<ClientSocketPool::SocketParams> socket_params,
      ClientSocketHandle* handle,
      TransportClientSocketPool* pool)
      : group_id_(group_id),
        socket_params_(socket_params),
        handle_(handle),
        pool_(pool) {}

  RequestSocketCallback(const RequestSocketCallback&) = delete;
  RequestSocketCallback& operator=(const RequestSocketCallback&) = delete;

  ~RequestSocketCallback() override = default;

  CompletionOnceCallback callback() {
    return base::BindOnce(&RequestSocketCallback::OnComplete,
                          base::Unretained(this));
  }

 private:
  void OnComplete(int result) {
    SetResult(result);
    ASSERT_THAT(result, IsOk());

    if (!within_callback_) {
      // Don't allow reuse of the socket.  Disconnect it and then release it and
      // run through the MessageLoop once to get it completely released.
      handle_->socket()->Disconnect();
      handle_->Reset();
      base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
      within_callback_ = true;
      int rv = handle_->Init(
          group_id_, socket_params_, std::nullopt /* proxy_annotation_tag */,
          LOWEST, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
          callback(), ClientSocketPool::ProxyAuthCallback(), pool_,
          NetLogWithSource());
      EXPECT_THAT(rv, IsOk());
    }
  }

  const ClientSocketPool::GroupId group_id_;
  scoped_refptr<ClientSocketPool::SocketParams> socket_params_;
  const raw_ptr<ClientSocketHandle> handle_;
  const raw_ptr<TransportClientSocketPool> pool_;
  bool within_callback_ = false;
};

TEST_F(TransportClientSocketPoolTest, RequestTwice) {
  ClientSocketHandle handle;
  RequestSocketCallback callback(group_id_, params_, &handle, pool_.get());
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOWEST, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_.get(), NetLogWithSource());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // The callback is going to request "www.google.com". We want it to complete
  // synchronously this time.
  session_deps_.host_resolver->set_synchronous_mode(true);

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(TransportClientSocketPoolTest, CancelActiveRequestWithPendingRequests) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kPending);

  // Queue up all the requests
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));
  EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));

  // Now, kMaxSocketsPerGroup requests should be active.  Let's cancel them.
  ASSERT_LE(kMaxSocketsPerGroup, static_cast<int>(requests()->size()));
  for (int i = 0; i < kMaxSocketsPerGroup; i++)
    (*requests())[i]->handle()->Reset();

  // Let's wait for the rest to complete now.
  for (size_t i = kMaxSocketsPerGroup; i < requests()->size(); ++i) {
    EXPECT_THAT((*requests())[i]->WaitForResult(), IsOk());
    (*requests())[i]->handle()->Reset();
  }

  EXPECT_EQ(requests()->size() - kMaxSocketsPerGroup, completion_count());
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(TransportClientSocketPoolTest, FailingActiveRequestWithPendingRequests) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kPendingFailing);

  const int kNumRequests = 2 * kMaxSocketsPerGroup + 1;
  ASSERT_LE(kNumRequests, kMaxSockets);  // Otherwise the test will hang.

  // Queue up all the requests
  for (int i = 0; i < kNumRequests; i++)
    EXPECT_THAT(StartRequest("a", kDefaultPriority), IsError(ERR_IO_PENDING));

  for (int i = 0; i < kNumRequests; i++)
    EXPECT_THAT((*requests())[i]->WaitForResult(),
                IsError(ERR_CONNECTION_FAILED));
}

TEST_F(TransportClientSocketPoolTest, IdleSocketLoadTiming) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfoConnectedNotReused(handle);

  handle.Reset();
  // Need to run all pending to release the socket back to the pool.
  base::RunLoop().RunUntilIdle();

  // Now we should have 1 idle socket.
  EXPECT_EQ(1, pool_->IdleSocketCount());

  rv = handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                   LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ(0, pool_->IdleSocketCount());
  TestLoadTimingInfoConnectedReused(handle);
}

TEST_F(TransportClientSocketPoolTest, CloseIdleSocketsOnIPAddressChange) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  handle.Reset();

  // Need to run all pending to release the socket back to the pool.
  base::RunLoop().RunUntilIdle();

  // Now we should have 1 idle socket.
  EXPECT_EQ(1, pool_->IdleSocketCount());

  // After an IP address change, we should have 0 idle sockets.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::RunLoop().RunUntilIdle();  // Notification happens async.

  EXPECT_EQ(0, pool_->IdleSocketCount());
}

TEST(TransportClientSocketPoolStandaloneTest, DontCleanupOnIPAddressChange) {
  // This test manually sets things up in the same way
  // TransportClientSocketPoolTest does, but it creates a
  // TransportClientSocketPool with cleanup_on_ip_address_changed = false. Since
  // this is the only test doing this, it's not worth extending
  // TransportClientSocketPoolTest to support this scenario.
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<MockCertVerifier> cert_verifier =
      std::make_unique<MockCertVerifier>();
  SpdySessionDependencies session_deps;
  session_deps.cert_verifier = std::move(cert_verifier);
  std::unique_ptr<HttpNetworkSession> http_network_session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);
  auto common_connect_job_params = std::make_unique<CommonConnectJobParams>(
      http_network_session->CreateCommonConnectJobParams());
  MockTransportClientSocketFactory client_socket_factory(NetLog::Get());
  common_connect_job_params->client_socket_factory = &client_socket_factory;

  scoped_refptr<ClientSocketPool::SocketParams> params(
      ClientSocketPool::SocketParams::CreateForHttpForTesting());
  auto pool = std::make_unique<TransportClientSocketPool>(
      kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout,
      ProxyChain::Direct(), /*is_for_websockets=*/false,
      common_connect_job_params.get(),
      /*cleanup_on_ip_address_change=*/false);
  const ClientSocketPool::GroupId group_id(
      url::SchemeHostPort(url::kHttpScheme, "www.google.com", 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id, params, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  handle.Reset();
  // Need to run all pending to release the socket back to the pool.
  base::RunLoop().RunUntilIdle();
  // Now we should have 1 idle socket.
  EXPECT_EQ(1, pool->IdleSocketCount());

  // Since we set cleanup_on_ip_address_change = false, we should still have 1
  // idle socket after an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::RunLoop().RunUntilIdle();  // Notification happens async.
  EXPECT_EQ(1, pool->IdleSocketCount());
}

TEST_F(TransportClientSocketPoolTest, SSLCertError) {
  StaticSocketDataProvider data;
  tagging_client_socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_COMMON_NAME_INVALID);
  tagging_client_socket_factory_.AddSSLSocketDataProvider(&ssl);

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "ssl.server.test",
                                      443);

  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      base::MakeRefCounted<ClientSocketPool::SocketParams>(
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv =
      handle.Init(ClientSocketPool::GroupId(
                      kEndpoint, PrivacyMode::PRIVACY_MODE_DISABLED,
                      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                      /*disable_cert_network_fetches=*/false),
                  socket_params, std::nullopt /* proxy_annotation_tag */,
                  MEDIUM, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  tagging_pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CERT_COMMON_NAME_INVALID));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

namespace {
class TransportClientSocketPoolSSLConfigChangeTest
    : public TransportClientSocketPoolTest,
      public ::testing::WithParamInterface<
          SSLClientContext::SSLConfigChangeType> {
 public:
  void SimulateChange() {
    switch (GetParam()) {
      case SSLClientContext::SSLConfigChangeType::kSSLConfigChanged:
        session_deps_.ssl_config_service->NotifySSLContextConfigChange();
        break;
      case SSLClientContext::SSLConfigChangeType::kCertDatabaseChanged:
        // TODO(mattm): For more realistic testing this should call
        // `CertDatabase::GetInstance()->NotifyObserversCertDBChanged()`,
        // however that delivers notifications asynchronously, and running
        // the message loop to allow the notification to be delivered allows
        // other parts of the tested code to advance, breaking the test
        // expectations.
        pool_->OnSSLConfigChanged(GetParam());
        break;
      case SSLClientContext::SSLConfigChangeType::kCertVerifierChanged:
        session_deps_.cert_verifier->SimulateOnCertVerifierChanged();
        break;
    }
  }

  const char* ExpectedMessage() {
    switch (GetParam()) {
      case SSLClientContext::SSLConfigChangeType::kSSLConfigChanged:
        return TransportClientSocketPool::kNetworkChanged;
      case SSLClientContext::SSLConfigChangeType::kCertDatabaseChanged:
        return TransportClientSocketPool::kCertDatabaseChanged;
      case SSLClientContext::SSLConfigChangeType::kCertVerifierChanged:
        return TransportClientSocketPool::kCertVerifierChanged;
    }
  }
};
}  // namespace

TEST_P(TransportClientSocketPoolSSLConfigChangeTest, GracefulConfigChange) {
  // Create a request and finish connection of the socket, and release the
  // handle.
  {
    TestCompletionCallback callback;
    ClientSocketHandle handle1;
    int rv =
        handle1.Init(group_id_, params_, /*proxy_annotation_tag=*/std::nullopt,
                     LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                     callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                     pool_.get(), NetLogWithSource());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    EXPECT_FALSE(handle1.is_initialized());
    EXPECT_FALSE(handle1.socket());

    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_TRUE(handle1.is_initialized());
    EXPECT_TRUE(handle1.socket());
    EXPECT_EQ(0, handle1.group_generation());
    EXPECT_EQ(0, pool_->IdleSocketCount());

    handle1.Reset();
  }

  // Need to run all pending to release the socket back to the pool.
  base::RunLoop().RunUntilIdle();

  // Now we should have 1 idle socket.
  EXPECT_EQ(1, pool_->IdleSocketCount());

  // Create another request and finish connection of the socket, but hold on to
  // the handle until later in the test.
  ClientSocketHandle handle2;
  {
    ClientSocketPool::GroupId group_id2(
        url::SchemeHostPort(url::kHttpScheme, "bar.example.com", 80),
        PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
        SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
    TestCompletionCallback callback;
    int rv =
        handle2.Init(group_id2, params_, /*proxy_annotation_tag=*/std::nullopt,
                     LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                     callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                     pool_.get(), NetLogWithSource());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    EXPECT_FALSE(handle2.is_initialized());
    EXPECT_FALSE(handle2.socket());

    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_TRUE(handle2.is_initialized());
    EXPECT_TRUE(handle2.socket());
    EXPECT_EQ(0, handle2.group_generation());
  }

  // Still only have 1 idle socket since handle2 is still alive.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, pool_->IdleSocketCount());

  // Create a pending request but don't finish connection.
  ClientSocketPool::GroupId group_id3(
      url::SchemeHostPort(url::kHttpScheme, "foo.example.com", 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  TestCompletionCallback callback3;
  ClientSocketHandle handle3;
  int rv =
      handle3.Init(group_id3, params_, /*proxy_annotation_tag=*/std::nullopt,
                   LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                   callback3.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle3.is_initialized());
  EXPECT_FALSE(handle3.socket());

  // Do a configuration change.
  RecordingNetLogObserver net_log_observer;
  SimulateChange();

  // Allow handle3 to advance.
  base::RunLoop().RunUntilIdle();
  // After a configuration change, we should have 0 idle sockets. The first
  // idle socket should have been closed, and handle2 and handle3 are still
  // alive.
  EXPECT_EQ(0, pool_->IdleSocketCount());

  // Verify the netlog messages recorded the correct reason for closing the
  // idle sockets.
  auto events = net_log_observer.GetEntriesWithType(
      NetLogEventType::SOCKET_POOL_CLOSING_SOCKET);
  ASSERT_EQ(events.size(), 1u);
  std::string* reason = events[0].params.FindString("reason");
  ASSERT_TRUE(reason);
  EXPECT_EQ(*reason, ExpectedMessage());

  // The pending request for handle3 should have succeeded under the new
  // generation since it didn't start until after the change.
  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_TRUE(handle3.is_initialized());
  EXPECT_TRUE(handle3.socket());
  EXPECT_EQ(1, handle3.group_generation());

  // After releasing handle2, it does not become an idle socket since it was
  // part of the first generation.
  handle2.Reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, pool_->IdleSocketCount());

  // After releasing handle3, there is now one idle socket, since that socket
  // was connected during the new generation.
  handle3.Reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, pool_->IdleSocketCount());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TransportClientSocketPoolSSLConfigChangeTest,
    testing::Values(
        SSLClientContext::SSLConfigChangeType::kSSLConfigChanged,
        SSLClientContext::SSLConfigChangeType::kCertDatabaseChanged,
        SSLClientContext::SSLConfigChangeType::kCertVerifierChanged));

TEST_F(TransportClientSocketPoolTest, BackupSocketConnect) {
  // Case 1 tests the first socket stalling, and the backup connecting.
  MockTransportClientSocketFactory::Rule rules1[] = {
      // The first socket will not connect.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalled),
      // The second socket will connect more quickly.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kSynchronous),
  };

  // Case 2 tests the first socket being slow, so that we start the
  // second connect, but the second connect stalls, and we still
  // complete the first.
  MockTransportClientSocketFactory::Rule rules2[] = {
      // The first socket will connect, although delayed.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kDelayed),
      // The second socket will not connect.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalled),
  };

  base::span<const MockTransportClientSocketFactory::Rule> cases[2] = {rules1,
                                                                       rules2};

  for (auto rules : cases) {
    client_socket_factory_.SetRules(rules);

    EXPECT_EQ(0, pool_->IdleSocketCount());

    TestCompletionCallback callback;
    ClientSocketHandle handle;
    int rv =
        handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                    LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                    callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                    pool_.get(), NetLogWithSource());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    EXPECT_FALSE(handle.is_initialized());
    EXPECT_FALSE(handle.socket());

    // Create the first socket, set the timer.
    base::RunLoop().RunUntilIdle();

    // Wait for the backup socket timer to fire.
    base::PlatformThread::Sleep(
        base::Milliseconds(ClientSocketPool::kMaxConnectRetryIntervalMs + 50));

    // Let the appropriate socket connect.
    base::RunLoop().RunUntilIdle();

    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_TRUE(handle.is_initialized());
    EXPECT_TRUE(handle.socket());

    // One socket is stalled, the other is active.
    EXPECT_EQ(0, pool_->IdleSocketCount());
    handle.Reset();

    // Close all pending connect jobs and existing sockets.
    pool_->FlushWithError(ERR_NETWORK_CHANGED, "Network changed");
  }
}

// Test the case where a socket took long enough to start the creation
// of the backup socket, but then we cancelled the request after that.
TEST_F(TransportClientSocketPoolTest, BackupSocketCancel) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::Type::kStalled);

  enum { CANCEL_BEFORE_WAIT, CANCEL_AFTER_WAIT };

  for (int index = CANCEL_BEFORE_WAIT; index < CANCEL_AFTER_WAIT; ++index) {
    EXPECT_EQ(0, pool_->IdleSocketCount());

    TestCompletionCallback callback;
    ClientSocketHandle handle;
    int rv =
        handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                    LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                    callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                    pool_.get(), NetLogWithSource());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    EXPECT_FALSE(handle.is_initialized());
    EXPECT_FALSE(handle.socket());

    // Create the first socket, set the timer.
    base::RunLoop().RunUntilIdle();

    if (index == CANCEL_AFTER_WAIT) {
      // Wait for the backup socket timer to fire.
      base::PlatformThread::Sleep(
          base::Milliseconds(ClientSocketPool::kMaxConnectRetryIntervalMs));
    }

    // Let the appropriate socket connect.
    base::RunLoop().RunUntilIdle();

    handle.Reset();

    EXPECT_FALSE(callback.have_result());
    EXPECT_FALSE(handle.is_initialized());
    EXPECT_FALSE(handle.socket());

    // One socket is stalled, the other is active.
    EXPECT_EQ(0, pool_->IdleSocketCount());
  }
}

// Test the case where a socket took long enough to start the creation
// of the backup socket and never completes, and then the backup
// connection fails.
TEST_F(TransportClientSocketPoolTest, BackupSocketFailAfterStall) {
  MockTransportClientSocketFactory::Rule rules[] = {
      // The first socket will not connect.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kStalled),
      // The second socket will fail immediately.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing),
  };

  client_socket_factory_.SetRules(rules);

  EXPECT_EQ(0, pool_->IdleSocketCount());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  // Create the first socket, set the timer.
  base::RunLoop().RunUntilIdle();

  // Wait for the backup socket timer to fire.
  base::PlatformThread::Sleep(
      base::Milliseconds(ClientSocketPool::kMaxConnectRetryIntervalMs));

  // Let the second connect be synchronous. Otherwise, the emulated
  // host resolution takes an extra trip through the message loop.
  session_deps_.host_resolver->set_synchronous_mode(true);

  // Let the appropriate socket connect.
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_FAILED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  ASSERT_EQ(1u, handle.connection_attempts().size());
  EXPECT_THAT(handle.connection_attempts()[0].result,
              IsError(ERR_CONNECTION_FAILED));
  EXPECT_EQ(0, pool_->IdleSocketCount());
  handle.Reset();
}

// Test the case where a socket took long enough to start the creation
// of the backup socket and eventually completes, but the backup socket
// fails.
TEST_F(TransportClientSocketPoolTest, BackupSocketFailAfterDelay) {
  MockTransportClientSocketFactory::Rule rules[] = {
      // The first socket will connect, although delayed.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kDelayed),
      // The second socket will not connect.
      MockTransportClientSocketFactory::Rule(
          MockTransportClientSocketFactory::Type::kFailing),
  };

  client_socket_factory_.SetRules(rules);
  client_socket_factory_.set_delay(base::Seconds(5));

  EXPECT_EQ(0, pool_->IdleSocketCount());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv =
      handle.Init(group_id_, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  // Create the first socket, set the timer.
  base::RunLoop().RunUntilIdle();

  // Wait for the backup socket timer to fire.
  base::PlatformThread::Sleep(
      base::Milliseconds(ClientSocketPool::kMaxConnectRetryIntervalMs));

  // Let the second connect be synchronous. Otherwise, the emulated
  // host resolution takes an extra trip through the message loop.
  session_deps_.host_resolver->set_synchronous_mode(true);

  // Let the appropriate socket connect.
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_FAILED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  ASSERT_EQ(1u, handle.connection_attempts().size());
  EXPECT_THAT(handle.connection_attempts()[0].result,
              IsError(ERR_CONNECTION_FAILED));
  handle.Reset();
}

// Test the case that SOCKSSocketParams are provided.
TEST_F(TransportClientSocketPoolTest, SOCKS) {
  const url::SchemeHostPort kDestination(url::kHttpScheme, "host", 80);

  TransportClientSocketPool proxy_pool(
      kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout,
      ProxyUriToProxyChain("socks5://foopy",
                           /*default_scheme=*/ProxyServer::SCHEME_HTTP),
      /*is_for_websockets=*/false, tagging_common_connect_job_params_.get());

  for (IoMode socket_io_mode : {SYNCHRONOUS, ASYNC}) {
    scoped_refptr<ClientSocketPool::SocketParams> socket_params =
        ClientSocketPool::SocketParams::CreateForHttpForTesting();

    SOCKS5MockData data(socket_io_mode);
    data.data_provider()->set_connect_data(MockConnect(socket_io_mode, OK));
    tagging_client_socket_factory_.AddSocketDataProvider(data.data_provider());
    ClientSocketHandle handle;
    TestCompletionCallback callback;
    int rv = handle.Init(
        ClientSocketPool::GroupId(
            kDestination, PrivacyMode::PRIVACY_MODE_DISABLED,
            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
            /*disable_cert_network_fetches=*/false),
        socket_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
        ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
        ClientSocketPool::ProxyAuthCallback(), &proxy_pool, NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());
    EXPECT_TRUE(handle.is_initialized());
    EXPECT_TRUE(handle.socket());
    EXPECT_TRUE(data.data_provider()->AllReadDataConsumed());
    EXPECT_TRUE(data.data_provider()->AllWriteDataConsumed());
  }
}

// Make sure there's no crash when an auth challenge is received over HTTP2
// and there are two pending Requests to the socket pool, with a single
// ConnectJob.
//
// See https://crbug.com/940848
TEST_F(TransportClientSocketPoolTest, SpdyOneConnectJobTwoRequestsError) {
  const url::SchemeHostPort kEndpoint(url::kHttpsScheme,
                                      "unresolvable.host.name", 443);

  session_deps_.host_resolver->set_synchronous_mode(true);

  // Create a socket pool which only allows a single connection at a time.
  TransportClientSocketPool pool(
      1, 1, kUnusedIdleSocketTimeout,
      ProxyUriToProxyChain("https://unresolvable.proxy.name",
                           /*default_scheme=*/ProxyServer::SCHEME_HTTP),
      /*is_for_websockets=*/false, tagging_common_connect_job_params_.get());

  // First connection attempt will get an error after creating the SpdyStream.

  SpdyTestUtil spdy_util;
  spdy::SpdySerializedFrame connect(spdy_util.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair::FromSchemeHostPort(kEndpoint)));

  MockWrite writes[] = {
      CreateMockWrite(connect, 0, ASYNC),
      MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  MockRead reads[] = {
      MockRead(ASYNC, ERR_FAILED, 1),
  };

  SequencedSocketData socket_data(MockConnect(SYNCHRONOUS, OK), reads, writes);
  tagging_client_socket_factory_.AddSocketDataProvider(&socket_data);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  ssl_data.next_proto = kProtoHTTP2;
  tagging_client_socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  // Second connection also fails.  Not a vital part of this test, but allows
  // waiting for the second request to complete without too much extra code.
  SequencedSocketData socket_data2(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_TIMED_OUT),
      base::span<const MockRead>(), base::span<const MockWrite>());
  tagging_client_socket_factory_.AddSocketDataProvider(&socket_data2);
  SSLSocketDataProvider ssl_data2(SYNCHRONOUS, OK);
  tagging_client_socket_factory_.AddSSLSocketDataProvider(&ssl_data2);

  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      base::MakeRefCounted<ClientSocketPool::SocketParams>(
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());

  ClientSocketPool::GroupId group_id(
      kEndpoint, PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);

  // Start the first connection attempt.
  TestCompletionCallback callback1;
  ClientSocketHandle handle1;
  int rv1 = handle1.Init(
      group_id, socket_params, TRAFFIC_ANNOTATION_FOR_TESTS, HIGHEST,
      SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
      callback1.callback(), ClientSocketPool::ProxyAuthCallback(), &pool,
      NetLogWithSource());
  ASSERT_THAT(rv1, IsError(ERR_IO_PENDING));

  // Create a second request with a lower priority.
  TestCompletionCallback callback2;
  ClientSocketHandle handle2;
  int rv2 = handle2.Init(
      group_id, socket_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOWEST,
      SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
      callback2.callback(), ClientSocketPool::ProxyAuthCallback(), &pool,
      NetLogWithSource());
  ASSERT_THAT(rv2, IsError(ERR_IO_PENDING));

  // First connection fails after creating a SpdySession and a SpdyStream on
  // that session. The SpdyStream will be destroyed under the
  // SpdyProxyClientSocket. The failure will result in temporarily assigning the
  // failed ConnectJob to the second request, which results in an unneeded
  // reprioritization, which should not dereference the null SpdyStream.
  //
  // TODO(mmenke): Avoid that temporary reassignment.
  ASSERT_THAT(callback1.WaitForResult(), IsError(ERR_FAILED));

  // Second connection fails, getting a connection error.
  ASSERT_THAT(callback2.WaitForResult(), IsError(ERR_PROXY_CONNECTION_FAILED));
}

// Make sure there's no crash when an auth challenge is received over HTTP2
// and there are two pending Requests to the socket pool, with a single
// ConnectJob.
//
// See https://crbug.com/940848
TEST_F(TransportClientSocketPoolTest, SpdyAuthOneConnectJobTwoRequests) {
  const url::SchemeHostPort kEndpoint(url::kHttpsScheme,
                                      "unresolvable.host.name", 443);
  const HostPortPair kProxy("unresolvable.proxy.name", 443);

  session_deps_.host_resolver->set_synchronous_mode(true);

  // Create a socket pool which only allows a single connection at a time.
  TransportClientSocketPool pool(
      1, 1, kUnusedIdleSocketTimeout,
      ProxyUriToProxyChain("https://unresolvable.proxy.name",
                           /*default_scheme=*/ProxyServer::SCHEME_HTTP),
      /*is_for_websockets=*/false, tagging_common_connect_job_params_.get());

  SpdyTestUtil spdy_util;
  spdy::SpdySerializedFrame connect(spdy_util.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair::FromSchemeHostPort(kEndpoint)));

  MockWrite writes[] = {
      CreateMockWrite(connect, 0, ASYNC),
      MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  // The proxy responds to the connect with a 407, and them an
  // ERROR_CODE_HTTP_1_1_REQUIRED.

  const char kAuthStatus[] = "407";
  const char* const kAuthChallenge[] = {
      "proxy-authenticate",
      "NTLM",
  };
  spdy::SpdySerializedFrame connect_auth_resp(spdy_util.ConstructSpdyReplyError(
      kAuthStatus, kAuthChallenge, std::size(kAuthChallenge) / 2, 1));
  spdy::SpdySerializedFrame reset(
      spdy_util.ConstructSpdyRstStream(1, spdy::ERROR_CODE_HTTP_1_1_REQUIRED));
  MockRead reads[] = {
      CreateMockRead(connect_auth_resp, 1, ASYNC),
      CreateMockRead(reset, 2, SYNCHRONOUS),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 3),
  };

  SequencedSocketData socket_data(MockConnect(SYNCHRONOUS, OK), reads, writes);
  tagging_client_socket_factory_.AddSocketDataProvider(&socket_data);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  ssl_data.next_proto = kProtoHTTP2;
  tagging_client_socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  // Second connection fails, and gets a different error.  Not a vital part of
  // this test, but allows waiting for the second request to complete without
  // too much extra code.
  SequencedSocketData socket_data2(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_TIMED_OUT),
      base::span<const MockRead>(), base::span<const MockWrite>());
  tagging_client_socket_factory_.AddSocketDataProvider(&socket_data2);
  SSLSocketDataProvider ssl_data2(SYNCHRONOUS, OK);
  tagging_client_socket_factory_.AddSSLSocketDataProvider(&ssl_data2);

  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      base::MakeRefCounted<ClientSocketPool::SocketParams>(
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());

  ClientSocketPool::GroupId group_id(
      kEndpoint, PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);

  // Start the first connection attempt.
  TestCompletionCallback callback1;
  ClientSocketHandle handle1;
  base::RunLoop run_loop;
  int rv1 = handle1.Init(group_id, socket_params, TRAFFIC_ANNOTATION_FOR_TESTS,
                         HIGHEST, SocketTag(),
                         ClientSocketPool::RespectLimits::ENABLED,
                         callback1.callback(),
                         base::BindLambdaForTesting(
                             [&](const HttpResponseInfo& response,
                                 HttpAuthController* auth_controller,
                                 base::OnceClosure restart_with_auth_callback) {
                               run_loop.Quit();
                             }),
                         &pool, NetLogWithSource());
  ASSERT_THAT(rv1, IsError(ERR_IO_PENDING));

  // Create a second request with a lower priority.
  TestCompletionCallback callback2;
  ClientSocketHandle handle2;
  int rv2 = handle2.Init(
      group_id, socket_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOWEST,
      SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
      callback2.callback(), ClientSocketPool::ProxyAuthCallback(), &pool,
      NetLogWithSource());
  ASSERT_THAT(rv2, IsError(ERR_IO_PENDING));

  // The ConnectJob connection sees the auth challenge and HTTP2 error, which
  // causes the SpdySession to be destroyed, as well as the SpdyStream. Then the
  // ConnectJob is bound to the first request. Binding the request will result
  // in temporarily assigning the ConnectJob to the second request, which
  // results in an unneeded reprioritization, which should not dereference the
  // null SpdyStream.
  //
  // TODO(mmenke): Avoid that temporary reassignment.
  run_loop.Run();

  // Just tear down everything without continuing - there are other tests for
  // auth over HTTP2.
}

TEST_F(TransportClientSocketPoolTest, HttpTunnelSetupRedirect) {
  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "host.test", 443);

  const std::string kRedirectTarget = "https://some.other.host.test/";

  const std::string kResponseText =
      "HTTP/1.1 302 Found\r\n"
      "Location: " +
      kRedirectTarget +
      "\r\n"
      "Set-Cookie: foo=bar\r\n"
      "\r\n";

  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);

    for (bool use_https_proxy : {false, true}) {
      SCOPED_TRACE(use_https_proxy);

      TransportClientSocketPool proxy_pool(
          kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout,
          ProxyUriToProxyChain(
              use_https_proxy ? "https://proxy.test" : "http://proxy.test",
              /*default_scheme=*/ProxyServer::SCHEME_HTTP),
          /*is_for_websockets=*/false,
          tagging_common_connect_job_params_.get());

      MockWrite writes[] = {
          MockWrite(ASYNC, 0,
                    "CONNECT host.test:443 HTTP/1.1\r\n"
                    "Host: host.test:443\r\n"
                    "Proxy-Connection: keep-alive\r\n"
                    "User-Agent: test-ua\r\n\r\n"),
      };
      MockRead reads[] = {
          MockRead(ASYNC, 1, kResponseText.c_str()),
      };

      SequencedSocketData data(reads, writes);
      tagging_client_socket_factory_.AddSocketDataProvider(&data);
      SSLSocketDataProvider ssl(ASYNC, OK);
      tagging_client_socket_factory_.AddSSLSocketDataProvider(&ssl);

      ClientSocketHandle handle;
      TestCompletionCallback callback;

      scoped_refptr<ClientSocketPool::SocketParams> socket_params =
          base::MakeRefCounted<ClientSocketPool::SocketParams>(
              /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());

      int rv = handle.Init(
          ClientSocketPool::GroupId(
              kEndpoint, PrivacyMode::PRIVACY_MODE_DISABLED,
              NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
              /*disable_cert_network_fetches=*/false),
          socket_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
          ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
          ClientSocketPool::ProxyAuthCallback(), &proxy_pool,
          NetLogWithSource());
      rv = callback.GetResult(rv);

      // We don't trust 302 responses to CONNECT.
      EXPECT_THAT(rv, IsError(ERR_TUNNEL_CONNECTION_FAILED));
      EXPECT_FALSE(handle.is_initialized());
    }
  }
}

TEST_F(TransportClientSocketPoolTest, NetworkAnonymizationKey) {
  const SchemefulSite kSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);
  const char kHost[] = "bar.test";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  session_deps_.host_resolver->set_ondemand_mode(true);

  TransportClientSocketPool::GroupId group_id(
      url::SchemeHostPort(url::kHttpScheme, kHost, 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_THAT(
      handle.Init(group_id,
                  ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                  TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
                  ClientSocketPool::ProxyAuthCallback(), pool_.get(),
                  NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, session_deps_.host_resolver->last_id());
  EXPECT_EQ(kHost, session_deps_.host_resolver->request_host(1));
  EXPECT_EQ(kNetworkAnonymizationKey,
            session_deps_.host_resolver->request_network_anonymization_key(1));
}

TEST_F(TransportClientSocketPoolTest, NetworkAnonymizationKeySsl) {
  const SchemefulSite kSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);
  const char kHost[] = "bar.test";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  session_deps_.host_resolver->set_ondemand_mode(true);

  TransportClientSocketPool::GroupId group_id(
      url::SchemeHostPort(url::kHttpsScheme, kHost, 443),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_THAT(
      handle.Init(
          group_id,
          base::MakeRefCounted<ClientSocketPool::SocketParams>(
              /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>()),
          TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
          ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
          ClientSocketPool::ProxyAuthCallback(), pool_.get(),
          NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  ASSERT_EQ(1u, session_deps_.host_resolver->last_id());
  EXPECT_EQ(kHost, session_deps_.host_resolver->request_host(1));
  EXPECT_EQ(kNetworkAnonymizationKey,
            session_deps_.host_resolver->request_network_anonymization_key(1));
}

// Test that, in the case of an HTTP proxy, the same transient
// NetworkAnonymizationKey is reused for resolving the proxy's host, regardless
// of input NAK.
TEST_F(TransportClientSocketPoolTest, NetworkAnonymizationKeyHttpProxy) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);
  const char kHost[] = "bar.test";
  const ProxyChain kProxyChain = ProxyUriToProxyChain(
      "http://proxy.test", /*default_scheme=*/ProxyServer::SCHEME_HTTP);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  session_deps_.host_resolver->set_ondemand_mode(true);

  TransportClientSocketPool proxy_pool(
      kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout, kProxyChain,
      /*is_for_websockets=*/false, tagging_common_connect_job_params_.get());

  TransportClientSocketPool::GroupId group_id1(
      url::SchemeHostPort(url::kHttpScheme, kHost, 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey1,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle1;
  TestCompletionCallback callback1;
  EXPECT_THAT(
      handle1.Init(group_id1,
                   ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback1.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &proxy_pool, NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  TransportClientSocketPool::GroupId group_id2(
      url::SchemeHostPort(url::kHttpScheme, kHost, 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey2,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle2;
  TestCompletionCallback callback2;
  EXPECT_THAT(
      handle2.Init(group_id2,
                   ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback1.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &proxy_pool, NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  ASSERT_EQ(2u, session_deps_.host_resolver->last_id());
  EXPECT_EQ(kProxyChain.First().host_port_pair().host(),
            session_deps_.host_resolver->request_host(1));
  EXPECT_EQ(kProxyChain.First().host_port_pair().host(),
            session_deps_.host_resolver->request_host(2));
  EXPECT_TRUE(session_deps_.host_resolver->request_network_anonymization_key(1)
                  .IsTransient());
  EXPECT_EQ(session_deps_.host_resolver->request_network_anonymization_key(1),
            session_deps_.host_resolver->request_network_anonymization_key(2));
}

// Test that, in the case of an HTTPS proxy, the same transient
// NetworkAnonymizationKey is reused for resolving the proxy's host, regardless
// of input NAK.
TEST_F(TransportClientSocketPoolTest, NetworkAnonymizationKeyHttpsProxy) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);
  const char kHost[] = "bar.test";
  const ProxyChain kProxyChain = ProxyUriToProxyChain(
      "https://proxy.test", /*default_scheme=*/ProxyServer::SCHEME_HTTP);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  session_deps_.host_resolver->set_ondemand_mode(true);

  TransportClientSocketPool proxy_pool(
      kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout, kProxyChain,
      false /* is_for_websockets */, tagging_common_connect_job_params_.get());

  TransportClientSocketPool::GroupId group_id1(
      url::SchemeHostPort(url::kHttpScheme, kHost, 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey1,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle1;
  TestCompletionCallback callback1;
  EXPECT_THAT(
      handle1.Init(group_id1,
                   ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback1.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &proxy_pool, NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  TransportClientSocketPool::GroupId group_id2(
      url::SchemeHostPort(url::kHttpScheme, kHost, 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey2,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle2;
  TestCompletionCallback callback2;
  EXPECT_THAT(
      handle2.Init(group_id2,
                   ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &proxy_pool, NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  ASSERT_EQ(2u, session_deps_.host_resolver->last_id());
  EXPECT_EQ(kProxyChain.First().host_port_pair().host(),
            session_deps_.host_resolver->request_host(1));
  EXPECT_EQ(kProxyChain.First().host_port_pair().host(),
            session_deps_.host_resolver->request_host(2));
  EXPECT_TRUE(session_deps_.host_resolver->request_network_anonymization_key(1)
                  .IsTransient());
  EXPECT_EQ(session_deps_.host_resolver->request_network_anonymization_key(1),
            session_deps_.host_resolver->request_network_anonymization_key(2));
}

// Test that, in the case of a SOCKS5 proxy, the passed in
// NetworkAnonymizationKey is used for the destination DNS lookup, and the same
// transient NetworkAnonymizationKey is reused for resolving the proxy's host,
// regardless of input NAK.
TEST_F(TransportClientSocketPoolTest, NetworkAnonymizationKeySocks4Proxy) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);
  const char kHost[] = "bar.test";
  const ProxyChain kProxyChain = ProxyUriToProxyChain(
      "socks4://proxy.test", /*default_scheme=*/ProxyServer::SCHEME_HTTP);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  session_deps_.host_resolver->set_ondemand_mode(true);

  // Test will establish two connections, but never use them to transfer data,
  // since thet stall on the followup DNS lookups.
  StaticSocketDataProvider data1;
  data1.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  tagging_client_socket_factory_.AddSocketDataProvider(&data1);
  StaticSocketDataProvider data2;
  data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  tagging_client_socket_factory_.AddSocketDataProvider(&data2);

  TransportClientSocketPool proxy_pool(
      kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout, kProxyChain,
      /*is_for_websockets=*/false, tagging_common_connect_job_params_.get());

  TransportClientSocketPool::GroupId group_id1(
      url::SchemeHostPort(url::kHttpScheme, kHost, 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey1,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle1;
  TestCompletionCallback callback1;
  EXPECT_THAT(
      handle1.Init(group_id1,
                   ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback1.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &proxy_pool, NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  TransportClientSocketPool::GroupId group_id2(
      url::SchemeHostPort(url::kHttpScheme, kHost, 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey2,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle2;
  TestCompletionCallback callback2;
  EXPECT_THAT(
      handle2.Init(group_id2,
                   ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &proxy_pool, NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  // First two lookups are for the proxy's hostname, and should use the same
  // transient NAK.
  ASSERT_EQ(2u, session_deps_.host_resolver->last_id());
  EXPECT_EQ(kProxyChain.First().host_port_pair().host(),
            session_deps_.host_resolver->request_host(1));
  EXPECT_EQ(kProxyChain.First().host_port_pair().host(),
            session_deps_.host_resolver->request_host(2));
  EXPECT_TRUE(session_deps_.host_resolver->request_network_anonymization_key(1)
                  .IsTransient());
  EXPECT_EQ(session_deps_.host_resolver->request_network_anonymization_key(1),
            session_deps_.host_resolver->request_network_anonymization_key(2));

  // First two lookups completes, starting the next two, which should be for the
  // destination's hostname, and should use the passed in NAKs.
  session_deps_.host_resolver->ResolveNow(1);
  session_deps_.host_resolver->ResolveNow(2);
  ASSERT_EQ(4u, session_deps_.host_resolver->last_id());
  EXPECT_EQ(kHost, session_deps_.host_resolver->request_host(3));
  EXPECT_EQ(kNetworkAnonymizationKey1,
            session_deps_.host_resolver->request_network_anonymization_key(3));
  EXPECT_EQ(kHost, session_deps_.host_resolver->request_host(4));
  EXPECT_EQ(kNetworkAnonymizationKey2,
            session_deps_.host_resolver->request_network_anonymization_key(4));
}

// Test that, in the case of a SOCKS5 proxy, the same transient
// NetworkAnonymizationKey is reused for resolving the proxy's host, regardless
// of input NAK.
TEST_F(TransportClientSocketPoolTest, NetworkAnonymizationKeySocks5Proxy) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);
  const char kHost[] = "bar.test";
  const ProxyChain kProxyChain = ProxyUriToProxyChain(
      "socks5://proxy.test", /*default_scheme=*/ProxyServer::SCHEME_HTTP);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  session_deps_.host_resolver->set_ondemand_mode(true);

  TransportClientSocketPool proxy_pool(
      kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout, kProxyChain,
      /*is_for_websockets=*/false, tagging_common_connect_job_params_.get());

  TransportClientSocketPool::GroupId group_id1(
      url::SchemeHostPort(url::kHttpScheme, kHost, 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey1,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle1;
  TestCompletionCallback callback1;
  EXPECT_THAT(
      handle1.Init(group_id1,
                   ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback1.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &proxy_pool, NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  TransportClientSocketPool::GroupId group_id2(
      url::SchemeHostPort(url::kHttpScheme, kHost, 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, kNetworkAnonymizationKey2,
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketHandle handle2;
  TestCompletionCallback callback2;
  EXPECT_THAT(
      handle2.Init(group_id2,
                   ClientSocketPool::SocketParams::CreateForHttpForTesting(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, LOW, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &proxy_pool, NetLogWithSource()),
      IsError(ERR_IO_PENDING));

  ASSERT_EQ(2u, session_deps_.host_resolver->last_id());
  EXPECT_EQ(kProxyChain.First().host_port_pair().host(),
            session_deps_.host_resolver->request_host(1));
  EXPECT_EQ(kProxyChain.First().host_port_pair().host(),
            session_deps_.host_resolver->request_host(2));
  EXPECT_TRUE(session_deps_.host_resolver->request_network_anonymization_key(1)
                  .IsTransient());
  EXPECT_EQ(session_deps_.host_resolver->request_network_anonymization_key(1),
            session_deps_.host_resolver->request_network_anonymization_key(2));
}

TEST_F(TransportClientSocketPoolTest, HasActiveSocket) {
  const url::SchemeHostPort kEndpoint1(url::kHttpScheme, "host1.test", 80);
  const url::SchemeHostPort kEndpoint2(url::kHttpScheme, "host2.test", 80);

  ClientSocketHandle handle;
  ClientSocketPool::GroupId group_id1(
      kEndpoint1, PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  ClientSocketPool::GroupId group_id2(
      kEndpoint2, PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);

  // HasActiveSocket() must return false before creating a socket.
  EXPECT_FALSE(pool_->HasActiveSocket(group_id1));

  TestCompletionCallback callback1;
  int rv1 =
      handle.Init(group_id1, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback1.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv1, IsError(ERR_IO_PENDING));

  // HasActiveSocket() must return true while connecting.
  EXPECT_TRUE(pool_->HasActiveSocket(group_id1));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback1.WaitForResult(), IsOk());

  // HasActiveSocket() must return true after handed out.
  EXPECT_TRUE(pool_->HasActiveSocket(group_id1));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  handle.Reset();

  // HasActiveSocket returns true for the idle socket.
  EXPECT_TRUE(pool_->HasActiveSocket(group_id1));
  // Now we should have 1 idle socket.
  EXPECT_EQ(1, pool_->IdleSocketCount());

  // HasActiveSocket() for group_id2 must still return false.
  EXPECT_FALSE(pool_->HasActiveSocket(group_id2));

  TestCompletionCallback callback2;
  int rv2 =
      handle.Init(group_id2, params_, std::nullopt /* proxy_annotation_tag */,
                  LOW, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
                  callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv2, IsError(ERR_IO_PENDING));

  // HasActiveSocket(group_id2) must return true while connecting.
  EXPECT_TRUE(pool_->HasActiveSocket(group_id2));

  // HasActiveSocket(group_id1) must still return true.
  EXPECT_TRUE(pool_->HasActiveSocket(group_id2));

  // Close the sockets.
  pool_->FlushWithError(ERR_NETWORK_CHANGED, "Network changed");

  // HasActiveSocket() must return false after closing the socket.
  EXPECT_FALSE(pool_->HasActiveSocket(group_id1));
  EXPECT_FALSE(pool_->HasActiveSocket(group_id2));
}

// Test that SocketTag passed into TransportClientSocketPool is applied to
// returned sockets.
#if BUILDFLAG(IS_ANDROID)
TEST_F(TransportClientSocketPoolTest, Tag) {
  if (!CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  // Start test server.
  EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);

  // Test socket is tagged before connected.
  uint64_t old_traffic = GetTaggedBytes(tag_val1);
  const ClientSocketPool::GroupId kGroupId(
      url::SchemeHostPort(test_server.base_url()),
      PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  scoped_refptr<ClientSocketPool::SocketParams> params =
      ClientSocketPool::SocketParams::CreateForHttpForTesting();
  TestCompletionCallback callback;
  int rv =
      handle.Init(kGroupId, params, std::nullopt /* proxy_annotation_tag */,
                  LOW, tag1, ClientSocketPool::RespectLimits::ENABLED,
                  callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                  pool_for_real_sockets_.get(), NetLogWithSource());
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  // Test reused socket is retagged.
  StreamSocket* socket = handle.socket();
  handle.Reset();
  old_traffic = GetTaggedBytes(tag_val2);
  rv = handle.Init(kGroupId, params, std::nullopt /* proxy_annotation_tag */,
                   LOW, tag2, ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_for_real_sockets_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  const char kRequest[] = "GET / HTTP/1.0\n\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv =
      handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                             callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
  // Disconnect socket to prevent reuse.
  handle.socket()->Disconnect();
  handle.Reset();

  // Test connect jobs that are orphaned and then adopted, appropriately apply
  // new tag. Request socket with |tag1|.
  TestCompletionCallback callback2;
  rv = handle.Init(kGroupId, params, std::nullopt /* proxy_annotation_tag */,
                   LOW, tag1, ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_for_real_sockets_.get(), NetLogWithSource());
  EXPECT_TRUE(rv == OK || rv == ERR_IO_PENDING) << "Result: " << rv;
  // Abort and request socket with |tag2|.
  handle.Reset();
  rv = handle.Init(kGroupId, params, std::nullopt /* proxy_annotation_tag */,
                   LOW, tag2, ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_for_real_sockets_.get(), NetLogWithSource());
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  // Verify socket has |tag2| applied.
  old_traffic = GetTaggedBytes(tag_val2);
  rv =
      handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                             callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
  // Disconnect socket to prevent reuse.
  handle.socket()->Disconnect();
  handle.Reset();
  // Eat the left over connect job from the second request.
  // TODO(pauljensen): remove when crbug.com/800731 fixed.
  rv = handle.Init(kGroupId, params, std::nullopt /* proxy_annotation_tag */,
                   LOW, tag1, ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_for_real_sockets_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  // Disconnect socket to prevent reuse.
  handle.socket()->Disconnect();
  handle.Reset();

  // Test two connect jobs of differing priorities. Start the lower priority one
  // first but expect its socket to get vended to the higher priority request.
  ClientSocketHandle handle_high_pri;
  TestCompletionCallback callback_high_pri;
  rv = handle.Init(kGroupId, params, std::nullopt /* proxy_annotation_tag */,
                   LOW, tag1, ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_for_real_sockets_.get(), NetLogWithSource());
  EXPECT_TRUE(rv == OK || rv == ERR_IO_PENDING) << "Result: " << rv;
  int rv_high_pri = handle_high_pri.Init(
      kGroupId, params, std::nullopt /* proxy_annotation_tag */, HIGHEST, tag2,
      ClientSocketPool::RespectLimits::ENABLED, callback_high_pri.callback(),
      ClientSocketPool::ProxyAuthCallback(), pool_for_real_sockets_.get(),
      NetLogWithSource());
  EXPECT_THAT(callback_high_pri.GetResult(rv_high_pri), IsOk());
  EXPECT_TRUE(handle_high_pri.socket());
  EXPECT_TRUE(handle_high_pri.socket()->IsConnected());
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  // Verify |handle_high_pri| has |tag2| applied.
  old_traffic = GetTaggedBytes(tag_val2);
  rv = handle_high_pri.socket()->Write(write_buffer.get(), strlen(kRequest),
                                       callback.callback(),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
  // Verify |handle| has |tag1| applied.
  old_traffic = GetTaggedBytes(tag_val1);
  rv =
      handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                             callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);
}

TEST_F(TransportClientSocketPoolTest, TagSOCKSProxy) {
  session_deps_.host_resolver->set_synchronous_mode(true);

  TransportClientSocketPool proxy_pool(
      kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout,
      ProxyUriToProxyChain("socks5://proxy",
                           /*default_scheme=*/ProxyServer::SCHEME_HTTP),
      /*is_for_websockets=*/false, tagging_common_connect_job_params_.get());

  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
  const url::SchemeHostPort kDestination(url::kHttpScheme, "host", 80);
  const ClientSocketPool::GroupId kGroupId(
      kDestination, PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false);
  scoped_refptr<ClientSocketPool::SocketParams> socks_params =
      ClientSocketPool::SocketParams::CreateForHttpForTesting();

  // Test socket is tagged when created synchronously.
  SOCKS5MockData data_sync(SYNCHRONOUS);
  data_sync.data_provider()->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  tagging_client_socket_factory_.AddSocketDataProvider(
      data_sync.data_provider());
  ClientSocketHandle handle;
  int rv = handle.Init(
      kGroupId, socks_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW, tag1,
      ClientSocketPool::RespectLimits::ENABLED, CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback(), &proxy_pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  EXPECT_EQ(tagging_client_socket_factory_.GetLastProducedTCPSocket()->tag(),
            tag1);
  EXPECT_TRUE(tagging_client_socket_factory_.GetLastProducedTCPSocket()
                  ->tagged_before_connected());

  // Test socket is tagged when reused synchronously.
  StreamSocket* socket = handle.socket();
  handle.Reset();
  rv = handle.Init(
      kGroupId, socks_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW, tag2,
      ClientSocketPool::RespectLimits::ENABLED, CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback(), &proxy_pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  EXPECT_EQ(tagging_client_socket_factory_.GetLastProducedTCPSocket()->tag(),
            tag2);
  handle.socket()->Disconnect();
  handle.Reset();

  // Test socket is tagged when created asynchronously.
  SOCKS5MockData data_async(ASYNC);
  tagging_client_socket_factory_.AddSocketDataProvider(
      data_async.data_provider());
  TestCompletionCallback callback;
  rv = handle.Init(kGroupId, socks_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW,
                   tag1, ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                   &proxy_pool, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  EXPECT_EQ(tagging_client_socket_factory_.GetLastProducedTCPSocket()->tag(),
            tag1);
  EXPECT_TRUE(tagging_client_socket_factory_.GetLastProducedTCPSocket()
                  ->tagged_before_connected());

  // Test socket is tagged when reused after being created asynchronously.
  socket = handle.socket();
  handle.Reset();
  rv = handle.Init(
      kGroupId, socks_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW, tag2,
      ClientSocketPool::RespectLimits::ENABLED, CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback(), &proxy_pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  EXPECT_EQ(tagging_client_socket_factory_.GetLastProducedTCPSocket()->tag(),
            tag2);
}

TEST_F(TransportClientSocketPoolTest, TagSSLDirect) {
  if (!CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  // Start test server.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, SSLServerConfig());
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);
  const ClientSocketPool::GroupId kGroupId(
      url::SchemeHostPort(test_server.base_url()),
      PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);

  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      base::MakeRefCounted<ClientSocketPool::SocketParams>(
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());

  // Test socket is tagged before connected.
  uint64_t old_traffic = GetTaggedBytes(tag_val1);
  int rv = handle.Init(
      kGroupId, socket_params, std::nullopt /* proxy_annotation_tag */, LOW,
      tag1, ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
      ClientSocketPool::ProxyAuthCallback(), pool_for_real_sockets_.get(),
      NetLogWithSource());
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  // Test reused socket is retagged.
  StreamSocket* socket = handle.socket();
  handle.Reset();
  old_traffic = GetTaggedBytes(tag_val2);
  TestCompletionCallback callback2;
  rv = handle.Init(kGroupId, socket_params,
                   std::nullopt /* proxy_annotation_tag */, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_for_real_sockets_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  const char kRequest[] = "GET / HTTP/1.1\r\n\r\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv =
      handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                             callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1);
  rv = handle.socket()->Read(read_buffer.get(), read_buffer->size(),
                             callback.callback());
  EXPECT_EQ(read_buffer->size(), callback.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
  // Disconnect socket to prevent reuse.
  handle.socket()->Disconnect();
  handle.Reset();
}

TEST_F(TransportClientSocketPoolTest, TagSSLDirectTwoSockets) {
  if (!CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  // Start test server.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, SSLServerConfig());
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);
  const ClientSocketPool::GroupId kGroupId(
      url::SchemeHostPort(test_server.base_url()),
      PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      base::MakeRefCounted<ClientSocketPool::SocketParams>(
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());

  // Test connect jobs that are orphaned and then adopted, appropriately apply
  // new tag. Request socket with |tag1|.
  TestCompletionCallback callback;
  int rv = handle.Init(
      kGroupId, socket_params, std::nullopt /* proxy_annotation_tag */, LOW,
      tag1, ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
      ClientSocketPool::ProxyAuthCallback(), pool_for_real_sockets_.get(),
      NetLogWithSource());
  EXPECT_TRUE(rv == OK || rv == ERR_IO_PENDING) << "Result: " << rv;
  // Abort and request socket with |tag2|.
  handle.Reset();
  TestCompletionCallback callback2;
  rv = handle.Init(kGroupId, socket_params,
                   std::nullopt /* proxy_annotation_tag */, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_for_real_sockets_.get(), NetLogWithSource());
  EXPECT_THAT(callback2.GetResult(rv), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  // Verify socket has |tag2| applied.
  uint64_t old_traffic = GetTaggedBytes(tag_val2);
  const char kRequest[] = "GET / HTTP/1.1\r\n\r\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv = handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                              callback2.callback(),
                              TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback2.GetResult(rv));
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1);
  rv = handle.socket()->Read(read_buffer.get(), read_buffer->size(),
                             callback2.callback());
  EXPECT_EQ(read_buffer->size(), callback2.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
}

TEST_F(TransportClientSocketPoolTest, TagSSLDirectTwoSocketsFullPool) {
  if (!CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  // Start test server.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, SSLServerConfig());
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);
  const ClientSocketPool::GroupId kGroupId(
      url::SchemeHostPort(test_server.base_url()),
      PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      base::MakeRefCounted<ClientSocketPool::SocketParams>(
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());

  // Test that sockets paused by a full underlying socket pool are properly
  // connected and tagged when underlying pool is freed up.
  // Fill up all slots in TCP pool.
  ClientSocketHandle tcp_handles[kMaxSocketsPerGroup];
  int rv;
  for (auto& tcp_handle : tcp_handles) {
    rv = tcp_handle.Init(
        kGroupId, socket_params, std::nullopt /* proxy_annotation_tag */, LOW,
        tag1, ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
        ClientSocketPool::ProxyAuthCallback(), pool_for_real_sockets_.get(),
        NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());
    EXPECT_TRUE(tcp_handle.socket());
    EXPECT_TRUE(tcp_handle.socket()->IsConnected());
  }
  // Request two SSL sockets.
  ClientSocketHandle handle_to_be_canceled;
  rv = handle_to_be_canceled.Init(
      kGroupId, socket_params, std::nullopt /* proxy_annotation_tag */, LOW,
      tag1, ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
      ClientSocketPool::ProxyAuthCallback(), pool_for_real_sockets_.get(),
      NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = handle.Init(kGroupId, socket_params,
                   std::nullopt /* proxy_annotation_tag */, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), ClientSocketPool::ProxyAuthCallback(),
                   pool_for_real_sockets_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Cancel first request.
  handle_to_be_canceled.Reset();
  // Disconnect a TCP socket to free up a slot.
  tcp_handles[0].socket()->Disconnect();
  tcp_handles[0].Reset();
  // Verify |handle| gets a valid tagged socket.
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  uint64_t old_traffic = GetTaggedBytes(tag_val2);
  const char kRequest[] = "GET / HTTP/1.1\r\n\r\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv =
      handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                             callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1);
  EXPECT_EQ(handle.socket()->Read(read_buffer.get(), read_buffer->size(),
                                  callback.callback()),
            ERR_IO_PENDING);
  EXPECT_THAT(callback.WaitForResult(), read_buffer->size());
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
}

TEST_F(TransportClientSocketPoolTest, TagHttpProxyNoTunnel) {
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);

  TransportClientSocketPool proxy_pool(
      kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout,
      ProxyUriToProxyChain("http://proxy",
                           /*default_scheme=*/ProxyServer::SCHEME_HTTP),
      /*is_for_websockets=*/false, tagging_common_connect_job_params_.get());

  session_deps_.host_resolver->set_synchronous_mode(true);
  SequencedSocketData socket_data;
  socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  tagging_client_socket_factory_.AddSocketDataProvider(&socket_data);

  const url::SchemeHostPort kDestination(url::kHttpScheme, "www.google.com",
                                         80);
  const ClientSocketPool::GroupId kGroupId(
      kDestination, PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false);
  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      ClientSocketPool::SocketParams::CreateForHttpForTesting();

  // Verify requested socket is tagged properly.
  ClientSocketHandle handle;
  int rv = handle.Init(
      kGroupId, socket_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW, tag1,
      ClientSocketPool::RespectLimits::ENABLED, CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback(), &proxy_pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.is_initialized());
  ASSERT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(tagging_client_socket_factory_.GetLastProducedTCPSocket()->tag(),
            tag1);
  EXPECT_TRUE(tagging_client_socket_factory_.GetLastProducedTCPSocket()
                  ->tagged_before_connected());

  // Verify reused socket is retagged properly.
  StreamSocket* socket = handle.socket();
  handle.Reset();
  rv = handle.Init(
      kGroupId, socket_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW, tag2,
      ClientSocketPool::RespectLimits::ENABLED, CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback(), &proxy_pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  EXPECT_EQ(tagging_client_socket_factory_.GetLastProducedTCPSocket()->tag(),
            tag2);
  handle.socket()->Disconnect();
  handle.Reset();
}

// This creates a tunnel without SSL on top of it - something not normally done,
// though some non-HTTP consumers use this path to create tunnels for other
// uses.
TEST_F(TransportClientSocketPoolTest, TagHttpProxyTunnel) {
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);

  TransportClientSocketPool proxy_pool(
      kMaxSockets, kMaxSocketsPerGroup, kUnusedIdleSocketTimeout,
      ProxyUriToProxyChain("http://proxy",
                           /*default_scheme=*/ProxyServer::SCHEME_HTTP),
      /*is_for_websockets=*/false, tagging_common_connect_job_params_.get());

  session_deps_.host_resolver->set_synchronous_mode(true);

  std::string request =
      "CONNECT www.google.com:443 HTTP/1.1\r\n"
      "Host: www.google.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n\r\n";
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, request.c_str()),
  };
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  SequencedSocketData socket_data(MockConnect(SYNCHRONOUS, OK), reads, writes);
  tagging_client_socket_factory_.AddSocketDataProvider(&socket_data);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  tagging_client_socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  const url::SchemeHostPort kDestination(url::kHttpsScheme, "www.google.com",
                                         443);
  const ClientSocketPool::GroupId kGroupId(
      kDestination, PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false);

  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      base::MakeRefCounted<ClientSocketPool::SocketParams>(
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());

  // Verify requested socket is tagged properly.
  ClientSocketHandle handle;
  int rv = handle.Init(
      kGroupId, socket_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW, tag1,
      ClientSocketPool::RespectLimits::ENABLED, CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback(), &proxy_pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.is_initialized());
  ASSERT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(tagging_client_socket_factory_.GetLastProducedTCPSocket()->tag(),
            tag1);
  EXPECT_TRUE(tagging_client_socket_factory_.GetLastProducedTCPSocket()
                  ->tagged_before_connected());

  // Verify reused socket is retagged properly.
  StreamSocket* socket = handle.socket();
  handle.Reset();
  rv = handle.Init(
      kGroupId, socket_params, TRAFFIC_ANNOTATION_FOR_TESTS, LOW, tag2,
      ClientSocketPool::RespectLimits::ENABLED, CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback(), &proxy_pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  EXPECT_EQ(tagging_client_socket_factory_.GetLastProducedTCPSocket()->tag(),
            tag2);
  handle.socket()->Disconnect();
  handle.Reset();
}

#endif  // BUILDFLAG(IS_ANDROID)

// Class that enables tests to set mock time.
class TransportClientSocketPoolMockNowSourceTest
    : public TransportClientSocketPoolTest {
 public:
  TransportClientSocketPoolMockNowSourceTest(
      const TransportClientSocketPoolMockNowSourceTest&) = delete;
  TransportClientSocketPoolMockNowSourceTest& operator=(
      const TransportClientSocketPoolMockNowSourceTest&) = delete;

 protected:
  TransportClientSocketPoolMockNowSourceTest()
      : TransportClientSocketPoolTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// Tests that changing the idle unused socket timeout using the experiment
// works. The test first sets the value of timeout duration for idle sockets.
// Next, it opens |kNumIdleSockets| sockets. To trigger the cleanup of idle
// sockets that may have timedout, it then opens one more socket. This is
// required since requesting a new socket triggers cleanup of idle timedout
// sockets. Next, the test verifies the count of idle timed-out sockets.
TEST_F(TransportClientSocketPoolMockNowSourceTest, IdleUnusedSocketTimeout) {
  const url::SchemeHostPort kSchemeHostPort1(url::kHttpScheme, "www.foo.com",
                                             80);
  const url::SchemeHostPort kSchemeHostPort2(url::kHttpScheme, "www.bar.com",
                                             80);

  const struct {
    bool use_first_socket;
    int fast_forward_seconds;
    int unused_idle_socket_timeout_seconds;
    bool expect_idle_socket;
  } kTests[] = {
      // When the clock is fast forwarded by a duration longer than
      // |unused_idle_socket_timeout_seconds|, the first unused idle socket is
      // expected to be timedout, and cleared.
      {false, 0, 0, false},
      {false, 9, 10, true},
      {false, 11, 10, false},
      {false, 19, 20, true},
      {false, 21, 20, false},
      // If |use_first_socket| is true, then the test would write some data to
      // the socket, thereby marking it as "used". Thereafter, this idle socket
      // should be timedout based on used idle socket timeout, and changing
      // |unused_idle_socket_timeout_seconds| should not affect the
      // |expected_idle_sockets|.
      {true, 0, 0, true},
      {true, 9, 10, true},
      {true, 11, 10, true},
      {true, 19, 20, true},
      {true, 21, 20, true},
  };

  for (const auto& test : kTests) {
    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateDirect());
    std::unique_ptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));

    base::test::ScopedFeatureList scoped_feature_list_;
    std::map<std::string, std::string> parameters;
    parameters["unused_idle_socket_timeout_seconds"] =
        base::NumberToString(test.unused_idle_socket_timeout_seconds);
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kNetUnusedIdleSocketTimeout, parameters);

    const char kWriteData[] = "1";
    const MockWrite kWrites[] = {MockWrite(SYNCHRONOUS, kWriteData)};

    SequencedSocketData provider_socket_1(MockConnect(ASYNC, OK),
                                          base::span<MockRead>(), kWrites);
    {
      // Create 1 socket.
      scoped_refptr<ClientSocketPool::SocketParams> socket_params =
          ClientSocketPool::SocketParams::CreateForHttpForTesting();
      session_deps.socket_factory->AddSocketDataProvider(&provider_socket_1);
      ClientSocketHandle connection;
      TestCompletionCallback callback;
      int rv = connection.Init(
          ClientSocketPool::GroupId(
              kSchemeHostPort1, PrivacyMode::PRIVACY_MODE_DISABLED,
              NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
              /*disable_cert_network_fetches=*/false),
          ClientSocketPool::SocketParams::CreateForHttpForTesting(),
          /*proxy_annotation_tag=*/std::nullopt, MEDIUM, SocketTag(),
          ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
          ClientSocketPool::ProxyAuthCallback(),
          session->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()),
          NetLogWithSource());
      EXPECT_THAT(callback.GetResult(rv), IsOk());
      EXPECT_FALSE(connection.socket()->WasEverUsed());

      // Writing some data to the socket should set WasEverUsed.
      if (test.use_first_socket) {
        // Generate |socket_write_data| from kMockWriteData by appending null
        // character to the latter.
        auto write_buffer = base::MakeRefCounted<StringIOBuffer>(kWriteData);
        TestCompletionCallback write_callback;
        rv = connection.socket()->Write(
            write_buffer.get(), write_buffer->size(), write_callback.callback(),
            TRAFFIC_ANNOTATION_FOR_TESTS);
        EXPECT_EQ(rv, 1);
        EXPECT_TRUE(connection.socket()->WasEverUsed());
      }
    }

    EXPECT_EQ(1, session
                     ->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                     ProxyChain::Direct())
                     ->IdleSocketCount());

    // Moving the clock forward may cause the idle socket to be timedout.
    FastForwardBy(base::Seconds(test.fast_forward_seconds));

    {
      // Request a new socket to trigger cleanup of idle timedout sockets.
      scoped_refptr<ClientSocketPool::SocketParams> socket_params =
          ClientSocketPool::SocketParams::CreateForHttpForTesting();
      SequencedSocketData provider_socket_2(MockConnect(ASYNC, OK),
                                            base::span<MockRead>(),
                                            base::span<MockWrite>());
      session_deps.socket_factory->AddSocketDataProvider(&provider_socket_2);
      ClientSocketHandle connection;
      TestCompletionCallback callback;
      int rv = connection.Init(
          ClientSocketPool::GroupId(
              kSchemeHostPort2, PrivacyMode::PRIVACY_MODE_DISABLED,
              NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
              /*disable_cert_network_fetches=*/false),
          socket_params, /*proxy_annotation_tag=*/std::nullopt, MEDIUM,
          SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
          callback.callback(), ClientSocketPool::ProxyAuthCallback(),
          session->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()),
          NetLogWithSource());
      EXPECT_THAT(callback.GetResult(rv), IsOk());
      connection.socket()->Disconnect();
    }

    EXPECT_EQ(test.expect_idle_socket ? 1 : 0,
              session
                  ->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                  ProxyChain::Direct())
                  ->IdleSocketCount());
  }
}

}  // namespace

}  // namespace net
