// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_session_pool.h"

#include <cstddef>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "net/base/proxy_string_util.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/base/tracing.h"
#include "net/dns/host_cache.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::trace_event::MemoryAllocatorDump;
using net::test::IsError;
using net::test::IsOk;
using testing::ByRef;
using testing::Contains;
using testing::Eq;

namespace net {

class SpdySessionPoolTest : public TestWithTaskEnvironment {
 protected:
  // Used by RunIPPoolingTest().
  enum SpdyPoolCloseSessionsType {
    SPDY_POOL_CLOSE_SESSIONS_MANUALLY,
    SPDY_POOL_CLOSE_CURRENT_SESSIONS,
    SPDY_POOL_CLOSE_IDLE_SESSIONS,
  };

  SpdySessionPoolTest() = default;

  void CreateNetworkSession() {
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    spdy_session_pool_ = http_session_->spdy_session_pool();
  }

  void AddSSLSocketData() {
    auto ssl = std::make_unique<SSLSocketDataProvider>(SYNCHRONOUS, OK);
    ssl->ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
    ASSERT_TRUE(ssl->ssl_info.cert);
    session_deps_.socket_factory->AddSSLSocketDataProvider(ssl.get());
    ssl_data_vector_.push_back(std::move(ssl));
  }

  void RunIPPoolingTest(SpdyPoolCloseSessionsType close_sessions_type);
  void RunIPPoolingDisabledTest(SSLSocketDataProvider* ssl);

  size_t num_active_streams(base::WeakPtr<SpdySession> session) {
    return session->active_streams_.size();
  }

  size_t max_concurrent_streams(base::WeakPtr<SpdySession> session) {
    return session->max_concurrent_streams_;
  }

  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> http_session_;
  raw_ptr<SpdySessionPool, DanglingUntriaged> spdy_session_pool_ = nullptr;
  std::vector<std::unique_ptr<SSLSocketDataProvider>> ssl_data_vector_;
};

class SpdySessionRequestDelegate
    : public SpdySessionPool::SpdySessionRequest::Delegate {
 public:
  SpdySessionRequestDelegate() = default;

  SpdySessionRequestDelegate(const SpdySessionRequestDelegate&) = delete;
  SpdySessionRequestDelegate& operator=(const SpdySessionRequestDelegate&) =
      delete;

  ~SpdySessionRequestDelegate() override = default;

  void OnSpdySessionAvailable(
      base::WeakPtr<SpdySession> spdy_session) override {
    EXPECT_FALSE(callback_invoked_);
    callback_invoked_ = true;
    spdy_session_ = spdy_session;
  }

  bool callback_invoked() const { return callback_invoked_; }

  SpdySession* spdy_session() { return spdy_session_.get(); }

 private:
  bool callback_invoked_ = false;
  base::WeakPtr<SpdySession> spdy_session_;
};

// Attempts to set up an alias for |key| using an already existing session in
// |pool|. To do this, simulates a host resolution that returns
// |endpoints|.
bool TryCreateAliasedSpdySession(
    SpdySessionPool* pool,
    const SpdySessionKey& key,
    const std::vector<HostResolverEndpointResult>& endpoints,
    bool enable_ip_based_pooling = true,
    bool is_websocket = false) {
  // The requested session must not already exist.
  EXPECT_FALSE(pool->FindAvailableSession(key, enable_ip_based_pooling,
                                          is_websocket, NetLogWithSource()));

  // Create a request for the session. There should be no matching session
  // (aliased or otherwise) yet. A pending request is necessary for the session
  // to create an alias on host resolution completion.
  std::unique_ptr<SpdySessionPool::SpdySessionRequest> request;
  bool is_blocking_request_for_session = false;
  SpdySessionRequestDelegate request_delegate;
  EXPECT_FALSE(pool->RequestSession(
      key, enable_ip_based_pooling, is_websocket, NetLogWithSource(),
      /* on_blocking_request_destroyed_callback = */ base::RepeatingClosure(),
      &request_delegate, &request, &is_blocking_request_for_session));
  EXPECT_TRUE(request);
  EXPECT_TRUE(is_blocking_request_for_session);

  // Simulate a host resolution completing.
  OnHostResolutionCallbackResult result = pool->OnHostResolutionComplete(
      key, is_websocket, endpoints, /*aliases=*/{});

  // Spin the message loop and see if it creates an H2 session.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(request_delegate.callback_invoked(),
            result == OnHostResolutionCallbackResult::kMayBeDeletedAsync);
  EXPECT_EQ(request_delegate.callback_invoked(),
            request_delegate.spdy_session() != nullptr);
  request.reset();

  // Calling RequestSession again should return request_delegate.spdy_session()
  // (i.e. the newly created session, if a session was created, or nullptr, if
  // one was not.)
  EXPECT_EQ(request_delegate.spdy_session(),
            pool->RequestSession(key, enable_ip_based_pooling, is_websocket,
                                 NetLogWithSource(),
                                 /* on_blocking_request_destroyed_callback = */
                                 base::RepeatingClosure(), &request_delegate,
                                 &request, &is_blocking_request_for_session)
                .get());

  return request_delegate.spdy_session() != nullptr;
}

// Attempts to set up an alias for |key| using an already existing session in
// |pool|. To do this, simulates a host resolution that returns
// |ip_address_list|.
bool TryCreateAliasedSpdySession(SpdySessionPool* pool,
                                 const SpdySessionKey& key,
                                 const std::string& ip_address_list,
                                 bool enable_ip_based_pooling = true,
                                 bool is_websocket = false) {
  std::vector<IPEndPoint> ip_endpoints;
  EXPECT_THAT(ParseAddressList(ip_address_list, &ip_endpoints), IsOk());
  HostResolverEndpointResult endpoint;
  for (auto& ip_endpoint : ip_endpoints) {
    endpoint.ip_endpoints.emplace_back(ip_endpoint.address(), 443);
  }
  return TryCreateAliasedSpdySession(pool, key, {endpoint},
                                     enable_ip_based_pooling, is_websocket);
}

// A delegate that opens a new session when it is closed.
class SessionOpeningDelegate : public SpdyStream::Delegate {
 public:
  SessionOpeningDelegate(SpdySessionPool* spdy_session_pool,
                         const SpdySessionKey& key)
      : spdy_session_pool_(spdy_session_pool), key_(key) {}

  ~SessionOpeningDelegate() override = default;

  void OnHeadersSent() override {}

  void OnEarlyHintsReceived(const quiche::HttpHeaderBlock& headers) override {}

  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override {}

  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override {}

  void OnDataSent() override {}

  void OnTrailers(const quiche::HttpHeaderBlock& trailers) override {}

  void OnClose(int status) override {
    std::ignore = CreateFakeSpdySession(spdy_session_pool_, key_);
  }

  bool CanGreaseFrameType() const override { return false; }

  NetLogSource source_dependency() const override { return NetLogSource(); }

 private:
  const raw_ptr<SpdySessionPool> spdy_session_pool_;
  const SpdySessionKey key_;
};

// Set up a SpdyStream to create a new session when it is closed.
// CloseCurrentSessions should not close the newly-created session.
TEST_F(SpdySessionPoolTest, CloseCurrentSessions) {
  const char kTestHost[] = "www.foo.com";
  const int kTestPort = 80;

  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  SpdySessionKey test_key = SpdySessionKey(
      test_host_port_pair, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
      SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Setup the first session to the first host.
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), test_key, NetLogWithSource());

  // Flush the SpdySession::OnReadComplete() task.
  base::RunLoop().RunUntilIdle();

  // Verify that we have sessions for everything.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_key));

  // Set the stream to create a new session when it is closed.
  base::WeakPtr<SpdyStream> spdy_stream = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session, GURL("http://www.foo.com"), MEDIUM,
      NetLogWithSource());
  SessionOpeningDelegate delegate(spdy_session_pool_, test_key);
  spdy_stream->SetDelegate(&delegate);

  // Close the current session.
  spdy_session_pool_->CloseCurrentSessions(ERR_ABORTED);

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_key));
}

