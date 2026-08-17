// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/delayed_datagram_socket.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using net::test::IsError;
using net::test::IsOk;
using ::testing::AnyOf;

constexpr base::TimeDelta kLatency = base::Milliseconds(200);
constexpr uint64_t kDownloadThroughput = 1000;  // bytes/sec
constexpr uint64_t kUploadThroughput = 1000;
constexpr base::TimeDelta kHalfRtt = kLatency / 2;

const IPEndPoint kEndpoint(IPAddress::IPv4Localhost(), 443);

// Test payloads of known sizes.
constexpr char kData100[] =
    "0123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789"
    "01234567890123456789";
constexpr char kData50[] = "01234567890123456789012345678901234567890123456789";
constexpr char kData12[] = "client_hello";
constexpr char kData10[] = "0123456789";
constexpr char kData5[] = "01234";
constexpr char kData4[] = "test";

static_assert(sizeof(kData100) - 1 == 100);
static_assert(sizeof(kData50) - 1 == 50);
static_assert(sizeof(kData12) - 1 == 12);
static_assert(sizeof(kData10) - 1 == 10);
static_assert(sizeof(kData5) - 1 == 5);

class DelayedDatagramSocketTest : public testing::Test,
                                  public WithTaskEnvironment {
 public:
  DelayedDatagramSocketTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  // `download_throttle` / `upload_throttle` opt the test socket into shared
  // bandwidth coordination. When left null and the config's throughput is
  // finite, this helper auto-creates a per-socket throttle with that rate
  // and a 1µs burst (≈ strict serial-transmission of the configured
  // bytes/sec), so most existing tests' timing expectations carry over
  // from the original local model without modification.
  std::unique_ptr<DelayedDatagramSocket> CreateSocket(
      SocketDataProvider* data_provider,
      const DelayedSocketConfig& config,
      scoped_refptr<BandwidthThrottle> download_throttle = nullptr,
      scoped_refptr<BandwidthThrottle> upload_throttle = nullptr) {
    if (!download_throttle &&
        config.download_throughput_bytes_per_sec.has_value()) {
      download_throttle = base::MakeRefCounted<BandwidthThrottle>(
          *config.download_throughput_bytes_per_sec, base::Microseconds(1));
    }
    if (!upload_throttle &&
        config.upload_throughput_bytes_per_sec.has_value()) {
      upload_throttle = base::MakeRefCounted<BandwidthThrottle>(
          *config.upload_throughput_bytes_per_sec, base::Microseconds(1));
    }
    auto udp_socket = std::make_unique<MockUDPClientSocket>(
        data_provider, /*net_log=*/nullptr);
    return std::make_unique<DelayedDatagramSocket>(
        std::move(udp_socket), config, std::move(download_throttle),
        std::move(upload_throttle));
  }

  DelayedSocketConfig MakeConfig(
      base::TimeDelta rtt = kLatency,
      std::optional<uint64_t> download_bytes_per_sec = kDownloadThroughput,
      std::optional<uint64_t> upload_bytes_per_sec = kUploadThroughput) {
    return {.rtt = rtt,
            .download_throughput_bytes_per_sec = download_bytes_per_sec,
            .upload_throughput_bytes_per_sec = upload_bytes_per_sec};
  }

  DelayedSocketConfig NoDelayConfig() { return {.rtt = base::TimeDelta()}; }
};

// --- Connect tests ---

TEST_F(DelayedDatagramSocketTest, SyncConnectNoDelay) {
  StaticSocketDataProvider data;
  auto socket = CreateSocket(&data, NoDelayConfig());
  EXPECT_THAT(socket->Connect(kEndpoint), IsOk());
}

TEST_F(DelayedDatagramSocketTest, SyncConnectPassesThroughWithDelay) {
  // Sync Connect() is a local operation; it should not be delayed.
  StaticSocketDataProvider data;
  auto socket = CreateSocket(&data, MakeConfig());
  EXPECT_THAT(socket->Connect(kEndpoint), IsOk());
}

