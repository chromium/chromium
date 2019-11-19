// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_client_socket.h"
#include "net/socket/udp_server_socket.h"
#include "net/socket/udp_socket.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "testing/platform_test.h"

using net::test::IsOk;

namespace net {

namespace {

static constexpr char kMetricPrefixUDPSocket[] = "UDPSocketWrite.";
static constexpr char kMetricElapsedTimeMs[] = "elapsed_time";
static constexpr char kMetricWriteSpeedBytesPerSecond[] = "write_speed";

perf_test::PerfResultReporter SetUpUDPSocketReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixUDPSocket, story);
  reporter.RegisterImportantMetric(kMetricElapsedTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricWriteSpeedBytesPerSecond,
                                   "bytesPerSecond_biggerIsBetter");
  return reporter;
}

class UDPSocketPerfTest : public PlatformTest {
 public:
  UDPSocketPerfTest()
      : buffer_(base::MakeRefCounted<IOBufferWithSize>(kPacketSize)) {}

  void DoneWritePacketsToSocket(UDPClientSocket* socket,
                                int num_of_packets,
                                base::Closure done_callback,
                                int error) {
    WritePacketsToSocket(socket, num_of_packets, done_callback);
  }

  // Send |num_of_packets| to |socket|. Invoke |done_callback| when done.
  void WritePacketsToSocket(UDPClientSocket* socket,
                            int num_of_packets,
                            base::Closure done_callback);

  // Use non-blocking IO if |use_nonblocking_io| is true. This variable only
  // has effect on Windows.
  void WriteBenchmark(bool use_nonblocking_io);

 protected:
  static const int kPacketSize = 1024;
  scoped_refptr<IOBufferWithSize> buffer_;
  base::WeakPtrFactory<UDPSocketPerfTest> weak_factory_{this};
};

const int UDPSocketPerfTest::kPacketSize;

// Creates and address from an ip/port and returns it in |address|.
void CreateUDPAddress(const std::string& ip_str,
                      uint16_t port,
                      IPEndPoint* address) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(ip_str))
    return;
  *address = IPEndPoint(ip_address, port);
}

void UDPSocketPerfTest::WritePacketsToSocket(UDPClientSocket* socket,
                                             int num_of_packets,
                                             base::Closure done_callback) {
  scoped_refptr<IOBufferWithSize> io_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kPacketSize);
  memset(io_buffer->data(), 'G', kPacketSize);

  while (num_of_packets) {
    int rv = socket->Write(
        io_buffer.get(), io_buffer->size(),
        base::BindOnce(&UDPSocketPerfTest::DoneWritePacketsToSocket,
                       weak_factory_.GetWeakPtr(), socket, num_of_packets - 1,
                       done_callback),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    if (rv == ERR_IO_PENDING)
      break;
    --num_of_packets;
  }
  if (!num_of_packets) {
    done_callback.Run();
    return;
  }
}

void UDPSocketPerfTest::WriteBenchmark(bool use_nonblocking_io) {
  base::ElapsedTimer total_elapsed_timer;
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  const uint16_t kPort = 9999;

  // Setup the server to listen.
  IPEndPoint bind_address;
  CreateUDPAddress("127.0.0.1", kPort, &bind_address);
  std::unique_ptr<UDPServerSocket> server(
      new UDPServerSocket(nullptr, NetLogSource()));
  if (use_nonblocking_io)
    server->UseNonBlockingIO();
  int rv = server->Listen(bind_address);
  ASSERT_THAT(rv, IsOk());

  // Setup the client.
  IPEndPoint server_address;
  CreateUDPAddress("127.0.0.1", kPort, &server_address);
  std::unique_ptr<UDPClientSocket> client(new UDPClientSocket(
      DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource()));
  if (use_nonblocking_io)
    client->UseNonBlockingIO();
  rv = client->Connect(server_address);
  EXPECT_THAT(rv, IsOk());

  base::RunLoop run_loop;
  base::ElapsedTimer write_elapsed_timer;
  int packets = 100000;
  client->SetSendBufferSize(1024);
  WritePacketsToSocket(client.get(), packets, run_loop.QuitClosure());
  run_loop.Run();

  double write_elapsed = write_elapsed_timer.Elapsed().InSecondsF();
  double total_elapsed = total_elapsed_timer.Elapsed().InMillisecondsF();
  auto reporter =
      SetUpUDPSocketReporter(use_nonblocking_io ? "nonblocking" : "blocking");
  reporter.AddResult(kMetricElapsedTimeMs, total_elapsed);
  reporter.AddResult(kMetricWriteSpeedBytesPerSecond,
                     packets * 1024 / write_elapsed);
}

TEST_F(UDPSocketPerfTest, Write) {
  WriteBenchmark(false);
}

TEST_F(UDPSocketPerfTest, WriteNonBlocking) {
  WriteBenchmark(true);
}

}  // namespace

}  // namespace net