TEST_F(SpdySessionPoolTest, CloseCurrentIdleSessions) {
  const std::string close_session_description = "Closing idle sessions.";
  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data1(reads, base::span<MockWrite>());
  data1.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  AddSSLSocketData();
  AddSSLSocketData();
  AddSSLSocketData();

  CreateNetworkSession();

  // Set up session 1
  const GURL url1("https://www.example.org");
  HostPortPair test_host_port_pair1(HostPortPair::FromURL(url1));
  SpdySessionKey key1(test_host_port_pair1, PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session1 =
      CreateSpdySession(http_session_.get(), key1, NetLogWithSource());
  base::WeakPtr<SpdyStream> spdy_stream1 = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session1, url1, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream1);

  // Set up session 2
  StaticSocketDataProvider data2(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data2);
  const GURL url2("https://mail.example.org");
  HostPortPair test_host_port_pair2(HostPortPair::FromURL(url2));
  SpdySessionKey key2(test_host_port_pair2, PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session2 =
      CreateSpdySession(http_session_.get(), key2, NetLogWithSource());
  base::WeakPtr<SpdyStream> spdy_stream2 = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session2, url2, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream2);

  // Set up session 3
  StaticSocketDataProvider data3(reads, base::span<MockWrite>());
  data3.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data3);
  const GURL url3("https://mail.example.com");
  HostPortPair test_host_port_pair3(HostPortPair::FromURL(url3));
  SpdySessionKey key3(test_host_port_pair3, PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session3 =
      CreateSpdySession(http_session_.get(), key3, NetLogWithSource());
  base::WeakPtr<SpdyStream> spdy_stream3 = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session3, url3, MEDIUM, NetLogWithSource());
  ASSERT_TRUE(spdy_stream3);

  // All sessions are active and not closed
  EXPECT_TRUE(session1->is_active());
  EXPECT_TRUE(session1->IsAvailable());
  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());
  EXPECT_TRUE(session3->is_active());
  EXPECT_TRUE(session3->IsAvailable());

  // Should not do anything, all are active
  spdy_session_pool_->CloseCurrentIdleSessions(close_session_description);
  EXPECT_TRUE(session1->is_active());
  EXPECT_TRUE(session1->IsAvailable());
  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());
  EXPECT_TRUE(session3->is_active());
  EXPECT_TRUE(session3->IsAvailable());

  // Make sessions 1 and 3 inactive, but keep them open.
  // Session 2 still open and active
  session1->CloseCreatedStream(spdy_stream1, OK);
  EXPECT_FALSE(spdy_stream1);
  session3->CloseCreatedStream(spdy_stream3, OK);
  EXPECT_FALSE(spdy_stream3);
  EXPECT_FALSE(session1->is_active());
  EXPECT_TRUE(session1->IsAvailable());
  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());
  EXPECT_FALSE(session3->is_active());
  EXPECT_TRUE(session3->IsAvailable());

  // Should close session 1 and 3, 2 should be left open
  spdy_session_pool_->CloseCurrentIdleSessions(close_session_description);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(session1);
  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());
  EXPECT_FALSE(session3);

  // Should not do anything
  spdy_session_pool_->CloseCurrentIdleSessions(close_session_description);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());

  // Make 2 not active
  session2->CloseCreatedStream(spdy_stream2, OK);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(spdy_stream2);
  EXPECT_FALSE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());

  // This should close session 2
  spdy_session_pool_->CloseCurrentIdleSessions(close_session_description);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(session2);
}

// Set up a SpdyStream to create a new session when it is closed.
// CloseAllSessions should close the newly-created session.
TEST_F(SpdySessionPoolTest, CloseAllSessions) {
  const char kTestHost[] = "www.foo.com";
  const int kTestPort = 80;

  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  SpdySessionKey test_key = SpdySessionKey(
      test_host_port_pair, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
      SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Setup the first session to the first host.
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), test_key, NetLogWithSource());

  // Flush the SpdySession::OnReadComplete() task.
  base::RunLoop().RunUntilIdle();

  // Verify that we have sessions for everything.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_key));

  // Set the stream to create a new session when it is closed.
  base::WeakPtr<SpdyStream> spdy_stream = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session, GURL("http://www.foo.com"), MEDIUM,
      NetLogWithSource());
  SessionOpeningDelegate delegate(spdy_session_pool_, test_key);
  spdy_stream->SetDelegate(&delegate);

  // Close the current session.
  spdy_session_pool_->CloseAllSessions();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_key));
}

// Code testing SpdySessionPool::OnIPAddressChange requires a SpdySessionPool
// with some active sessions. This fixture takes care of setting most things up
// but doesn't create the pool yet, allowing tests to possibly further
// configure sessions_deps_.
class SpdySessionPoolOnIPAddressChangeTest : public SpdySessionPoolTest {
 protected:
  SpdySessionPoolOnIPAddressChangeTest()
      : test_host_port_pair_(kTestHost, kTestPort),
        reads_({
            MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
        }),
        test_key_(SpdySessionKey(
            test_host_port_pair_,
            PRIVACY_MODE_DISABLED,
            ProxyChain::Direct(),
            SessionUsage::kDestination,
            SocketTag(),
            NetworkAnonymizationKey(),
            SecureDnsPolicy::kAllow,
            /*disable_cert_verification_network_fetches=*/false)),
        connect_data_(SYNCHRONOUS, OK),
        data_(reads_, base::span<MockWrite>()),
        ssl_(SYNCHRONOUS, OK) {
    data_.set_connect_data(connect_data_);
    session_deps_.socket_factory->AddSocketDataProvider(&data_);
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_);
  }

  static constexpr char kTestHost[] = "www.foo.com";
  static constexpr int kTestPort = 80;
  static constexpr int kReadSize = 1;

  const HostPortPair test_host_port_pair_;
  const std::array<MockRead, kReadSize> reads_;
  const SpdySessionKey test_key_;
  const MockConnect connect_data_;
  StaticSocketDataProvider data_;
  SSLSocketDataProvider ssl_;
};

TEST_F(SpdySessionPoolOnIPAddressChangeTest, DoNotIgnoreIPAddressChanges) {
  // Default behavior should be ignore_ip_address_changes = false;
  CreateNetworkSession();

  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), test_key_, NetLogWithSource());

  // Flush the SpdySession::OnReadComplete() task.
  base::RunLoop().RunUntilIdle();
  // Verify that we have a session.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_key_));

  // Without setting session_deps_.ignore_ip_address_changes = true the pool
  // should close (or make unavailable) all sessions after an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_key_));
}

TEST_F(SpdySessionPoolOnIPAddressChangeTest, IgnoreIPAddressChanges) {
  session_deps_.ignore_ip_address_changes = true;
  CreateNetworkSession();

  // Setup the first session to the first host.
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), test_key_, NetLogWithSource());
  // Flush the SpdySession::OnReadComplete() task.
  base::RunLoop().RunUntilIdle();
  // Verify that we have a session.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_key_));

  // Since we set ignore_ip_address_changes = true, the session should still be
  // there after an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_key_));
}

