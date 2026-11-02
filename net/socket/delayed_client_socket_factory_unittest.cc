// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/delayed_client_socket_factory.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using net::test::IsError;
using net::test::IsOk;

constexpr base::TimeDelta kLatency = base::Milliseconds(100);

class DelayedClientSocketFactoryTest : public testing::Test,
                                       public WithTaskEnvironment {
 public:
  DelayedClientSocketFactoryTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  AddressList localhost_address_ =
      AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 80));
};

TEST_F(DelayedClientSocketFactoryTest, TransportSocketHasDelay) {
  // Set up the mock factory with connect data.
  MockClientSocketFactory mock_factory;
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  mock_factory.AddSocketDataProvider(&data);

  DelayedSocketConfig config{
      .rtt = kLatency,
  };
  DelayedClientSocketFactory factory(&mock_factory, config);

  auto socket = factory.CreateTransportClientSocket(
      localhost_address_, handles::kInvalidNetworkHandle, nullptr, nullptr,
      NetLog::Get(), NetLogSource());

  // Connect should be delayed by latency.
  base::test::TestFuture<int> callback;
  EXPECT_THAT(socket->Connect(callback.GetCallback()), IsError(ERR_IO_PENDING));

  // Not ready before latency elapses.
  FastForwardBy(kLatency - base::Milliseconds(1));
  EXPECT_FALSE(callback.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(callback.IsReady());
  EXPECT_THAT(callback.Get(), IsOk());
}

// Bind(), SetNoDelay() and SetKeepAlive() configure the real transport, so
// they must reach the inner socket (which the mock reflects in `data`).
TEST_F(DelayedClientSocketFactoryTest, TransportOptionsReachInnerSocket) {
  MockClientSocketFactory mock_factory;
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  mock_factory.AddSocketDataProvider(&data);

  DelayedSocketConfig config{
      .rtt = kLatency,
  };
  DelayedClientSocketFactory factory(&mock_factory, config);

  auto socket = factory.CreateTransportClientSocket(
      localhost_address_, handles::kInvalidNetworkHandle, nullptr, nullptr,
      NetLog::Get(), NetLogSource());

  const IPEndPoint local_address(IPAddress::IPv4Localhost(), 4321);
  EXPECT_THAT(socket->Bind(local_address), IsOk());
  IPEndPoint bound_address;
  ASSERT_THAT(socket->GetLocalAddress(&bound_address), IsOk());
  EXPECT_EQ(bound_address, local_address);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // The inner socket's results are passed through, too.
  data.set_set_no_delay_result(false);
  EXPECT_FALSE(socket->SetNoDelay(false));
  EXPECT_FALSE(data.no_delay());

  EXPECT_TRUE(socket->SetKeepAlive(/*enable=*/true, /*delay_secs=*/45));
  EXPECT_EQ(data.keep_alive_state(),
            SocketDataProvider::KeepAliveState::kEnabled);
  EXPECT_EQ(data.keep_alive_delay(), 45);
}

TEST_F(DelayedClientSocketFactoryTest, ReadHasDelay) {
  MockClientSocketFactory mock_factory;
  const std::string kData = "response data";
  MockRead reads[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  mock_factory.AddSocketDataProvider(&data);

  DelayedSocketConfig config{
      .rtt = kLatency,
  };
  DelayedClientSocketFactory factory(&mock_factory, config);

  auto socket = factory.CreateTransportClientSocket(
      localhost_address_, handles::kInvalidNetworkHandle, nullptr, nullptr,
      NetLog::Get(), NetLogSource());

  // Connect.
  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // A read pays exactly half-RTT propagation delay: not ready a millisecond
  // early (which would also catch a regression to a full-RTT delay being
  // masked by an over-generous wait), ready at half-RTT.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buf.get(), 1024, read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  FastForwardBy(kLatency / 2 - base::Milliseconds(1));
  EXPECT_FALSE(read_cb.IsReady());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(read_cb.IsReady());
  EXPECT_EQ(read_cb.Get(), static_cast<int>(kData.size()));
}

TEST_F(DelayedClientSocketFactoryTest, SSLPassesThrough) {
  // SSL socket creation does not add additional wrapping — the delay lives
  // on the transport socket below SSL, so the TLS handshake round trips
  // naturally pay it.
  MockClientSocketFactory mock_factory;
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  mock_factory.AddSSLSocketDataProvider(&ssl_data);

  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  mock_factory.AddSocketDataProvider(&data);

  DelayedSocketConfig config{
      .rtt = kLatency,
  };
  DelayedClientSocketFactory factory(&mock_factory, config);

  // The transport socket below carries the delay: its connect pays the RTT.
  auto transport = factory.CreateTransportClientSocket(
      localhost_address_, handles::kInvalidNetworkHandle, nullptr, nullptr,
      NetLog::Get(), NetLogSource());
  base::test::TestFuture<int> connect_cb;
  EXPECT_THAT(transport->Connect(connect_cb.GetCallback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Layering SSL on top delegates straight to the inner factory (which
  // consumes the SSL data provider) and completes without further delay.
  MockCertVerifier cert_verifier;
  TransportSecurityState transport_security_state;
  SSLClientContext ssl_context(/*ssl_config_service=*/nullptr, &cert_verifier,
                               &transport_security_state,
                               /*ssl_client_session_cache=*/nullptr,
                               /*sct_auditing_delegate=*/nullptr);
  auto ssl_socket = factory.CreateSSLClientSocket(
      &ssl_context, std::move(transport), HostPortPair("example.com", 443),
      SSLConfig());
  ASSERT_TRUE(ssl_socket);
  TestCompletionCallback ssl_connect_cb;
  EXPECT_THAT(ssl_socket->Connect(ssl_connect_cb.callback()), IsOk());
  EXPECT_TRUE(ssl_data.is_connect_data_consumed);
}

TEST_F(DelayedClientSocketFactoryTest, DatagramIsDelayed) {
  // Datagram sockets are wrapped in DelayedDatagramSocket (sharing this
  // factory's throttles with its stream sockets). A shaped Write() reports
  // synchronous success but the datagram only reaches the wire after the
  // half-RTT propagation delay.
  MockClientSocketFactory mock_factory;
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, "ping")};
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  mock_factory.AddSocketDataProvider(&data);

  DelayedSocketConfig config{
      .rtt = kLatency,
  };
  DelayedClientSocketFactory factory(&mock_factory, config);

  auto socket = factory.CreateDatagramClientSocket(
      DatagramSocket::DEFAULT_BIND, handles::kInvalidNetworkHandle,
      NetLog::Get(), NetLogSource());
  ASSERT_TRUE(socket);
  ASSERT_THAT(socket->Connect(IPEndPoint(IPAddress::IPv4Localhost(), 80)),
              IsOk());

  auto buf = base::MakeRefCounted<StringIOBuffer>("ping");
  EXPECT_EQ(socket->Write(buf.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);
  // Not on the wire yet — the packet is delayed by half-RTT.
  EXPECT_FALSE(data.AllWriteDataConsumed());
  FastForwardBy(kLatency / 2);
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(DelayedClientSocketFactoryTest, SharedBandwidthAcrossSockets) {
  MockClientSocketFactory mock_factory;

  // 100 bytes/sec throughput, 100ms burst = 10 bytes burst.
  DelayedSocketConfig config{
      .rtt = base::TimeDelta(),
      .download_throughput_bytes_per_sec = 100,
  };
  DelayedClientSocketFactory factory(&mock_factory, config);

  // Socket 1: reads 10 bytes (exhausts burst).
  const std::string kData(10, 'x');
  MockRead reads1[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data1(reads1, base::span<MockWrite>());
  data1.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  mock_factory.AddSocketDataProvider(&data1);

  auto socket1 = factory.CreateTransportClientSocket(
      localhost_address_, handles::kInvalidNetworkHandle, nullptr, nullptr,
      NetLog::Get(), NetLogSource());
  base::test::TestFuture<int> cc1;
  socket1->Connect(cc1.GetCallback());
  // No latency, but throttle path is async.
  FastForwardBy(base::TimeDelta());

  auto buf1 = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> rc1;
  EXPECT_THAT(socket1->Read(buf1.get(), 1024, rc1.GetCallback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(base::TimeDelta());
  EXPECT_EQ(rc1.Get(), 10);

  // Socket 2: reads 10 bytes. Burst exhausted by socket 1, so this waits.
  MockRead reads2[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data2(reads2, base::span<MockWrite>());
  data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  mock_factory.AddSocketDataProvider(&data2);

  auto socket2 = factory.CreateTransportClientSocket(
      localhost_address_, handles::kInvalidNetworkHandle, nullptr, nullptr,
      NetLog::Get(), NetLogSource());
  base::test::TestFuture<int> cc2;
  socket2->Connect(cc2.GetCallback());
  FastForwardBy(base::TimeDelta());

  auto buf2 = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> rc2;
  EXPECT_THAT(socket2->Read(buf2.get(), 1024, rc2.GetCallback()),
              IsError(ERR_IO_PENDING));

  // Burst exhausted. 10 bytes at 100 bps = 100ms.
  FastForwardBy(base::Milliseconds(99));
  EXPECT_FALSE(rc2.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(rc2.IsReady());
  EXPECT_EQ(rc2.Get(), 10);
}

TEST_F(DelayedClientSocketFactoryTest, SetConfigUpdatesBehavior) {
  MockClientSocketFactory mock_factory;

  // Start with no delay.
  DelayedSocketConfig config{
      .rtt = base::TimeDelta(),
  };
  DelayedClientSocketFactory factory(&mock_factory, config);

  // Update config to add latency.
  factory.SetConfig({
      .rtt = kLatency,
  });

  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  mock_factory.AddSocketDataProvider(&data);

  auto socket = factory.CreateTransportClientSocket(
      localhost_address_, handles::kInvalidNetworkHandle, nullptr, nullptr,
      NetLog::Get(), NetLogSource());

  // New socket should have the updated latency.
  base::test::TestFuture<int> callback;
  EXPECT_THAT(socket->Connect(callback.GetCallback()), IsError(ERR_IO_PENDING));

  FastForwardBy(kLatency);
  EXPECT_THAT(callback.Get(), IsOk());
}

}  // namespace
}  // namespace net
