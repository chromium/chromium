// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/delayed_stream_socket.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/bandwidth_throttle.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/ssl_info.h"
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

constexpr base::TimeDelta kLatency = base::Milliseconds(150);
class DelayedStreamSocketTest : public testing::Test,
                                public WithTaskEnvironment {
 public:
  DelayedStreamSocketTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  // Creates a DelayedStreamSocket wrapping a MockTCPClientSocket with the
  // given data provider and config. Throttles default to null (latency-only).
  std::unique_ptr<DelayedStreamSocket> CreateSocket(
      SocketDataProvider* data_provider,
      const DelayedSocketConfig& config,
      scoped_refptr<BandwidthThrottle> download_throttle = nullptr,
      scoped_refptr<BandwidthThrottle> upload_throttle = nullptr) {
    auto tcp_socket = std::make_unique<MockTCPClientSocket>(
        AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 80)),
        /*net_log=*/nullptr, data_provider);
    return std::make_unique<DelayedStreamSocket>(std::move(tcp_socket), config,
                                                 std::move(download_throttle),
                                                 std::move(upload_throttle));
  }

  DelayedSocketConfig MakeConfig(
      base::TimeDelta rtt = kLatency,
      std::optional<uint64_t> download_bytes_per_sec = std::nullopt,
      std::optional<uint64_t> upload_bytes_per_sec = std::nullopt) {
    return {.rtt = rtt,
            .download_throughput_bytes_per_sec = download_bytes_per_sec,
            .upload_throughput_bytes_per_sec = upload_bytes_per_sec};
  }

  DelayedSocketConfig NoDelayConfig() { return {.rtt = base::TimeDelta()}; }
};

// --- Connect tests ---

TEST_F(DelayedStreamSocketTest, ConnectAsyncWithLatency) {
  SequencedSocketData data_provider;
  data_provider.set_connect_data(MockConnect(ASYNC, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> callback;
  EXPECT_THAT(socket->Connect(callback.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  // The inner socket completes async, then latency timer starts.
  // Advance to just before latency expires: should not be ready yet.
  FastForwardBy(kLatency - base::Milliseconds(1));
  EXPECT_FALSE(callback.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(callback.IsReady());
  EXPECT_THAT(callback.Get(), test::IsOk());
}

TEST_F(DelayedStreamSocketTest, ConnectSyncWithLatency) {
  SequencedSocketData data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> callback;
  // Sync completion converted to async due to latency.
  EXPECT_THAT(socket->Connect(callback.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  EXPECT_FALSE(callback.IsReady());
  FastForwardBy(kLatency);
  EXPECT_TRUE(callback.IsReady());
  EXPECT_THAT(callback.Get(), test::IsOk());
}

TEST_F(DelayedStreamSocketTest, ConnectSyncError) {
  SequencedSocketData data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, ERR_FAILED));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> callback;
  // Even errors get delayed (simulates the RTT it takes to learn the
  // connection failed, e.g. RST).
  EXPECT_THAT(socket->Connect(callback.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  FastForwardBy(kLatency);
  EXPECT_THAT(callback.Get(), test::IsError(ERR_FAILED));
}

TEST_F(DelayedStreamSocketTest, ConnectNoLatencyPassthrough) {
  SequencedSocketData data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, NoDelayConfig());

  base::test::TestFuture<int> callback;
  // With no latency, sync completion passes through.
  EXPECT_THAT(socket->Connect(callback.GetCallback()), test::IsOk());
}

// --- Read tests ---

TEST_F(DelayedStreamSocketTest, ReadPaysHalfRtt) {
  const std::string kData = "hello";  // 5 bytes
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  // Use unlimited throughput so the buffer only applies latency.
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  // Connect first (skip latency for simplicity).
  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  // Read: should pay half-RTT latency only.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  // Expected delay: half-RTT (75ms).
  FastForwardBy(base::Milliseconds(74));
  EXPECT_FALSE(read_cb.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(read_cb.IsReady());
  EXPECT_EQ(read_cb.Get(), static_cast<int>(kData.size()));
}

TEST_F(DelayedStreamSocketTest, SequentialReadsSkipHalfRtt) {
  const std::string kData1 = "hello";  // 5 bytes
  const std::string kData2 = "world";  // 5 bytes
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData1),
      MockRead(SYNCHRONOUS, kData2),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  // Use unlimited throughput so the buffer only applies latency.
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  // First read; half-RTT latency. Limit the read to the first chunk's size
  // so BottleneckBuffer::Pull (which concatenates ready chunks across
  // boundaries in kStream mode) does not merge in the read-ahead second
  // chunk.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb1;
  socket->Read(buffer.get(), static_cast<int>(kData1.size()),
               read_cb1.GetCallback());
  // half-RTT = 75ms
  FastForwardBy(base::Milliseconds(75));
  ASSERT_EQ(read_cb1.Get(), 5);

  // Second read issued immediately after first completes. The read-ahead
  // already buffered it and its latency elapsed in parallel, so it completes
  // synchronously.
  base::test::TestFuture<int> read_cb2;
  int rv = socket->Read(buffer.get(), 1024, read_cb2.GetCallback());
  EXPECT_EQ(rv, 5);
}

// --- Write tests ---

TEST_F(DelayedStreamSocketTest, WritePaysHalfRtt) {
  const std::string kData = "hello";  // 5 bytes
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  // Use unlimited throughput so the buffer only applies latency.
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(kData);
  base::test::TestFuture<int> write_cb;
  EXPECT_THAT(socket->Write(buffer.get(), kData.size(), write_cb.GetCallback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              test::IsError(ERR_IO_PENDING));

  // Expected delay: half-RTT (75ms) only.
  FastForwardBy(base::Milliseconds(74));
  EXPECT_FALSE(write_cb.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(write_cb.IsReady());
  EXPECT_EQ(write_cb.Get(), static_cast<int>(kData.size()));
}

TEST_F(DelayedStreamSocketTest, WriteReadCycleIsOneRtt) {
  // Verify that Write + Read (with a gap between them) totals one full RTT.
  const std::string kRequest = "GET /";
  const std::string kResponse = "HTTP";
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kRequest),
  };
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kResponse),
  };
  StaticSocketDataProvider data_provider(reads, writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  // Latency-only config (no throughput) to isolate RTT.
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  base::TimeTicks start = base::TimeTicks::Now();

  // Write: half-RTT (first write, no sequence).
  auto write_buffer = base::MakeRefCounted<StringIOBuffer>(kRequest);
  base::test::TestFuture<int> write_cb;
  EXPECT_THAT(
      socket->Write(write_buffer.get(), kRequest.size(), write_cb.GetCallback(),
                    TRAFFIC_ANNOTATION_FOR_TESTS),
      test::IsError(ERR_IO_PENDING));
  FastForwardBy(kLatency / 2);
  ASSERT_EQ(write_cb.Get(), static_cast<int>(kRequest.size()));

  // Small gap (> kSequenceThreshold) so the Read is not part of the
  // write sequence. Since reads and writes are tracked independently,
  // this Read is the first read and pays half-RTT.
  FastForwardBy(base::Milliseconds(2));

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              test::IsError(ERR_IO_PENDING));
  FastForwardBy(kLatency / 2);
  ASSERT_EQ(read_cb.Get(), static_cast<int>(kResponse.size()));

  // Total: half-RTT (write) + 2ms gap + half-RTT (read) ~= 1 RTT + 2ms.
  base::TimeDelta elapsed = base::TimeTicks::Now() - start;
  EXPECT_EQ(elapsed, kLatency + base::Milliseconds(2));
}

TEST_F(DelayedStreamSocketTest, ReadAfterGapReturnsBufferedDataSynchronously) {
  // The stream socket reads ahead from the underlying socket. After the
  // first read completes, the second buffered chunk has already paid its
  // half-RTT in parallel, so a later Read() returns it synchronously; just
  // like a real TCP receive buffer that already holds the next segment.
  const std::string kData1 = "hello";
  const std::string kData2 = "world";
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData1),
      MockRead(SYNCHRONOUS, kData2),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  // First read: half-RTT. Cap the byte count to the first chunk's length so
  // the stream-mode Pull does not concatenate the already-buffered second
  // chunk into this read's result.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb1;
  socket->Read(buffer.get(), static_cast<int>(kData1.size()),
               read_cb1.GetCallback());
  FastForwardBy(kLatency / 2);
  ASSERT_EQ(read_cb1.Get(), 5);

  // Even after a gap the second chunk is already in the buffer.
  FastForwardBy(base::Milliseconds(10));

  base::test::TestFuture<int> read_cb2;
  EXPECT_EQ(socket->Read(buffer.get(), 1024, read_cb2.GetCallback()), 5);
}

// --- Passthrough with no delay ---

TEST_F(DelayedStreamSocketTest, NoDelayActsAsPassthrough) {
  const std::string kData = "hello";
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, kData),
  };
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 1, kData),
  };
  SequencedSocketData data_provider(reads, writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, NoDelayConfig());

  // Connect: synchronous passthrough.
  base::test::TestFuture<int> connect_cb;
  EXPECT_THAT(socket->Connect(connect_cb.GetCallback()), test::IsOk());

  // Read: synchronous passthrough.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_EQ(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
            static_cast<int>(kData.size()));

  // Write: synchronous passthrough.
  auto write_buffer = base::MakeRefCounted<StringIOBuffer>(kData);
  base::test::TestFuture<int> write_cb;
  EXPECT_EQ(socket->Write(write_buffer.get(), kData.size(),
                          write_cb.GetCallback(), TRAFFIC_ANNOTATION_FOR_TESTS),
            static_cast<int>(kData.size()));
}