// This test has three variants, one for each style of closing the connection.
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_SESSIONS_MANUALLY,
// the sessions are closed manually, calling SpdySessionPool::Remove() directly.
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_CURRENT_SESSIONS,
// sessions are closed with SpdySessionPool::CloseCurrentSessions().
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_IDLE_SESSIONS,
// sessions are closed with SpdySessionPool::CloseIdleSessions().
void SpdySessionPoolTest::RunIPPoolingTest(
    SpdyPoolCloseSessionsType close_sessions_type) {
  constexpr int kTestPort = 443;
  struct TestHosts {
    std::string url;
    std::string name;
    std::string iplist;
    SpdySessionKey key;
  } test_hosts[] = {
      {"http://www.example.org", "www.example.org",
       "192.0.2.33,192.168.0.1,192.168.0.5"},
      {"http://mail.example.org", "mail.example.org",
       "192.168.0.2,192.168.0.3,192.168.0.5,192.0.2.33"},
      {"http://mail.example.com", "mail.example.com",
       "192.168.0.4,192.168.0.3"},
  };

  for (auto& test_host : test_hosts) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_host.name, test_host.iplist, std::string());

    test_host.key = SpdySessionKey(
        HostPortPair(test_host.name, kTestPort), PRIVACY_MODE_DISABLED,
        ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
        NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        /*disable_cert_verification_network_fetches=*/false);
  }

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data1(reads, base::span<MockWrite>());
  data1.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  AddSSLSocketData();

  CreateNetworkSession();

  // Setup the first session to the first host.
  base::WeakPtr<SpdySession> session = CreateSpdySession(
      http_session_.get(), test_hosts[0].key, NetLogWithSource());

  // Flush the SpdySession::OnReadComplete() task.
  base::RunLoop().RunUntilIdle();

  // The third host has no overlap with the first, so it can't pool IPs.
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, test_hosts[2].key, test_hosts[2].iplist));

  // The second host overlaps with the first, and should IP pool.
  EXPECT_TRUE(TryCreateAliasedSpdySession(spdy_session_pool_, test_hosts[1].key,
                                          test_hosts[1].iplist));

  // However, if IP pooling is disabled, FindAvailableSession() should not find
  // |session| for the second host.
  base::WeakPtr<SpdySession> session1 =
      spdy_session_pool_->FindAvailableSession(
          test_hosts[1].key, /* enable_ip_based_pooling = */ false,
          /* is_websocket = */ false, NetLogWithSource());
  EXPECT_FALSE(session1);

  // Verify that the second host, through a proxy, won't share the IP, even if
  // the IP list matches.
  SpdySessionKey proxy_key(
      test_hosts[1].key.host_port_pair(), PRIVACY_MODE_DISABLED,
      PacResultElementToProxyChain("HTTP http://proxy.foo.com/"),
      SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_FALSE(TryCreateAliasedSpdySession(spdy_session_pool_, proxy_key,
                                           test_hosts[1].iplist));

  // Verify that the second host, with a different SecureDnsPolicy,
  // won't share the IP, even if the IP list matches.
  SpdySessionKey disable_secure_dns_key(
      test_hosts[1].key.host_port_pair(), PRIVACY_MODE_DISABLED,
      ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
      /*disable_cert_verification_network_fetches=*/false);
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, disable_secure_dns_key, test_hosts[1].iplist));

  // Overlap between 2 and 3 is not transitive to 1.
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, test_hosts[2].key, test_hosts[2].iplist));

  // Create a new session to host 2.
  StaticSocketDataProvider data2(reads, base::span<MockWrite>());
  data2.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  AddSSLSocketData();

  base::WeakPtr<SpdySession> session2 = CreateSpdySession(
      http_session_.get(), test_hosts[2].key, NetLogWithSource());

  // Verify that we have sessions for everything.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[0].key));
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[1].key));
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[2].key));

  // Grab the session to host 1 and verify that it is the same session
  // we got with host 0, and that is a different from host 2's session.
  session1 = spdy_session_pool_->FindAvailableSession(
      test_hosts[1].key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ false, NetLogWithSource());
  EXPECT_EQ(session.get(), session1.get());
  EXPECT_NE(session2.get(), session1.get());

  // Remove the aliases and observe that we still have a session for host1.
  SpdySessionPoolPeer pool_peer(spdy_session_pool_);
  pool_peer.RemoveAliases(test_hosts[0].key);
  pool_peer.RemoveAliases(test_hosts[1].key);
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[1].key));

  // Cleanup the sessions.
  switch (close_sessions_type) {
    case SPDY_POOL_CLOSE_SESSIONS_MANUALLY:
      session->CloseSessionOnError(ERR_ABORTED, std::string());
      session2->CloseSessionOnError(ERR_ABORTED, std::string());
      base::RunLoop().RunUntilIdle();
      EXPECT_FALSE(session);
      EXPECT_FALSE(session2);
      break;
    case SPDY_POOL_CLOSE_CURRENT_SESSIONS:
      spdy_session_pool_->CloseCurrentSessions(ERR_ABORTED);
      break;
    case SPDY_POOL_CLOSE_IDLE_SESSIONS:
      GURL url(test_hosts[0].url);
      base::WeakPtr<SpdyStream> spdy_stream = CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, MEDIUM, NetLogWithSource());
      GURL url1(test_hosts[1].url);
      base::WeakPtr<SpdyStream> spdy_stream1 =
          CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session1, url1,
                                    MEDIUM, NetLogWithSource());
      GURL url2(test_hosts[2].url);
      base::WeakPtr<SpdyStream> spdy_stream2 =
          CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session2, url2,
                                    MEDIUM, NetLogWithSource());

      // Close streams to make spdy_session and spdy_session1 inactive.
      session->CloseCreatedStream(spdy_stream, OK);
      EXPECT_FALSE(spdy_stream);
      session1->CloseCreatedStream(spdy_stream1, OK);
      EXPECT_FALSE(spdy_stream1);

      // Check spdy_session and spdy_session1 are not closed.
      EXPECT_FALSE(session->is_active());
      EXPECT_TRUE(session->IsAvailable());
      EXPECT_FALSE(session1->is_active());
      EXPECT_TRUE(session1->IsAvailable());
      EXPECT_TRUE(session2->is_active());
      EXPECT_TRUE(session2->IsAvailable());

      // Test that calling CloseIdleSessions, does not cause a crash.
      // http://crbug.com/181400
      spdy_session_pool_->CloseCurrentIdleSessions("Closing idle sessions.");
      base::RunLoop().RunUntilIdle();

      // Verify spdy_session and spdy_session1 are closed.
      EXPECT_FALSE(session);
      EXPECT_FALSE(session1);
      EXPECT_TRUE(session2->is_active());
      EXPECT_TRUE(session2->IsAvailable());

      spdy_stream2->Cancel(ERR_ABORTED);
      EXPECT_FALSE(spdy_stream);
      EXPECT_FALSE(spdy_stream1);
      EXPECT_FALSE(spdy_stream2);

      session2->CloseSessionOnError(ERR_ABORTED, std::string());
      base::RunLoop().RunUntilIdle();
      EXPECT_FALSE(session2);
      break;
  }

  // Verify that the map is all cleaned up.
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[0].key));
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[1].key));
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[2].key));
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, test_hosts[0].key, test_hosts[0].iplist));
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, test_hosts[1].key, test_hosts[1].iplist));
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, test_hosts[2].key, test_hosts[2].iplist));
}

void SpdySessionPoolTest::RunIPPoolingDisabledTest(SSLSocketDataProvider* ssl) {
  constexpr int kTestPort = 443;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
  } test_hosts[] = {
      {"www.webkit.org", "192.0.2.33,192.168.0.1,192.168.0.5"},
      {"js.webkit.com", "192.168.0.4,192.168.0.1,192.0.2.33"},
  };

  session_deps_.host_resolver->set_synchronous_mode(true);
  for (auto& test_host : test_hosts) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_host.name, test_host.iplist, std::string());

    // Setup a SpdySessionKey
    test_host.key = SpdySessionKey(
        HostPortPair(test_host.name, kTestPort), PRIVACY_MODE_DISABLED,
        ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
        NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        /*disable_cert_verification_network_fetches=*/false);
  }

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(ssl);

  CreateNetworkSession();

  base::WeakPtr<SpdySession> spdy_session = CreateSpdySession(
      http_session_.get(), test_hosts[0].key, NetLogWithSource());
  EXPECT_TRUE(
      HasSpdySession(http_session_->spdy_session_pool(), test_hosts[0].key));
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, test_hosts[1].key, test_hosts[1].iplist,
      /* enable_ip_based_pooling = */ false));

  http_session_->spdy_session_pool()->CloseAllSessions();
}

TEST_F(SpdySessionPoolTest, IPPooling) {
  RunIPPoolingTest(SPDY_POOL_CLOSE_SESSIONS_MANUALLY);
}

TEST_F(SpdySessionPoolTest, IPPoolingCloseCurrentSessions) {
  RunIPPoolingTest(SPDY_POOL_CLOSE_CURRENT_SESSIONS);
}

TEST_F(SpdySessionPoolTest, IPPoolingCloseIdleSessions) {
  RunIPPoolingTest(SPDY_POOL_CLOSE_IDLE_SESSIONS);
}

// Regression test for https://crbug.com/643025.
TEST_F(SpdySessionPoolTest, IPPoolingNetLog) {
  // Define two hosts with identical IP address.
  constexpr int kTestPort = 443;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
  } test_hosts[] = {
      {"www.example.org", "192.168.0.1"},
      {"mail.example.org", "192.168.0.1"},
  };

  // Populate the HostResolver cache.
  session_deps_.host_resolver->set_synchronous_mode(true);
  for (auto& test_host : test_hosts) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_host.name, test_host.iplist, std::string());

    test_host.key = SpdySessionKey(
        HostPortPair(test_host.name, kTestPort), PRIVACY_MODE_DISABLED,
        ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
        NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        /*disable_cert_verification_network_fetches=*/false);
  }

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();

  // Open SpdySession to the first host.
  base::WeakPtr<SpdySession> session0 = CreateSpdySession(
      http_session_.get(), test_hosts[0].key, NetLogWithSource());

  // The second host should pool to the existing connection.
  RecordingNetLogObserver net_log_observer;
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(TryCreateAliasedSpdySession(spdy_session_pool_, test_hosts[1].key,
                                          test_hosts[1].iplist));
  histogram_tester.ExpectTotalCount("Net.SpdySessionGet", 1);

  base::WeakPtr<SpdySession> session1 =
      spdy_session_pool_->FindAvailableSession(
          test_hosts[1].key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false,
          NetLogWithSource::Make(NetLogSourceType::NONE));
  EXPECT_EQ(session0.get(), session1.get());

  ASSERT_EQ(1u, net_log_observer.GetSize());
  histogram_tester.ExpectTotalCount("Net.SpdySessionGet", 2);

  // FindAvailableSession() should have logged a netlog event indicating IP
  // pooling.
  auto entry_list = net_log_observer.GetEntries();
  EXPECT_EQ(
      NetLogEventType::HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
      entry_list[0].type);

  // Both FindAvailableSession() calls (including one from
  // TryCreateAliasedSpdySession) should log histogram entries indicating IP
  // pooling.
  histogram_tester.ExpectUniqueSample("Net.SpdySessionGet", 2, 2);
}

