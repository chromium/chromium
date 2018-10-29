// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include <cstddef>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/dns/host_cache.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_tag.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::trace_event::MemoryAllocatorDump;
using net::test::IsError;
using net::test::IsOk;
using testing::Contains;
using testing::Eq;
using testing::Contains;
using testing::ByRef;

namespace net {

class SpdySessionPoolTest : public TestWithScopedTaskEnvironment {
 protected:
  // Used by RunIPPoolingTest().
  enum SpdyPoolCloseSessionsType {
    SPDY_POOL_CLOSE_SESSIONS_MANUALLY,
    SPDY_POOL_CLOSE_CURRENT_SESSIONS,
    SPDY_POOL_CLOSE_IDLE_SESSIONS,
  };

  SpdySessionPoolTest() : spdy_session_pool_(NULL) {}

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

  size_t num_active_streams(base::WeakPtr<SpdySession> session) {
    return session->active_streams_.size();
  }

  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> http_session_;
  SpdySessionPool* spdy_session_pool_;
  std::vector<std::unique_ptr<SSLSocketDataProvider>> ssl_data_vector_;
};

// A delegate that opens a new session when it is closed.
class SessionOpeningDelegate : public SpdyStream::Delegate {
 public:
  SessionOpeningDelegate(SpdySessionPool* spdy_session_pool,
                         const SpdySessionKey& key)
      : spdy_session_pool_(spdy_session_pool),
        key_(key) {}

  ~SessionOpeningDelegate() override = default;

  void OnHeadersSent() override {}

  void OnHeadersReceived(
      const spdy::SpdyHeaderBlock& response_headers,
      const spdy::SpdyHeaderBlock* pushed_request_headers) override {}

  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override {}

  void OnDataSent() override {}

  void OnTrailers(const spdy::SpdyHeaderBlock& trailers) override {}

  void OnClose(int status) override {
    ignore_result(CreateFakeSpdySession(spdy_session_pool_, key_));
  }

  NetLogSource source_dependency() const override { return NetLogSource(); }

 private:
  SpdySessionPool* const spdy_session_pool_;
  const SpdySessionKey key_;
};

// Set up a SpdyStream to create a new session when it is closed.
// CloseCurrentSessions should not close the newly-created session.
TEST_F(SpdySessionPoolTest, CloseCurrentSessions) {
  const char kTestHost[] = "www.foo.com";
  const int kTestPort = 80;

  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  SpdySessionKey test_key =
      SpdySessionKey(test_host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED, SocketTag());

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
  SpdySessionKey key1(test_host_port_pair1, ProxyServer::Direct(),
                      PRIVACY_MODE_DISABLED, SocketTag());
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
  SpdySessionKey key2(test_host_port_pair2, ProxyServer::Direct(),
                      PRIVACY_MODE_DISABLED, SocketTag());
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
  SpdySessionKey key3(test_host_port_pair3, ProxyServer::Direct(),
                      PRIVACY_MODE_DISABLED, SocketTag());
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
  spdy_session_pool_->CloseCurrentIdleSessions();
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
  spdy_session_pool_->CloseCurrentIdleSessions();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(session1);
  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());
  EXPECT_FALSE(session3);

  // Should not do anything
  spdy_session_pool_->CloseCurrentIdleSessions();
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
  spdy_session_pool_->CloseCurrentIdleSessions();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(session2);
}