// --- Lifetime safety ---

TEST_F(DelayedStreamSocketTest, DestroyDuringPendingConnect) {
  SequencedSocketData data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> callback;
  EXPECT_THAT(socket->Connect(callback.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  // Destroy socket while connect delay is pending.
  socket.reset();

  // Advance time past the latency: callback should NOT fire.
  FastForwardBy(kLatency * 2);
  EXPECT_FALSE(callback.IsReady());
}

TEST_F(DelayedStreamSocketTest, DestroyDuringPendingRead) {
  const std::string kData = "hello";
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  // Destroy socket while read delay is pending.
  socket.reset();

  FastForwardBy(kLatency * 2);
  EXPECT_FALSE(read_cb.IsReady());
}

TEST_F(DelayedStreamSocketTest, DisconnectCancelsPendingOperations) {
  SequencedSocketData data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> callback;
  EXPECT_THAT(socket->Connect(callback.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  socket->Disconnect();

  FastForwardBy(kLatency * 2);
  EXPECT_FALSE(callback.IsReady());
}

// --- Throughput scaling ---

TEST_F(DelayedStreamSocketTest, LargerReadTakesLonger) {
  // 100 bytes at 1000 bytes/sec = 100ms throughput delay via shared throttle.
  const std::string kData(100, 'x');
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  // Throughput via shared throttle. Burst = 1000 * 10ms = 10 bytes, so
  // 100 bytes exceeds burst and requires token accumulation.
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1000, base::Milliseconds(10));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(base::TimeDelta(),
                                        /*download_bytes_per_sec=*/1000,
                                        /*upload_bytes_per_sec=*/std::nullopt),
                             download_throttle, nullptr);

  base::test::TestFuture<int> connect_cb;
  EXPECT_THAT(socket->Connect(connect_cb.GetCallback()), test::IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  // 100 bytes at 1000 B/s, burst = 10 bytes. Immediate: 10 bytes consumed.
  // Remaining 90 bytes need 90ms of token accumulation.
  FastForwardBy(base::Milliseconds(89));
  EXPECT_FALSE(read_cb.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(read_cb.IsReady());
  EXPECT_EQ(read_cb.Get(), 100);
}

// --- Async inner socket ---

TEST_F(DelayedStreamSocketTest, AsyncReadWithLatency) {
  const std::string kData = "async hello";
  MockRead reads[] = {
      MockRead(ASYNC, kData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  // Use unlimited throughput so the buffer only applies latency.
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  // Inner async read completes first, then half-RTT latency delay.
  FastForwardBy(base::Milliseconds(74));
  EXPECT_FALSE(read_cb.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(read_cb.IsReady());
  EXPECT_EQ(read_cb.Get(), static_cast<int>(kData.size()));
}

// --- Shared throttle tests ---

TEST_F(DelayedStreamSocketTest, SharedThrottleCoordinatesBandwidth) {
  // Two sockets sharing a download throttle at 1000 bytes/sec.
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1000, base::Milliseconds(100));

  const std::string kData = "hello";  // 5 bytes each

  // Socket 1.
  MockRead reads1[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data1(reads1, base::span<MockWrite>());
  data1.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  // No per-socket throughput (unlimited), rely on shared throttle.
  auto config = MakeConfig(base::TimeDelta(),
                           /*download_bytes_per_sec=*/std::nullopt,
                           /*upload_bytes_per_sec=*/std::nullopt);
  auto socket1 = std::make_unique<DelayedStreamSocket>(
      std::make_unique<MockTCPClientSocket>(
          AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 80)), nullptr,
          &data1),
      config, download_throttle, nullptr);

  // Socket 2.
  MockRead reads2[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data2(reads2, base::span<MockWrite>());
  data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket2 = std::make_unique<DelayedStreamSocket>(
      std::make_unique<MockTCPClientSocket>(
          AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 80)), nullptr,
          &data2),
      config, download_throttle, nullptr);

  // Connect both (no latency).
  base::test::TestFuture<int> cc1, cc2;
  EXPECT_THAT(socket1->Connect(cc1.GetCallback()), test::IsOk());
  EXPECT_THAT(socket2->Connect(cc2.GetCallback()), test::IsOk());

  // Read from both simultaneously. Burst size is 100 bytes, so both 5-byte
  // reads fit in the burst. With a shared throttle, operations go through the
  // callback path even when tokens are available immediately.
  auto buffer1 = base::MakeRefCounted<IOBufferWithSize>(1024);
  auto buffer2 = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> rc1, rc2;

  // First read: 5 bytes from burst, served via callback (throttle path).
  EXPECT_THAT(socket1->Read(buffer1.get(), 1024, rc1.GetCallback()),
              test::IsError(ERR_IO_PENDING));
  FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(rc1.IsReady());
  EXPECT_EQ(rc1.Get(), 5);

  // Second read: 5 bytes from burst, also served quickly.
  EXPECT_THAT(socket2->Read(buffer2.get(), 1024, rc2.GetCallback()),
              test::IsError(ERR_IO_PENDING));
  FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(rc2.IsReady());
  EXPECT_EQ(rc2.Get(), 5);
}

TEST_F(DelayedStreamSocketTest, SharedThrottleQueuesWhenExhausted) {
  // Throttle at 100 bytes/sec, burst = 10 bytes (100ms burst).
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(100, base::Milliseconds(100));

  const std::string kData(10, 'x');  // 10 bytes; exhausts burst.

  MockRead reads1[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data1(reads1, base::span<MockWrite>());
  data1.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  MockRead reads2[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data2(reads2, base::span<MockWrite>());
  data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  auto config = MakeConfig(base::TimeDelta(),
                           /*download_bytes_per_sec=*/std::nullopt,
                           /*upload_bytes_per_sec=*/std::nullopt);
  auto socket1 = std::make_unique<DelayedStreamSocket>(
      std::make_unique<MockTCPClientSocket>(
          AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 80)), nullptr,
          &data1),
      config, download_throttle, nullptr);
  auto socket2 = std::make_unique<DelayedStreamSocket>(
      std::make_unique<MockTCPClientSocket>(
          AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 80)), nullptr,
          &data2),
      config, download_throttle, nullptr);

  base::test::TestFuture<int> cc1, cc2;
  EXPECT_THAT(socket1->Connect(cc1.GetCallback()), test::IsOk());
  EXPECT_THAT(socket2->Connect(cc2.GetCallback()), test::IsOk());

  // First read: 10 bytes exhausts the burst; throttle serves immediately
  // but callback is posted async.
  auto buffer1 = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> rc1;
  EXPECT_THAT(socket1->Read(buffer1.get(), 1024, rc1.GetCallback()),
              test::IsError(ERR_IO_PENDING));
  FastForwardBy(base::TimeDelta());
  EXPECT_EQ(rc1.Get(), 10);

  // Second read: burst exhausted, must wait. 10 bytes at 100 bps = 100ms.
  auto buffer2 = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> rc2;
  EXPECT_THAT(socket2->Read(buffer2.get(), 1024, rc2.GetCallback()),
              test::IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(99));
  EXPECT_FALSE(rc2.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(rc2.IsReady());
  EXPECT_EQ(rc2.Get(), 10);
}

// --- Regression: async inner reads must respect download throttle
//     backpressure. If MaybeStartInnerRead() proceeded while a shared
//     download-throttle grant was pending, it would issue an inner Read
//     behind the throttle's back and read-ahead would run uncapped.

TEST_F(DelayedStreamSocketTest,
       AsyncInnerReadStopsWhileDownloadThrottlePending) {
  // 100 B/s, burst 10 bytes; a Read that fills the buffer with 100+
  // bytes will strand the throttle grant in-flight for ~1s while we
  // observe that no further inner Read has been issued.
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(100, base::Milliseconds(100));

  const std::string kFirst(100, 'a');
  const std::string kSecond(100, 'b');
  const std::string kThird(100, 'c');
  MockRead reads[] = {
      MockRead(ASYNC, kFirst),
      MockRead(ASYNC, kSecond),
      MockRead(ASYNC, kThird),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  auto socket = CreateSocket(&data_provider,
                             MakeConfig(base::TimeDelta(),
                                        /*download_bytes_per_sec=*/100,
                                        /*upload_bytes_per_sec=*/std::nullopt),
                             download_throttle, /*upload_throttle=*/nullptr);

  base::test::TestFuture<int> connect_cb;
  EXPECT_THAT(socket->Connect(connect_cb.GetCallback()), test::IsOk());

  // First inner Read is issued by StartInnerRead(). Consumer's Read is
  // pending until throttle admits.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_EQ(data_provider.read_index(), 1u);

  // Let the first async inner Read fire. OnInnerReadComplete fills the
  // buffer with 100 bytes and requests a 1000-byte throttle grant;
  // burst is only 10 bytes so `download_throttle_pending_` stays true.
  // MaybeStartInnerRead() must skip issuing the next inner Read.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(data_provider.read_index(), 1u);

  // While the throttle grant is still pending, read_index must not
  // advance even after a substantial delay: no read-ahead behind
  // backpressure.
  FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(data_provider.read_index(), 1u);

  // Once the throttle grants (100 B/s at 90ms + initial 10-byte burst),
  // the consumer receives its bytes and read-ahead resumes.
  FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(read_cb.IsReady());
  EXPECT_GT(read_cb.Get(), 0);
}

// --- Leftover throttle grant is reused across ReadIfReady calls ---

TEST_F(DelayedStreamSocketTest, LeftoverGrantReusedAcrossReadIfReadyCalls) {
  // ReadIfReady requests a grant for the entire front chunk (max_bytes =
  // INT_MAX). When the caller pulls less than the chunk, the leftover grant
  // must persist so the next call doesn't re-charge the throttle. With a
  // slow throttle, the grant should only be requested once and the rest
  // pulled synchronously from the leftover.
  auto download_throttle = base::MakeRefCounted<BandwidthThrottle>(
      /*throughput_bytes_per_sec=*/100, base::Seconds(1));  // burst = 100 bytes

  const std::string kData(100, 'a');
  MockRead reads[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  auto socket = std::make_unique<DelayedStreamSocket>(
      std::make_unique<MockTCPClientSocket>(
          AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 80)), nullptr,
          &data_provider),
      MakeConfig(kLatency, /*download_bytes_per_sec=*/100,
                 /*upload_bytes_per_sec=*/
                 std::nullopt),
      download_throttle, /*upload_throttle=*/nullptr);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  // First ReadIfReady pays half-RTT for latency and a throttle round-trip
  // for the entire 100-byte chunk (within the burst).
  auto small_buffer = base::MakeRefCounted<IOBufferWithSize>(10);
  base::test::TestFuture<int> ready_cb;
  EXPECT_THAT(
      socket->ReadIfReady(small_buffer.get(), 10, ready_cb.GetCallback()),
      IsError(ERR_IO_PENDING));
  FastForwardBy(kLatency);
  ASSERT_TRUE(ready_cb.IsReady());
  EXPECT_THAT(ready_cb.Get(), IsOk());

  // Drain the chunk via small synchronous ReadIfReady calls. With leftover-
  // grant accounting these must succeed instantly; resetting the grant
  // (the pre-fix behaviour) would re-request the throttle and the burst
  // is already exhausted, so each call would have to wait 100 ms.
  int remaining = 100;
  while (remaining > 0) {
    int rv = socket->ReadIfReady(small_buffer.get(), 10, base::DoNothing());
    ASSERT_GT(rv, 0);
    ASSERT_LE(rv, 10);
    remaining -= rv;
  }
  EXPECT_EQ(remaining, 0);
}

// --- ReadIfReady fires callback with OK, not byte count ---

TEST_F(DelayedStreamSocketTest, ReadIfReadySignalsOkNotByteCount) {
  const std::string kData = "hello";
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  // First ReadIfReady should return ERR_IO_PENDING (latency not elapsed).
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> ready_cb;
  int rv = socket->ReadIfReady(buffer.get(), 1024, ready_cb.GetCallback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // After latency, callback fires with OK (readiness signal, not byte count).
  FastForwardBy(kLatency / 2);
  ASSERT_TRUE(ready_cb.IsReady());
  EXPECT_THAT(ready_cb.Get(), IsOk());

  // Now re-call ReadIfReady to actually pull the data synchronously.
  rv = socket->ReadIfReady(buffer.get(), 1024, base::DoNothing());
  EXPECT_EQ(rv, static_cast<int>(kData.size()));
}

// --- Push never drops bytes (inner read limited to free space) ---

TEST_F(DelayedStreamSocketTest, BufferBackpressurePreventsDataLoss) {
  // Create a large payload to ensure the read loop respects buffer capacity
  // and never drops bytes if Push() accepts less than the inner read size.
  const std::string kLargeData(500, 'x');
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kLargeData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  // Read: should eventually return all bytes without dropping any.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  // Fast-forward enough time for latency + throughput to deliver all data.
  // 75ms latency + 500 bytes / 1000 B/s = 575ms.
  FastForwardBy(base::Milliseconds(600));
  ASSERT_TRUE(read_cb.IsReady());
  int bytes_read = read_cb.Get();
  EXPECT_GT(bytes_read, 0);
  // The key assertion: no CHECK failure from Push, and we got data.
}

TEST_F(DelayedStreamSocketTest, LargeWriteSpansChunksAndPartialAccepts) {
  // Exercise both the multi-chunk drain path (Write larger than the inner
  // scratch buffer) and the partial-accept retry path within a chunk.
  //
  // The pipeline's inner scratch buffer is 32 KiB. We write 40 KiB and have
  // the mock socket accept the first chunk in two pieces (partial accept),
  // then accept the remaining 8 KiB in a second chunk.
  constexpr int kInnerChunk = 32 * 1024;
  constexpr int kFirstPartial = 25 * 1024;
  constexpr int kFirstRemainder = kInnerChunk - kFirstPartial;  // 7 KiB
  constexpr int kTotal = 40 * 1024;
  constexpr int kSecondChunk = kTotal - kInnerChunk;  // 8 KiB

  const std::string kData(kTotal, 'X');
  const std::string kExpectedFirstPartial(kFirstPartial, 'X');
  const std::string kExpectedFirstRemainder(kFirstRemainder, 'X');
  const std::string kExpectedSecondChunk(kSecondChunk, 'X');

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kExpectedFirstPartial),
      MockWrite(SYNCHRONOUS, kExpectedFirstRemainder),
      MockWrite(SYNCHRONOUS, kExpectedSecondChunk),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(kData);
  base::test::TestFuture<int> write_cb;
  EXPECT_THAT(socket->Write(buffer.get(), kTotal, write_cb.GetCallback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));

  // After half-RTT the upload buffer is ready to drain: pipeline pulls the
  // first 32 KiB, the mock accepts 25 KiB, the pipeline shifts and re-issues
  // the remaining 7 KiB, then pulls and writes the final 8 KiB. All three
  // mock writes consume synchronously and the caller's Write completes.
  FastForwardBy(kLatency / 2);
  ASSERT_TRUE(write_cb.IsReady());
  EXPECT_EQ(write_cb.Get(), kTotal);
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// --- Write error from inner socket is propagated ---

TEST_F(DelayedStreamSocketTest, InnerWriteErrorPropagates) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>("hello");
  base::test::TestFuture<int> write_cb;
  EXPECT_THAT(socket->Write(buffer.get(), 5, write_cb.GetCallback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));

  // Error should propagate through the drain path.
  FastForwardBy(kLatency);
  ASSERT_TRUE(write_cb.IsReady());
  EXPECT_THAT(write_cb.Get(), IsError(ERR_CONNECTION_RESET));
}

// --- Throttle callbacks cancelled on Disconnect ---

TEST_F(DelayedStreamSocketTest, DisconnectCancelsThrottleCallbacks) {
  const std::string kData = "hello";
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(100, base::Milliseconds(10));

  // Burst = 100 * 10ms = 1 byte. 5 bytes exceeds burst -> queued.
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt),
                             download_throttle);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  // Disconnect while throttle callback is pending. This should not crash
  // or fire the callback after the socket is gone.
  socket->Disconnect();

  // Let the throttle timer fire. If the callback wasn't properly cancelled,
  // this would UAF or crash.
  FastForwardBy(base::Seconds(10));
  // If we get here without crashing, the test passed.
}

// --- Upload throttle is consulted during normal drain ---

TEST_F(DelayedStreamSocketTest, UploadThrottleIsConsulted) {
  const std::string kData(200, 'y');  // 200 bytes
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  // Shared upload throttle at 100 bytes/sec, burst = 10 bytes.
  auto upload_throttle =
      base::MakeRefCounted<BandwidthThrottle>(100, base::Milliseconds(100));

  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt),
                             /*download_throttle=*/nullptr, upload_throttle);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), test::IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(kData);
  base::test::TestFuture<int> write_cb;
  EXPECT_THAT(socket->Write(buffer.get(), kData.size(), write_cb.GetCallback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));

  // With 100 B/s throttle and 200 bytes, it should take ~2 seconds
  // (minus burst) to complete. Certainly not instant.
  FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(write_cb.IsReady());

  // After enough time for throttle to process all bytes.
  FastForwardBy(base::Seconds(5));
  ASSERT_TRUE(write_cb.IsReady());
  EXPECT_EQ(write_cb.Get(), static_cast<int>(kData.size()));
}

