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

namespace blink {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;

class MockSocketService : public network::mojom::blink::P2PSocket {
 public:
  MOCK_METHOD(void,
              Send,
              (base::span<const uint8_t>, const network::P2PPacketInfo&),
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
               const base::TimeTicks&),
              (override));
};

class SocketClientImplTest : public ::testing::Test {
 public:
  SocketClientImplTest() {
    receiver_.Bind(client_.CreatePendingReceiver());
    remote_.Bind(client_.CreatePendingRemote());
    client_.Init(&delegate_);
  }
  ~SocketClientImplTest() override { client_.Close(); }

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

TEST_F(SocketClientImplTest, OnOpenCalled) {
  EXPECT_CALL(delegate_, OnOpen);
  remote_->SocketCreated(net::IPEndPoint(), net::IPEndPoint());
  task_environment_.RunUntilIdle();
}

TEST_F(SocketClientImplTest, OnDataReceivedCalled) {
  using network::mojom::blink::P2PReceivedPacket;
  using network::mojom::blink::P2PReceivedPacketPtr;
  Open();
  WTF::Vector<P2PReceivedPacketPtr> packets;
  auto first = base::TimeTicks() + base::Microseconds(1);
  auto second = base::TimeTicks() + base::Microseconds(2);
  auto data = WTF::Vector<uint8_t>(1);
  packets.push_back(P2PReceivedPacket::New(data, net::IPEndPoint(), first));
  packets.push_back(P2PReceivedPacket::New(data, net::IPEndPoint(), second));
  InSequence s;
  EXPECT_CALL(delegate_, OnDataReceived(_, _, first));
  EXPECT_CALL(delegate_, OnDataReceived(_, _, second));
  remote_->DataReceived(std::move(packets));
  task_environment_.RunUntilIdle();
}

TEST_F(SocketClientImplTest, OnSendCompleteCalled) {
  Open();
  EXPECT_CALL(delegate_, OnSendComplete);
  remote_->SendComplete(network::P2PSendPacketMetrics());
  task_environment_.RunUntilIdle();
}

TEST_F(SocketClientImplTest, OnConnectionErrorCalled) {
  Open();
  EXPECT_CALL(delegate_, OnError);
  remote_.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(SocketClientImplTest, SendsWithIncreasingPacketId) {
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

TEST_F(SocketClientImplTest, SetsOption) {
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

}  // namespace
}  // namespace blink
