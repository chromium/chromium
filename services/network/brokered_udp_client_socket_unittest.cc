// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/brokered_udp_client_socket.h"

#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_source.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/udp_server_socket.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/brokered_client_socket_factory.h"
#include "services/network/test/test_socket_broker_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "services/network/broker_helper_win.h"
#endif

using net::test::IsError;
using net::test::IsOk;
using testing::Not;

namespace network {

#if BUILDFLAG(IS_WIN)
// A BrokerHelper delegate to manually set whether a socket needs to be
// brokered. This is necessary to make sure we can test connecting unbrokered
// sockets on Windows, since otherwise ShouldBroker would return true for
// localhost addresses.
class TestBrokerHelperDelegate : public BrokerHelperWin::Delegate {
 public:
  explicit TestBrokerHelperDelegate(bool should_broker)
      : should_broker_(should_broker) {}

  bool ShouldBroker() const override { return should_broker_; }

 private:
  bool should_broker_;
};
#endif

// This class's only purpose is to return a BrokeredUdpClientSocket instead of a
// DatagramClientSocket. This is necessary as BrokeredUdpClientSocket has
// specific helper methods for unit tests that DatagramclientSocket does not
// need.
class TestBrokeredClientSocketFactory : public BrokeredClientSocketFactory {
 public:
  explicit TestBrokeredClientSocketFactory(
      mojo::PendingRemote<mojom::SocketBroker> pending_remote)
      : BrokeredClientSocketFactory(std::move(pending_remote)) {}

  std::unique_ptr<BrokeredUdpClientSocket> CreateBrokeredUdpClientSocket(
      net::DatagramSocket::BindType bind_type,
      net::NetLog* net_log,
      const net::NetLogSource& source) {
    return std::make_unique<BrokeredUdpClientSocket>(bind_type, net_log, source,
                                                     this);
  }
};

class BrokeredUdpClientSocketTest : public testing::Test,
                                    public net::WithTaskEnvironment {
 public:
  BrokeredUdpClientSocketTest()
      : receiver_(&socket_broker_impl_),
        client_socket_factory_(TestBrokeredClientSocketFactory(
            receiver_.BindNewPipeAndPassRemote())),
        buffer_(base::MakeRefCounted<net::IOBufferWithSize>(kMaxRead)) {}

  ~BrokeredUdpClientSocketTest() override = default;

  void SetUp() override {
    // Set up the socket_
    socket_ = client_socket_factory_.CreateBrokeredUdpClientSocket(
        net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
        net::NetLogSource());

    // Confirm that we fail gracefully when making certain calls before
    // connecting.
    net::IPEndPoint test_address;
    EXPECT_EQ(socket_->GetPeerAddress(&test_address),
              net::ERR_SOCKET_NOT_CONNECTED);
    EXPECT_EQ(socket_->GetLocalAddress(&test_address),
              net::ERR_SOCKET_NOT_CONNECTED);
  }

  // Writes specified message to the socket.
  int WriteToClientSocket(const std::string& msg) {
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(msg);
    net::TestCompletionCallback callback;
    int rv = socket_->Write(io_buffer.get(), io_buffer->size(),
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    return callback.GetResult(rv);
  }

  std::string ReadFromClientSocket() {
    net::TestCompletionCallback callback;

    int rv = socket_->Read(buffer_.get(), kMaxRead, callback.callback());
    rv = callback.GetResult(rv);
    if (rv < 0) {
      return std::string();
    }
    return std::string(buffer_->data(), rv);
  }

  int SendToSocket(net::UDPServerSocket* socket,
                   std::string msg,
                   const net::IPEndPoint& address) {
    auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(msg);
    net::TestCompletionCallback callback;
    int rv = socket->SendTo(io_buffer.get(), io_buffer->size(), address,
                            callback.callback());
    return callback.GetResult(rv);
  }

  // Blocks until data is read from the socket.
  std::string RecvFromSocket(net::UDPServerSocket* socket,
                             net::IPEndPoint& address) {
    net::TestCompletionCallback callback;
    int rv = socket->RecvFrom(buffer_.get(), kMaxRead, &address,
                              callback.callback());
    rv = callback.GetResult(rv);
    if (rv < 0) {
      return std::string();
    }
    return std::string(buffer_->data(), rv);
  }

  void SimpleReadAndWrite(net::UDPServerSocket* server) {
    std::string simple_message("hello world!");
    int rv = WriteToClientSocket(simple_message);
    net::IPEndPoint address;
    EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));
    // Server waits for message.
    std::string str = RecvFromSocket(server, address);
    EXPECT_EQ(simple_message, str);
    // Server echoes reply.
    rv = SendToSocket(server, simple_message, address);
    EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));
    // Client waits for response.
    str = ReadFromClientSocket();
    EXPECT_EQ(simple_message, str);
  }

 protected:
  std::unique_ptr<BrokeredUdpClientSocket> socket_;
  mojo::Receiver<mojom::SocketBroker> receiver_;
  TestBrokeredClientSocketFactory client_socket_factory_;
  scoped_refptr<net::IOBufferWithSize> buffer_;
  static const int kMaxRead = 1024;

  TestSocketBrokerImpl socket_broker_impl_;
};

