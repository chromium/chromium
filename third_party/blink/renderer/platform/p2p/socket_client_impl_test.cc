#include "third_party/blink/renderer/platform/p2p/socket_client_impl.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom-blink-forward.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/p2p/socket_client_delegate.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace network {
bool operator==(const P2PSendPacketMetrics& a, const P2PSendPacketMetrics& b) {
  return a.packet_id == b.packet_id && a.rtc_packet_id == b.rtc_packet_id &&
         a.send_time_ms == b.send_time_ms;
}
}  // namespace network

namespace blink {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Values;
using ::testing::WithArgs;

class MockSocketService : public network::mojom::blink::P2PSocket {
 public:
  MOCK_METHOD(void,
              Send,
              (base::span<const uint8_t>, const network::P2PPacketInfo&),
              (override));
  MOCK_METHOD(void,
              SendBatch,
              (WTF::Vector<network::mojom::blink::P2PSendPacketPtr>),
              (override));
  MOCK_METHOD(void,
              SetOption,
              (network::P2PSocketOption option, int32_t value),
              (override));
};

class MockDelegate : public P2PSocketClientDelegate {
 public:
  MOCK_METHOD(void,
              OnOpen,
              (const net::IPEndPoint&, const net::IPEndPoint&),
              (override));
  MOCK_METHOD(void,
              OnSendComplete,
              (const network::P2PSendPacketMetrics&),
              (override));
  MOCK_METHOD(void, OnError, (), (override));
  MOCK_METHOD(void,
              OnDataReceived,
              (const net::IPEndPoint&,
               base::span<const uint8_t>,
               const base::TimeTicks&,
               rtc::EcnMarking),
              (override));
};

class SocketClientImplTestBase {
 public:
  explicit SocketClientImplTestBase(bool batch_packets)
      : client_(batch_packets) {
    receiver_.Bind(client_.CreatePendingReceiver());
    remote_.Bind(client_.CreatePendingRemote());
    client_.Init(&delegate_);
  }
  virtual ~SocketClientImplTestBase() { client_.Close(); }

  void Open() {
    ON_CALL(delegate_, OnOpen).WillByDefault(Return());
    remote_->SocketCreated(net::IPEndPoint(), net::IPEndPoint());
    task_environment_.RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockSocketService socket_;
  mojo::Receiver<network::mojom::blink::P2PSocket> receiver_{&socket_};
  P2PSocketClientImpl client_;
  mojo::Remote<network::mojom::blink::P2PSocketClient> remote_;
  NiceMock<MockDelegate> delegate_;
};

class SocketClientImplParametrizedTest : public SocketClientImplTestBase,
                                         public ::testing::TestWithParam<bool> {
 public:
  SocketClientImplParametrizedTest() : SocketClientImplTestBase(GetParam()) {}
};

TEST_P(SocketClientImplParametrizedTest, OnOpenCalled) {
  EXPECT_CALL(delegate_, OnOpen);
  remote_->SocketCreated(net::IPEndPoint(), net::IPEndPoint());
  task_environment_.RunUntilIdle();
}

TEST_P(SocketClientImplParametrizedTest, OnDataReceivedCalled) {
  using network::mojom::blink::P2PReceivedPacket;
  using network::mojom::blink::P2PReceivedPacketPtr;
  Open();
  WTF::Vector<P2PReceivedPacketPtr> packets;
  auto first = base::TimeTicks() + base::Microseconds(1);
  auto second = base::TimeTicks() + base::Microseconds(2);
  auto data = WTF::Vector<uint8_t>(1);
  auto ecn = rtc::EcnMarking::kNotEct;
  packets.push_back(
      P2PReceivedPacket::New(data, net::IPEndPoint(), first, ecn));
  packets.push_back(
      P2PReceivedPacket::New(data, net::IPEndPoint(), second, ecn));
  InSequence s;
  EXPECT_CALL(delegate_, OnDataReceived(_, _, first, ecn));
  EXPECT_CALL(delegate_, OnDataReceived(_, _, second, ecn));
  remote_->DataReceived(std::move(packets));
  task_environment_.RunUntilIdle();
}

TEST_P(SocketClientImplParametrizedTest, OnSendCompleteCalled) {
  Open();
  EXPECT_CALL(delegate_, OnSendComplete);
  remote_->SendComplete(network::P2PSendPacketMetrics());
  task_environment_.RunUntilIdle();
}

TEST_P(SocketClientImplParametrizedTest, OnConnectionErrorCalled) {
  Open();
  EXPECT_CALL(delegate_, OnError);
  remote_.reset();
  task_environment_.RunUntilIdle();
}

TEST_P(SocketClientImplParametrizedTest, SendsWithIncreasingPacketId) {
  Open();
  network::P2PPacketInfo first_info;
  InSequence s;
  EXPECT_CALL(socket_, Send).WillOnce(SaveArg<1>(&first_info));
  EXPECT_CALL(socket_, Send)
      .WillOnce(WithArgs<1>([&first_info](const network::P2PPacketInfo& info) {
        EXPECT_EQ(info.packet_id, first_info.packet_id + 1);
      }));
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1),
               rtc::PacketOptions());
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1),
               rtc::PacketOptions());
  task_environment_.RunUntilIdle();
}