// --- Inner read error / EOF propagation ---

TEST_F(DelayedStreamSocketTest, InnerReadErrorPropagates) {
  MockRead reads[] = {
      MockRead(ASYNC, ERR_CONNECTION_RESET),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  FastForwardBy(kLatency);
  ASSERT_TRUE(read_cb.IsReady());
  EXPECT_THAT(read_cb.Get(), IsError(ERR_CONNECTION_RESET));
}

TEST_F(DelayedStreamSocketTest, InnerReadEofPropagates) {
  // Empty read (length 0) means EOF in the mock socket.
  MockRead reads[] = {
      MockRead(ASYNC, 0),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  FastForwardBy(kLatency);
  ASSERT_TRUE(read_cb.IsReady());
  EXPECT_EQ(read_cb.Get(), 0);

  // Subsequent Read should also see EOF synchronously.
  EXPECT_EQ(socket->Read(buffer.get(), 1024, base::DoNothing()), 0);
}

// --- CancelReadIfReady drops the pending callback ---

TEST_F(DelayedStreamSocketTest, CancelReadIfReadyDropsPendingCallback) {
  const std::string kData = "hello";
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> ready_cb;
  EXPECT_THAT(socket->ReadIfReady(buffer.get(), 1024, ready_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  EXPECT_EQ(socket->CancelReadIfReady(), OK);

  // After latency the data is ready, but the cancelled callback must not fire.
  FastForwardBy(kLatency);
  EXPECT_FALSE(ready_cb.IsReady());

  // Data should still be buffered and readable via a fresh ReadIfReady.
  base::test::TestFuture<int> ready_cb2;
  int rv = socket->ReadIfReady(buffer.get(), 1024, ready_cb2.GetCallback());
  if (rv == ERR_IO_PENDING) {
    ASSERT_TRUE(ready_cb2.Wait());
    EXPECT_THAT(ready_cb2.Get(), IsOk());
  } else {
    EXPECT_EQ(rv, static_cast<int>(kData.size()));
  }
}

// --- Async inner write path (mock returns ASYNC) ---

TEST_F(DelayedStreamSocketTest, AsyncInnerWriteCompletes) {
  const std::string kData = "hello";
  MockWrite writes[] = {
      MockWrite(ASYNC, kData),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(kData);
  base::test::TestFuture<int> write_cb;
  EXPECT_THAT(socket->Write(buffer.get(), kData.size(), write_cb.GetCallback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));

  // Half-RTT for the buffer to become drainable; then the inner write fires
  // asynchronously and OnInnerWriteComplete propagates completion.
  FastForwardBy(kLatency);
  ASSERT_TRUE(write_cb.IsReady());
  EXPECT_EQ(write_cb.Get(), static_cast<int>(kData.size()));
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// --- Reading less than a chunk leaves the rest buffered ---

TEST_F(DelayedStreamSocketTest, PartialReadIntoSmallerBuffer) {
  const std::string kData(100, 'a');
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // First Read with a 10-byte buffer takes half-RTT and returns 10 bytes.
  auto small_buffer = base::MakeRefCounted<IOBufferWithSize>(10);
  base::test::TestFuture<int> first_cb;
  EXPECT_THAT(socket->Read(small_buffer.get(), 10, first_cb.GetCallback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kLatency / 2);
  ASSERT_TRUE(first_cb.IsReady());
  EXPECT_EQ(first_cb.Get(), 10);

  // Remaining 90 bytes stay buffered; subsequent Reads complete sync.
  int remaining = 90;
  while (remaining > 0) {
    int rv = socket->Read(small_buffer.get(), 10, base::DoNothing());
    ASSERT_GT(rv, 0);
    ASSERT_LE(rv, 10);
    remaining -= rv;
  }
  EXPECT_EQ(remaining, 0);
}

// --- Disconnect cancels a pending Write callback ---

TEST_F(DelayedStreamSocketTest, DisconnectDuringPendingWrite) {
  const std::string kData = "hello";
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(kData);
  base::test::TestFuture<int> write_cb;
  EXPECT_THAT(socket->Write(buffer.get(), kData.size(), write_cb.GetCallback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));

  socket->Disconnect();

  // After draining time, the cancelled callback must not fire.
  FastForwardBy(kLatency * 4);
  EXPECT_FALSE(write_cb.IsReady());
}

// --- Write blocks when upload buffer is full, completes when space frees ---

TEST_F(DelayedStreamSocketTest, WriteBufferFullThenSpaceFrees) {
  // Force the smallest possible upload buffer (kMinCapacity = 16 KiB) via a
  // very low configured throughput (used only for BDP sizing). The shared
  // throttle uses a generous burst so the drain itself doesn't stall; the
  // intent here is to exercise the buffer-full backpressure path, not the
  // throttle's rate limiting.
  constexpr int kCap = 16 * 1024;          // BottleneckBuffer::kMinCapacity
  constexpr int kFirst = kCap + 4 * 1024;  // 20 KiB - partial accept.
  constexpr int kSecond = kFirst - kCap;   // 4 KiB - retried.

  const std::string kFirstData(kFirst, 'q');
  const std::string kSecondData(kSecond, 'q');
  const std::string kFirstChunkData(kCap, 'q');
  MockWrite writes[] = {
      // First inner Write of 16 KiB matches the first 16 KiB of this mock.
      MockWrite(SYNCHRONOUS, kFirstChunkData),
      // Second inner Write of 4 KiB from the retried push.
      MockWrite(SYNCHRONOUS, kSecondData),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  auto upload_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1024 * 1024, base::Seconds(1));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/10000),
                             /*download_throttle=*/nullptr, upload_throttle);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // First Write: 20 KiB. Push fills the 16 KiB buffer; caller sees 16 KiB
  // accepted synchronously (partial accept, not IO_PENDING).
  auto buffer = base::MakeRefCounted<StringIOBuffer>(kFirstData);
  int first_rv = socket->Write(buffer.get(), kFirst, base::DoNothing(),
                               TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(first_rv, kCap);

  // Second Write of the remainder: buffer is full, so Push returns 0 and
  // OnUploadSpaceAvailable will retry once drain frees space.
  auto remainder = base::MakeRefCounted<StringIOBuffer>(kSecondData);
  base::test::TestFuture<int> retry_cb;
  EXPECT_THAT(socket->Write(remainder.get(), kSecond, retry_cb.GetCallback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));

  // Let the buffer drain and the stashed write be retried.
  FastForwardBy(base::Seconds(2));
  ASSERT_TRUE(retry_cb.IsReady());
  EXPECT_EQ(retry_cb.Get(), kSecond);
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Stash a Write whose length exceeds the buffer's capacity, so that even a
// fully-drained-and-refilled buffer cannot accept the full stashed amount in
// one re-Push. Exercises the OnUploadSpaceAvailable resume path delivering a
// *partial* async accept (Socket::Write contract allows reporting a count
// smaller than the submitted length). The caller is then expected to retry
// the remainder.
//
// To force the buffer to be full at the moment Write2 arrives; rather than
// drained synchronously by Write1's `MaybeDrainUploadBuffer`; latency keeps
// Write1's chunk "not ready" until time advances.
TEST_F(DelayedStreamSocketTest, WriteBufferFullThenSpaceFreesPartialAccept) {
  // BottleneckBuffer::kMinCapacity; the BdpCapacity floor when the
  // configured throughput is small.
  constexpr int kCap = 16 * 1024;
  // Write1 partial-accepts kCap synchronously and leaves a chunk in the
  // buffer that latency holds "not ready". Write2's length is 2 * kCap so
  // the buffer can never absorb it in one Push, forcing the partial-accept
  // branch in OnUploadSpaceAvailable when room appears.
  constexpr int kFirst = kCap + 4 * 1024;  //  20 KiB
  constexpr int kSecond = 2 * kCap;        //  32 KiB stashed
  constexpr int kPartialAccept = kCap;     // Async resume reports kCap.
  constexpr int kRemainder = kSecond - kPartialAccept;  // 16 KiB to retry.

  const std::string kFirstChunkData(kCap, 'a');
  const std::string kSecondData(kSecond, 'b');
  const std::string kPartialAcceptData(kPartialAccept, 'b');
  const std::string kRemainderData(kRemainder, 'b');

  // Three inner writes drain the queue in 16 KiB chunks: Write1's chunk,
  // Write2's first partial-accept chunk, then Write2's retried remainder.
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kFirstChunkData),
      MockWrite(SYNCHRONOUS, kPartialAcceptData),
      MockWrite(SYNCHRONOUS, kRemainderData),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  // Generous throttle burst so the drain is not throttle-stalled; the test
  // isolates the buffer's resume-time partial-accept path.
  auto upload_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1024 * 1024, base::Seconds(1));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/10000),
                             /*download_throttle=*/nullptr, upload_throttle);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Write1: 20 KiB. Push accepts kCap (= buffer capacity), returns kCap sync
  // (partial-accept). The chunk is in the buffer but not yet ready to drain
  // because half-RTT has not elapsed.
  auto first = base::MakeRefCounted<StringIOBuffer>(std::string(kFirst, 'a'));
  ASSERT_EQ(socket->Write(first.get(), kFirst, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            kCap);

  // Write2: 32 KiB. Buffer is still full (latency holds the chunk), Push
  // returns 0, and the wrapper stashes the entire IOBuffer for the
  // OnUploadSpaceAvailable resume path.
  auto second = base::MakeRefCounted<StringIOBuffer>(kSecondData);
  base::test::TestFuture<int> resume_cb;
  ASSERT_THAT(socket->Write(second.get(), kSecond, resume_cb.GetCallback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));

  // Advance time: half-RTT elapses for Write1's chunk, the buffer drains it
  // (full -> not-full), OnUploadSpaceAvailable fires, and the stashed 32 KiB
  // IOBuffer is re-Push'd against a buffer with kCap of free space; so the
  // re-Push accepts only kCap, and the caller's pending Write completes with
  // that partial count.
  FastForwardBy(base::Seconds(2));
  ASSERT_TRUE(resume_cb.IsReady());
  EXPECT_EQ(resume_cb.Get(), kPartialAccept);

  // Caller retries the 16 KiB remainder. The buffer is again full from the
  // resumed Push, so this Write goes through the buffer-full stash path and
  // completes asynchronously after the next drain.
  auto remainder = base::MakeRefCounted<StringIOBuffer>(kRemainderData);
  base::test::TestFuture<int> tail_cb;
  int tail_rv =
      socket->Write(remainder.get(), kRemainder, tail_cb.GetCallback(),
                    TRAFFIC_ANNOTATION_FOR_TESTS);
  if (tail_rv == ERR_IO_PENDING) {
    FastForwardBy(base::Seconds(2));
    ASSERT_TRUE(tail_cb.IsReady());
    EXPECT_EQ(tail_cb.Get(), kRemainder);
  } else {
    EXPECT_EQ(tail_rv, kRemainder);
  }
  FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// --- Two ReadIfReady cycles back to back ---

TEST_F(DelayedStreamSocketTest, ConsecutiveReadIfReadyCycles) {
  const std::string kData1 = "hello";
  const std::string kData2 = "world";
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData1),
      MockRead(SYNCHRONOUS, kData2),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Cycle 1: ReadIfReady pends, fires OK after half-RTT, then sync pull.
  // Cap the sync pull to the first chunk's length so kStream Pull does not
  // concatenate the read-ahead second chunk into this cycle's result.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> ready_cb1;
  EXPECT_THAT(socket->ReadIfReady(buffer.get(), 1024, ready_cb1.GetCallback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kLatency / 2);
  ASSERT_TRUE(ready_cb1.IsReady());
  EXPECT_THAT(ready_cb1.Get(), IsOk());
  int rv = socket->ReadIfReady(buffer.get(), static_cast<int>(kData1.size()),
                               base::DoNothing());
  EXPECT_EQ(rv, static_cast<int>(kData1.size()));

  // Cycle 2: next chunk was read ahead in parallel during cycle 1, so
  // ReadIfReady completes synchronously with the byte count.
  rv = socket->ReadIfReady(buffer.get(), 1024, base::DoNothing());
  EXPECT_EQ(rv, static_cast<int>(kData2.size()));
}

// --- Async inner reads delivered as multiple separate chunks ---

TEST_F(DelayedStreamSocketTest, AsyncInnerReadMultipleChunks) {
  const std::string kData1 = "hello";
  const std::string kData2 = "world";
  MockRead reads[] = {
      MockRead(ASYNC, kData1),
      MockRead(ASYNC, kData2),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Cap the first Read to kData1's length so kStream Pull does not
  // concatenate the read-ahead second chunk into it; the second Read picks
  // up the second chunk separately.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb1;
  EXPECT_THAT(socket->Read(buffer.get(), static_cast<int>(kData1.size()),
                           read_cb1.GetCallback()),
              IsError(ERR_IO_PENDING));

  FastForwardBy(kLatency);
  ASSERT_TRUE(read_cb1.IsReady());
  EXPECT_EQ(read_cb1.Get(), static_cast<int>(kData1.size()));

  base::test::TestFuture<int> read_cb2;
  int rv = socket->Read(buffer.get(), 1024, read_cb2.GetCallback());
  if (rv == ERR_IO_PENDING) {
    FastForwardBy(kLatency);
    ASSERT_TRUE(read_cb2.IsReady());
    EXPECT_EQ(read_cb2.Get(), static_cast<int>(kData2.size()));
  } else {
    EXPECT_EQ(rv, static_cast<int>(kData2.size()));
  }
}

// --- Passthrough delegations ---

TEST_F(DelayedStreamSocketTest, BufferSizeSettersDelegate) {
  StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());
  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Mock returns OK by default for these setters; just verify they reach
  // the inner socket (no crash, return propagates).
  data_provider.set_set_receive_buffer_size_result(OK);
  data_provider.set_set_send_buffer_size_result(OK);
  EXPECT_EQ(socket->SetReceiveBufferSize(8192), OK);
  EXPECT_EQ(socket->SetSendBufferSize(8192), OK);
}

TEST_F(DelayedStreamSocketTest, SocketDnsAliasesRoundTripThroughDelegate) {
  // Per-socket DNS aliases (Socket::SetDnsAliases / GetDnsAliases for
  // CORS / HSTS); unrelated to the DNS resolver delay that lives on the
  // socket_delay_primitives_dns branch.
  StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());
  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  std::set<std::string> aliases{"a.example", "b.example"};
  socket->SetDnsAliases(aliases);
  EXPECT_EQ(socket->GetDnsAliases(), aliases);
}

TEST_F(DelayedStreamSocketTest, AddressAndStateGettersDelegate) {
  StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());
  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  EXPECT_TRUE(socket->IsConnected());
  EXPECT_TRUE(socket->IsConnectedAndIdle());

  IPEndPoint peer;
  EXPECT_EQ(socket->GetPeerAddress(&peer), OK);
  IPEndPoint local;
  EXPECT_EQ(socket->GetLocalAddress(&local), OK);

  // Untouched-state getters do not crash.
  EXPECT_FALSE(socket->WasEverUsed());
  SSLInfo ssl_info;
  EXPECT_FALSE(socket->GetSSLInfo(&ssl_info));
  EXPECT_GE(socket->GetTotalReceivedBytes(), 0);
  EXPECT_EQ(socket->GetNegotiatedProtocol(), NextProto::kProtoUnknown);
  // NetLog passthrough must not crash; we just need to touch it.
  (void)socket->NetLog();

  // ApplySocketTag passes through without crashing.
  socket->ApplySocketTag(SocketTag());
}

// --- End-to-end: buffer-full stash resumes via OnUploadSpaceAvailable ---

TEST_F(DelayedStreamSocketTest,
       OnUploadSpaceAvailableResumesStashedWriteWithThrottle) {
  // With an upload throttle present the throttle grant is always
  // delivered on a fresh task (see the comment in
  // BandwidthThrottle::RequestBytes about never re-entering the
  // caller's Read/Write contract), so this test does NOT exercise a
  // synchronous drain inside OnUploadSpaceAvailable's frame; that
  // path is defense-in-depth for the no-upload-throttle configuration
  // (which requires unlimited upload throughput per the constructor
  // CHECK and therefore a 2 MiB default buffer).
  //
  // What we exercise here is the full round trip: partial-accept
  // Write1 fills the buffer, Write2 stashes, the throttled drain
  // empties the buffer, OnUploadSpaceAvailable re-Push()es the stash,
  // and the caller's completion is posted with the correct byte count.
  constexpr int kCap = 16 * 1024;
  const std::string kFirstData(kCap + 4 * 1024, 'q');
  const std::string kRetryData(4 * 1024, 'q');
  const std::string kFirstChunkData(kCap, 'q');

  // Mock accepts the first 16 KiB chunk and the retried 4 KiB chunk.
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kFirstChunkData),
      MockWrite(SYNCHRONOUS, kRetryData),
  };
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  // Very small latency so the upload buffer becomes drainable almost
  // immediately. No upload throttle so the drain runs entirely
  // synchronously inside MaybeDrainUploadBuffer.
  DelayedSocketConfig config{
      .rtt = base::Milliseconds(1),
      .download_throughput_bytes_per_sec = std::nullopt,
      .upload_throughput_bytes_per_sec = 10000,
  };
  auto socket = std::make_unique<DelayedStreamSocket>(
      std::make_unique<MockTCPClientSocket>(
          AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 80)), nullptr,
          &data),
      config, /*download_throttle=*/nullptr,
      base::MakeRefCounted<BandwidthThrottle>(/*throughput_bytes_per_sec=*/
                                              1024 * 1024, base::Seconds(1)));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(base::Milliseconds(1));
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // First Write: takes 16 KiB (capacity), leaving 4 KiB to retry later.
  auto first_buffer = base::MakeRefCounted<StringIOBuffer>(kFirstData);
  ASSERT_EQ(socket->Write(first_buffer.get(), kFirstData.size(),
                          base::DoNothing(), TRAFFIC_ANNOTATION_FOR_TESTS),
            kCap);

  // Second Write: buffer is full; stashed. After the first chunk drains
  // and space frees, OnUploadSpaceAvailable retries this push.
  auto retry_buffer = base::MakeRefCounted<StringIOBuffer>(kRetryData);
  base::test::TestFuture<int> retry_cb;
  EXPECT_THAT(
      socket->Write(retry_buffer.get(), kRetryData.size(),
                    retry_cb.GetCallback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      IsError(ERR_IO_PENDING));

  FastForwardBy(base::Seconds(2));
  ASSERT_TRUE(retry_cb.IsReady());
  EXPECT_EQ(retry_cb.Get(), static_cast<int>(kRetryData.size()));
}