// Test IP pooling when the DNS responses have ALPNs.
TEST_F(SpdySessionPoolTest, IPPoolingDnsAlpn) {
  // Define two hosts with identical IP address.
  constexpr int kTestPort = 443;
  struct TestHosts {
    std::string name;
    std::vector<HostResolverEndpointResult> endpoints;
    SpdySessionKey key;
  } test_hosts[] = {{"www.example.org"},
                    {"mail.example.org"},
                    {"mail.example.com"},
                    {"example.test"}};

  const IPEndPoint kRightIP(*IPAddress::FromIPLiteral("192.168.0.1"),
                            kTestPort);
  const IPEndPoint kWrongIP(*IPAddress::FromIPLiteral("192.168.0.2"),
                            kTestPort);
  const std::string kRightALPN = "h2";
  const std::string kWrongALPN = "h3";

  // `test_hosts[0]` and `test_hosts[1]` resolve to the same IP address, without
  // any ALPN information.
  test_hosts[0].endpoints.emplace_back();
  test_hosts[0].endpoints[0].ip_endpoints = {kRightIP};
  test_hosts[1].endpoints.emplace_back();
  test_hosts[1].endpoints[0].ip_endpoints = {kRightIP};

  // `test_hosts[2]` resolves to the same IP address, but only via an
  // alternative endpoint with matching ALPN.
  test_hosts[2].endpoints.emplace_back();
  test_hosts[2].endpoints[0].ip_endpoints = {kRightIP};
  test_hosts[2].endpoints[0].metadata.supported_protocol_alpns = {kRightALPN};

  // `test_hosts[3]` resolves to the same IP address, but only via an
  // alternative endpoint with a mismatching ALPN.
  test_hosts[3].endpoints.resize(2);
  test_hosts[3].endpoints[0].ip_endpoints = {kRightIP};
  test_hosts[3].endpoints[0].metadata.supported_protocol_alpns = {kWrongALPN};
  test_hosts[3].endpoints[1].ip_endpoints = {kWrongIP};
  test_hosts[3].endpoints[1].metadata.supported_protocol_alpns = {kRightALPN};

  // Populate the HostResolver cache.
  session_deps_.host_resolver->set_synchronous_mode(true);
  for (auto& test_host : test_hosts) {
    session_deps_.host_resolver->rules()->AddRule(
        test_host.name,
        MockHostResolverBase::RuleResolver::RuleResult(test_host.endpoints));

    test_host.key = SpdySessionKey(
        HostPortPair(test_host.name, kTestPort), PRIVACY_MODE_DISABLED,
        ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
        NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        /*disable_cert_verification_network_fetches=*/false);
  }

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();

  // Open SpdySession to the first host.
  base::WeakPtr<SpdySession> session0 = CreateSpdySession(
      http_session_.get(), test_hosts[0].key, NetLogWithSource());

  // The second host should pool to the existing connection. Although the
  // addresses are not associated with ALPNs, the default connection flow for
  // HTTPS is compatible with HTTP/2.
  EXPECT_TRUE(TryCreateAliasedSpdySession(spdy_session_pool_, test_hosts[1].key,
                                          test_hosts[1].endpoints));
  base::WeakPtr<SpdySession> session1 =
      spdy_session_pool_->FindAvailableSession(
          test_hosts[1].key, /*enable_ip_based_pooling=*/true,
          /*is_websocket=*/false,
          NetLogWithSource::Make(NetLogSourceType::NONE));
  EXPECT_EQ(session0.get(), session1.get());

  // The third host should also pool to the existing connection.
  EXPECT_TRUE(TryCreateAliasedSpdySession(spdy_session_pool_, test_hosts[2].key,
                                          test_hosts[2].endpoints));
  base::WeakPtr<SpdySession> session2 =
      spdy_session_pool_->FindAvailableSession(
          test_hosts[2].key, /*enable_ip_based_pooling=*/true,
          /*is_websocket=*/false,
          NetLogWithSource::Make(NetLogSourceType::NONE));
  EXPECT_EQ(session0.get(), session2.get());

  // The fourth host should not pool. The only matching endpoint is specific to
  // QUIC.
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, test_hosts[3].key, test_hosts[3].endpoints));
}