// Set up a SpdyStream to create a new session when it is closed.
// CloseAllSessions should close the newly-created session.
TEST_F(SpdySessionPoolTest, CloseAllSessions) {
  const char kTestHost[] = "www.foo.com";
  const int kTestPort = 80;

  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  SpdySessionKey test_key =
      SpdySessionKey(test_host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED, SocketTag());

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

// This test has three variants, one for each style of closing the connection.
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_SESSIONS_MANUALLY,
// the sessions are closed manually, calling SpdySessionPool::Remove() directly.
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_CURRENT_SESSIONS,
// sessions are closed with SpdySessionPool::CloseCurrentSessions().
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_IDLE_SESSIONS,
// sessions are closed with SpdySessionPool::CloseIdleSessions().
void SpdySessionPoolTest::RunIPPoolingTest(
    SpdyPoolCloseSessionsType close_sessions_type) {
  const int kTestPort = 80;
  struct TestHosts {
    std::string url;
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
      {"http://www.example.org", "www.example.org",
       "192.0.2.33,192.168.0.1,192.168.0.5"},
      {"http://mail.example.org", "mail.example.org",
       "192.168.0.2,192.168.0.3,192.168.0.5,192.0.2.33"},
      {"http://mail.example.com", "mail.example.com",
       "192.168.0.4,192.168.0.3"},
  };

  session_deps_.host_resolver->set_synchronous_mode(true);
  for (size_t i = 0; i < arraysize(test_hosts); i++) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    // This test requires that the HostResolver cache be populated.  Normal
    // code would have done this already, but we do it manually.
    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    std::unique_ptr<HostResolver::Request> request;
    int rv = session_deps_.host_resolver->Resolve(
        info, DEFAULT_PRIORITY, &test_hosts[i].addresses,
        CompletionOnceCallback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());

    // Setup a SpdySessionKey.
    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        PRIVACY_MODE_DISABLED, SocketTag());
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
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[2].key));

  // The second host overlaps with the first, and should IP pool.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[1].key));

  // However, if IP pooling is disabled, FindAvailableSession() should not find
  // |session| for the second host.
  base::WeakPtr<SpdySession> session1 =
      spdy_session_pool_->FindAvailableSession(
          test_hosts[1].key, /* enable_ip_based_pooling = */ false,
          /* is_websocket = */ false, NetLogWithSource());
  EXPECT_FALSE(session1);

  // Verify that the second host, through a proxy, won't share the IP.
  SpdySessionKey proxy_key(
      test_hosts[1].key.host_port_pair(),
      ProxyServer::FromPacString("HTTP http://proxy.foo.com/"),
      PRIVACY_MODE_DISABLED, SocketTag());
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, proxy_key));

  // Overlap between 2 and 3 does is not transitive to 1.
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[2].key));

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

  // Expire the host cache
  session_deps_.host_resolver->GetHostCache()->clear();
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
      spdy_session_pool_->CloseCurrentIdleSessions();
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
  const int kTestPort = 443;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
      {"www.example.org", "192.168.0.1"}, {"mail.example.org", "192.168.0.1"},
  };

  // Populate the HostResolver cache.
  session_deps_.host_resolver->set_synchronous_mode(true);
  for (size_t i = 0; i < arraysize(test_hosts); i++) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    std::unique_ptr<HostResolver::Request> request;
    int rv = session_deps_.host_resolver->Resolve(
        info, DEFAULT_PRIORITY, &test_hosts[i].addresses,
        CompletionOnceCallback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());

    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        PRIVACY_MODE_DISABLED, SocketTag());
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

  // A request to the second host should pool to the existing connection.
  BoundTestNetLog net_log;
  base::HistogramTester histogram_tester;
  base::WeakPtr<SpdySession> session1 =
      spdy_session_pool_->FindAvailableSession(
          test_hosts[1].key, /* enable_ip_based_pooling = */ true,
          /* is_websocket = */ false, net_log.bound());
  EXPECT_EQ(session0.get(), session1.get());

  ASSERT_EQ(1u, net_log.GetSize());
  histogram_tester.ExpectTotalCount("Net.SpdySessionGet", 1);

  // A request to the second host should still pool to the existing connection.
  session1 = spdy_session_pool_->FindAvailableSession(
      test_hosts[1].key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ false, net_log.bound());
  EXPECT_EQ(session0.get(), session1.get());

  ASSERT_EQ(2u, net_log.GetSize());
  histogram_tester.ExpectTotalCount("Net.SpdySessionGet", 2);

  // Both FindAvailableSession() calls should log netlog events
  // indicating IP pooling.
  TestNetLogEntry::List entry_list;
  net_log.GetEntries(&entry_list);
  EXPECT_EQ(
      NetLogEventType::HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
      entry_list[0].type);
  EXPECT_EQ(
      NetLogEventType::HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
      entry_list[1].type);

  // Both FindAvailableSession() calls should log histogram entries
  // indicating IP pooling.
  histogram_tester.ExpectUniqueSample("Net.SpdySessionGet", 2, 2);
}

TEST_F(SpdySessionPoolTest, IPPoolingDisabled) {
  // Define two hosts with identical IP address.
  const int kTestPort = 443;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
      {"www.example.org", "192.168.0.1"}, {"mail.example.org", "192.168.0.1"},
  };

  // Populate the HostResolver cache.
  session_deps_.host_resolver->set_synchronous_mode(true);
  for (size_t i = 0; i < arraysize(test_hosts); i++) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    std::unique_ptr<HostResolver::Request> request;
    int rv = session_deps_.host_resolver->Resolve(
        info, DEFAULT_PRIORITY, &test_hosts[i].addresses,
        CompletionOnceCallback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());

    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        PRIVACY_MODE_DISABLED, SocketTag());
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

  // A request to the second host should pool to the existing connection.
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