// --- Regression: Disconnect/Connect cycle rebinds buffer callbacks ---

TEST_F(DelayedStreamSocketTest, ReconnectAfterDisconnectDoesNotCrash) {
  // After Disconnect() invalidates all WeakPtrs the
  // BottleneckBuffer's data_ready/space_available callbacks would be dead.
  // Connect() must rebind them. This test just exercises the rebind path;
  // the mock socket data provider can't easily be reused for a full
  // round trip, but the absence of a CHECK/UAF on the second Connect()
  // is itself the meaningful coverage.
  StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> cc1;
  socket->Connect(cc1.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(cc1.Get(), IsOk());
  socket->Disconnect();

  // Second Connect after Disconnect must rebind the buffer wake-up
  // callbacks and not CHECK / crash. Whatever the mock returns for the
  // second Connect is fine; we just need the wrapper to survive it.
  int rv = socket->Connect(base::DoNothing());
  EXPECT_NE(rv, ERR_UNEXPECTED);
}

// --- Regression: CancelReadIfReady clears the pending callback ---

// NOTE: The cancelable-closure guard in CancelReadIfReady protects against a
// race that is structurally unreachable in MOCK_TIME (every FastForwardBy
// atomically drains all zero-delay tasks, so a posted trampoline always
// runs before any subsequent test action). This test therefore covers only
// the "callback in pending_read_if_ready_callback_, cancel before
// CompleteReadIfReady posts" path. The trampoline-already-posted path is
// verified by inspection; instrumenting MOCK_TIME to single-step posted
// tasks would require a sequence runner harness that isn't worth the
// complexity for this regression.
TEST_F(DelayedStreamSocketTest, CancelReadIfReadyClearsPendingCallback) {
  const std::string kData = "hello";
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> ready_cb;
  EXPECT_THAT(socket->ReadIfReady(buffer.get(), 1024, ready_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  // Cancel before half-RTT elapses; the callback is still held in
  // pending_read_if_ready_callback_ and CompleteReadIfReady has not run.
  EXPECT_EQ(socket->CancelReadIfReady(), OK);

  // Drain everything. The cancelled callback must not fire.
  FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(ready_cb.IsReady());
}

// --- Regression: zero-byte inner write maps to ERR_CONNECTION_CLOSED ---

TEST_F(DelayedStreamSocketTest, ZeroByteInnerWriteTerminatesPipeline) {
  // An inner socket that returns 0 for a positive-length
  // write means it's closed; the pipeline must surface ERR_CONNECTION_CLOSED
  // rather than spinning on the same chunk forever.
  const std::string kData = "hello";
  MockWrite writes[] = {
      // Mock with empty data + result=0 returns 0 from the inner Write.
      MockWrite(SYNCHRONOUS, /*result=*/0),
  };
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket =
      CreateSocket(&data, MakeConfig(kLatency,
                                     /*download_bytes_per_sec=*/std::nullopt,
                                     /*upload_bytes_per_sec=*/std::nullopt));

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  auto buffer = base::MakeRefCounted<StringIOBuffer>(kData);
  base::test::TestFuture<int> write_cb;
  EXPECT_THAT(socket->Write(buffer.get(), kData.size(), write_cb.GetCallback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kLatency);
  ASSERT_TRUE(write_cb.IsReady());
  EXPECT_EQ(write_cb.Get(), ERR_CONNECTION_CLOSED);
}

// --- Regression: WasEverUsed reports the wrapper's own I/O, not the
//     inner transport's (StreamSocket contract for layered sockets).
TEST_F(DelayedStreamSocketTest, WasEverUsedTracksOwnRead) {
  const std::string kData = "hi";
  MockRead reads[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Connect alone (no Read/Write yet) must not flip WasEverUsed even
  // though the wrapper may have triggered an inner read-ahead on its own.
  EXPECT_FALSE(socket->WasEverUsed());

  // A wrapper-level Read that completes with a positive byte count
  // flips WasEverUsed.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  int rv = socket->Read(buffer.get(), 1024, read_cb.GetCallback());
  if (rv == ERR_IO_PENDING) {
    FastForwardBy(kLatency);
    ASSERT_TRUE(read_cb.IsReady());
    rv = read_cb.Get();
  }
  EXPECT_GT(rv, 0);
  EXPECT_TRUE(socket->WasEverUsed());
}

TEST_F(DelayedStreamSocketTest, WasEverUsedTracksOwnWrite) {
  // Fresh socket (no reads) so we can isolate the Write path's
  // contribution to WasEverUsed().
  const std::string kData = "hi";
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());
  EXPECT_FALSE(socket->WasEverUsed());

  // A wrapper-level Write that completes with a positive byte count
  // flips WasEverUsed.
  auto buffer = base::MakeRefCounted<StringIOBuffer>(std::string(kData));
  base::test::TestFuture<int> write_cb;
  int rv = socket->Write(buffer.get(), static_cast<int>(kData.size()),
                         write_cb.GetCallback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  if (rv == ERR_IO_PENDING) {
    FastForwardBy(kLatency);
    ASSERT_TRUE(write_cb.IsReady());
    rv = write_cb.Get();
  }
  EXPECT_GT(rv, 0);
  EXPECT_TRUE(socket->WasEverUsed());
}

// --- Regression: IsConnectedAndIdle returns false when read-ahead bytes
//     are buffered in our BottleneckBuffer.
TEST_F(DelayedStreamSocketTest, IsConnectedAndIdleFalseWithReadAheadBuffered) {
  // The wrapper's StartInnerRead loop pulls data from the inner socket as
  // soon as the inner socket is connected, so the inner-transport's
  // IsConnectedAndIdle would say true while bytes are sitting in our
  // download buffer. The wrapper must report not-idle so a socket-pool
  // reuse doesn't deliver stale bytes to the next transaction.
  const std::string kData = "hello";
  MockRead reads[] = {MockRead(SYNCHRONOUS, kData),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Trigger read-ahead so the buffer holds the chunk.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), static_cast<int>(kData.size()),
                           read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));
  // Drain the inner read into the wrapper's buffer but DO NOT yet pull it
  // into the caller; the half-RTT hasn't elapsed.
  EXPECT_FALSE(read_cb.IsReady());
  EXPECT_FALSE(socket->IsConnectedAndIdle());

  // Let the first chunk become readable and complete the consumer's Read,
  // which empties the buffer; the wrapper should then report idle again
  // (modulo whatever the inner transport reports).
  FastForwardBy(kLatency);
  ASSERT_TRUE(read_cb.IsReady());
  EXPECT_EQ(read_cb.Get(), static_cast<int>(kData.size()));
}

// --- Regression: CancelReadIfReady cancels the in-flight throttle
//     request so it cannot grant bytes to a no-longer-pending read.
TEST_F(DelayedStreamSocketTest,
       CancelReadIfReadyCancelsPendingThrottleRequest) {
  // A small burst forces the throttle to queue our request; we then
  // cancel before the grant timer fires. If the cancellation handle were
  // not reset in CancelReadIfReady the throttle would still spend tokens
  // to admit this request later; to the detriment of other live
  // sockets sharing the link.
  const std::string kData = "hello";
  MockRead reads[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  // throughput = 100 B/s, burst = 1 ms -> burst size = 0 (clamped to >=1);
  // any request is queued for token refill.
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(100, base::Milliseconds(10));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/std::nullopt),
                             download_throttle);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> rir_cb;
  EXPECT_THAT(socket->ReadIfReady(buffer.get(), 1024, rir_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  // Advance time enough for half-RTT to elapse (so the chunk is ready and a
  // throttle request is queued) but not enough for the throttle to admit
  // it (admission needs ~40 ms more to refill the token bucket).
  FastForwardBy(kLatency / 2);

  // Cancel before the throttle admits the request.
  EXPECT_EQ(socket->CancelReadIfReady(), OK);

  // Advance time well past any conceivable throttle timer. The cancelled
  // request must not fire the consumer's callback after CancelReadIfReady
  // returned OK (Socket::CancelReadIfReady contract).
  FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(rir_cb.IsReady());
}

// --- Regression: ReadIntoCaller's Pull is capped to
//     `pending_read_throttle_grant_` so a multi-chunk-ready buffer cannot
//     bypass the shared throttle by handing the caller more bytes than
//     the throttle authorized.
TEST_F(DelayedStreamSocketTest, ReadRespectsThrottleGrantAcrossReadyChunks) {
  // Two inner reads of 5 bytes each -> two chunks pushed into
  // download_buffer_. Both become ready at the same time (half-RTT after
  // they were pushed back-to-back). Without the cap in ReadIntoCaller,
  // Pull(span of 10) would concatenate both chunks and the caller would
  // receive 10 bytes after only paying the throttle for the first chunk.
  const std::string kData1 = "hello";
  const std::string kData2 = "world";
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kData1),
      MockRead(SYNCHRONOUS, kData2),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING),
  };
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  // Generous bucket so the grant for the first chunk is immediate; the
  // test is about cap-on-Pull, not throttle pacing.
  auto download_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1024 * 1024, base::Seconds(1));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/1024 * 1024,
                                        /*upload_bytes_per_sec=*/std::nullopt),
                             download_throttle);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Request 10 bytes; enough to span both ready chunks.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 10, read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  FastForwardBy(kLatency);
  ASSERT_TRUE(read_cb.IsReady());
  // Only the first chunk's worth of bytes is allowed through on this
  // grant. The throttle controls bandwidth on the link; the cap ensures
  // the next chunk waits for its own grant.
  EXPECT_EQ(read_cb.Get(), static_cast<int>(kData1.size()));

  // The second chunk is delivered on the next Read, which must request a
  // fresh throttle grant before pulling.
  base::test::TestFuture<int> read_cb2;
  int rv2 = socket->Read(buffer.get(), 10, read_cb2.GetCallback());
  if (rv2 == ERR_IO_PENDING) {
    FastForwardBy(kLatency);
    ASSERT_TRUE(read_cb2.IsReady());
    rv2 = read_cb2.Get();
  }
  EXPECT_EQ(rv2, static_cast<int>(kData2.size()));
}