TEST_F(SpdySessionPoolTest, IPPoolingDisabled) {
  // Define two hosts with identical IP address.
  constexpr int kTestPort = 443;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
  } test_hosts[] = {
      {"www.example.org", "192.168.0.1"},
      {"mail.example.org", "192.168.0.1"},
  };

  // Populate the HostResolver cache.
  session_deps_.host_resolver->set_synchronous_mode(true);
  for (auto& test_host : test_hosts) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_host.name, test_host.iplist, std::string());

    test_host.key = SpdySessionKey(
        HostPortPair(test_host.name, kTestPort), PRIVACY_MODE_DISABLED,
        ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
        NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        /*disable_cert_verification_network_fetches=*/false);
  }

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  MockRead reads1[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  StaticSocketDataProvider data1(reads1, base::span<MockWrite>());
  MockConnect connect_data1(SYNCHRONOUS, OK);
  data1.set_connect_data(connect_data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  AddSSLSocketData();

  CreateNetworkSession();

  // Open SpdySession to the first host.
  base::WeakPtr<SpdySession> session0 = CreateSpdySession(
      http_session_.get(), test_hosts[0].key, NetLogWithSource());

  // |test_hosts[1]| should pool to the existing connection.
  EXPECT_TRUE(TryCreateAliasedSpdySession(spdy_session_pool_, test_hosts[1].key,
                                          test_hosts[1].iplist));
  base::WeakPtr<SpdySession> session1 =
      spdy_session_pool_->FindAvailableSession(
          test_hosts[1].key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false, NetLogWithSource());
  EXPECT_EQ(session0.get(), session1.get());

  // A request to the second host should not pool to the existing connection if
  // IP based pooling is disabled.
  session1 = spdy_session_pool_->FindAvailableSession(
      test_hosts[1].key, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ false, NetLogWithSource());
  EXPECT_FALSE(session1);

  // It should be possible to open a new SpdySession, even if a previous call to
  // FindAvailableSession() linked the second key to the first connection in the
  // IP pooled bucket of SpdySessionPool::available_session_map_.
  session1 = CreateSpdySessionWithIpBasedPoolingDisabled(
      http_session_.get(), test_hosts[1].key, NetLogWithSource());
  EXPECT_TRUE(session1);
  EXPECT_NE(session0.get(), session1.get());
}

// Verifies that an SSL connection with client authentication disables SPDY IP
// pooling.
TEST_F(SpdySessionPoolTest, IPPoolingClientCert) {
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.ssl_info.cert = X509Certificate::CreateFromBytes(webkit_der);
  ASSERT_TRUE(ssl.ssl_info.cert);
  ssl.ssl_info.client_cert_sent = true;
  ssl.next_proto = kProtoHTTP2;
  RunIPPoolingDisabledTest(&ssl);
}

namespace {
enum class ChangeType {
  kIpAddress = 0,
  kSSLConfig,
  kCertDatabase,
  kCertVerifier
};

class SpdySessionGoAwayOnChangeTest
    : public SpdySessionPoolTest,
      public ::testing::WithParamInterface<ChangeType> {
 public:
  void SetUp() override {
    SpdySessionPoolTest::SetUp();

    if (GetParam() == ChangeType::kIpAddress) {
      session_deps_.go_away_on_ip_change = true;
    }
  }

  void SimulateChange() {
    switch (GetParam()) {
      case ChangeType::kIpAddress:
        spdy_session_pool_->OnIPAddressChanged();
        break;
      case ChangeType::kSSLConfig:
        session_deps_.ssl_config_service->NotifySSLContextConfigChange();
        break;
      case ChangeType::kCertDatabase:
        // TODO(mattm): For more realistic testing this should call
        // `CertDatabase::GetInstance()->NotifyObserversCertDBChanged()`,
        // however that delivers notifications asynchronously, and running
        // the message loop to allow the notification to be delivered allows
        // other parts of the tested code to advance, breaking the test
        // expectations.
        spdy_session_pool_->OnSSLConfigChanged(
            SSLClientContext::SSLConfigChangeType::kCertDatabaseChanged);
        break;
      case ChangeType::kCertVerifier:
        session_deps_.cert_verifier->SimulateOnCertVerifierChanged();
        break;
    }
  }

  Error ExpectedNetError() const {
    switch (GetParam()) {
      case ChangeType::kIpAddress:
        return ERR_NETWORK_CHANGED;
      case ChangeType::kSSLConfig:
        return ERR_NETWORK_CHANGED;
      case ChangeType::kCertDatabase:
        return ERR_CERT_DATABASE_CHANGED;
      case ChangeType::kCertVerifier:
        return ERR_CERT_VERIFIER_CHANGED;
    }
  }
};
}  // namespace

// Construct a Pool with SpdySessions in various availability states. Simulate
// an IP address change. Ensure sessions gracefully shut down. Regression test
// for crbug.com/379469.
TEST_P(SpdySessionGoAwayOnChangeTest, GoAwayOnChange) {
  MockConnect connect_data(SYNCHRONOUS, OK);
  session_deps_.host_resolver->set_synchronous_mode(true);

  // This isn't testing anything having to do with SPDY frames; we
  // can ignore issues of how dependencies are set.  We default to
  // setting them (when doing the appropriate protocol) since that's
  // where we're eventually headed for all HTTP/2 connections.
  SpdyTestUtil spdy_util;

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  spdy::SpdySerializedFrame req(
      spdy_util.ConstructSpdyGet("http://www.example.org", 1, MEDIUM));
  MockWrite writes[] = {CreateMockWrite(req, 1)};

  StaticSocketDataProvider dataA(reads, writes);
  dataA.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&dataA);

  AddSSLSocketData();

  CreateNetworkSession();

  // Set up session A: Going away, but with an active stream.
  const std::string kTestHostA("www.example.org");
  HostPortPair test_host_port_pairA(kTestHostA, 80);
  SpdySessionKey keyA(test_host_port_pairA, PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> sessionA =
      CreateSpdySession(http_session_.get(), keyA, NetLogWithSource());

  GURL urlA("http://www.example.org");
  base::WeakPtr<SpdyStream> spdy_streamA = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, sessionA, urlA, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegateA(spdy_streamA);
  spdy_streamA->SetDelegate(&delegateA);

  quiche::HttpHeaderBlock headers(
      spdy_util.ConstructGetHeaderBlock(urlA.spec()));
  spdy_streamA->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();  // Allow headers to write.
  EXPECT_TRUE(delegateA.send_headers_completed());

  sessionA->MakeUnavailable();
  EXPECT_TRUE(sessionA->IsGoingAway());
  EXPECT_FALSE(delegateA.StreamIsClosed());

  // Set up session B: Available, with a created stream.
  StaticSocketDataProvider dataB(reads, writes);
  dataB.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&dataB);

  AddSSLSocketData();

  const std::string kTestHostB("mail.example.org");
  HostPortPair test_host_port_pairB(kTestHostB, 80);
  SpdySessionKey keyB(test_host_port_pairB, PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> sessionB =
      CreateSpdySession(http_session_.get(), keyB, NetLogWithSource());
  EXPECT_TRUE(sessionB->IsAvailable());

  GURL urlB("http://mail.example.org");
  base::WeakPtr<SpdyStream> spdy_streamB = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, sessionB, urlB, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegateB(spdy_streamB);
  spdy_streamB->SetDelegate(&delegateB);

  // Set up session C: Draining.
  StaticSocketDataProvider dataC(reads, writes);
  dataC.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&dataC);

  AddSSLSocketData();

  const std::string kTestHostC("mail.example.com");
  HostPortPair test_host_port_pairC(kTestHostC, 80);
  SpdySessionKey keyC(test_host_port_pairC, PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> sessionC =
      CreateSpdySession(http_session_.get(), keyC, NetLogWithSource());

  sessionC->CloseSessionOnError(ERR_HTTP2_PROTOCOL_ERROR, "Error!");
  EXPECT_TRUE(sessionC->IsDraining());

  SimulateChange();

  EXPECT_TRUE(sessionA->IsGoingAway());
  EXPECT_TRUE(sessionB->IsDraining());
  EXPECT_TRUE(sessionC->IsDraining());

  EXPECT_EQ(1u,
            num_active_streams(sessionA));  // Active stream is still active.
  EXPECT_FALSE(delegateA.StreamIsClosed());

  EXPECT_TRUE(delegateB.StreamIsClosed());  // Created stream was closed.
  EXPECT_THAT(delegateB.WaitForClose(), IsError(ExpectedNetError()));

  sessionA->CloseSessionOnError(ERR_ABORTED, "Closing");
  sessionB->CloseSessionOnError(ERR_ABORTED, "Closing");

  EXPECT_TRUE(delegateA.StreamIsClosed());
  EXPECT_THAT(delegateA.WaitForClose(), IsError(ERR_ABORTED));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SpdySessionGoAwayOnChangeTest,
                         testing::Values(ChangeType::kIpAddress,
                                         ChangeType::kSSLConfig,
                                         ChangeType::kCertDatabase,
                                         ChangeType::kCertVerifier));

// Construct a Pool with SpdySessions in various availability states. Simulate
// an IP address change. Ensure sessions gracefully shut down. Regression test
// for crbug.com/379469.
TEST_F(SpdySessionPoolTest, CloseOnIPAddressChanged) {
  MockConnect connect_data(SYNCHRONOUS, OK);
  session_deps_.host_resolver->set_synchronous_mode(true);

  // This isn't testing anything having to do with SPDY frames; we
  // can ignore issues of how dependencies are set.  We default to
  // setting them (when doing the appropriate protocol) since that's
  // where we're eventually headed for all HTTP/2 connections.
  SpdyTestUtil spdy_util;

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  spdy::SpdySerializedFrame req(
      spdy_util.ConstructSpdyGet("http://www.example.org", 1, MEDIUM));
  MockWrite writes[] = {CreateMockWrite(req, 1)};

  StaticSocketDataProvider dataA(reads, writes);
  dataA.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&dataA);

  AddSSLSocketData();

  session_deps_.go_away_on_ip_change = false;
  CreateNetworkSession();

  // Set up session A: Going away, but with an active stream.
  const std::string kTestHostA("www.example.org");
  HostPortPair test_host_port_pairA(kTestHostA, 80);
  SpdySessionKey keyA(test_host_port_pairA, PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> sessionA =
      CreateSpdySession(http_session_.get(), keyA, NetLogWithSource());

  GURL urlA("http://www.example.org");
  base::WeakPtr<SpdyStream> spdy_streamA = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, sessionA, urlA, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegateA(spdy_streamA);
  spdy_streamA->SetDelegate(&delegateA);

  quiche::HttpHeaderBlock headers(
      spdy_util.ConstructGetHeaderBlock(urlA.spec()));
  spdy_streamA->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();  // Allow headers to write.
  EXPECT_TRUE(delegateA.send_headers_completed());

  sessionA->MakeUnavailable();
  EXPECT_TRUE(sessionA->IsGoingAway());
  EXPECT_FALSE(delegateA.StreamIsClosed());

  // Set up session B: Available, with a created stream.
  StaticSocketDataProvider dataB(reads, writes);
  dataB.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&dataB);

  AddSSLSocketData();

  const std::string kTestHostB("mail.example.org");
  HostPortPair test_host_port_pairB(kTestHostB, 80);
  SpdySessionKey keyB(test_host_port_pairB, PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> sessionB =
      CreateSpdySession(http_session_.get(), keyB, NetLogWithSource());
  EXPECT_TRUE(sessionB->IsAvailable());

  GURL urlB("http://mail.example.org");
  base::WeakPtr<SpdyStream> spdy_streamB = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, sessionB, urlB, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegateB(spdy_streamB);
  spdy_streamB->SetDelegate(&delegateB);

  // Set up session C: Draining.
  StaticSocketDataProvider dataC(reads, writes);
  dataC.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&dataC);

  AddSSLSocketData();

  const std::string kTestHostC("mail.example.com");
  HostPortPair test_host_port_pairC(kTestHostC, 80);
  SpdySessionKey keyC(test_host_port_pairC, PRIVACY_MODE_DISABLED,
                      ProxyChain::Direct(), SessionUsage::kDestination,
                      SocketTag(), NetworkAnonymizationKey(),
                      SecureDnsPolicy::kAllow,
                      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> sessionC =
      CreateSpdySession(http_session_.get(), keyC, NetLogWithSource());

  sessionC->CloseSessionOnError(ERR_HTTP2_PROTOCOL_ERROR, "Error!");
  EXPECT_TRUE(sessionC->IsDraining());

  spdy_session_pool_->OnIPAddressChanged();

  EXPECT_TRUE(sessionA->IsDraining());
  EXPECT_TRUE(sessionB->IsDraining());
  EXPECT_TRUE(sessionC->IsDraining());

  // Both streams were closed with an error.
  EXPECT_TRUE(delegateA.StreamIsClosed());
  EXPECT_THAT(delegateA.WaitForClose(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_TRUE(delegateB.StreamIsClosed());
  EXPECT_THAT(delegateB.WaitForClose(), IsError(ERR_NETWORK_CHANGED));
}

// Regression test for https://crbug.com/789791.
TEST_F(SpdySessionPoolTest, HandleIPAddressChangeThenShutdown) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  SpdyTestUtil spdy_util;
  spdy::SpdySerializedFrame req(
      spdy_util.ConstructSpdyGet(kDefaultUrl, 1, MEDIUM));
  MockWrite writes[] = {CreateMockWrite(req, 1)};
  StaticSocketDataProvider data(reads, writes);

  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();

  const GURL url(kDefaultUrl);
  SpdySessionKey key(HostPortPair::FromURL(url), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), key, NetLogWithSource());

  base::WeakPtr<SpdyStream> spdy_stream = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session, url, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util.ConstructGetHeaderBlock(url.spec()));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(delegate.send_headers_completed());

  spdy_session_pool_->OnIPAddressChanged();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
  EXPECT_EQ(1u, num_active_streams(session));
  EXPECT_TRUE(session->IsGoingAway());
  EXPECT_FALSE(session->IsDraining());
#else
  EXPECT_EQ(0u, num_active_streams(session));
  EXPECT_FALSE(session->IsGoingAway());
  EXPECT_TRUE(session->IsDraining());
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)

  http_session_.reset();

  data.AllReadDataConsumed();
  data.AllWriteDataConsumed();
}