// Construct a Pool with SpdySessions in various availability states. Simulate
// an IP address change. Ensure sessions gracefully shut down. Regression test
// for crbug.com/379469.
TEST_F(SpdySessionPoolTest, IPAddressChanged) {
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
  SpdySessionKey keyA(test_host_port_pairA, ProxyServer::Direct(),
                      PRIVACY_MODE_DISABLED, SocketTag());
  base::WeakPtr<SpdySession> sessionA =
      CreateSpdySession(http_session_.get(), keyA, NetLogWithSource());

  GURL urlA("http://www.example.org");
  base::WeakPtr<SpdyStream> spdy_streamA = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, sessionA, urlA, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegateA(spdy_streamA);
  spdy_streamA->SetDelegate(&delegateA);

  spdy::SpdyHeaderBlock headers(spdy_util.ConstructGetHeaderBlock(urlA.spec()));
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
  SpdySessionKey keyB(test_host_port_pairB, ProxyServer::Direct(),
                      PRIVACY_MODE_DISABLED, SocketTag());
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
  SpdySessionKey keyC(test_host_port_pairC, ProxyServer::Direct(),
                      PRIVACY_MODE_DISABLED, SocketTag());
  base::WeakPtr<SpdySession> sessionC =
      CreateSpdySession(http_session_.get(), keyC, NetLogWithSource());

  sessionC->CloseSessionOnError(ERR_SPDY_PROTOCOL_ERROR, "Error!");
  EXPECT_TRUE(sessionC->IsDraining());

  spdy_session_pool_->OnIPAddressChanged();

#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
  EXPECT_TRUE(sessionA->IsGoingAway());
  EXPECT_TRUE(sessionB->IsDraining());
  EXPECT_TRUE(sessionC->IsDraining());

  EXPECT_EQ(1u,
            num_active_streams(sessionA));  // Active stream is still active.
  EXPECT_FALSE(delegateA.StreamIsClosed());

  EXPECT_TRUE(delegateB.StreamIsClosed());  // Created stream was closed.
  EXPECT_THAT(delegateB.WaitForClose(), IsError(ERR_NETWORK_CHANGED));

  sessionA->CloseSessionOnError(ERR_ABORTED, "Closing");
  sessionB->CloseSessionOnError(ERR_ABORTED, "Closing");

  EXPECT_TRUE(delegateA.StreamIsClosed());
  EXPECT_THAT(delegateA.WaitForClose(), IsError(ERR_ABORTED));
#else
  EXPECT_TRUE(sessionA->IsDraining());
  EXPECT_TRUE(sessionB->IsDraining());
  EXPECT_TRUE(sessionC->IsDraining());

  // Both streams were closed with an error.
  EXPECT_TRUE(delegateA.StreamIsClosed());
  EXPECT_THAT(delegateA.WaitForClose(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_TRUE(delegateB.StreamIsClosed());
  EXPECT_THAT(delegateB.WaitForClose(), IsError(ERR_NETWORK_CHANGED));
#endif  // defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
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
  SpdySessionKey key(HostPortPair::FromURL(url), ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED, SocketTag());
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), key, NetLogWithSource());

  base::WeakPtr<SpdyStream> spdy_stream = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session, url, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  spdy::SpdyHeaderBlock headers(spdy_util.ConstructGetHeaderBlock(url.spec()));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(delegate.send_headers_completed());

  spdy_session_pool_->OnIPAddressChanged();

#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
  EXPECT_EQ(1u, num_active_streams(session));
  EXPECT_TRUE(session->IsGoingAway());
  EXPECT_FALSE(session->IsDraining());
#else
  EXPECT_EQ(0u, num_active_streams(session));
  EXPECT_FALSE(session->IsGoingAway());
  EXPECT_TRUE(session->IsDraining());
#endif  // defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)

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
  SpdySessionKey key(HostPortPair::FromURL(url), ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED, SocketTag());
  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), key, NetLogWithSource());

  base::WeakPtr<SpdyStream> spdy_stream = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session, url, MEDIUM, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  spdy::SpdyHeaderBlock headers(spdy_util.ConstructGetHeaderBlock(url.spec()));
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

class SpdySessionMemoryDumpTest
    : public SpdySessionPoolTest,
      public testing::WithParamInterface<
          base::trace_event::MemoryDumpLevelOfDetail> {};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    SpdySessionMemoryDumpTest,
    ::testing::Values(base::trace_event::MemoryDumpLevelOfDetail::DETAILED,
                      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND));