const int BrokeredUdpClientSocketTest::kMaxRead;

TEST_F(BrokeredUdpClientSocketTest, FailedConnectAsync) {
  net::TestCompletionCallback callback;
  base::test::ScopedDisableRunLoopTimeout disable_timeout;
  net::IPEndPoint server_address(net::IPAddress::IPv4Localhost(),
                                 /*port=*/8080);

  socket_broker_impl_.SetConnectionFailure(true);

  int rv = socket_->ConnectAsync(server_address, callback.callback());

  ASSERT_EQ(rv, net::ERR_IO_PENDING);
  rv = callback.WaitForResult();
  EXPECT_EQ(rv, net::ERR_CONNECTION_FAILED);
}

TEST_F(BrokeredUdpClientSocketTest, ConnectAsync) {
  ASSERT_EQ(0, net::GetGlobalUDPSocketCountForTesting());
  net::TestCompletionCallback callback;
  net::IPEndPoint server_address(net::IPAddress::IPv4Localhost(),
                                 /*port=*/8080);

  int rv = socket_->ConnectAsync(server_address, callback.callback());

  ASSERT_EQ(rv, net::ERR_IO_PENDING);
  rv = callback.WaitForResult();
  EXPECT_EQ(rv, net::OK);
  EXPECT_EQ(net::handles::kInvalidNetworkHandle, socket_->GetBoundNetwork());
  ASSERT_EQ(1, net::GetGlobalUDPSocketCountForTesting());
  socket_->Close();
  ASSERT_EQ(0, net::GetGlobalUDPSocketCountForTesting());
}

TEST_F(BrokeredUdpClientSocketTest, Connect) {
  ASSERT_EQ(0, net::GetGlobalUDPSocketCountForTesting());
  net::TestCompletionCallback callback;
  net::IPEndPoint server_address(net::IPAddress::IPv4Localhost(),
                                 /*port=*/8080);
  int rv = net::OK;

#if BUILDFLAG(IS_WIN)
  // Pretending we don't need to broker a localhost address to be able to
  // reliably test connecting synchronously.
  socket_->SetBrokerHelperDelegateForTesting(
      std::make_unique<TestBrokerHelperDelegate>(false));
  rv = socket_->Connect(server_address);
  ASSERT_EQ(rv, net::OK);
  EXPECT_EQ(net::handles::kInvalidNetworkHandle, socket_->GetBoundNetwork());

  // ConnectUsingNetwork and ConnectUsingDefaultNetwork should return
  // ERR_NOT_IMPLEMENTED even if brokering is not required on windows.
  auto socket2 = client_socket_factory_.CreateBrokeredUdpClientSocket(
      net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
      net::NetLogSource());
  socket2->SetBrokerHelperDelegateForTesting(
      std::make_unique<TestBrokerHelperDelegate>(false));
  rv = socket2->ConnectUsingNetwork(net::handles::kInvalidNetworkHandle,
                                    server_address);
  ASSERT_EQ(rv, net::ERR_NOT_IMPLEMENTED);
  EXPECT_EQ(net::handles::kInvalidNetworkHandle, socket2->GetBoundNetwork());

  auto socket3 = client_socket_factory_.CreateBrokeredUdpClientSocket(
      net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
      net::NetLogSource());
  socket3->SetBrokerHelperDelegateForTesting(
      std::make_unique<TestBrokerHelperDelegate>(false));
  rv = socket3->ConnectUsingDefaultNetwork(server_address);
  ASSERT_EQ(rv, net::ERR_NOT_IMPLEMENTED);
  EXPECT_EQ(net::handles::kInvalidNetworkHandle, socket3->GetBoundNetwork());
#else
  rv = socket_->Connect(server_address);
  ASSERT_EQ(rv, net::ERR_NOT_IMPLEMENTED);
  EXPECT_EQ(net::handles::kInvalidNetworkHandle, socket_->GetBoundNetwork());
#endif

  // ConnectUsingNetwork and ConnectUsingDefaultNetwork should also return
  // ERR_NOT_IMPLEMENTED on all platforms.
  auto socket4 = client_socket_factory_.CreateDatagramClientSocket(
      net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
      net::NetLogSource());
  rv = socket4->ConnectUsingNetwork(net::handles::kInvalidNetworkHandle,
                                    server_address);
  ASSERT_EQ(rv, net::ERR_NOT_IMPLEMENTED);
  EXPECT_EQ(net::handles::kInvalidNetworkHandle, socket4->GetBoundNetwork());
  auto socket5 = client_socket_factory_.CreateDatagramClientSocket(
      net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
      net::NetLogSource());
  rv = socket5->ConnectUsingDefaultNetwork(server_address);
  ASSERT_EQ(rv, net::ERR_NOT_IMPLEMENTED);
  EXPECT_EQ(net::handles::kInvalidNetworkHandle, socket5->GetBoundNetwork());
}