// Regression test for https://crbug.com/789791.
TEST_F(SpdySessionPoolTest, HandleGracefulGoawayThenShutdown) {
  SpdyTestUtil spdy_util;
  spdy::SpdySerializedFrame goaway(spdy_util.ConstructSpdyGoAway(
      0x7fffffff, spdy::ERROR_CODE_NO_ERROR, "Graceful shutdown."));
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), CreateMockRead(goaway, 2),
      MockRead(ASYNC, ERR_IO_PENDING, 3), MockRead(ASYNC, OK, 4)};
  spdy::SpdySerializedFrame req(
      spdy_util.ConstructSpdyGet(kDefaultUrl, 1, MEDIUM));
  MockWrite writes[] = {CreateMockWrite(req, 0)};
  SequencedSocketData data(reads, writes);

  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();

  CreateNetworkSession();

  const GURL url(kDefaultUrl);
  SpdySessionKey key(HostPortPair::FromURL(url), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), key, NetLogWithSource());

  base::WeakPtr<SpdyStream> spdy_stream = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session, url, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util.ConstructGetHeaderBlock(url.spec()));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  // Send headers.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(delegate.send_headers_completed());

  EXPECT_EQ(1u, num_active_streams(session));
  EXPECT_FALSE(session->IsGoingAway());
  EXPECT_FALSE(session->IsDraining());

  // Read GOAWAY.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, num_active_streams(session));
  EXPECT_TRUE(session->IsGoingAway());
  EXPECT_FALSE(session->IsDraining());

  http_session_.reset();

  data.AllReadDataConsumed();
  data.AllWriteDataConsumed();
}

TEST_F(SpdySessionPoolTest, IPConnectionPoolingWithWebSockets) {
  // Define two hosts with identical IP address.
  const int kTestPort = 443;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
  } test_hosts[] = {
      {"www.example.org", "192.168.0.1"},
      {"mail.example.org", "192.168.0.1"},
  };

  // Populate the HostResolver cache.
  session_deps_.host_resolver->set_synchronous_mode(true);
  for (auto& test_host : test_hosts) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_host.name, test_host.iplist, std::string());

    test_host.key = SpdySessionKey(
        HostPortPair(test_host.name, kTestPort), PRIVACY_MODE_DISABLED,
        ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
        NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        /*disable_cert_verification_network_fetches=*/false);
  }

  SpdyTestUtil spdy_util;

  spdy::SpdySerializedFrame req(
      spdy_util.ConstructSpdyGet(nullptr, 0, 1, LOWEST));
  spdy::SpdySerializedFrame settings_ack(spdy_util.ConstructSpdySettingsAck());
  MockWrite writes[] = {CreateMockWrite(req, 0),
                        CreateMockWrite(settings_ack, 2)};

  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame(
      spdy_util.ConstructSpdySettings(settings));
  spdy::SpdySerializedFrame resp(
      spdy_util.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame body(spdy_util.ConstructSpdyDataFrame(1, true));
  MockRead reads[] = {CreateMockRead(settings_frame, 1),
                      CreateMockRead(resp, 3), CreateMockRead(body, 4),
                      MockRead(ASYNC, ERR_IO_PENDING, 5),
                      MockRead(ASYNC, 0, 6)};

  SequencedSocketData data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  AddSSLSocketData();
  CreateNetworkSession();

  // Create a connection to the first host.
  base::WeakPtr<SpdySession> session = CreateSpdySession(
      http_session_.get(), test_hosts[0].key, NetLogWithSource());

  // SpdySession does not support Websocket before SETTINGS frame is read.
  EXPECT_FALSE(session->support_websocket());
  NetLogWithSource net_log_with_source{
      NetLogWithSource::Make(NetLogSourceType::NONE)};
  // TryCreateAliasedSpdySession should not find |session| for either
  // SpdySessionKeys if |is_websocket| argument is set.
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, test_hosts[0].key, test_hosts[0].iplist,
      /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ true));
  EXPECT_FALSE(TryCreateAliasedSpdySession(
      spdy_session_pool_, test_hosts[1].key, test_hosts[1].iplist,
      /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ true));

  // Start request that triggers reading the SETTINGS frame.
  const GURL url(kDefaultUrl);
  base::WeakPtr<SpdyStream> spdy_stream = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  quiche::HttpHeaderBlock headers(
      spdy_util.ConstructGetHeaderBlock(url.spec()));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  // Now SpdySession has read the SETTINGS frame and thus supports Websocket.
  EXPECT_TRUE(session->support_websocket());

  // FindAvailableSession() on the first host should now find the existing
  // session with websockets enabled, and TryCreateAliasedSpdySession() should
  // now set up aliases for |session| for the second one.
  base::WeakPtr<SpdySession> result = spdy_session_pool_->FindAvailableSession(
      test_hosts[0].key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ true, net_log_with_source);
  EXPECT_EQ(session.get(), result.get());
  EXPECT_TRUE(TryCreateAliasedSpdySession(spdy_session_pool_, test_hosts[1].key,
                                          test_hosts[1].iplist,
                                          /* enable_ip_based_pooling = */ true,
                                          /* is_websocket = */ true));

  // FindAvailableSession() should return |session| for either SpdySessionKeys
  // when IP based pooling is enabled.
  result = spdy_session_pool_->FindAvailableSession(
      test_hosts[0].key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ true, net_log_with_source);
  EXPECT_EQ(session.get(), result.get());
  result = spdy_session_pool_->FindAvailableSession(
      test_hosts[1].key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ true, net_log_with_source);
  EXPECT_EQ(session.get(), result.get());

  // FindAvailableSession() should only return |session| for the first
  // SpdySessionKey when IP based pooling is disabled.
  result = spdy_session_pool_->FindAvailableSession(
      test_hosts[0].key, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ true, net_log_with_source);
  EXPECT_EQ(session.get(), result.get());
  result = spdy_session_pool_->FindAvailableSession(
      test_hosts[1].key, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ true, net_log_with_source);
  EXPECT_FALSE(result);

  // Read EOF.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

class TestOnRequestDeletedCallback {
 public:
  TestOnRequestDeletedCallback() = default;

  TestOnRequestDeletedCallback(const TestOnRequestDeletedCallback&) = delete;
  TestOnRequestDeletedCallback& operator=(const TestOnRequestDeletedCallback&) =
      delete;

  ~TestOnRequestDeletedCallback() = default;

  base::RepeatingClosure Callback() {
    return base::BindRepeating(&TestOnRequestDeletedCallback::OnRequestDeleted,
                               base::Unretained(this));
  }

  bool invoked() const { return invoked_; }