TEST_F(DelayedDatagramSocketTest, ConnectAsyncPassesThroughWithDelay) {
  // UDP connect is local socket setup. The async API shape should not add
  // network delay; packet latency is modeled by Write()/Read().
  StaticSocketDataProvider data;
  auto socket = CreateSocket(&data, MakeConfig());

  TestCompletionCallback callback;
  int rv = socket->ConnectAsync(kEndpoint, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  FastForwardBy(base::TimeDelta());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

// --- Read delay tests ---

TEST_F(DelayedDatagramSocketTest, EveryReadPaysHalfRttPlusThroughput) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData100),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket = CreateSocket(&data, MakeConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(200);
  TestCompletionCallback read_cb;
  int rv = socket->Read(read_buffer.get(), 200, read_cb.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Expected: half-RTT (100ms) + throughput (100 bytes / 1000 B/s = 100ms)
  base::TimeDelta expected = kHalfRtt + base::Milliseconds(100);

  FastForwardBy(expected - base::Milliseconds(1));
  EXPECT_FALSE(read_cb.have_result());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(read_cb.WaitForResult(), 100);
}

TEST_F(DelayedDatagramSocketTest, ConsecutiveReadsSerializeThroughBottleneck) {
  // Packets arriving at the inner socket at the same time are serialized
  // by the simulated bottleneck (the shared BandwidthThrottle): each
  // pays its own wire-time before propagating for half-RTT to the
  // consumer. This matches a real network's bottleneck - back-to-back
  // packets at the link aren't delivered in lockstep; they spread out by
  // N/rate. (The previous wrapper had a per-socket throughput model that
  // mistakenly tagged both packets with the same `ready_at`; the shared
  // throttle is more faithful to a real link.)
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData50),
      MockRead(SYNCHRONOUS, kData50),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket = CreateSocket(&data, MakeConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(200);

  // Read #1: half-RTT (100 ms) + wire-time (50 bytes / 1000 B/s = 50 ms)
  // = 150 ms.
  TestCompletionCallback cb1;
  int rv = socket->Read(buffer.get(), 200, cb1.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  FastForwardBy(base::Milliseconds(150));
  EXPECT_EQ(cb1.WaitForResult(), 50);

  // Read #2: packet 2 was admitted by the throttle 50 ms after packet 1
  // (one packet's wire-time later), so it arrives at the consumer
  // another 50 ms after packet 1 - a total of 200 ms after the first
  // Read was issued, i.e. 50 ms from here.
  TestCompletionCallback cb2;
  EXPECT_THAT(socket->Read(buffer.get(), 200, cb2.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(base::Milliseconds(49));
  EXPECT_FALSE(cb2.have_result());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(cb2.WaitForResult(), 50);
}

TEST_F(DelayedDatagramSocketTest, ReadLatencyOnlyNoThroughput) {
  // With no throughput limit, reads still pay half-RTT.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData50),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency,
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb;
  int rv = socket->Read(buffer.get(), 100, cb.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  FastForwardBy(kHalfRtt - base::Milliseconds(1));
  EXPECT_FALSE(cb.have_result());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(cb.WaitForResult(), 50);
}

// --- Write delay tests ---

TEST_F(DelayedDatagramSocketTest, EveryWritePaysHalfRttPlusThroughput) {
  // Write returns synchronously so the consumer (e.g. QuicPacketWriter)
  // never goes write-blocked, but the bytes are not handed to the inner
  // socket until half-RTT (100 ms) + throughput cost (100 bytes / 1000 B/s
  // = 100 ms) = 200 ms have elapsed.
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kData100),
  };
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  auto socket = CreateSocket(&data, MakeConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto write_buffer = base::MakeRefCounted<StringIOBuffer>(kData100);
  EXPECT_EQ(socket->Write(write_buffer.get(), 100, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            100);

  // Inner socket has not seen the bytes yet.
  EXPECT_FALSE(data.AllWriteDataConsumed());
  FastForwardBy(base::Milliseconds(199));
  EXPECT_FALSE(data.AllWriteDataConsumed());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(DelayedDatagramSocketTest, WriteLatencyOnlyNoThroughput) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kData12),
  };
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency,
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto write_buffer = base::MakeRefCounted<StringIOBuffer>(kData12);
  EXPECT_EQ(socket->Write(write_buffer.get(), 12, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            12);

  FastForwardBy(kHalfRtt - base::Milliseconds(1));
  EXPECT_FALSE(data.AllWriteDataConsumed());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// --- No delay passthrough ---

TEST_F(DelayedDatagramSocketTest, NoDelayPassesThrough) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData5),
  };
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kData5),
  };
  StaticSocketDataProvider data(reads, writes);
  auto socket = CreateSocket(&data, NoDelayConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto write_buffer = base::MakeRefCounted<StringIOBuffer>(kData5);
  TestCompletionCallback write_cb;
  int rv = socket->Write(write_buffer.get(), 5, write_cb.callback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(rv, 5);

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback read_cb;
  rv = socket->Read(read_buffer.get(), 100, read_cb.callback());
  EXPECT_EQ(rv, 5);
}

// --- Error passthrough ---

TEST_F(DelayedDatagramSocketTest, ReadErrorPropagatesAsync) {
  // The inner read error fires through the read-ahead pipeline; the
  // application sees ERR_IO_PENDING and the error arrives on the next
  // task hop.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_CONNECTION_REFUSED),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket = CreateSocket(&data, MakeConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb;
  int rv = socket->Read(buffer.get(), 100, cb.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb.WaitForResult(), IsError(ERR_CONNECTION_REFUSED));
}

TEST_F(DelayedDatagramSocketTest, InnerWriteErrorSilentlyDropped) {
  // The consumer received a synchronous success from Write(); an error from
  // the delayed inner Write must not retroactively surface as a callback or
  // wedge the send pipeline. UDP packet loss is a normal mode for the
  // protocol.
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, ERR_CONNECTION_REFUSED),
  };
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  auto socket = CreateSocket(&data, MakeConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(kData4);
  EXPECT_EQ(socket->Write(buffer.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);

  FastForwardBy(kHalfRtt + base::Seconds(1));
  // The mock saw the failing Write attempt; we just verify nothing crashed
  // and the queue moved on.
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// --- Close cancels timers ---

TEST_F(DelayedDatagramSocketTest, CloseStopsPendingCallbacks) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData4),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket = CreateSocket(&data, MakeConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb;
  int rv = socket->Read(buffer.get(), 100, cb.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  socket->Close();

  FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(cb.have_result());
}

// --- Delegation tests ---

TEST_F(DelayedDatagramSocketTest, GetPeerAddressDelegates) {
  StaticSocketDataProvider data;
  auto socket = CreateSocket(&data, MakeConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  IPEndPoint address;
  EXPECT_THAT(socket->GetPeerAddress(&address), IsOk());
  EXPECT_EQ(address, kEndpoint);
}

// --- Half-RTT model verification ---

TEST_F(DelayedDatagramSocketTest, ReadDelayUsesHalfConfiguredLatency) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData10),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data, MakeConfig(base::Milliseconds(300),
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(200);
  TestCompletionCallback cb;
  int rv = socket->Read(buffer.get(), 200, cb.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Half of 300ms = 150ms.
  FastForwardBy(base::Milliseconds(149));
  EXPECT_FALSE(cb.have_result());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(cb.WaitForResult(), 10);
}

// Verify that a QUIC-like handshake pattern (Write + Read = 1 RTT) has
// correct timing. QUIC's reader and writer operate independently, but
// from a timing perspective: the client sends ClientHello (half-RTT to
// reach server) and then receives ServerHello (half-RTT back) = 1 RTT.
TEST_F(DelayedDatagramSocketTest, QuicHandshakeAccumulatesOneRtt) {
  // SequencedSocketData orders the inner Read after the inner Write, so the
  // mocked "server" only delivers its response once our delayed Write has
  // actually hit the wire. From the consumer's perspective:
  //   Write at T0 (sync return) -> wire send at T0+half-RTT
  //   -> inner Read consumed at T0+half-RTT
  //   -> Read delivered at T0+half-RTT+half-RTT = T0+RTT.
  MockRead reads[] = {
      MockRead(ASYNC, /*seq=*/1, kData12),
      // The read-ahead loop will issue one more inner Read after consuming
      // the response above; make it fail terminally so we don't trip on
      // missing mock data. The outer (consumer) Read has already completed
      // by then, so the error is harmless.
      MockRead(SYNCHRONOUS, ERR_FAILED, /*seq=*/2),
  };
  MockWrite writes[] = {
      MockWrite(ASYNC, /*seq=*/0, kData12),
  };
  SequencedSocketData data(reads, writes);
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency,
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  base::TimeTicks start = base::TimeTicks::Now();

  auto write_buffer = base::MakeRefCounted<StringIOBuffer>(kData12);
  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(100);

  EXPECT_EQ(socket->Write(write_buffer.get(), 12, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            12);

  TestCompletionCallback rcb;
  EXPECT_THAT(socket->Read(read_buffer.get(), 100, rcb.callback()),
              IsError(ERR_IO_PENDING));

  // Half a RTT after the start, the wire send fires but the response hasn't
  // propagated back yet.
  FastForwardBy(kHalfRtt);
  EXPECT_FALSE(rcb.have_result());
  EXPECT_TRUE(data.AllWriteDataConsumed());

  // Another half-RTT later, the response is delivered.
  FastForwardBy(kHalfRtt);
  EXPECT_EQ(rcb.WaitForResult(), 12);

  base::TimeDelta elapsed = base::TimeTicks::Now() - start;
  EXPECT_EQ(elapsed, kLatency);
}

// Verify that concurrent read and write (as QUIC does) both independently
// pay their half-RTT cost.
TEST_F(DelayedDatagramSocketTest, ConcurrentReadAndWritePipelined) {
  // Reads and writes are independent: an in-flight delayed write must not
  // block reads, and vice versa. Read takes half-RTT; Write returns sync
  // and the inner wire send fires at half-RTT.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData50),
  };
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kData50),
  };
  StaticSocketDataProvider data(reads, writes);
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency,
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  auto write_buffer = base::MakeRefCounted<StringIOBuffer>(kData50);

  TestCompletionCallback rcb;
  EXPECT_THAT(socket->Read(read_buffer.get(), 100, rcb.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_EQ(socket->Write(write_buffer.get(), 50, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            50);

  // Wire send and read delivery both fire at half-RTT.
  FastForwardBy(kHalfRtt - base::Milliseconds(1));
  EXPECT_FALSE(rcb.have_result());
  EXPECT_FALSE(data.AllWriteDataConsumed());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(rcb.WaitForResult(), 50);
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// --- Burst arrival doesn't serialize delivery ---

TEST_F(DelayedDatagramSocketTest, BurstArrivalNotSerialized) {
  // A burst of N packets arriving in the kernel close together must NOT be
  // serialized into N consecutive half-RTT delays. The read-ahead pipeline
  // queues each packet with ready_at = arrival + half-RTT, so once the
  // first packet is delivered the rest are immediately available - total
  // delay is O(half-RTT) for the whole burst, not O(N * half-RTT).
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData10),
      MockRead(SYNCHRONOUS, kData10),
      MockRead(SYNCHRONOUS, kData10),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency,
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer1 = base::MakeRefCounted<IOBufferWithSize>(100);
  auto buffer2 = base::MakeRefCounted<IOBufferWithSize>(100);
  auto buffer3 = base::MakeRefCounted<IOBufferWithSize>(100);

  base::TimeTicks start = base::TimeTicks::Now();

  // Read #1 pays the half-RTT from arrival.
  TestCompletionCallback cb1;
  EXPECT_THAT(socket->Read(buffer1.get(), 100, cb1.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kHalfRtt);
  EXPECT_EQ(cb1.WaitForResult(), 10);

  // The other two packets arrived in parallel and have the same ready_at,
  // so they are immediately deliverable - no additional wait.
  TestCompletionCallback cb2;
  EXPECT_EQ(socket->Read(buffer2.get(), 100, cb2.callback()), 10);

  TestCompletionCallback cb3;
  EXPECT_EQ(socket->Read(buffer3.get(), 100, cb3.callback()), 10);

  // Whole burst delivered within a single half-RTT, not 3×.
  EXPECT_EQ(base::TimeTicks::Now() - start, kHalfRtt);
}

// --- Unconstrained throughput doesn't crash ---

TEST_F(DelayedDatagramSocketTest, UnconstrainedThroughputDoesNotCrash) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData10),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  // Unconstrained throughput (std::nullopt) must apply latency only, with
  // no divide-by-zero or throttle wiring.
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency, std::nullopt, std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb;
  int rv = socket->Read(buffer.get(), 100, cb.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Should complete with latency only, no crash.
  FastForwardBy(kHalfRtt);
  EXPECT_EQ(cb.WaitForResult(), 10);
}

// --- Zero-length UDP datagrams are valid empty packets, not EOF ---

TEST_F(DelayedDatagramSocketTest, ZeroLengthDatagramIsDelivered) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, /*data=*/""),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency,
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb;
  EXPECT_THAT(socket->Read(buffer.get(), 100, cb.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kHalfRtt);
  EXPECT_EQ(cb.WaitForResult(), 0);
}

// --- Multi-datagram synchronous delivery does not crash or lose packets ---

TEST_F(DelayedDatagramSocketTest, MultiDatagramSyncDeliveryHasNoLoss) {
  // Regression test for the TryDeliverNextPacket reentrancy bug: when the
  // consumer reads packets one at a time and the inner socket has more
  // packets ready synchronously, the read-ahead loop must not race the
  // outer frame's already-moved pending state.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData10),
      MockRead(SYNCHRONOUS, kData10),
      MockRead(SYNCHRONOUS, kData10),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING),  // park further read-ahead
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency,
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(100);

  // First Read: pending; queue fills from read-ahead. After kHalfRtt all three
  // packets are ready_at the same moment.
  TestCompletionCallback cb1;
  EXPECT_THAT(socket->Read(buffer.get(), 100, cb1.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kHalfRtt);
  EXPECT_EQ(cb1.WaitForResult(), 10);

  // The remaining two packets are delivered synchronously.
  EXPECT_EQ(socket->Read(buffer.get(), 100, base::DoNothing()), 10);
  EXPECT_EQ(socket->Read(buffer.get(), 100, base::DoNothing()), 10);
}

// --- Per-packet sender-side state snapshot is applied before each send ---

// A test DatagramClientSocket that records the interleaving of
// SetTos/SetMsgConfirm/ApplySocketTag calls with Write payloads, so we can
// verify the wrapper restores per-packet state in DrainOneWireSend.
class RecordingDatagramSocket : public DatagramClientSocket {
 public:
  struct Event {
    enum class Kind { kSetTos, kSetMsgConfirm, kApplyTag, kWrite };
    Kind kind;
    DiffServCodePoint dscp = DSCP_NO_CHANGE;
    EcnCodePoint ecn = ECN_NO_CHANGE;
    bool msg_confirm = false;
    std::string write_data;
  };

  RecordingDatagramSocket() = default;
  ~RecordingDatagramSocket() override = default;

  const std::vector<Event>& events() const { return events_; }

  // When set, inner Write() returns ERR_IO_PENDING and parks the callback.
  // Lets a test pin the wrapper's pipeline (inner_write_pending_) so the
  // send queue can be held full.
  void set_write_returns_pending(bool pending) {
    write_returns_pending_ = pending;
  }
  bool HasPendingWrite() const { return !pending_writes_.empty(); }

  // Completes the oldest parked inner Write with its full byte count.
  void CompleteOldestWrite() {
    CHECK(!pending_writes_.empty());
    PendingWrite pending = std::move(pending_writes_.front());
    pending_writes_.pop_front();
    std::move(pending.callback).Run(pending.length);
  }

  // A scripted synchronous inner read. `result` < 0 is an error (data
  // ignored); `result` >= 0 delivers `data` (which must be `result` bytes).
  struct ScriptedRead {
    int result;
    std::string data;
  };
  // Serves `reads` in order from Read(); once exhausted, Read() parks
  // (ERR_IO_PENDING). Unlike MockUDPClientSocket, an error read is NOT
  // sticky - the next Read() advances to the following scripted entry.
  void set_scripted_reads(std::vector<ScriptedRead> reads) {
    scripted_reads_ = std::move(reads);
  }

  // Socket
  int Read(IOBuffer* buf, int buf_len, CompletionOnceCallback) override {
    if (read_index_ >= scripted_reads_.size()) {
      return ERR_IO_PENDING;  // Park once the script is exhausted.
    }
    const ScriptedRead& r = scripted_reads_[read_index_++];
    if (r.result < 0) {
      return r.result;
    }
    int n = std::min(buf_len, static_cast<int>(r.data.size()));
    buf->span()
        .first(static_cast<size_t>(n))
        .copy_from(base::as_byte_span(r.data).first(static_cast<size_t>(n)));
    return n;
  }
  base::expected<DatagramsMetadata, Error> ReadMultiple(
      IOBuffer*,
      size_t,
      size_t,
      base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)>)
      override {
    NOTREACHED();
  }
  int Write(IOBuffer* buffer,
            int buffer_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag&) override {
    Event e{Event::Kind::kWrite};
    auto span = buffer->span().first(static_cast<size_t>(buffer_len));
    e.write_data.assign(reinterpret_cast<const char*>(span.data()),
                        span.size());
    events_.push_back(std::move(e));
    if (write_returns_pending_) {
      pending_writes_.push_back({buffer_len, std::move(callback)});
      return ERR_IO_PENDING;
    }
    return buffer_len;
  }
  int SetReceiveBufferSize(int32_t) override { return OK; }
  int SetSendBufferSize(int32_t) override { return OK; }

  // DatagramSocket
  void Close() override {}
  int GetPeerAddress(IPEndPoint*) const override { return OK; }
  int GetLocalAddress(IPEndPoint*) const override { return OK; }
  void UseNonBlockingIO() override {}
  int SetDoNotFragment() override { return OK; }
  int SetRecvTos() override { return OK; }
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) override {
    events_.push_back({Event::Kind::kSetTos, dscp, ecn, false, {}});
    return OK;
  }
  void SetMsgConfirm(bool confirm) override {
    events_.push_back({Event::Kind::kSetMsgConfirm,
                       DSCP_NO_CHANGE,
                       ECN_NO_CHANGE,
                       confirm,
                       {}});
  }
  const NetLogWithSource& NetLog() const override { return net_log_; }
  DscpAndEcn GetLastTos() const override {
    return {DSCP_NO_CHANGE, ECN_NO_CHANGE};
  }

  // DatagramClientSocket
  int Connect(const IPEndPoint&) override { return OK; }
  int ConnectUsingNetwork(handles::NetworkHandle, const IPEndPoint&) override {
    return OK;
  }
  int ConnectUsingDefaultNetwork(const IPEndPoint&) override { return OK; }
  int ConnectAsync(const IPEndPoint&, CompletionOnceCallback) override {
    return OK;
  }
  int ConnectUsingNetworkAsync(handles::NetworkHandle,
                               const IPEndPoint&,
                               CompletionOnceCallback) override {
    return OK;
  }
  int ConnectUsingDefaultNetworkAsync(const IPEndPoint&,
                                      CompletionOnceCallback) override {
    return OK;
  }
  handles::NetworkHandle GetBoundNetwork() const override {
    return handles::kInvalidNetworkHandle;
  }
  void ApplySocketTag(const SocketTag&) override {
    events_.push_back({Event::Kind::kApplyTag});
  }
  void EnableRecvOptimization() override {}
  int SetMulticastInterface(uint32_t) override { return OK; }
  void SetIOSNetworkServiceType(int) override {}
  void RegisterQuicConnectionClosePayload(base::span<uint8_t>) override {}
  void UnregisterQuicConnectionClosePayload() override {}

 private:
  struct PendingWrite {
    int length;
    CompletionOnceCallback callback;
  };

  NetLogWithSource net_log_;
  std::vector<Event> events_;
  bool write_returns_pending_ = false;
  base::circular_deque<PendingWrite> pending_writes_;
  std::vector<ScriptedRead> scripted_reads_;
  size_t read_index_ = 0;
};

TEST_F(DelayedDatagramSocketTest, PerPacketTosSnapshotApplied) {
  // Write packet A with DSCP_AF31, change DSCP to CS5, then write packet B.
  // After both packets drain to the wire, the recorder must show the
  // inner socket received SetTos(AF31), Write(A), SetTos(CS5), Write(B) -
  // not the application-current DSCP at wire-send time.
  auto recorder_owner = std::make_unique<RecordingDatagramSocket>();
  RecordingDatagramSocket* recorder = recorder_owner.get();
  auto socket = std::make_unique<DelayedDatagramSocket>(
      std::move(recorder_owner),
      MakeConfig(kLatency,
                 /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt),
      /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  // Configure DSCP_AF31 and write packet A.
  EXPECT_EQ(socket->SetTos(DSCP_AF31, ECN_NO_CHANGE), OK);
  auto buffer_a = base::MakeRefCounted<StringIOBuffer>("AAAA");
  EXPECT_EQ(socket->Write(buffer_a.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);

  // Switch to DSCP_CS5 and write packet B before the first wire send fires.
  EXPECT_EQ(socket->SetTos(DSCP_CS5, ECN_NO_CHANGE), OK);
  auto buffer_b = base::MakeRefCounted<StringIOBuffer>("BBBB");
  EXPECT_EQ(socket->Write(buffer_b.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);

  // Drain both wire sends.
  FastForwardBy(kHalfRtt + base::Seconds(1));

  // For each Write in the recorder, walk back to find the most recent
  // SetTos and confirm it carries that packet's snapshotted DSCP.
  // DrainOneWireSend now also issues SetMsgConfirm and ApplySocketTag
  // before each Write, so the SetTos call is not necessarily the
  // immediately-preceding event - only the last SetTos before the Write
  // matters.
  const auto& events = recorder->events();
  int write_count = 0;
  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i].kind != RecordingDatagramSocket::Event::Kind::kWrite) {
      continue;
    }
    DiffServCodePoint last_set_dscp = DSCP_NO_CHANGE;
    bool found_set_tos = false;
    for (size_t j = i; j > 0; --j) {
      const auto& prev = events[j - 1];
      if (prev.kind == RecordingDatagramSocket::Event::Kind::kWrite) {
        break;  // Reached a previous Write; SetTos for this packet must
                // appear between then and us.
      }
      if (prev.kind == RecordingDatagramSocket::Event::Kind::kSetTos) {
        last_set_dscp = prev.dscp;
        found_set_tos = true;
        break;
      }
    }
    ASSERT_TRUE(found_set_tos)
        << "No SetTos before Write " << events[i].write_data;
    if (events[i].write_data == "AAAA") {
      EXPECT_EQ(last_set_dscp, DSCP_AF31);
      ++write_count;
    } else if (events[i].write_data == "BBBB") {
      EXPECT_EQ(last_set_dscp, DSCP_CS5);
      ++write_count;
    }
  }
  EXPECT_EQ(write_count, 2);
}

TEST_F(DelayedDatagramSocketTest, PerPacketMsgConfirmSnapshotApplied) {
  // Regression for the unsound "skip when send.msg_confirm is false"
  // optimization. With the bug, the second packet's DrainOneWireSend would
  // skip SetMsgConfirm(false), leaving the inner socket in the
  // msg_confirm=true state from the previous packet's DrainOneWireSend.
  // Verify the inner socket sees a SetMsgConfirm call (with the snapshotted
  // value) before each Write.
  auto recorder_owner = std::make_unique<RecordingDatagramSocket>();
  RecordingDatagramSocket* recorder = recorder_owner.get();
  auto socket = std::make_unique<DelayedDatagramSocket>(
      std::move(recorder_owner),
      MakeConfig(kLatency,
                 /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt),
      /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  socket->SetMsgConfirm(true);
  auto buffer_a = base::MakeRefCounted<StringIOBuffer>("AAAA");
  EXPECT_EQ(socket->Write(buffer_a.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);

  socket->SetMsgConfirm(false);
  auto buffer_b = base::MakeRefCounted<StringIOBuffer>("BBBB");
  EXPECT_EQ(socket->Write(buffer_b.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);

  FastForwardBy(kHalfRtt + base::Seconds(1));

  // Walk the trace looking at each Write event and confirming a
  // SetMsgConfirm with the right value precedes it.
  const auto& events = recorder->events();
  int writes_checked = 0;
  for (size_t i = 1; i < events.size(); ++i) {
    if (events[i].kind != RecordingDatagramSocket::Event::Kind::kWrite) {
      continue;
    }
    // Find the most recent SetMsgConfirm before this Write.
    bool found = false;
    for (size_t j = i; j > 0; --j) {
      const auto& prev = events[j - 1];
      if (prev.kind == RecordingDatagramSocket::Event::Kind::kSetMsgConfirm) {
        bool expected = (events[i].write_data == "AAAA");
        EXPECT_EQ(prev.msg_confirm, expected)
            << "Write " << events[i].write_data
            << " not preceded by SetMsgConfirm(" << expected << ")";
        found = true;
        break;
      }
      if (prev.kind == RecordingDatagramSocket::Event::Kind::kWrite) {
        break;  // Reached previous Write without finding SetMsgConfirm.
      }
    }
    EXPECT_TRUE(found) << "No SetMsgConfirm before Write "
                       << events[i].write_data;
    ++writes_checked;
  }
  EXPECT_EQ(writes_checked, 2);
}

TEST_F(DelayedDatagramSocketTest, PerPacketSocketTagSnapshotApplied) {
  // Regression for the unsound "skip when send.socket_tag is default"
  // optimization. After the fix, every wire send is preceded by an
  // ApplySocketTag call so a previous packet's non-default tag does not
  // leak into the current send.
  auto recorder_owner = std::make_unique<RecordingDatagramSocket>();
  RecordingDatagramSocket* recorder = recorder_owner.get();
  auto socket = std::make_unique<DelayedDatagramSocket>(
      std::move(recorder_owner),
      MakeConfig(kLatency,
                 /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt),
      /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  socket->ApplySocketTag(SocketTag());
  auto buffer_a = base::MakeRefCounted<StringIOBuffer>("AAAA");
  EXPECT_EQ(socket->Write(buffer_a.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);

  socket->ApplySocketTag(SocketTag());
  auto buffer_b = base::MakeRefCounted<StringIOBuffer>("BBBB");
  EXPECT_EQ(socket->Write(buffer_b.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);

  FastForwardBy(kHalfRtt + base::Seconds(1));

  // For each Write event, confirm the immediately preceding non-Write
  // event is an ApplySocketTag. With the old buggy optimization, an
  // ApplySocketTag(default) on the second packet would have been
  // skipped, and the SetMsgConfirm would have been immediately before
  // the Write instead.
  const auto& events = recorder->events();
  int writes_with_tag = 0;
  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i].kind != RecordingDatagramSocket::Event::Kind::kWrite) {
      continue;
    }
    bool found_tag_before = false;
    for (size_t j = i; j > 0; --j) {
      const auto& prev = events[j - 1];
      if (prev.kind == RecordingDatagramSocket::Event::Kind::kApplyTag) {
        found_tag_before = true;
        break;
      }
      if (prev.kind == RecordingDatagramSocket::Event::Kind::kWrite) {
        break;
      }
    }
    EXPECT_TRUE(found_tag_before)
        << "No ApplySocketTag before Write " << events[i].write_data;
    ++writes_with_tag;
  }
  EXPECT_EQ(writes_with_tag, 2);
}

TEST_F(DelayedDatagramSocketTest, NoSetTosWhenApplicationNeverConfiguredIt) {
  // DSCP/ECN have explicit "don't change" sentinels, so the wrapper skips
  // SetTos for vanilla packets the application never customised. The
  // wrapper does *not* skip SetMsgConfirm(false) or ApplySocketTag(default)
  // because those values are real states the inner socket may need to be
  // restored to (see DrainOneWireSend).
  auto recorder_owner = std::make_unique<RecordingDatagramSocket>();
  RecordingDatagramSocket* recorder = recorder_owner.get();
  auto socket = std::make_unique<DelayedDatagramSocket>(
      std::move(recorder_owner),
      MakeConfig(kLatency,
                 /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt),
      /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>("hello");
  EXPECT_EQ(socket->Write(buffer.get(), 5, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            5);
  FastForwardBy(kHalfRtt + base::Seconds(1));

  int writes = 0;
  for (const auto& e : recorder->events()) {
    // SetTos is skipped for vanilla packets.
    EXPECT_NE(e.kind, RecordingDatagramSocket::Event::Kind::kSetTos);
    // SetMsgConfirm must only ever carry `false` when the app never set it.
    if (e.kind == RecordingDatagramSocket::Event::Kind::kSetMsgConfirm) {
      EXPECT_FALSE(e.msg_confirm)
          << "SetMsgConfirm(true) leaked through vanilla path";
    }
    if (e.kind == RecordingDatagramSocket::Event::Kind::kWrite) {
      ++writes;
    }
  }
  EXPECT_EQ(writes, 1);
}

// --- Multi-write must not block (covers B regression) ---

TEST_F(DelayedDatagramSocketTest, MultipleWritesReturnSyncWithoutBlocking) {
  // Regression test for the QUIC throughput collapse bug. The consumer must
  // be able to issue many writes back-to-back without each one waiting for
  // an ERR_IO_PENDING callback; the wire-shaping happens asynchronously
  // inside the wrapper.
  std::vector<std::string> packets;
  std::vector<MockWrite> writes;
  for (int i = 0; i < 8; ++i) {
    packets.emplace_back(kData50);
  }
  for (const auto& packet : packets) {
    writes.emplace_back(SYNCHRONOUS, packet);
  }
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  auto socket = CreateSocket(&data, MakeConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  base::TimeTicks start = base::TimeTicks::Now();
  auto write_buffer = base::MakeRefCounted<StringIOBuffer>(kData50);
  for (size_t i = 0; i < packets.size(); ++i) {
    EXPECT_EQ(socket->Write(write_buffer.get(), 50, base::DoNothing(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              50);
  }
  // All Writes returned synchronously - no real time elapsed.
  EXPECT_EQ(base::TimeTicks::Now() - start, base::TimeDelta());

  // Wire send is throttled: at 1000 B/s and 50-byte packets, each takes 50 ms
  // (after half-RTT propagation for the first one).
  FastForwardBy(kHalfRtt + base::Milliseconds(50 * 8));
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// --- Passthrough delegations ---

TEST_F(DelayedDatagramSocketTest, PassthroughMethodsDelegate) {
  StaticSocketDataProvider data;
  data.set_set_receive_buffer_size_result(OK);
  data.set_set_send_buffer_size_result(OK);
  auto socket = CreateSocket(&data, MakeConfig());
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  // Setters and pure-passthrough delegations should reach the inner socket.
  EXPECT_EQ(socket->SetReceiveBufferSize(8192), OK);
  EXPECT_EQ(socket->SetSendBufferSize(8192), OK);

  IPEndPoint peer;
  EXPECT_EQ(socket->GetPeerAddress(&peer), OK);
  EXPECT_EQ(peer, kEndpoint);
  IPEndPoint local;
  EXPECT_EQ(socket->GetLocalAddress(&local), OK);

  // Pure-passthrough methods on DatagramSocket / DatagramClientSocket.
  socket->UseNonBlockingIO();
  EXPECT_EQ(socket->SetDoNotFragment(), OK);
  EXPECT_EQ(socket->SetRecvTos(), OK);
  EXPECT_EQ(socket->SetTos(DSCP_DEFAULT, ECN_DEFAULT), OK);
  socket->SetMsgConfirm(true);
  socket->ApplySocketTag(SocketTag());
  socket->EnableRecvOptimization();
  EXPECT_EQ(socket->SetMulticastInterface(0), OK);
  socket->SetIOSNetworkServiceType(0);

  std::vector<uint8_t> close_payload(8);
  socket->RegisterQuicConnectionClosePayload(base::span(close_payload));
  socket->UnregisterQuicConnectionClosePayload();

  // Getters that don't crash on a vanilla mock.
  (void)socket->NetLog();
  (void)socket->GetLastTos();
  (void)socket->GetBoundNetwork();
}

TEST_F(DelayedDatagramSocketTest, ConnectUsingNetworkVariantsDelegate) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data, MakeConfig());

  // Sync ConnectUsingNetwork variants pass through to the inner socket.
  // Different platforms / mocks return different results for an invalid
  // handle, so accept any of the documented outcomes.
  int rv =
      socket->ConnectUsingNetwork(handles::kInvalidNetworkHandle, kEndpoint);
  EXPECT_THAT(rv, AnyOf(IsOk(), IsError(ERR_NOT_IMPLEMENTED),
                        IsError(ERR_NETWORK_CHANGED)));
}

TEST_F(DelayedDatagramSocketTest, ConnectUsingDefaultNetworkAsyncDelegates) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data, MakeConfig());
  TestCompletionCallback cb;
  int rv = socket->ConnectUsingDefaultNetworkAsync(kEndpoint, cb.callback());
  // The mock either returns OK sync or pends; either is acceptable for the
  // delegation check.
  EXPECT_THAT(
      rv, AnyOf(IsOk(), IsError(ERR_IO_PENDING), IsError(ERR_NOT_IMPLEMENTED)));
}

// --- Shared BandwidthThrottle tests ---

TEST_F(DelayedDatagramSocketTest,
       SharedDownloadThrottleCoordinatesAcrossSockets) {
  // Two sockets share one download throttle. Throughput = 1000 B/s, burst
  // = 5 ms (= 5 bytes). The first 5-byte read drains the burst; the
  // second socket's read has to wait ~5 ms for the bucket to refill.
  // Without coordination each socket would receive its 5-byte packet
  // immediately.
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1000, base::Milliseconds(5));

  MockRead reads1[] = {MockRead(SYNCHRONOUS, kData5)};
  StaticSocketDataProvider data1(reads1, base::span<MockWrite>());
  MockRead reads2[] = {MockRead(SYNCHRONOUS, kData5)};
  StaticSocketDataProvider data2(reads2, base::span<MockWrite>());

  // No latency and unlimited per-socket throughput - the shared throttle
  // is the only rate gate; per-socket throughput math is disabled.
  auto config = MakeConfig(
      /*rtt=*/base::TimeDelta(),
      /*download_bytes_per_sec=*/std::nullopt,
      /*upload_bytes_per_sec=*/std::nullopt);
  auto socket1 = CreateSocket(&data1, config, download_throttle);
  auto socket2 = CreateSocket(&data2, config, download_throttle);
  ASSERT_THAT(socket1->Connect(kEndpoint), IsOk());
  ASSERT_THAT(socket2->Connect(kEndpoint), IsOk());

  auto buffer1 = base::MakeRefCounted<IOBufferWithSize>(64);
  auto buffer2 = base::MakeRefCounted<IOBufferWithSize>(64);
  TestCompletionCallback cb1, cb2;
  EXPECT_THAT(socket1->Read(buffer1.get(), 64, cb1.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(socket2->Read(buffer2.get(), 64, cb2.callback()),
              IsError(ERR_IO_PENDING));

  // Socket 1 takes the burst (5 bytes available) - admitted on the next
  // task hop, no latency on top so it delivers immediately.
  FastForwardBy(base::TimeDelta());
  ASSERT_TRUE(cb1.have_result());
  EXPECT_EQ(cb1.WaitForResult(), static_cast<int>(sizeof(kData5) - 1));

  // Socket 2 finds the bucket empty and waits for tokens to refill at
  // 1000 B/s - 5 bytes → ~5 ms.
  FastForwardBy(base::Milliseconds(4));
  EXPECT_FALSE(cb2.have_result());
  FastForwardBy(base::Milliseconds(1));
  ASSERT_TRUE(cb2.have_result());
  EXPECT_EQ(cb2.WaitForResult(), static_cast<int>(sizeof(kData5) - 1));
}

TEST_F(DelayedDatagramSocketTest,
       SharedUploadThrottleCoordinatesAcrossSockets) {
  // Symmetric to the download test: two sockets share one upload
  // throttle, the first write drains the burst, the second waits for
  // refill. We assert ordering via the mocks' AllWriteDataConsumed():
  // the second socket's inner Write must NOT have happened until the
  // throttle's bucket has refilled.
  auto upload_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1000, base::Milliseconds(5));

  MockWrite writes1[] = {MockWrite(SYNCHRONOUS, kData5)};
  StaticSocketDataProvider data1(base::span<MockRead>(), writes1);
  MockWrite writes2[] = {MockWrite(SYNCHRONOUS, kData5)};
  StaticSocketDataProvider data2(base::span<MockRead>(), writes2);

  auto config = MakeConfig(
      /*rtt=*/base::TimeDelta(),
      /*download_bytes_per_sec=*/std::nullopt,
      /*upload_bytes_per_sec=*/std::nullopt);
  auto socket1 = CreateSocket(&data1, config, /*download_throttle=*/nullptr,
                              upload_throttle);
  auto socket2 = CreateSocket(&data2, config, /*download_throttle=*/nullptr,
                              upload_throttle);
  ASSERT_THAT(socket1->Connect(kEndpoint), IsOk());
  ASSERT_THAT(socket2->Connect(kEndpoint), IsOk());

  // Both Write()s return synchronously with the byte count. The throttle
  // gates when each packet is actually submitted to the inner socket.
  auto buf1 = base::MakeRefCounted<StringIOBuffer>(
      std::string(kData5, sizeof(kData5) - 1));
  auto buf2 = base::MakeRefCounted<StringIOBuffer>(
      std::string(kData5, sizeof(kData5) - 1));
  EXPECT_EQ(socket1->Write(buf1.get(), sizeof(kData5) - 1, base::DoNothing(),
                           TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(sizeof(kData5) - 1));
  EXPECT_EQ(socket2->Write(buf2.get(), sizeof(kData5) - 1, base::DoNothing(),
                           TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(sizeof(kData5) - 1));

  // Socket 1's packet takes the burst and reaches the inner socket on
  // the next task hop. Socket 2's is still queued waiting for tokens.
  FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(data1.AllWriteDataConsumed());
  EXPECT_FALSE(data2.AllWriteDataConsumed());

  // Just before the refill completes, socket 2 is still waiting.
  FastForwardBy(base::Milliseconds(4));
  EXPECT_FALSE(data2.AllWriteDataConsumed());

  // After ~5 ms total the bucket has refilled and socket 2's packet
  // reaches the inner socket.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(data2.AllWriteDataConsumed());
}

TEST_F(DelayedDatagramSocketTest, CloseCancelsPendingDownloadThrottleRequest) {
  // Refill is intentionally slow so a single 5-byte read waits in the
  // throttle queue. We close the wrapper before the throttle admits the
  // bytes; the consumer callback must never fire.
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(100, base::Milliseconds(10));

  MockRead reads[] = {MockRead(SYNCHRONOUS, kData5)};
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto config = MakeConfig(
      /*rtt=*/base::TimeDelta(),
      /*download_bytes_per_sec=*/std::nullopt,
      /*upload_bytes_per_sec=*/std::nullopt);
  auto socket = CreateSocket(&data, config, download_throttle);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(64);
  TestCompletionCallback cb;
  EXPECT_THAT(socket->Read(buffer.get(), 64, cb.callback()),
              IsError(ERR_IO_PENDING));

  // Inner read returned sync 5 bytes and a throttle request is queued.
  // Closing must cancel the queued request so a late grant doesn't fire
  // the consumer's callback after the socket is gone.
  socket->Close();

  // Advance well past the refill horizon. The cancelled request must not
  // result in a callback.
  FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(cb.have_result());
}

// --- Reconnect after Close ---

TEST_F(DelayedDatagramSocketTest, ReconnectAfterCloseDoesNotCrash) {
  // After Close() invalidates all WeakPtrs and clears `next_send_to_grant_`
  // / queues / throttle handles, a second Connect() must not DCHECK / UAF
  // / leak state from the previous session. We don't try to do a full
  // round trip after the second Connect - mock UDP sockets don't
  // gracefully support full reuse - the meaningful coverage is that the
  // wrapper survives the reconnect path itself. Mirrors
  // DelayedStreamSocketTest.ReconnectAfterDisconnectDoesNotCrash.
  StaticSocketDataProvider data;
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency,
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());
  socket->Close();

  // Second Connect after Close must not crash or DCHECK. Whatever the
  // inner mock returns is fine; we just need the wrapper to survive it.
  int rv = socket->Connect(kEndpoint);
  EXPECT_NE(rv, ERR_UNEXPECTED);
}

// --- Send queue saturation drops the overflowing packet ---

TEST_F(DelayedDatagramSocketTest, WriteDropsWhenSendQueueFull) {
  // When the send queue is full, additional Write()s are dropped (not passed
  // through to the inner socket): the wrapper reports synchronous success but
  // does not enqueue or transmit the packet. This avoids unbounded memory
  // growth on a buggy sender and never issues a second concurrent inner
  // write.
  //
  // A slow upload throttle (100 B/s, 10 ms burst = 1 byte) keeps the 1024
  // queued packets pending during the synchronous fill, so the next Write
  // overflows. Only kQueueCap inner writes are provided; if the overflow
  // packet were (incorrectly) passed through, the mock would see an
  // unexpected extra write.
  constexpr int kQueueCap = 1024;  // Must match kMaxQueuedSendPackets.
  std::vector<MockWrite> writes;
  writes.reserve(kQueueCap);
  for (int i = 0; i < kQueueCap; ++i) {
    writes.emplace_back(SYNCHRONOUS, kData5);
  }
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  auto slow_throttle =
      base::MakeRefCounted<BandwidthThrottle>(100, base::Milliseconds(10));
  auto config = MakeConfig(
      /*rtt=*/base::TimeDelta(),
      /*download_bytes_per_sec=*/std::nullopt,
      /*upload_bytes_per_sec=*/std::nullopt);
  auto socket =
      CreateSocket(&data, config, /*download_throttle=*/nullptr, slow_throttle);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(kData5, sizeof(kData5) - 1));
  // Fill the queue with kQueueCap packets; the slow throttle keeps them
  // pending (none drain during this synchronous burst).
  for (int i = 0; i < kQueueCap; ++i) {
    EXPECT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              static_cast<int>(sizeof(kData5) - 1));
  }
  EXPECT_FALSE(data.AllWriteDataConsumed());

  // The (kQueueCap+1)th Write overflows: it is dropped but still reports the
  // byte count synchronously, and must NOT reach the inner socket.
  EXPECT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(sizeof(kData5) - 1));

  // Drain the queued packets. Exactly kQueueCap writes reach the inner
  // socket; the dropped overflow packet does not.
  FastForwardBy(base::Seconds(60));
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// Counts how many kWrite events a RecordingDatagramSocket has seen.
size_t CountRecordedWrites(const RecordingDatagramSocket& recorder) {
  return std::ranges::count_if(recorder.events(), [](const auto& e) {
    return e.kind == RecordingDatagramSocket::Event::Kind::kWrite;
  });
}

TEST_F(DelayedDatagramSocketTest, OverflowDropsPacketWhenInnerWritePending) {
  // When the send queue is full AND an inner Write is already in flight, an
  // overflowing Write() must NOT start a second concurrent inner Write: a
  // real UDPSocketPosix/Win CHECK(write_callback_.is_null()) would crash.
  // The packet is dropped and the caller still sees synchronous success.
  auto recorder_owner = std::make_unique<RecordingDatagramSocket>();
  RecordingDatagramSocket* recorder = recorder_owner.get();
  recorder->set_write_returns_pending(true);  // Inner write stays in flight.
  auto socket = std::make_unique<DelayedDatagramSocket>(
      std::move(recorder_owner),
      MakeConfig(kLatency,
                 /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt),
      /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  constexpr int kQueueCap = 1024;  // Must match kMaxQueuedSendPackets.
  auto fill = base::MakeRefCounted<StringIOBuffer>("fill");
  for (int i = 0; i < kQueueCap; ++i) {
    ASSERT_EQ(socket->Write(fill.get(), 4, base::DoNothing(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              4);
  }
  // Drain one packet; its inner Write parks in flight (inner_write_pending_).
  FastForwardBy(kHalfRtt);
  ASSERT_TRUE(recorder->HasPendingWrite());
  // Refill so the queue is full again while the inner write is still pending.
  ASSERT_EQ(socket->Write(fill.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);

  const size_t writes_before = CountRecordedWrites(*recorder);

  // Overflow while the inner write is pending: must drop (return sync
  // success) and must NOT issue a second inner Write.
  auto overflow = base::MakeRefCounted<StringIOBuffer>("OVERFLOW");
  EXPECT_EQ(socket->Write(overflow.get(), 8, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            8);

  EXPECT_EQ(CountRecordedWrites(*recorder), writes_before)
      << "Overflow issued a concurrent inner Write while one was pending";
  for (const auto& e : recorder->events()) {
    if (e.kind == RecordingDatagramSocket::Event::Kind::kWrite) {
      EXPECT_NE(e.write_data, "OVERFLOW") << "Dropped packet reached the wire";
    }
  }
}

TEST_F(DelayedDatagramSocketTest, GetLastTosDefaultsBeforeReadAndAfterClose) {
  // GetLastTos() must report the wrapped socket's no-packet-yet default
  // (ToS 0 == {DSCP_DEFAULT, ECN_DEFAULT}) before any packet is delivered
  // and after Close(), never the setter-only NO_CHANGE sentinels.
  StaticSocketDataProvider data{base::span<MockRead>(),
                                base::span<MockWrite>()};
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  DscpAndEcn before = socket->GetLastTos();
  EXPECT_EQ(before.dscp, DSCP_DEFAULT);
  EXPECT_EQ(before.ecn, ECN_DEFAULT);

  socket->Close();
  DscpAndEcn after = socket->GetLastTos();
  EXPECT_EQ(after.dscp, DSCP_DEFAULT);
  EXPECT_EQ(after.ecn, ECN_DEFAULT);
}

TEST_F(DelayedDatagramSocketTest, GetLastTosReflectsEachDeliveredPacket) {
  // Read-ahead ingests packet B before packet A is delivered, so each
  // packet's TOS is snapshotted at ingest time. GetLastTos() must report the
  // TOS of the packet actually delivered to the consumer, not the most
  // recently ingested one.
  constexpr uint8_t kTosA = 0x01;  // {DSCP_DEFAULT, ECN_ECT1}.
  constexpr uint8_t kTosB = 0xa2;  // {DSCP_CS5 (40), ECN_ECT0 (2)}.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData5, /*result=*/0, /*seq=*/0,
               /*tos_byte=*/kTosA),
      MockRead(SYNCHRONOUS, kData10, /*result=*/0, /*seq=*/0,
               /*tos_byte=*/kTosB),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING),  // Park read-ahead.
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  // The first Read() drives read-ahead, which ingests BOTH packets (each with
  // its own TOS snapshot) before delivering packet A after half-RTT.
  auto buf1 = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb1;
  ASSERT_THAT(socket->Read(buf1.get(), 100, cb1.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kHalfRtt);
  ASSERT_EQ(cb1.WaitForResult(), static_cast<int>(sizeof(kData5) - 1));
  DscpAndEcn tos_a = socket->GetLastTos();
  EXPECT_EQ(tos_a.dscp, static_cast<DiffServCodePoint>(0));
  EXPECT_EQ(tos_a.ecn, ECN_ECT1);

  // Packet B was queued with the same ready_at as A, so it is now deliverable
  // synchronously; GetLastTos() must switch to B's snapshotted TOS.
  auto buf2 = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb2;
  int rv = socket->Read(buf2.get(), 100, cb2.callback());
  if (rv == ERR_IO_PENDING) {
    rv = cb2.WaitForResult();
  }
  ASSERT_EQ(rv, static_cast<int>(sizeof(kData10) - 1));
  DscpAndEcn tos_b = socket->GetLastTos();
  EXPECT_EQ(tos_b.dscp, static_cast<DiffServCodePoint>(40));
  EXPECT_EQ(tos_b.ecn, ECN_ECT0);
}

TEST_F(DelayedDatagramSocketTest, ReadMultiplePreservesPerPacketTos) {
  // ReadMultiple() delegates to Read() via ReadMultipleEmulator. The emulator
  // must preserve each delivered packet's TOS in DatagramMetadata::tos (QUIC
  // reads it for ECN); a bare 0 would make every packet appear Not-ECT.
  constexpr uint8_t kTosA = 0x01;  // ECN_ECT1.
  constexpr uint8_t kTosB = 0xa2;  // {DSCP_CS5 (40), ECN_ECT0 (2)}.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData5, /*result=*/0, /*seq=*/0,
               /*tos_byte=*/kTosA),
      MockRead(SYNCHRONOUS, kData10, /*result=*/0, /*seq=*/0,
               /*tos_byte=*/kTosB),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING),  // Park read-ahead.
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto read_once = [&](size_t expected_len, uint8_t expected_tos) {
    auto buf = base::MakeRefCounted<IOBufferWithSize>(100);
    std::optional<base::expected<DatagramsMetadata, Error>> result;
    auto callback = base::BindLambdaForTesting(
        [&](base::expected<DatagramsMetadata, Error> r) {
          result = std::move(r);
        });
    base::expected<DatagramsMetadata, Error> rv = socket->ReadMultiple(
        buf.get(), 100, /*max_message_size=*/100, std::move(callback));
    if (!rv.has_value() && rv.error() == ERR_IO_PENDING) {
      FastForwardBy(kHalfRtt);
      ASSERT_TRUE(result.has_value());
      rv = std::move(*result);
    }
    ASSERT_TRUE(rv.has_value());
    ASSERT_EQ(rv->size(), 1u);
    EXPECT_EQ((*rv)[0].length, expected_len);
    EXPECT_EQ((*rv)[0].tos, expected_tos);
  };

  read_once(sizeof(kData5) - 1, kTosA);
  read_once(sizeof(kData10) - 1, kTosB);
}

TEST_F(DelayedDatagramSocketTest, ReadAheadNotStartedUntilFirstRead) {
  // The read-ahead loop must not run at Connect() time; it kicks off on the
  // first consumer Read(). Until then the inner socket's datagram is left
  // sitting unread (documented contract).
  MockRead reads[] = {MockRead(SYNCHRONOUS, kData10)};
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  // No consumer Read() yet - the read-ahead loop has not run, so the inner
  // socket's queued datagram is still unconsumed.
  EXPECT_FALSE(data.AllReadDataConsumed());

  // The first Read() starts the read-ahead loop and drains the datagram.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb;
  EXPECT_THAT(socket->Read(buffer.get(), 100, cb.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kHalfRtt);
  EXPECT_EQ(cb.WaitForResult(), 10);
  EXPECT_TRUE(data.AllReadDataConsumed());
}

TEST_F(DelayedDatagramSocketTest, ReadAheadIsBoundedByQueueCap) {
  // Provide far more datagrams than the read-ahead cap. With a single
  // consumer Read() and no further reads, the pump must stop issuing inner
  // reads once the receive queue is full, leaving most datagrams unconsumed
  // (i.e. read-ahead is bounded, not unbounded).
  constexpr int kManyReads = 300;  // >> kMaxQueuedPackets (64).
  std::vector<MockRead> reads;
  reads.reserve(kManyReads);
  for (int i = 0; i < kManyReads; ++i) {
    reads.emplace_back(SYNCHRONOUS, kData10);
  }
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb;
  EXPECT_THAT(socket->Read(buffer.get(), 100, cb.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kHalfRtt);
  EXPECT_EQ(cb.WaitForResult(), 10);

  // Let any posted read-ahead settle. Because the pump is capped, it stops
  // long before draining all 300 datagrams from the provider.
  FastForwardBy(kLatency);
  EXPECT_FALSE(data.AllReadDataConsumed());
}

TEST_F(DelayedDatagramSocketTest, RecoverableInnerReadErrorDoesNotLatchClosed) {
  // A recoverable inner read error (ERR_MSG_TOO_BIG) is surfaced to the
  // caller, but must NOT permanently latch the read pump closed: a
  // subsequent Read() (second hop) re-reads the inner socket and delivers
  // the next datagram. Uses a scripted inner socket whose error reads are
  // not sticky (unlike MockUDPClientSocket) so "error then data" is
  // observable.
  auto recorder_owner = std::make_unique<RecordingDatagramSocket>();
  RecordingDatagramSocket* recorder = recorder_owner.get();
  recorder->set_scripted_reads({
      {ERR_MSG_TOO_BIG, /*data=*/""},  // hop 1: recoverable error.
      {static_cast<int>(sizeof(kData10) - 1),
       std::string(kData10)},  // hop 2: real data.
  });
  auto socket = std::make_unique<DelayedDatagramSocket>(
      std::move(recorder_owner),
      MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt),
      /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  // Hop 1: the recoverable error is surfaced to the caller.
  auto buf1 = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb1;
  ASSERT_THAT(socket->Read(buf1.get(), 100, cb1.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb1.WaitForResult(), IsError(ERR_MSG_TOO_BIG));

  // Hop 2: the pump was not latched closed, so re-reading the inner socket
  // delivers the next datagram after its half-RTT propagation delay.
  auto buf2 = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb2;
  ASSERT_THAT(socket->Read(buf2.get(), 100, cb2.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kHalfRtt);
  EXPECT_EQ(cb2.WaitForResult(), static_cast<int>(sizeof(kData10) - 1));
}

TEST_F(DelayedDatagramSocketTest, ZeroLengthDatagramWithDownloadThrottle) {
  // A valid zero-length UDP datagram must not call RequestBytes(0) on the
  // download throttle (which would CHECK); it is enqueued with propagation
  // delay only and delivered as a 0-byte Read.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, /*data=*/""),     // zero-length datagram.
      MockRead(SYNCHRONOUS, ERR_IO_PENDING),  // park read-ahead.
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1000, base::Milliseconds(100));
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/1000,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   download_throttle, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb;
  EXPECT_THAT(socket->Read(buffer.get(), 100, cb.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kHalfRtt);
  EXPECT_EQ(cb.WaitForResult(), 0);  // Delivered, no divide-by-zero CHECK.
}

TEST_F(DelayedDatagramSocketTest, OversizedDatagramReturnsMsgTooBig) {
  // A datagram larger than the caller's Read buffer is not silently
  // truncated: the wrapper returns ERR_MSG_TOO_BIG and drops it, matching
  // real UDP. The next datagram is still delivered.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData100),        // 100 bytes.
      MockRead(SYNCHRONOUS, kData5),          // 5 bytes.
      MockRead(SYNCHRONOUS, ERR_IO_PENDING),  // park.
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  // Read the 100-byte datagram into a 10-byte buffer -> ERR_MSG_TOO_BIG.
  auto small = base::MakeRefCounted<IOBufferWithSize>(10);
  TestCompletionCallback cb1;
  ASSERT_THAT(socket->Read(small.get(), 10, cb1.callback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kHalfRtt);
  EXPECT_THAT(cb1.WaitForResult(), IsError(ERR_MSG_TOO_BIG));

  // The oversized datagram was dropped; the next datagram is delivered.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(100);
  TestCompletionCallback cb2;
  int rv = socket->Read(buf.get(), 100, cb2.callback());
  if (rv == ERR_IO_PENDING) {
    FastForwardBy(kHalfRtt);
    rv = cb2.WaitForResult();
  }
  EXPECT_EQ(rv, static_cast<int>(sizeof(kData5) - 1));
}

TEST_F(DelayedDatagramSocketTest, ShapedWriteWhileNotConnectedIsRejected) {
  // A shaped Write() on a not-connected socket must fail synchronously with
  // ERR_SOCKET_NOT_CONNECTED rather than enqueue a packet whose eventual
  // inner-write failure would be silently dropped.
  StaticSocketDataProvider data{base::span<MockRead>(),
                                base::span<MockWrite>()};
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);

  auto buffer = base::MakeRefCounted<StringIOBuffer>("hello");
  // Before Connect().
  EXPECT_EQ(socket->Write(buffer.get(), 5, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            ERR_SOCKET_NOT_CONNECTED);

  // After a successful Connect(), a shaped Write() is accepted.
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());
  EXPECT_EQ(socket->Write(buffer.get(), 5, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            5);

  // After Close(), shaped writes are rejected again.
  socket->Close();
  EXPECT_EQ(socket->Write(buffer.get(), 5, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            ERR_SOCKET_NOT_CONNECTED);
}

TEST_F(DelayedDatagramSocketTest, OperationsRejectedWhenNotConnected) {
  // Read(), Write(), and ReadMultiple() must all synchronously return
  // ERR_SOCKET_NOT_CONNECTED before touching the inner socket when the
  // wrapper is not connected (or has been closed). This holds for both the
  // shaping and the unshaped-passthrough Write paths.
  auto check_rejected = [&](DelayedDatagramSocket* socket) {
    auto buf = base::MakeRefCounted<IOBufferWithSize>(100);
    EXPECT_THAT(socket->Read(buf.get(), 100, base::DoNothing()),
                IsError(ERR_SOCKET_NOT_CONNECTED));
    EXPECT_EQ(socket->Write(buf.get(), 5, base::DoNothing(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              ERR_SOCKET_NOT_CONNECTED);
    base::expected<DatagramsMetadata, Error> rv = socket->ReadMultiple(
        buf.get(), 100, /*max_message_size=*/100, base::DoNothing());
    ASSERT_FALSE(rv.has_value());
    EXPECT_THAT(rv.error(), IsError(ERR_SOCKET_NOT_CONNECTED));
  };

  // Shaping mode: rejected before Connect() and again after Close().
  {
    StaticSocketDataProvider data{base::span<MockRead>(),
                                  base::span<MockWrite>()};
    auto socket = CreateSocket(
        &data,
        MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                   /*upload_bytes_per_sec=*/std::nullopt),
        /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
    check_rejected(socket.get());
    ASSERT_THAT(socket->Connect(kEndpoint), IsOk());
    socket->Close();
    check_rejected(socket.get());
  }

  // Unshaped passthrough mode (rtt == 0, no throttles): rejected before
  // Connect() rather than forwarding to the inner socket.
  {
    StaticSocketDataProvider data{base::span<MockRead>(),
                                  base::span<MockWrite>()};
    auto socket = CreateSocket(&data, NoDelayConfig());
    check_rejected(socket.get());
  }
}

TEST_F(DelayedDatagramSocketTest, ShapedWriteSucceedsAfterConnectAsync) {
  // QUIC connects via ConnectAsync(). UDPClientSocket::ConnectAsync()
  // completes synchronously, but the async-completion path also occurs. Both
  // must mark the socket connected so subsequent shaped Writes are accepted
  // rather than rejected with ERR_SOCKET_NOT_CONNECTED.
  auto shaped_write = [&](DelayedDatagramSocket* socket) {
    auto buffer = base::MakeRefCounted<StringIOBuffer>("hello");
    EXPECT_EQ(socket->Write(buffer.get(), 5, base::DoNothing(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              5);
  };
  const DelayedSocketConfig config =
      MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt);

  // Synchronously-completing ConnectAsync() (QUIC's real path).
  {
    StaticSocketDataProvider data{base::span<MockRead>(),
                                  base::span<MockWrite>()};
    data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
    auto socket = CreateSocket(&data, config, /*download_throttle=*/nullptr,
                               /*upload_throttle=*/nullptr);
    TestCompletionCallback connect_cb;
    ASSERT_THAT(socket->ConnectAsync(kEndpoint, connect_cb.callback()), IsOk());
    shaped_write(socket.get());
  }

  // Asynchronously-completing ConnectAsync().
  {
    StaticSocketDataProvider data{base::span<MockRead>(),
                                  base::span<MockWrite>()};
    data.set_connect_data(MockConnect(ASYNC, OK));
    auto socket = CreateSocket(&data, config, /*download_throttle=*/nullptr,
                               /*upload_throttle=*/nullptr);
    TestCompletionCallback connect_cb;
    ASSERT_THAT(socket->ConnectAsync(kEndpoint, connect_cb.callback()),
                IsError(ERR_IO_PENDING));
    ASSERT_THAT(connect_cb.WaitForResult(), IsOk());
    shaped_write(socket.get());
  }
}

TEST_F(DelayedDatagramSocketTest, FatalWireErrorLatchesSocket) {
  // A fatal inner wire-write error (anything other than the per-packet
  // ERR_MSG_TOO_BIG or transient ERR_NO_BUFFER_SPACE) latches the socket
  // dead: the next Write() surfaces the error - so a consumer's write-error
  // handling can react instead of sending into a black hole - and Reads fail
  // with ERR_SOCKET_NOT_CONNECTED.
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE)};
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(kData5, sizeof(kData5) - 1));
  // Accepted synchronously; the wire write happens half-RTT later and fails.
  ASSERT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(sizeof(kData5) - 1));
  FastForwardBy(kHalfRtt);

  // The fatal error is latched and surfaced by the next Write().
  EXPECT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            ERR_ADDRESS_UNREACHABLE);
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(100);
  EXPECT_THAT(socket->Read(read_buf.get(), 100, base::DoNothing()),
              IsError(ERR_SOCKET_NOT_CONNECTED));

  // Close() clears the latch; a post-Close Write fails with the generic
  // not-connected error.
  socket->Close();
  EXPECT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            ERR_SOCKET_NOT_CONNECTED);
}

TEST_F(DelayedDatagramSocketTest, MsgTooBigWireErrorIsDroppedAsLoss) {
  // ERR_MSG_TOO_BIG on the wire is a per-packet error: the packet is dropped
  // like ordinary UDP loss, the queue keeps draining, and the socket stays
  // usable. (QUIC MTU probing degrades safely: the oversized probe is simply
  // never ACKed.)
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, ERR_MSG_TOO_BIG),
      MockWrite(SYNCHRONOUS, kData5),
  };
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  auto socket =
      CreateSocket(&data,
                   MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                              /*upload_bytes_per_sec=*/std::nullopt),
                   /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(kData5, sizeof(kData5) - 1));
  ASSERT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(sizeof(kData5) - 1));
  ASSERT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(sizeof(kData5) - 1));
  FastForwardBy(kHalfRtt);

  // Both wire writes were attempted: the first died with ERR_MSG_TOO_BIG
  // (dropped), the second still went out.
  EXPECT_TRUE(data.AllWriteDataConsumed());
  // Not latched: another shaped write is accepted.
  EXPECT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(sizeof(kData5) - 1));
}

TEST_F(DelayedDatagramSocketTest, FatalWireErrorCancelsQueuedThrottleRequests) {
  // With a shared upload throttle, packets B and C still have outstanding
  // RequestBytes() grants when packet A's wire write fails fatally. The
  // teardown must cancel those grants (releasing their tokens); otherwise
  // the throttle's later grant callbacks would fire against the emptied
  // send queue.
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE)};
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  auto socket = CreateSocket(
      &data, MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                        /*upload_bytes_per_sec=*/10));
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(kData5, sizeof(kData5) - 1));
  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              static_cast<int>(sizeof(kData5) - 1));
  }

  // A's grant lands at 500 ms (5 bytes @ 10 B/s), its wire write fails at
  // grant + half-RTT = 600 ms, while B's and C's grants (1000 ms, 1500 ms)
  // are still outstanding. Fast-forward far past those grant times: with the
  // teardown in place nothing fires; without it the stale grants hit the
  // emptied queue.
  FastForwardBy(base::Seconds(5));
  EXPECT_EQ(socket->Write(buffer.get(), sizeof(kData5) - 1, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            ERR_ADDRESS_UNREACHABLE);
}

TEST_F(DelayedDatagramSocketTest,
       PartialTosSentinelResolvedAgainstEffectiveState) {
  // SetTos(DSCP_CS5, ECN_NO_CHANGE) followed by SetTos(DSCP_NO_CHANGE,
  // ECN_ECT1) must resolve to the effective (CS5, ECT1) for the next
  // packet, rather than leaving a NO_CHANGE sentinel to be resolved against
  // whatever the inner socket's state happens to be at drain time.
  auto recorder_owner = std::make_unique<RecordingDatagramSocket>();
  RecordingDatagramSocket* recorder = recorder_owner.get();
  auto socket = std::make_unique<DelayedDatagramSocket>(
      std::move(recorder_owner),
      MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt),
      /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  EXPECT_EQ(socket->SetTos(DSCP_CS5, ECN_NO_CHANGE), OK);
  EXPECT_EQ(socket->SetTos(DSCP_NO_CHANGE, ECN_ECT1), OK);
  auto buffer = base::MakeRefCounted<StringIOBuffer>("AAAA");
  EXPECT_EQ(socket->Write(buffer.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);
  FastForwardBy(kHalfRtt + base::Seconds(1));

  // The inner SetTos preceding the wire Write must carry the resolved
  // effective TOS (CS5, ECT1), never a NO_CHANGE sentinel.
  const auto& events = recorder->events();
  bool found = false;
  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i].kind != RecordingDatagramSocket::Event::Kind::kWrite) {
      continue;
    }
    for (size_t j = i; j > 0; --j) {
      const auto& prev = events[j - 1];
      if (prev.kind == RecordingDatagramSocket::Event::Kind::kSetTos) {
        EXPECT_EQ(prev.dscp, DSCP_CS5);
        EXPECT_EQ(prev.ecn, ECN_ECT1);
        found = true;
        break;
      }
      if (prev.kind == RecordingDatagramSocket::Event::Kind::kWrite) {
        break;
      }
    }
  }
  EXPECT_TRUE(found) << "No SetTos with resolved effective TOS before Write";
}

