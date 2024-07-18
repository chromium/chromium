// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_socket_data_provider.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "net/base/io_buffer.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/diff_serv_code_point.h"
#include "net/socket/socket_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

class QuicSocketDataProviderTest : public TestWithTaskEnvironment {
 public:
  QuicSocketDataProviderTest()
      : packet_maker_(std::make_unique<QuicTestPacketMaker>(
            version_,
            quic::QuicUtils::CreateRandomConnectionId(
                context_.random_generator()),
            context_.clock(),
            "hostname",
            quic::Perspective::IS_CLIENT,
            /*client_priority_uses_incremental=*/true,
            /*use_priority_header=*/true)) {}

  // Create a simple test packet.
  std::unique_ptr<quic::QuicReceivedPacket> TestPacket(uint64_t packet_number) {
    return packet_maker_->Packet(packet_number)
        .AddMessageFrame(base::NumberToString(packet_number))
        .Build();
  }

 protected:
  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLogSourceType::NONE)};
  quic::ParsedQuicVersion version_ = quic::ParsedQuicVersion::RFCv1();
  MockQuicContext context_;
  std::unique_ptr<QuicTestPacketMaker> packet_maker_;
};

// A linear sequence of sync expectations completes.
TEST_F(QuicSocketDataProviderTest, LinearSequenceSync) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  socket_data.AddWrite("p1", TestPacket(1)).Sync();
  socket_data.AddWrite("p2", TestPacket(2)).Sync();
  socket_data.AddWrite("p3", TestPacket(3)).Sync();

  socket_factory.AddSocketDataProvider(&socket_data);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        std::unique_ptr<DatagramClientSocket> socket =
            socket_factory.CreateDatagramClientSocket(
                DatagramSocket::BindType::DEFAULT_BIND, nullptr,
                net_log_with_source_.source());
        socket->Connect(IPEndPoint());

        for (uint64_t packet_number = 1; packet_number < 4; packet_number++) {
          std::unique_ptr<quic::QuicReceivedPacket> packet =
              TestPacket(packet_number);
          scoped_refptr<StringIOBuffer> buffer =
              base::MakeRefCounted<StringIOBuffer>(
                  std::string(packet->data(), packet->length()));
          EXPECT_EQ(
              static_cast<int>(packet->length()),
              socket->Write(buffer.get(), packet->length(), base::DoNothing(),
                            TRAFFIC_ANNOTATION_FOR_TESTS));
        }
      }));

  socket_data.RunUntilAllConsumed();
}

// A linear sequence of async expectations completes.
TEST_F(QuicSocketDataProviderTest, LinearSequenceAsync) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  socket_data.AddWrite("p1", TestPacket(1));
  socket_data.AddWrite("p2", TestPacket(2));
  socket_data.AddWrite("p3", TestPacket(3));

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  int next_packet = 1;
  base::RepeatingCallback<void(int)> callback =
      base::BindLambdaForTesting([&](int result) {
        EXPECT_GT(result, 0);  // Bytes written or, on the first call, one.
        if (next_packet <= 3) {
          std::unique_ptr<quic::QuicReceivedPacket> packet =
              TestPacket(next_packet++);
          scoped_refptr<StringIOBuffer> buffer =
              base::MakeRefCounted<StringIOBuffer>(
                  std::string(packet->data(), packet->length()));
          EXPECT_EQ(ERR_IO_PENDING,
                    socket->Write(buffer.get(), packet->length(), callback,
                                  TRAFFIC_ANNOTATION_FOR_TESTS));
        }
      });
  callback.Run(1);
  socket_data.RunUntilAllConsumed();
}

// The `TosByte` builder method results in a correct TOS byte in the read.
TEST_F(QuicSocketDataProviderTest, ReadTos) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;
  const uint8_t kTestTos = (DSCP_CS1 << 2) + ECN_CE;

  socket_data.AddRead("p1", TestPacket(1)).Sync().TosByte(kTestTos);

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  read_buffer->SetCapacity(100);
  EXPECT_EQ(static_cast<int>(TestPacket(1)->length()),
            socket->Read(read_buffer.get(), 100, base::DoNothing()));
  DscpAndEcn dscp_and_ecn = socket->GetLastTos();
  EXPECT_EQ(dscp_and_ecn.dscp, DSCP_CS1);
  EXPECT_EQ(dscp_and_ecn.ecn, ECN_CE);

  socket_data.RunUntilAllConsumed();
}

// AddReadError creates a read returning an error.
TEST_F(QuicSocketDataProviderTest, AddReadError) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  socket_data.AddReadError("p1", ERR_CONNECTION_ABORTED).Sync();

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  read_buffer->SetCapacity(100);
  EXPECT_EQ(ERR_CONNECTION_ABORTED,
            socket->Read(read_buffer.get(), 100, base::DoNothing()));

  socket_data.RunUntilAllConsumed();
}