TEST_F(BrokeredUdpClientSocketTest, SetOptions) {
  net::TestCompletionCallback callback;
  net::IPEndPoint server_address(net::IPAddress::IPv4Localhost(),
                                 /*port=*/8080);
  EXPECT_THAT(socket_->SetMulticastInterface(1), IsOk());
  socket_->SetMsgConfirm(true);
  socket_->EnableRecvOptimization();
  socket_->UseNonBlockingIO();
  int rv = socket_->ConnectAsync(server_address, callback.callback());

  ASSERT_EQ(rv, net::ERR_IO_PENDING);
  rv = callback.WaitForResult();
  EXPECT_EQ(rv, net::OK);

  EXPECT_EQ(socket_->get_multicast_interface_for_testing(), uint32_t(1));
#if (!BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN))
  EXPECT_TRUE(socket_->get_msg_confirm_for_testing());
#endif
#if BUILDFLAG(IS_POSIX)
  EXPECT_TRUE(socket_->get_recv_optimization_for_testing());
#endif
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(socket_->get_use_non_blocking_io_for_testing());

  // Set up a new socket to check that options are set correctly when sockets
  // don't need to be brokered on win.
  auto new_socket = client_socket_factory_.CreateBrokeredUdpClientSocket(
      net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
      net::NetLogSource());

  net::TestCompletionCallback callback2;
  net::IPEndPoint server_address2(net::IPAddress::IPv4AllZeros(),
                                  /*port=*/8080);
  EXPECT_THAT(new_socket->SetMulticastInterface(1), IsOk());
  new_socket->UseNonBlockingIO();
  rv = new_socket->ConnectAsync(server_address2, callback2.callback());

  // `new_socket` shouldn't successfully connect since the address is invalid,
  // but the options should still be set.
  EXPECT_EQ(rv, net::ERR_ADDRESS_INVALID);
  EXPECT_EQ(new_socket->get_multicast_interface_for_testing(), uint32_t(1));
  EXPECT_TRUE(new_socket->get_use_non_blocking_io_for_testing());
#endif
}

TEST_F(BrokeredUdpClientSocketTest, SimpleReadWrite) {
  net::TestCompletionCallback callback;
  net::UDPServerSocket server(nullptr, net::NetLogSource());
  ASSERT_THAT(
      server.Listen(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)),
      IsOk());
  net::IPEndPoint server_address;
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());

  int rv = socket_->ConnectAsync(server_address, callback.callback());

  ASSERT_EQ(rv, net::ERR_IO_PENDING);
  rv = callback.WaitForResult();
  EXPECT_EQ(rv, net::OK);

  SimpleReadAndWrite(&server);
}