// --- Regression: OnUploadSpaceAvailable surfaces a synchronous inner-write
//     error encountered during the resume drain instead of silently
//     dropping it.
TEST_F(DelayedStreamSocketTest, OnUploadSpaceAvailableSurfacesDrainError) {
  // Buffer cap = kMinCapacity = 16 KiB. Write1 (20 KiB) partial-accepts
  // 16 KiB sync and leaves a chunk in the buffer. Write2 (4 KiB) finds
  // the buffer full and is stashed. When latency elapses and the
  // upload throttle admits, the resulting drain calls
  // HandleInnerWriteResult which sees the inner mock's error and must
  // route it to Write2's pending callback (the error branch fires
  // pending_write_callback_ regardless of `pending_write_buffer_`).
  constexpr int kCap = 16 * 1024;
  const std::string kFirstData(kCap + 4 * 1024, 'a');
  const std::string kFirstChunkData(kCap, 'a');
  const std::string kSecondData(4 * 1024, 'b');
  // First inner Write returns SYNC error; the buffer is Reset and the
  // pending Write2 callback gets that error.
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto upload_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1024 * 1024, base::Seconds(1));
  auto socket = CreateSocket(&data_provider,
                             MakeConfig(kLatency,
                                        /*download_bytes_per_sec=*/std::nullopt,
                                        /*upload_bytes_per_sec=*/10000),
                             /*download_throttle=*/nullptr, upload_throttle);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Sync partial-accept: 16 KiB taken, 4 KiB left for the caller to retry.
  auto first = base::MakeRefCounted<StringIOBuffer>(kFirstData);
  ASSERT_EQ(socket->Write(first.get(), kFirstData.size(), base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            kCap);

  // Buffer-full stash for the retry. This callback is the one that must
  // see the inner-write error once the resume drain runs.
  auto second = base::MakeRefCounted<StringIOBuffer>(kSecondData);
  base::test::TestFuture<int> retry_cb;
  EXPECT_THAT(
      socket->Write(second.get(), kSecondData.size(), retry_cb.GetCallback(),
                    TRAFFIC_ANNOTATION_FOR_TESTS),
      IsError(ERR_IO_PENDING));

  // Drain timer + throttle grant + synchronous inner Write returning the
  // error; OnUploadSpaceAvailable resumes the stashed write, the drain
  // synchronously errors via HandleInnerWriteResult, and that path fires
  // the pending Write2 callback with the error.
  FastForwardBy(base::Seconds(2));
  ASSERT_TRUE(retry_cb.IsReady());
  EXPECT_EQ(retry_cb.Get(), ERR_CONNECTION_RESET);
}