TEST_P(SpdySessionMemoryDumpTest, DumpMemoryStats) {
  SpdySessionKey key(HostPortPair("www.example.org", 443),
                     ProxyServer::Direct(), PRIVACY_MODE_DISABLED, SocketTag());

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  base::WeakPtr<SpdySession> session =
      CreateSpdySession(http_session_.get(), key, NetLogWithSource());

  // Flush the SpdySession::OnReadComplete() task.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, key));
  base::trace_event::MemoryDumpArgs dump_args = {GetParam()};
  auto process_memory_dump =
      std::make_unique<base::trace_event::ProcessMemoryDump>(dump_args);
  base::trace_event::MemoryAllocatorDump* parent_dump =
      process_memory_dump->CreateAllocatorDump(
          "net/http_network_session_0x123");
  spdy_session_pool_->DumpMemoryStats(process_memory_dump.get(),
                                      parent_dump->absolute_name());

  // Whether SpdySession::DumpMemoryStats() is invoked.
  bool did_dump = false;
  const base::trace_event::ProcessMemoryDump::AllocatorDumpsMap&
      allocator_dumps = process_memory_dump->allocator_dumps();
  for (const auto& pair : allocator_dumps) {
    const std::string& dump_name = pair.first;
    if (dump_name.find("spdy_session_pool") == std::string::npos)
      continue;
    MemoryAllocatorDump::Entry expected("active_session_count",
                                        MemoryAllocatorDump::kUnitsObjects, 0);
    ASSERT_THAT(pair.second->entries(), Contains(Eq(ByRef(expected))));
    did_dump = true;
  }
  EXPECT_TRUE(did_dump);
  spdy_session_pool_->CloseCurrentSessions(ERR_ABORTED);
}

TEST_F(SpdySessionPoolTest, FindAvailableSessionForWebSocket) {
  // Define two hosts with identical IP address.
  const int kTestPort = 443;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
      {"www.example.org", "192.168.0.1"}, {"mail.example.org", "192.168.0.1"},
  };

  // Populate the HostResolver cache.
  session_deps_.host_resolver->set_synchronous_mode(true);
  for (size_t i = 0; i < arraysize(test_hosts); i++) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    std::unique_ptr<HostResolver::Request> request;
    int rv = session_deps_.host_resolver->Resolve(
        info, DEFAULT_PRIORITY, &test_hosts[i].addresses,
        CompletionOnceCallback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());

    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        PRIVACY_MODE_DISABLED, SocketTag());
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
  BoundTestNetLog net_log;
  // FindAvailableSession should not find |session| for either SpdySessionKeys
  // if |is_websocket| argument is set.
  base::WeakPtr<SpdySession> result = spdy_session_pool_->FindAvailableSession(
      test_hosts[0].key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ true, net_log.bound());
  EXPECT_FALSE(result.get());
  result = spdy_session_pool_->FindAvailableSession(
      test_hosts[1].key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ true, net_log.bound());
  EXPECT_FALSE(result.get());

  // Start request that triggers reading the SETTINGS frame.
  const GURL url(kDefaultUrl);
  base::WeakPtr<SpdyStream> spdy_stream = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, NetLogWithSource());
  test::StreamDelegateDoNothing delegate(spdy_stream);
  spdy_stream->SetDelegate(&delegate);

  spdy::SpdyHeaderBlock headers(spdy_util.ConstructGetHeaderBlock(url.spec()));
  spdy_stream->SendRequestHeaders(std::move(headers), NO_MORE_DATA_TO_SEND);

  base::RunLoop().RunUntilIdle();

  // Now SpdySession has read the SETTINGS frame and thus supports Websocket.
  EXPECT_TRUE(session->support_websocket());

  // FindAvailableSession() should return |session| for either SpdySessionKeys
  // when IP based pooling is enabled.
  result = spdy_session_pool_->FindAvailableSession(
      test_hosts[0].key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ true, net_log.bound());
  EXPECT_EQ(session.get(), result.get());
  result = spdy_session_pool_->FindAvailableSession(
      test_hosts[1].key, /* enable_ip_based_pooling = */ true,
      /* is_websocket = */ true, net_log.bound());
  EXPECT_EQ(session.get(), result.get());

  // FindAvailableSession() should only return |session| for the first
  // SpdySessionKey when IP based pooling is disabled.
  result = spdy_session_pool_->FindAvailableSession(
      test_hosts[0].key, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ true, net_log.bound());
  EXPECT_EQ(session.get(), result.get());
  result = spdy_session_pool_->FindAvailableSession(
      test_hosts[1].key, /* enable_ip_based_pooling = */ false,
      /* is_websocket = */ true, net_log.bound());
  EXPECT_FALSE(result);

  // Read EOF.
  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}
}  // namespace net