// AddRead with a QuicReceivedPacket correctly sets the ECN.
TEST_F(QuicSocketDataProviderTest, AddReadQuicReceivedPacketGetsEcn) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  packet_maker_->set_ecn_codepoint(quic::QuicEcnCodepoint::ECN_ECT0);
  socket_data.AddRead("p1", TestPacket(1)).Sync();

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  read_buffer->SetCapacity(100);
  EXPECT_EQ(static_cast<int>(TestPacket(1)->length()),
            socket->Read(read_buffer.get(), 100, base::DoNothing()));
  DscpAndEcn dscp_and_ecn = socket->GetLastTos();
  EXPECT_EQ(dscp_and_ecn.ecn, ECN_ECT0);

  socket_data.RunUntilAllConsumed();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// A write of data different from the expectation generates a failure.
TEST_F(QuicSocketDataProviderTest, MismatchedWrite) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  socket_data.AddWrite("p1", TestPacket(1)).Sync();

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  std::unique_ptr<quic::QuicReceivedPacket> packet = TestPacket(999);
  scoped_refptr<StringIOBuffer> buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(packet->data(), packet->length()));
  EXPECT_NONFATAL_FAILURE(
      EXPECT_EQ(ERR_UNEXPECTED,
                socket->Write(buffer.get(), packet->length(), base::DoNothing(),
                              TRAFFIC_ANNOTATION_FOR_TESTS)),
      "Expectation 'p1' not met.");
}

// AllDataConsumed is false if there are still pending expectations.
TEST_F(QuicSocketDataProviderTest, NotAllConsumed) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  socket_data.AddWrite("p1", TestPacket(1)).Sync();
  socket_data.AddWrite("p2", TestPacket(2)).Sync();

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  std::unique_ptr<quic::QuicReceivedPacket> packet = TestPacket(1);
  scoped_refptr<StringIOBuffer> buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(packet->data(), packet->length()));
  EXPECT_EQ(static_cast<int>(packet->length()),
            socket->Write(buffer.get(), packet->length(), base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_FALSE(socket_data.AllDataConsumed());
}

// When a Write call occurs with no matching expectation, that is treated as an
// error.
TEST_F(QuicSocketDataProviderTest, ReadBlocksWrite) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  socket_data.AddRead("p1", TestPacket(1)).Sync();
  socket_data.AddWrite("p2", TestPacket(2)).Sync();

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  std::unique_ptr<quic::QuicReceivedPacket> packet = TestPacket(1);
  scoped_refptr<StringIOBuffer> buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(packet->data(), packet->length()));
  EXPECT_NONFATAL_FAILURE(
      EXPECT_EQ(ERR_UNEXPECTED,
                socket->Write(buffer.get(), packet->length(), base::DoNothing(),
                              TRAFFIC_ANNOTATION_FOR_TESTS)),
      "Write call when none is expected:");
}

// When a Read call occurs with no matching expectation, it waits for a matching
// expectation to become read.
TEST_F(QuicSocketDataProviderTest, WriteDelaysRead) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  socket_data.AddWrite("p1", TestPacket(1)).Sync();
  socket_data.AddRead("p2", TestPacket(22222)).Sync();

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  // Begin a read operation which should not complete yet.
  bool read_completed = false;
  base::OnceCallback<void(int)> read_callback =
      base::BindLambdaForTesting([&](int result) {
        EXPECT_EQ(result, static_cast<int>(TestPacket(22222)->length()));
        read_completed = true;
      });
  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  read_buffer->SetCapacity(100);
  EXPECT_EQ(ERR_IO_PENDING,
            socket->Read(read_buffer.get(), 100, std::move(read_callback)));

  EXPECT_FALSE(read_completed);

  // Perform the write on which the read depends.
  std::unique_ptr<quic::QuicReceivedPacket> packet = TestPacket(1);
  scoped_refptr<StringIOBuffer> buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(packet->data(), packet->length()));
  EXPECT_EQ(static_cast<int>(packet->length()),
            socket->Write(buffer.get(), packet->length(), base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS));

  socket_data.RunUntilAllConsumed();
  EXPECT_TRUE(read_completed);
}