  void WaitUntilInvoked() { run_loop_.Run(); }

  void SetRequestDeletedCallback(base::OnceClosure request_deleted_callback) {
    DCHECK(!request_deleted_callback_);
    request_deleted_callback_ = std::move(request_deleted_callback);
  }

 private:
  void OnRequestDeleted() {
    EXPECT_FALSE(invoked_);
    invoked_ = true;
    if (request_deleted_callback_) {
      std::move(request_deleted_callback_).Run();
    }
    run_loop_.Quit();
  }

  bool invoked_ = false;
  base::RunLoop run_loop_;

  base::OnceClosure request_deleted_callback_;
};

class TestRequestDelegate
    : public SpdySessionPool::SpdySessionRequest::Delegate {
 public:
  TestRequestDelegate() = default;

  TestRequestDelegate(const TestRequestDelegate&) = delete;
  TestRequestDelegate& operator=(const TestRequestDelegate&) = delete;

  ~TestRequestDelegate() override = default;

  // SpdySessionPool::SpdySessionRequest::Delegate implementation:
  void OnSpdySessionAvailable(
      base::WeakPtr<SpdySession> spdy_session) override {}
};

TEST_F(SpdySessionPoolTest, RequestSessionWithNoSessions) {
  const SpdySessionKey kSessionKey(
      HostPortPair("foo.test", 443), PRIVACY_MODE_DISABLED,
      ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);

  CreateNetworkSession();

  // First request. Its request deleted callback should never be invoked.
  TestOnRequestDeletedCallback request_deleted_callback1;
  TestRequestDelegate request_delegate1;
  std::unique_ptr<SpdySessionPool::SpdySessionRequest> spdy_session_request1;
  bool is_first_request_for_session;
  EXPECT_FALSE(spdy_session_pool_->RequestSession(
      kSessionKey, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ false, NetLogWithSource(),
      request_deleted_callback1.Callback(), &request_delegate1,
      &spdy_session_request1, &is_first_request_for_session));
  EXPECT_TRUE(is_first_request_for_session);

  // Second request.
  TestOnRequestDeletedCallback request_deleted_callback2;
  TestRequestDelegate request_delegate2;
  std::unique_ptr<SpdySessionPool::SpdySessionRequest> spdy_session_request2;
  EXPECT_FALSE(spdy_session_pool_->RequestSession(
      kSessionKey, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ false, NetLogWithSource(),
      request_deleted_callback2.Callback(), &request_delegate2,
      &spdy_session_request2, &is_first_request_for_session));
  EXPECT_FALSE(is_first_request_for_session);

  // Third request.
  TestOnRequestDeletedCallback request_deleted_callback3;
  TestRequestDelegate request_delegate3;
  std::unique_ptr<SpdySessionPool::SpdySessionRequest> spdy_session_request3;
  EXPECT_FALSE(spdy_session_pool_->RequestSession(
      kSessionKey, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ false, NetLogWithSource(),
      request_deleted_callback3.Callback(), &request_delegate3,
      &spdy_session_request3, &is_first_request_for_session));
  EXPECT_FALSE(is_first_request_for_session);

  // Destroying the second request shouldn't cause anything to happen.
  spdy_session_request2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_deleted_callback1.invoked());
  EXPECT_FALSE(request_deleted_callback2.invoked());
  EXPECT_FALSE(request_deleted_callback3.invoked());

  // But destroying the first request should cause the second and third
  // callbacks to be invoked.
  spdy_session_request1.reset();
  request_deleted_callback2.WaitUntilInvoked();
  request_deleted_callback3.WaitUntilInvoked();
  EXPECT_FALSE(request_deleted_callback1.invoked());

  // Nothing should happen when the third request is destroyed.
  spdy_session_request3.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_deleted_callback1.invoked());
}

TEST_F(SpdySessionPoolTest, RequestSessionDuringNotification) {
  const SpdySessionKey kSessionKey(
      HostPortPair("foo.test", 443), PRIVACY_MODE_DISABLED,
      ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);

  CreateNetworkSession();

  // First request. Its request deleted callback should never be invoked.
  TestOnRequestDeletedCallback request_deleted_callback1;
  TestRequestDelegate request_delegate1;
  std::unique_ptr<SpdySessionPool::SpdySessionRequest> spdy_session_request1;
  bool is_first_request_for_session;
  EXPECT_FALSE(spdy_session_pool_->RequestSession(
      kSessionKey, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ false, NetLogWithSource(),
      request_deleted_callback1.Callback(), &request_delegate1,
      &spdy_session_request1, &is_first_request_for_session));
  EXPECT_TRUE(is_first_request_for_session);

  // Second request.
  TestOnRequestDeletedCallback request_deleted_callback2;
  TestRequestDelegate request_delegate2;
  std::unique_ptr<SpdySessionPool::SpdySessionRequest> spdy_session_request2;
  EXPECT_FALSE(spdy_session_pool_->RequestSession(
      kSessionKey, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ false, NetLogWithSource(),
      request_deleted_callback2.Callback(), &request_delegate2,
      &spdy_session_request2, &is_first_request_for_session));
  EXPECT_FALSE(is_first_request_for_session);

  TestOnRequestDeletedCallback request_deleted_callback3;
  TestRequestDelegate request_delegate3;
  std::unique_ptr<SpdySessionPool::SpdySessionRequest> spdy_session_request3;
  TestOnRequestDeletedCallback request_deleted_callback4;
  TestRequestDelegate request_delegate4;
  std::unique_ptr<SpdySessionPool::SpdySessionRequest> spdy_session_request4;
  request_deleted_callback2.SetRequestDeletedCallback(
      base::BindLambdaForTesting([&]() {
        // Third request. It should again be marked as the first request for the
        // session, since it's only created after the original two have been
        // removed.
        bool is_first_request_for_session;
        EXPECT_FALSE(spdy_session_pool_->RequestSession(
            kSessionKey, /* enable_ip_based_pooling = */ false,
            /* is_websocket = */ false, NetLogWithSource(),
            request_deleted_callback3.Callback(), &request_delegate3,
            &spdy_session_request3, &is_first_request_for_session));
        EXPECT_TRUE(is_first_request_for_session);

        // Fourth request.
        EXPECT_FALSE(spdy_session_pool_->RequestSession(
            kSessionKey, /* enable_ip_based_pooling = */ false,
            /* is_websocket = */ false, NetLogWithSource(),
            request_deleted_callback4.Callback(), &request_delegate4,
            &spdy_session_request4, &is_first_request_for_session));
        EXPECT_FALSE(is_first_request_for_session);
      }));

  // Destroying the first request should cause the second callback to be
  // invoked, and the third and fourth request to be made.
  spdy_session_request1.reset();
  request_deleted_callback2.WaitUntilInvoked();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_deleted_callback1.invoked());
  EXPECT_FALSE(request_deleted_callback3.invoked());
  EXPECT_FALSE(request_deleted_callback4.invoked());
  EXPECT_TRUE(spdy_session_request3);
  EXPECT_TRUE(spdy_session_request4);

  // Destroying the third request should cause the fourth callback to be
  // invoked.
  spdy_session_request3.reset();
  request_deleted_callback4.WaitUntilInvoked();
  EXPECT_FALSE(request_deleted_callback1.invoked());
  EXPECT_FALSE(request_deleted_callback3.invoked());
}

static const char kSSLServerTestHost[] = "config-changed.test";

static const struct {
  const char* url;
  const char* proxy_pac_string;
  bool expect_invalidated;
} kSSLServerTests[] = {
    // If the host and port match, the session should be invalidated.
    {"https://config-changed.test", "DIRECT", true},
    // If host and port do not match, the session should not be invalidated.
    {"https://mail.config-changed.test", "DIRECT", false},
    {"https://config-changed.test:444", "DIRECT", false},
    // If the proxy matches, the session should be invalidated independent of
    // the host.
    {"https://config-changed.test", "HTTPS config-changed.test:443", true},
    {"https://mail.config-changed.test", "HTTPS config-changed.test:443", true},
    // HTTP and SOCKS proxies do not have client certificates.
    {"https://mail.config-changed.test", "PROXY config-changed.test:443",
     false},
    {"https://mail.config-changed.test", "SOCKS5 config-changed.test:443",
     false},
    // The proxy host and port must match.
    {"https://mail.config-changed.test", "HTTPS mail.config-changed.test:443",
     false},
    {"https://mail.config-changed.test", "HTTPS config-changed.test:444",
     false},
};