TEST_F(BrokeredUdpClientSocketTest, ConnectUsingNetworkAsync) {
  // The specific value of this address doesn't really matter, and no
  // server needs to be running here. The test only needs to call
  // ConnectUsingNetworkAsync() and won't send any datagrams.
  net::IPEndPoint server_address(net::IPAddress::IPv4Localhost(),
                                 /*port=*/8080);
  const net::handles::NetworkHandle wrong_network_handle = 65536;
  net::TestCompletionCallback callback;
#if BUILDFLAG(IS_ANDROID)
  net::NetworkChangeNotifierFactoryAndroid ncn_factory;
  net::NetworkChangeNotifierDelegateAndroid::
      EnableNetworkChangeNotifierAutoDetectForTest();
  std::unique_ptr<net::NetworkChangeNotifier> ncn(ncn_factory.CreateInstance());
  if (!net::NetworkChangeNotifier::AreNetworkHandlesSupported()) {
    GTEST_SKIP() << "Network handles are required to test BindToNetwork.";
  }

  {
    // Connecting using a not existing network should fail but not report
    // ERR_NOT_IMPLEMENTED when network handles are supported.
    auto socket = client_socket_factory_.CreateDatagramClientSocket(
        net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
        net::NetLogSource());
    int rv = socket->ConnectUsingNetworkAsync(
        wrong_network_handle, server_address, callback.callback());
    EXPECT_EQ(rv, net::ERR_IO_PENDING);
    rv = callback.WaitForResult();
    EXPECT_NE(net::ERR_NOT_IMPLEMENTED, rv);
    EXPECT_NE(net::OK, rv);
  }

  {
    // Connecting using an existing network should succeed when
    // NetworkChangeNotifier returns a valid default network.
    const net::handles::NetworkHandle network_handle =
        net::NetworkChangeNotifier::GetDefaultNetwork();
    if (network_handle != net::handles::kInvalidNetworkHandle) {
      auto socket2 = client_socket_factory_.CreateDatagramClientSocket(
          net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
          net::NetLogSource());
      int rv = socket2->ConnectUsingNetworkAsync(network_handle, server_address,
                                                 callback.callback());
      EXPECT_EQ(rv, net::ERR_IO_PENDING);
      rv = callback.WaitForResult();
      EXPECT_EQ(net::OK, rv);
      EXPECT_EQ(network_handle, socket2->GetBoundNetwork());
      // Also check that connecting using the default network succeeds with a
      // valid default network.
      auto socket3 = client_socket_factory_.CreateDatagramClientSocket(
          net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
          net::NetLogSource());
      rv = socket3->ConnectUsingDefaultNetworkAsync(server_address,
                                                    callback.callback());
      EXPECT_EQ(rv, net::ERR_IO_PENDING);
      rv = callback.WaitForResult();
      EXPECT_EQ(net::OK, rv);
      EXPECT_EQ(network_handle, socket3->GetBoundNetwork());
    }
  }
#else
  EXPECT_EQ(net::ERR_NOT_IMPLEMENTED,
            socket_->ConnectUsingNetworkAsync(
                wrong_network_handle, server_address, callback.callback()));
  auto socket2 = client_socket_factory_.CreateDatagramClientSocket(
      net::DatagramSocket::DEFAULT_BIND, net::NetLog::Get(),
      net::NetLogSource());
  EXPECT_EQ(net::ERR_NOT_IMPLEMENTED, socket2->ConnectUsingDefaultNetworkAsync(
                                          server_address, callback.callback()));
#endif  // BUILDFLAG(IS_ANDROID)
}

// On Android, where socket tagging is supported, verify that the
// BrokeredUdpClientSocket sets tags correctly.
#if BUILDFLAG(IS_ANDROID)
TEST_F(BrokeredUdpClientSocketTest, Tag) {
  if (!net::CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  net::TestCompletionCallback callback;
  net::UDPServerSocket server(nullptr, net::NetLogSource());
  ASSERT_THAT(
      server.Listen(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)),
      IsOk());
  net::IPEndPoint server_address;
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());

  // Verify tag is properly set when ApplySocketTag is called before connecting.
  int32_t tag_val1 = 0x12345678;
  uint64_t old_traffic = net::GetTaggedBytes(tag_val1);
  net::SocketTag tag1(net::SocketTag::UNSET_UID, tag_val1);
  socket_->ApplySocketTag(tag1);
  int rv = socket_->ConnectAsync(server_address, callback.callback());

  ASSERT_EQ(rv, net::ERR_IO_PENDING);
  rv = callback.WaitForResult();
  EXPECT_EQ(rv, net::OK);
  SimpleReadAndWrite(&server);
  EXPECT_GT(net::GetTaggedBytes(tag_val1), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  int32_t tag_val2 = 0x87654321;
  old_traffic = net::GetTaggedBytes(tag_val2);
  net::SocketTag tag2(getuid(), tag_val2);
  socket_->ApplySocketTag(tag2);
  SimpleReadAndWrite(&server);
  EXPECT_GT(net::GetTaggedBytes(tag_val2), old_traffic);
}
#endif

}  // namespace network