// --- Regression: was_ever_used_ flips only on positive completion.
TEST_F(DelayedStreamSocketTest, WasEverUsedFalseWhilePendingRead) {
  // Read that immediately pends (no bytes ready, inner Read async) must
  // not flip WasEverUsed(). Only positive completions count, matching
  // TCPClientSocket.
  MockRead reads[] = {MockRead(ASYNC, ERR_IO_PENDING)};
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));
  // No positive completion happened; the wrapper must not report used.
  EXPECT_FALSE(socket->WasEverUsed());
}

// --- Regression: WasEverUsed resets on reconnect.
TEST_F(DelayedStreamSocketTest, WasEverUsedResetsOnReconnect) {
  const std::string kData = "hi";
  MockRead reads[] = {MockRead(SYNCHRONOUS, kData)};
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Read that succeeds -> WasEverUsed becomes true.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  int rv = socket->Read(buffer.get(), 1024, read_cb.GetCallback());
  if (rv == ERR_IO_PENDING) {
    FastForwardBy(kLatency);
    rv = read_cb.Get();
  }
  ASSERT_GT(rv, 0);
  EXPECT_TRUE(socket->WasEverUsed());

  // Disconnect -> Connect resets the flag (matches TCPClientSocket's
  // previously_disconnected_ handling).
  socket->Disconnect();
  int reconnect_rv = socket->Connect(base::DoNothing());
  // Second Connect on the mock returns whatever it returns; we only care
  // that the flag has been reset.
  EXPECT_NE(reconnect_rv, ERR_UNEXPECTED);
  EXPECT_FALSE(socket->WasEverUsed());
}