// Tests the OnSSLConfigForServersChanged() method matches SpdySessions as
// expected.
TEST_F(SpdySessionPoolTest, SSLConfigForServerChanged) {
  const MockConnect connect_data(SYNCHRONOUS, OK);
  const MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  std::vector<std::unique_ptr<StaticSocketDataProvider>> socket_data;
  size_t num_tests = std::size(kSSLServerTests);
  for (size_t i = 0; i < num_tests; i++) {
    socket_data.push_back(std::make_unique<StaticSocketDataProvider>(
        reads, base::span<MockWrite>()));
    socket_data.back()->set_connect_data(connect_data);
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data.back().get());
    AddSSLSocketData();
  }

  CreateNetworkSession();

  std::vector<base::WeakPtr<SpdySession>> sessions;
  for (size_t i = 0; i < num_tests; i++) {
    SpdySessionKey key(
        HostPortPair::FromURL(GURL(kSSLServerTests[i].url)),
        PRIVACY_MODE_DISABLED,
        PacResultElementToProxyChain(kSSLServerTests[i].proxy_pac_string),
        SessionUsage::kDestination, SocketTag(), NetworkAnonymizationKey(),
        SecureDnsPolicy::kAllow,
        /*disable_cert_verification_network_fetches=*/false);
    sessions.push_back(
        CreateSpdySession(http_session_.get(), key, NetLogWithSource()));
  }

  // All sessions are available.
  for (size_t i = 0; i < num_tests; i++) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(sessions[i]->IsAvailable());
  }

  spdy_session_pool_->OnSSLConfigForServersChanged(
      {HostPortPair(kSSLServerTestHost, 443)});
  base::RunLoop().RunUntilIdle();

  // Sessions were inactive, so the unavailable sessions are closed.
  for (size_t i = 0; i < num_tests; i++) {
    SCOPED_TRACE(i);
    if (kSSLServerTests[i].expect_invalidated) {
      EXPECT_FALSE(sessions[i]);
    } else {
      ASSERT_TRUE(sessions[i]);
      EXPECT_TRUE(sessions[i]->IsAvailable());
    }
  }
}

// Tests the OnSSLConfigForServersChanged() method matches SpdySessions
// containing proxy chains.
// TODO(crbug.com/365771838): Add tests for non-ip protection nested proxy
// chains if support is enabled for all builds.
TEST_F(SpdySessionPoolTest, SSLConfigForServerChangedWithProxyChain) {
  const MockConnect connect_data(SYNCHRONOUS, OK);
  const MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  auto proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "proxya", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "proxyb", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "proxyc", 443),
  });

  std::vector<std::unique_ptr<StaticSocketDataProvider>> socket_data;
  socket_data.push_back(std::make_unique<StaticSocketDataProvider>(
      reads, base::span<MockWrite>()));
  socket_data.back()->set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(socket_data.back().get());
  AddSSLSocketData();

  CreateNetworkSession();

  SpdySessionKey key(HostPortPair::FromURL(GURL("https://example.com")),
                     PRIVACY_MODE_DISABLED, proxy_chain,
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), key, NetLogWithSource());

  EXPECT_TRUE(session->IsAvailable());

  spdy_session_pool_->OnSSLConfigForServersChanged(
      {HostPortPair("proxyb", 443)});
  base::RunLoop().RunUntilIdle();

  // The unavailable session is closed.
  EXPECT_FALSE(session);
}

// Tests the OnSSLConfigForServersChanged() method when there are streams open.
TEST_F(SpdySessionPoolTest, SSLConfigForServerChangedWithStreams) {
  // Set up a SpdySession with an active, created, and pending stream.
  SpdyTestUtil spdy_util;
  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = 2;
  spdy::SpdySerializedFrame settings_frame =
      spdy_util.ConstructSpdySettings(settings);
  spdy::SpdySerializedFrame settings_ack = spdy_util.ConstructSpdySettingsAck();
  spdy::SpdySerializedFrame req(
      spdy_util.ConstructSpdyGet(nullptr, 0, 1, MEDIUM));

  const MockConnect connect_data(SYNCHRONOUS, OK);
  const MockRead reads[] = {
      CreateMockRead(settings_frame),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  const MockWrite writes[] = {
      CreateMockWrite(settings_ack),
      CreateMockWrite(req),
  };

  StaticSocketDataProvider socket_data(reads, writes);
  socket_data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&socket_data);
  AddSSLSocketData();

  CreateNetworkSession();

  const GURL url(kDefaultUrl);
  SpdySessionKey key(HostPortPair::FromURL(url), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), key, NetLogWithSource());

  // Pick up the SETTINGS frame to update SETTINGS_MAX_CONCURRENT_STREAMS.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, max_concurrent_streams(session));

  // The first two stream requests should succeed.
  base::WeakPtr<SpdyStream> active_stream = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session, url, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing active_stream_delegate(active_stream);
  active_stream->SetDelegate(&active_stream_delegate);
  base::WeakPtr<SpdyStream> created_stream = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session, url, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing created_stream_delegate(created_stream);
  created_stream->SetDelegate(&created_stream_delegate);

  // The third will block.
  TestCompletionCallback callback;
  SpdyStreamRequest stream_request;
  EXPECT_THAT(
      stream_request.StartRequest(SPDY_REQUEST_RESPONSE_STREAM, session, url,
                                  /*can_send_early=*/false, MEDIUM, SocketTag(),
                                  NetLogWithSource(), callback.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS),
      IsError(ERR_IO_PENDING));

  // Activate the first stream by sending data.
  quiche::HttpHeaderBlock headers(
      spdy_util.ConstructGetHeaderBlock(url.spec()));
  active_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);
  base::RunLoop().RunUntilIdle();

  // The active stream should now have a stream ID.
  EXPECT_EQ(1u, active_stream->stream_id());
  EXPECT_EQ(spdy::kInvalidStreamId, created_stream->stream_id());
  EXPECT_TRUE(session->is_active());
  EXPECT_TRUE(session->IsAvailable());

  spdy_session_pool_->OnSSLConfigForServersChanged(
      {HostPortPair::FromURL(url)});
  base::RunLoop().RunUntilIdle();

  // The active stream is still alive, so the session is still active.
  ASSERT_TRUE(session);
  EXPECT_TRUE(session->is_active());
  ASSERT_TRUE(active_stream);

  // The session is no longer available.
  EXPECT_FALSE(session->IsAvailable());
  EXPECT_TRUE(session->IsGoingAway());

  // The pending and created stream are cancelled.
  // TODO(crbug.com/40768859): Ideally, this would be recoverable.
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_THAT(created_stream_delegate.WaitForClose(),
              IsError(ERR_NETWORK_CHANGED));

  // Close the active stream.
  active_stream->Close();
  // TODO(crbug.com/41469912): The invalidated session should be closed
  // after a RunUntilIdle(), but it is not.
}

// Tests the OnSSLConfigForServersChanged() method when there only pending
// streams active.
TEST_F(SpdySessionPoolTest, SSLConfigForServerChangedWithOnlyPendingStreams) {
  // Set up a SpdySession that accepts no streams.
  SpdyTestUtil spdy_util;
  spdy::SettingsMap settings;
  settings[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] = 0;
  spdy::SpdySerializedFrame settings_frame =
      spdy_util.ConstructSpdySettings(settings);
  spdy::SpdySerializedFrame settings_ack = spdy_util.ConstructSpdySettingsAck();

  const MockConnect connect_data(SYNCHRONOUS, OK);
  const MockRead reads[] = {
      CreateMockRead(settings_frame),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  const MockWrite writes[] = {
      CreateMockWrite(settings_ack),
  };

  StaticSocketDataProvider socket_data(reads, writes);
  socket_data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&socket_data);
  AddSSLSocketData();

  CreateNetworkSession();

  const GURL url(kDefaultUrl);
  SpdySessionKey key(HostPortPair::FromURL(url), PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), key, NetLogWithSource());

  // Pick up the SETTINGS frame to update SETTINGS_MAX_CONCURRENT_STREAMS.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, max_concurrent_streams(session));

  // Create a stream. It should block on the stream limit.
  TestCompletionCallback callback;
  SpdyStreamRequest stream_request;
  ASSERT_THAT(
      stream_request.StartRequest(SPDY_REQUEST_RESPONSE_STREAM, session, url,
                                  /*can_send_early=*/false, MEDIUM, SocketTag(),
                                  NetLogWithSource(), callback.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS),
      IsError(ERR_IO_PENDING));

  spdy_session_pool_->OnSSLConfigForServersChanged(
      {HostPortPair::FromURL(url)});
  base::RunLoop().RunUntilIdle();

  // The pending stream is cancelled.
  // TODO(crbug.com/40768859): Ideally, this would be recoverable.
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_FALSE(session);
}

}  // namespace net