// When a pause becomes ready, subsequent calls are delayed.
TEST_F(QuicSocketDataProviderTest, PauseDelaysCalls) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  socket_data.AddWrite("p1", TestPacket(1)).Sync();
  auto pause = socket_data.AddPause("pause");
  socket_data.AddRead("p2", TestPacket(2)).After("pause");
  socket_data.AddWrite("p3", TestPacket(3)).After("pause");

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  // Perform a write in another task, and wait for the pause.
  bool write_completed = false;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        std::unique_ptr<quic::QuicReceivedPacket> packet = TestPacket(1);
        scoped_refptr<StringIOBuffer> buffer =
            base::MakeRefCounted<StringIOBuffer>(
                std::string(packet->data(), packet->length()));
        EXPECT_EQ(
            static_cast<int>(packet->length()),
            socket->Write(buffer.get(), packet->length(), base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS));
        write_completed = true;
      }));

  EXPECT_FALSE(write_completed);
  socket_data.RunUntilPause(pause);
  EXPECT_TRUE(write_completed);

  // Begin a read operation which should not complete yet.
  bool read_completed = false;
  base::OnceCallback<void(int)> read_callback =
      base::BindLambdaForTesting([&](int result) {
        EXPECT_EQ(result, static_cast<int>(TestPacket(2)->length()));
        read_completed = true;
      });
  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  read_buffer->SetCapacity(100);
  EXPECT_EQ(ERR_IO_PENDING,
            socket->Read(read_buffer.get(), 100, std::move(read_callback)));

  // Begin a write operation which should not complete yet.
  write_completed = false;
  base::OnceCallback<void(int)> write_callback =
      base::BindLambdaForTesting([&](int result) {
        EXPECT_EQ(result, static_cast<int>(TestPacket(3)->length()));
        write_completed = true;
      });
  std::unique_ptr<quic::QuicReceivedPacket> packet = TestPacket(3);
  scoped_refptr<StringIOBuffer> buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(packet->data(), packet->length()));
  EXPECT_EQ(ERR_IO_PENDING, socket->Write(buffer.get(), packet->length(),
                                          std::move(write_callback),
                                          TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_FALSE(read_completed);
  EXPECT_FALSE(write_completed);

  socket_data.Resume();
  socket_data.RunUntilAllConsumed();
  RunUntilIdle();

  EXPECT_TRUE(read_completed);
  EXPECT_TRUE(write_completed);
}

// Using `After`, a `Read` and `Write` can be allowed in either order.
TEST_F(QuicSocketDataProviderTest, ParallelReadAndWrite) {
  for (bool read_first : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "read_first: " << read_first);
    QuicSocketDataProvider socket_data(version_);
    MockClientSocketFactory socket_factory;

    socket_data.AddWrite("p1", TestPacket(1)).Sync();
    socket_data.AddRead("p2", TestPacket(2)).Sync().After("p1");
    socket_data.AddWrite("p3", TestPacket(3)).Sync().After("p1");

    socket_factory.AddSocketDataProvider(&socket_data);
    std::unique_ptr<DatagramClientSocket> socket =
        socket_factory.CreateDatagramClientSocket(
            DatagramSocket::BindType::DEFAULT_BIND, nullptr,
            net_log_with_source_.source());
    socket->Connect(IPEndPoint());

    // Write p1 to get things started.
    std::unique_ptr<quic::QuicReceivedPacket> packet = TestPacket(1);
    scoped_refptr<IOBuffer> buffer = base::MakeRefCounted<StringIOBuffer>(
        std::string(packet->data(), packet->length()));
    EXPECT_EQ(static_cast<int>(packet->length()),
              socket->Write(buffer.get(), packet->length(), base::DoNothing(),
                            TRAFFIC_ANNOTATION_FOR_TESTS));

    scoped_refptr<GrowableIOBuffer> read_buffer =
        base::MakeRefCounted<GrowableIOBuffer>();
    read_buffer->SetCapacity(100);
    auto do_read = [&]() {
      EXPECT_EQ(static_cast<int>(TestPacket(2)->length()),
                socket->Read(read_buffer.get(), 100, base::DoNothing()));
    };

    std::unique_ptr<quic::QuicReceivedPacket> write_packet = TestPacket(3);
    buffer = base::MakeRefCounted<StringIOBuffer>(
        std::string(write_packet->data(), write_packet->length()));

    auto do_write = [&]() {
      EXPECT_EQ(static_cast<int>(write_packet->length()),
                socket->Write(buffer.get(), write_packet->length(),
                              base::DoNothing(), TRAFFIC_ANNOTATION_FOR_TESTS));
    };

    // Read p2 and write p3 in both orders.
    if (read_first) {
      do_read();
      do_write();
    } else {
      do_write();
      do_read();
    }

    socket_data.RunUntilAllConsumed();
  }
}

// When multiple Read expectations become ready at the same time, fail with a
// CHECK error.
TEST_F(QuicSocketDataProviderTest, MultipleReadsReady) {
  QuicSocketDataProvider socket_data(version_);
  MockClientSocketFactory socket_factory;

  socket_data.AddWrite("p1", TestPacket(1)).Sync();
  socket_data.AddRead("p2", TestPacket(2)).After("p1");
  socket_data.AddRead("p3", TestPacket(3)).After("p1");

  socket_factory.AddSocketDataProvider(&socket_data);
  std::unique_ptr<DatagramClientSocket> socket =
      socket_factory.CreateDatagramClientSocket(
          DatagramSocket::BindType::DEFAULT_BIND, nullptr,
          net_log_with_source_.source());
  socket->Connect(IPEndPoint());

  std::unique_ptr<quic::QuicReceivedPacket> packet = TestPacket(1);
  scoped_refptr<StringIOBuffer> buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(packet->data(), packet->length()));
  EXPECT_EQ(static_cast<int>(packet->length()),
            socket->Write(buffer.get(), packet->length(), base::DoNothing(),
                          TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_CHECK_DEATH(
      socket->Read(buffer.get(), buffer->size(), base::DoNothing()));
}

}  // namespace net::test