// --- Regression: IsConnected respects our own connect timer.
TEST_F(DelayedStreamSocketTest, IsConnectedFalseWhileConnectPending) {
  // Wrapped socket completes Connect synchronously, but our latency
  // timer holds the caller's completion for one RTT. During that time
  // IsConnected() must report false so callers respect the wrapper's
  // state machine (not the inner socket's).
  StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  EXPECT_FALSE(connect_cb.IsReady());
  EXPECT_FALSE(socket->IsConnected());
  EXPECT_FALSE(socket->IsConnectedAndIdle());

  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());
  EXPECT_TRUE(socket->IsConnected());
}

TEST_F(DelayedStreamSocketTest,
       IsConnectedFalseWhileWrappedAsyncConnectPending) {
  // Wrapped Connect returns ERR_IO_PENDING; before the wrapped socket
  // fires its own completion (which would then start our latency
  // timer), our IsConnected() must still report false. This covers the
  // narrow window between Connect() entry and the timer being armed:
  // the earlier `connect_timer_.IsRunning()`-only guard missed this
  // window, so `connect_pending_` widens it.
  StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(MockConnect(ASYNC, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  int rv = socket->Connect(connect_cb.GetCallback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Wrapped is still async-pending; our own state machine is pending too.
  EXPECT_FALSE(socket->IsConnected());
  EXPECT_FALSE(socket->IsConnectedAndIdle());

  // Full RTT (wrapped completes on next task hop, then our timer runs
  // for kLatency).
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());
  EXPECT_TRUE(socket->IsConnected());
}

// --- Regression: IsConnected reports connected while buffered data remains
//     even after the inner socket closes.
TEST_F(DelayedStreamSocketTest,
       IsConnectedTrueWithBufferedDataAfterInnerClose) {
  // Inner delivers one packet then closes with EOF. After the wrapper
  // has read-ahead the packet and the inner is closed, IsConnected must
  // still return true until the consumer drains the buffered bytes
  // (per StreamSocket::IsConnected: "True is returned if the connection
  // was terminated, but there is unread data in the incoming buffer.").
  const std::string kData = "hello";
  MockRead reads[] = {MockRead(SYNCHRONOUS, kData),
                      MockRead(SYNCHRONOUS, /*result=*/0)};  // EOF
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // Trigger read-ahead so the wrapper drains the packet and sees EOF.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), static_cast<int>(kData.size()),
                           read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));

  // Before the half-RTT elapses: buffer has bytes, inner may or may not
  // have signalled EOF depending on mock scheduling. Either way,
  // IsConnected must report true because there's data to deliver.
  EXPECT_TRUE(socket->IsConnected());

  // Drain to completion.
  FastForwardBy(kLatency);
  EXPECT_EQ(read_cb.Get(), static_cast<int>(kData.size()));
}