TEST_P(SocketClientImplParametrizedTest, SetsOption) {
  Open();
  InSequence s;
  EXPECT_CALL(socket_,
              SetOption(network::P2PSocketOption::P2P_SOCKET_OPT_DSCP, 1));
  EXPECT_CALL(socket_,
              SetOption(network::P2PSocketOption::P2P_SOCKET_OPT_RCVBUF, 2));
  client_.SetOption(network::P2PSocketOption::P2P_SOCKET_OPT_DSCP, 1);
  client_.SetOption(network::P2PSocketOption::P2P_SOCKET_OPT_RCVBUF, 2);
  task_environment_.RunUntilIdle();
}

TEST_P(SocketClientImplParametrizedTest, OnSendBatchCompleteCalled) {
  Open();
  network::P2PSendPacketMetrics metrics1 = {0, 1, 2};
  network::P2PSendPacketMetrics metrics2 = {0, 1, 2};
  InSequence s;
  EXPECT_CALL(delegate_, OnSendComplete(metrics1));
  EXPECT_CALL(delegate_, OnSendComplete(metrics2));
  remote_->SendBatchComplete({metrics1, metrics2});
  task_environment_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         SocketClientImplParametrizedTest,
                         Values(false, true),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithBatching"
                                             : "WithoutBatching";
                         });

class SocketClientImplBatchingTest : public SocketClientImplTestBase,
                                     public ::testing::Test {
 public:
  SocketClientImplBatchingTest()
      : SocketClientImplTestBase(/*batch_packets=*/true) {}
};

TEST_F(SocketClientImplBatchingTest, OnePacketBatchUsesSend) {
  Open();
  EXPECT_CALL(socket_, Send);
  rtc::PacketOptions options;
  options.batchable = true;
  options.last_packet_in_batch = true;
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1), options);
  task_environment_.RunUntilIdle();
}

TEST_F(SocketClientImplBatchingTest, TwoPacketBatchUsesSendBatch) {
  Open();

  rtc::PacketOptions options;
  options.batchable = true;
  options.packet_id = 1;
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1), options);

  EXPECT_CALL(
      socket_,
      SendBatch(ElementsAre(
          Pointee(Field(&network::mojom::blink::P2PSendPacket::packet_info,
                        Field(&network::P2PPacketInfo::packet_options,
                              Field(&rtc::PacketOptions::packet_id, 1)))),
          Pointee(Field(&network::mojom::blink::P2PSendPacket::packet_info,
                        Field(&network::P2PPacketInfo::packet_options,
                              Field(&rtc::PacketOptions::packet_id, 2)))))));

  options.last_packet_in_batch = true;
  options.packet_id = 2;
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1), options);
  task_environment_.RunUntilIdle();
}

TEST_F(SocketClientImplBatchingTest,
       TwoPacketBatchWithNonbatchableInterleavedUsesSendBatch) {
  Open();

  rtc::PacketOptions batchable_options;
  batchable_options.batchable = true;
  batchable_options.packet_id = 1;
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1), batchable_options);
  rtc::PacketOptions interleaved_options;  // Not batchable.
  interleaved_options.packet_id = 2;
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1), interleaved_options);

  // The expectation is placed after the initial sends to fail the test in case
  // the first sends would create a batch.
  EXPECT_CALL(
      socket_,
      SendBatch(ElementsAre(
          Pointee(Field(&network::mojom::blink::P2PSendPacket::packet_info,
                        Field(&network::P2PPacketInfo::packet_options,
                              Field(&rtc::PacketOptions::packet_id, 1)))),
          Pointee(Field(&network::mojom::blink::P2PSendPacket::packet_info,
                        Field(&network::P2PPacketInfo::packet_options,
                              Field(&rtc::PacketOptions::packet_id, 2)))),
          Pointee(Field(&network::mojom::blink::P2PSendPacket::packet_info,
                        Field(&network::P2PPacketInfo::packet_options,
                              Field(&rtc::PacketOptions::packet_id, 3)))))));

  batchable_options.last_packet_in_batch = true;
  batchable_options.packet_id = 3;
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1), batchable_options);
  task_environment_.RunUntilIdle();
}

TEST_F(SocketClientImplBatchingTest, PacketBatchCompletedWithFlush) {
  Open();

  rtc::PacketOptions batchable_options;
  batchable_options.batchable = true;
  batchable_options.packet_id = 1;
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1), batchable_options);
  batchable_options.packet_id = 2;
  client_.Send(net::IPEndPoint(), std::vector<uint8_t>(1), batchable_options);

  // Expects packets to be sent on FlushBatch.
  EXPECT_CALL(
      socket_,
      SendBatch(ElementsAre(
          Pointee(Field(&network::mojom::blink::P2PSendPacket::packet_info,
                        Field(&network::P2PPacketInfo::packet_options,
                              Field(&rtc::PacketOptions::packet_id, 1)))),
          Pointee(Field(&network::mojom::blink::P2PSendPacket::packet_info,
                        Field(&network::P2PPacketInfo::packet_options,
                              Field(&rtc::PacketOptions::packet_id, 2)))))));
  client_.FlushBatch();
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace blink