TEST_F(DelayedDatagramSocketTest,
       SetTosWhileInnerWritePendingDoesNotMutateInFlightPacket) {
  // Packet A is written with no TOS configured and its inner write parks
  // (modelling a nonblocking retry in a real socket). The application then
  // configures ECN_CE and writes packet B. The wrapper must not touch the
  // inner socket's TOS while A's write is in flight - A must hit the wire
  // under its original (default) TOS and B under CE.
  auto recorder_owner = std::make_unique<RecordingDatagramSocket>();
  RecordingDatagramSocket* recorder = recorder_owner.get();
  recorder->set_write_returns_pending(true);
  auto socket = std::make_unique<DelayedDatagramSocket>(
      std::move(recorder_owner),
      MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt),
      /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer_a = base::MakeRefCounted<StringIOBuffer>("AAAA");
  ASSERT_EQ(socket->Write(buffer_a.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);
  FastForwardBy(kHalfRtt);  // Drain A; its inner write parks.
  ASSERT_TRUE(recorder->HasPendingWrite());

  // Configure TOS for the *next* packet while A is still in flight, then
  // queue B. B cannot drain yet (A's write is pending).
  EXPECT_EQ(socket->SetTos(DSCP_NO_CHANGE, ECN_CE), OK);
  auto buffer_b = base::MakeRefCounted<StringIOBuffer>("BBBB");
  ASSERT_EQ(socket->Write(buffer_b.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);
  FastForwardBy(kHalfRtt);

  // Nothing may have touched the inner socket's TOS while A is in flight -
  // a real socket's pending nonblocking send would pick the mutation up.
  for (const auto& event : recorder->events()) {
    EXPECT_NE(event.kind, RecordingDatagramSocket::Event::Kind::kSetTos);
  }

  // A's wire write completes; B drains with its own snapshot.
  recorder->CompleteOldestWrite();
  FastForwardBy(kHalfRtt);

  const auto& events = recorder->events();
  size_t write_a_index = events.size();
  size_t write_b_index = events.size();
  size_t set_tos_index = events.size();
  int set_tos_count = 0;
  for (size_t i = 0; i < events.size(); ++i) {
    const auto& event = events[i];
    if (event.kind == RecordingDatagramSocket::Event::Kind::kSetTos) {
      ++set_tos_count;
      set_tos_index = i;
      // B's snapshot resolved DSCP_NO_CHANGE against the effective default.
      EXPECT_EQ(event.dscp, DSCP_DEFAULT);
      EXPECT_EQ(event.ecn, ECN_CE);
    } else if (event.kind == RecordingDatagramSocket::Event::Kind::kWrite) {
      if (event.write_data == "AAAA") {
        write_a_index = i;
      } else if (event.write_data == "BBBB") {
        write_b_index = i;
      }
    }
  }
  // Exactly one inner SetTos: B's per-packet snapshot, applied after A's
  // write and before B's.
  EXPECT_EQ(set_tos_count, 1);
  ASSERT_LT(write_a_index, events.size());
  ASSERT_LT(write_b_index, events.size());
  ASSERT_LT(set_tos_index, events.size());
  EXPECT_LT(write_a_index, set_tos_index);
  EXPECT_LT(set_tos_index, write_b_index);
}

TEST_F(DelayedDatagramSocketTest,
       MsgConfirmAndSocketTagDeferredWhilePacketsQueued) {
  // Like SetTos, SetMsgConfirm() and ApplySocketTag() must not be forwarded
  // to the inner socket while packets are queued or in flight: each queued
  // packet carries its own Write()-time snapshot, applied by the drain path
  // right before that packet's wire write. Packet A parks pending on the
  // inner socket; the app then flips msg_confirm and re-applies a tag for
  // packet B. A must hit the wire under its own snapshot (msg_confirm ==
  // false) and B under the new one (msg_confirm == true).
  auto recorder_owner = std::make_unique<RecordingDatagramSocket>();
  RecordingDatagramSocket* recorder = recorder_owner.get();
  recorder->set_write_returns_pending(true);
  auto socket = std::make_unique<DelayedDatagramSocket>(
      std::move(recorder_owner),
      MakeConfig(kLatency, /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/std::nullopt),
      /*download_throttle=*/nullptr, /*upload_throttle=*/nullptr);
  ASSERT_THAT(socket->Connect(kEndpoint), IsOk());

  auto buffer_a = base::MakeRefCounted<StringIOBuffer>("AAAA");
  ASSERT_EQ(socket->Write(buffer_a.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);
  FastForwardBy(kHalfRtt);  // Drain A; its inner write parks.
  ASSERT_TRUE(recorder->HasPendingWrite());
  const size_t events_after_a = recorder->events().size();

  // Reconfigure per-packet sender state while A is in flight, then queue B.
  socket->SetMsgConfirm(true);
  socket->ApplySocketTag(SocketTag());
  auto buffer_b = base::MakeRefCounted<StringIOBuffer>("BBBB");
  ASSERT_EQ(socket->Write(buffer_b.get(), 4, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            4);
  FastForwardBy(kHalfRtt);

  // Neither call may have reached the inner socket while A is in flight.
  EXPECT_EQ(recorder->events().size(), events_after_a);

  // A's wire write completes; B drains with its own snapshot.
  recorder->CompleteOldestWrite();
  FastForwardBy(kHalfRtt);

  // Per-Write msg_confirm snapshots: false for A, true for B (each applied
  // by the drain path immediately before that packet's wire write).
  const auto& events = recorder->events();
  std::vector<std::pair<std::string, bool>> writes_seen;
  bool last_msg_confirm = false;
  for (const auto& event : events) {
    if (event.kind == RecordingDatagramSocket::Event::Kind::kSetMsgConfirm) {
      last_msg_confirm = event.msg_confirm;
    } else if (event.kind == RecordingDatagramSocket::Event::Kind::kWrite) {
      writes_seen.emplace_back(event.write_data, last_msg_confirm);
    }
  }
  ASSERT_EQ(writes_seen.size(), 2u);
  EXPECT_EQ(writes_seen[0], std::make_pair(std::string("AAAA"), false));
  EXPECT_EQ(writes_seen[1], std::make_pair(std::string("BBBB"), true));
}

}  // namespace
}  // namespace net