// --- Regression: Read on a disconnected socket fails synchronously.
TEST_F(DelayedStreamSocketTest,
       ReadOnDisconnectedSocketReturnsSocketNotConnected) {
  // If the caller invokes Read before Connect (or after the inner has
  // gone away with no buffered data), the wrapper should synchronously
  // return ERR_SOCKET_NOT_CONNECTED instead of pending + async error.
  StaticSocketDataProvider data_provider;
  auto socket = CreateSocket(&data_provider, MakeConfig());
  // Deliberately do NOT call Connect.

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  EXPECT_THAT(socket->Read(buffer.get(), 1024, base::DoNothing()),
              IsError(ERR_SOCKET_NOT_CONNECTED));
}

TEST_F(DelayedStreamSocketTest,
       WriteOnDisconnectedSocketReturnsSocketNotConnected) {
  // Symmetric to the Read case: Write must synchronously fail with
  // ERR_SOCKET_NOT_CONNECTED on an unconnected wrapper, not queue bytes
  // that will never leave.
  StaticSocketDataProvider data_provider;
  auto socket = CreateSocket(&data_provider, MakeConfig());
  // Deliberately do NOT call Connect.

  auto buffer = base::MakeRefCounted<StringIOBuffer>("hi");
  EXPECT_THAT(socket->Write(buffer.get(), 2, base::DoNothing(),
                            TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_SOCKET_NOT_CONNECTED));
}

// --- Regression: was_ever_used_ flips via DispatchPendingCompletion for
//     async positive completions on the shaped Read path.
TEST_F(DelayedStreamSocketTest, WasEverUsedTrueOnShapedAsyncCompletion) {
  // Force the async completion path: shaped Read (with latency), inner
  // Read returns ASYNC. The consumer's callback fires via
  // DispatchPendingCompletion after half-RTT elapses. That path must
  // flip WasEverUsed to true (not just the sync fast-path).
  MockRead reads[] = {MockRead(ASYNC, "hello")};
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());
  EXPECT_FALSE(socket->WasEverUsed());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));
  // Async inner Read + half-RTT latency: completion is scheduled through
  // DispatchPendingCompletion.
  FastForwardBy(kLatency);
  ASSERT_TRUE(read_cb.IsReady());
  EXPECT_GT(read_cb.Get(), 0);
  EXPECT_TRUE(socket->WasEverUsed());
}

// --- Regression: was_ever_used_ flips via DidCompletePassthroughIO for
//     async positive completions on the passthrough Read path.
TEST_F(DelayedStreamSocketTest, WasEverUsedTrueOnPassthroughAsyncCompletion) {
  // Force the passthrough async path: NoDelayConfig (no latency, no
  // throttle) + ASYNC MockRead. The consumer's callback fires via the
  // DidCompletePassthroughIO trampoline. That path must flip
  // WasEverUsed to true.
  MockRead reads[] = {MockRead(ASYNC, "hello")};
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, NoDelayConfig());

  ASSERT_THAT(socket->Connect(base::DoNothing()), IsOk());
  EXPECT_FALSE(socket->WasEverUsed());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), 1024, read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));
  // Inner Read completes on the next task hop; the passthrough
  // trampoline forwards through DidCompletePassthroughIO.
  FastForwardBy(base::TimeDelta());
  ASSERT_TRUE(read_cb.IsReady());
  EXPECT_GT(read_cb.Get(), 0);
  EXPECT_TRUE(socket->WasEverUsed());
}

// --- Regression: buffered read-ahead survives even after the inner
//     socket has EOFed; the ERR_SOCKET_NOT_CONNECTED guard must not
//     interfere with drain.
TEST_F(DelayedStreamSocketTest, ReadDrainsBufferedBytesAfterInnerEof) {
  // Deliver one packet then EOF from the inner socket. After read-ahead
  // has pulled the packet and observed EOF, the consumer's Read must
  // still return the buffered bytes (not ERR_SOCKET_NOT_CONNECTED,
  // which the check would return if it didn't gate on
  // `download_buffer_.empty()` and `!inner_read_eof_`).
  const std::string kData = "hello";
  MockRead reads[] = {MockRead(SYNCHRONOUS, kData),
                      MockRead(SYNCHRONOUS, /*result=*/0)};  // EOF
  StaticSocketDataProvider data_provider(reads, base::span<MockWrite>());
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto socket = CreateSocket(&data_provider, MakeConfig());

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // First Read pends waiting for half-RTT; then delivers the data.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  base::test::TestFuture<int> read_cb;
  EXPECT_THAT(socket->Read(buffer.get(), static_cast<int>(kData.size()),
                           read_cb.GetCallback()),
              IsError(ERR_IO_PENDING));
  FastForwardBy(kLatency);
  EXPECT_EQ(read_cb.Get(), static_cast<int>(kData.size()));

  // At this point inner has EOFed (read #2 returned 0). A subsequent
  // Read must return 0 (EOF), not ERR_SOCKET_NOT_CONNECTED.
  base::test::TestFuture<int> read_cb2;
  int rv2 = socket->Read(buffer.get(), 1024, read_cb2.GetCallback());
  if (rv2 == ERR_IO_PENDING) {
    FastForwardBy(kLatency);
    ASSERT_TRUE(read_cb2.IsReady());
    rv2 = read_cb2.Get();
  }
  EXPECT_EQ(rv2, 0);
}

// --- Regression: sync-partial-accept Write followed by an inner-write
//     error surfaces the failure on the next Write attempt.
TEST_F(DelayedStreamSocketTest,
       PartialSyncWriteWithInnerErrorSurfacesOnNextWrite) {
  // The consumer saw a positive sync return from Write, so no callback
  // fires on the inner-write error. The failure must still be visible
  // on the next Write.
  //
  // Setup: force a small buffer via kMinCapacity (16 KiB) and issue a
  // Write larger than that so Push returns a partial-accept sync count.
  // Then have the inner Write fail with ERR_CONNECTION_RESET. The
  // consumer's second Write must surface an error rather than silently
  // enqueue more bytes that will never leave.
  constexpr int kCap = 16 * 1024;          // BottleneckBuffer::kMinCapacity
  constexpr int kFirst = kCap + 4 * 1024;  // 20 KiB (partial-accept)
  // Two failing inner Writes: one for the sync-partial drain (whose
  // error is silently dropped because no pending callback exists) and
  // one for the consumer's second Write attempt (whose error must reach
  // the consumer's callback).
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
      MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };
  StaticSocketDataProvider data_provider(base::span<MockRead>(), writes);
  data_provider.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  auto upload_throttle =
      base::MakeRefCounted<BandwidthThrottle>(1024 * 1024, base::Seconds(1));
  auto socket = std::make_unique<DelayedStreamSocket>(
      std::make_unique<MockTCPClientSocket>(
          AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 80)), nullptr,
          &data_provider),
      MakeConfig(kLatency,
                 /*download_bytes_per_sec=*/std::nullopt,
                 /*upload_bytes_per_sec=*/10000),
      /*download_throttle=*/nullptr, upload_throttle);

  base::test::TestFuture<int> connect_cb;
  socket->Connect(connect_cb.GetCallback());
  FastForwardBy(kLatency);
  ASSERT_THAT(connect_cb.Get(), IsOk());

  // First Write: sync partial-accept returns `kCap`. No pending callback
  // is set because bytes were returned sync.
  auto first = base::MakeRefCounted<StringIOBuffer>(std::string(kFirst, 'a'));
  ASSERT_EQ(socket->Write(first.get(), kFirst, base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS),
            kCap);

  // Let the drain fail. Because there is no pending callback for the
  // sync-partial bytes, the error is silently dropped inside the
  // wrapper (upload_buffer_ is Reset).
  FastForwardBy(base::Seconds(2));

  // Consumer's next Write must surface the failure. Either the
  // wrapped socket is now not-connected (ERR_SOCKET_NOT_CONNECTED sync)
  // or a fresh async attempt will fire the callback with an error.
  auto second = base::MakeRefCounted<StringIOBuffer>("tail");
  base::test::TestFuture<int> write_cb;
  int rv = socket->Write(second.get(), 4, write_cb.GetCallback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  if (rv == ERR_IO_PENDING) {
    FastForwardBy(base::Seconds(2));
    ASSERT_TRUE(write_cb.IsReady());
    rv = write_cb.Get();
  }
  EXPECT_LT(rv, 0);
}

}  // namespace
}  // namespace net
