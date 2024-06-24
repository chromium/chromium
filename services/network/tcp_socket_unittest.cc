// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/server_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/mojo_socket_test_util.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom-forward.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/socket_factory.h"
#include "services/network/tcp_connected_socket.h"
#include "services/network/tcp_server_socket.h"
#include "services/network/test/test_socket_broker_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace network {

namespace {

// A mock ServerSocket that completes Accept() using a specified result.
class MockServerSocket : public net::ServerSocket {
 public:
  explicit MockServerSocket(
      std::vector<std::unique_ptr<net::StaticSocketDataProvider>>
          data_providers)
      : data_providers_(std::move(data_providers)) {}

  ~MockServerSocket() override {}

  // net::ServerSocket implementation.
  int Listen(const net::IPEndPoint& address,
             int backlog,
             std::optional<bool> ipv6_only) override {
    return net::OK;
  }

  int GetLocalAddress(net::IPEndPoint* address) const override {
    return net::OK;
  }

  int Accept(std::unique_ptr<net::StreamSocket>* socket,
             net::CompletionOnceCallback callback) override {
    DCHECK(accept_callback_.is_null());
    if (accept_result_ == net::OK && mode_ == net::SYNCHRONOUS)
      *socket = CreateMockAcceptSocket();
    if (mode_ == net::ASYNC || accept_result_ == net::ERR_IO_PENDING) {
      accept_socket_ = socket;
      accept_callback_ = std::move(callback);
    }
    run_loop_.Quit();

    if (mode_ == net::ASYNC) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MockServerSocket::CompleteAccept,
                                    base::Unretained(this), accept_result_));
      return net::ERR_IO_PENDING;
    }
    return accept_result_;
  }

  void SetAcceptResult(net::IoMode mode, int result) {
    // It doesn't make sense to return net::ERR_IO_PENDING asynchronously.
    DCHECK(!(mode == net::ASYNC && result == net::ERR_IO_PENDING));

    mode_ = mode;
    accept_result_ = result;
  }

  void WaitForFirstAccept() { run_loop_.Run(); }

  void CompleteAccept(int result) {
    DCHECK(!accept_callback_.is_null());
    DCHECK_NE(net::ERR_IO_PENDING, result);

    *accept_socket_ = CreateMockAcceptSocket();
    accept_socket_ = nullptr;
    std::move(accept_callback_).Run(result);
  }

 private:
  std::unique_ptr<net::StreamSocket> CreateMockAcceptSocket() {
    DCHECK_GT(data_providers_.size(), next_data_provider_index_);
    auto mock_socket = std::make_unique<net::MockTCPClientSocket>(
        net::AddressList(), nullptr /*netlog*/,
        data_providers_[next_data_provider_index_++].get());
    mock_socket->set_enable_read_if_ready(true);
    EXPECT_EQ(net::OK, mock_socket->Connect(base::DoNothing()));
    return std::move(mock_socket);
  }

  net::IoMode mode_ = net::SYNCHRONOUS;
  int accept_result_ = net::OK;
  net::CompletionOnceCallback accept_callback_;
  raw_ptr<std::unique_ptr<net::StreamSocket>> accept_socket_;
  base::RunLoop run_loop_;
  std::vector<std::unique_ptr<net::StaticSocketDataProvider>> data_providers_;
  size_t next_data_provider_index_ = 0;
};

// A MockServerSocket that fails at GetLocalAddress().
class FailingServerSocket : public MockServerSocket {
 public:
  FailingServerSocket()
      : MockServerSocket(
            std::vector<std::unique_ptr<net::StaticSocketDataProvider>>()) {}

  ~FailingServerSocket() override {}

  int GetLocalAddress(net::IPEndPoint* address) const override {
    return net::ERR_FAILED;
  }
};

// A server implemented using mojom::TCPServerSocket. It owns the server socket
// pointer and as well as client connections. SendData() and StartReading()
// operate on the newest client connection.
class TestServer {
 public:
  TestServer()
      : TestServer(net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0)) {}
  explicit TestServer(const net::IPEndPoint& server_addr)
      : url_request_context_(
            net::CreateTestURLRequestContextBuilder()->Build()),
        factory_(nullptr, url_request_context_.get()),
        readable_handle_watcher_(FROM_HERE,
                                 mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC),
        server_addr_(server_addr) {}
  ~TestServer() {}

  void Start(uint32_t backlog, bool should_fail_socket_creation = false) {
    int net_error = net::ERR_FAILED;
    base::RunLoop run_loop;
    auto options = mojom::TCPServerSocketOptions::New();
    options->backlog = backlog;
    factory_.CreateTCPServerSocket(
        server_addr_, std::move(options), TRAFFIC_ANNOTATION_FOR_TESTS,
        server_socket_.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting(
            [&](int result, const std::optional<net::IPEndPoint>& local_addr) {
              net_error = result;
              if (local_addr)
                server_addr_ = local_addr.value();
              run_loop.Quit();
            }));
    run_loop.Run();
    if (should_fail_socket_creation) {
      EXPECT_EQ(net::ERR_CONNECTION_FAILED, net_error);
    } else {
      EXPECT_EQ(net::OK, net_error);
    }
  }

#if BUILDFLAG(IS_WIN)
  void StartWithBroker(uint32_t backlog, bool should_fail_socket_creation) {
    socket_broker_impl_.SetConnectionFailure(should_fail_socket_creation);
    mojo::Receiver<mojom::SocketBroker> receiver(&socket_broker_impl_);
    factory_.BindSocketBroker(receiver.BindNewPipeAndPassRemote());

    Start(backlog, should_fail_socket_creation);
  }
#endif

  // Accepts one connection. Upon successful completion, |callback| will be
  // invoked.
  void AcceptOneConnection(net::CompletionOnceCallback callback) {
    server_socket_->Accept(
        mojo::NullRemote(),
        base::BindOnce(&TestServer::OnAccept, base::Unretained(this),
                       std::move(callback)));
  }

  // Sends data over the most recent connection that is established.
  void SendData(const std::string& msg) {
    EXPECT_TRUE(mojo::BlockingCopyFromString(msg, server_socket_send_handle_));
  }

  // Starts reading. Can be called multiple times. It cancels any previous
  // StartReading(). Once |expected_contents| is read, |callback| will be
  // invoked. If an error occurs or the pipe is broken before read can
  // complete, |callback| will be run, but ADD_FAILURE() will be called.
  void StartReading(const std::string& expected_contents,
                    base::OnceClosure callback) {
    readable_handle_watcher_.Cancel();
    received_contents_.clear();
    expected_contents_ = expected_contents;
    read_callback_ = std::move(callback);
    readable_handle_watcher_.Watch(
        server_socket_receive_handle_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&TestServer::OnReadable, base::Unretained(this)));
  }

  void DestroyServerSocket() { server_socket_.reset(); }

  const net::IPEndPoint& server_addr() { return server_addr_; }

  mojom::TCPConnectedSocket* most_recent_connected_socket() {
    return connected_sockets_.back().get();
  }

 private:
  void OnAccept(net::CompletionOnceCallback callback,
                int result,
                const std::optional<net::IPEndPoint>& remote_addr,
                mojo::PendingRemote<mojom::TCPConnectedSocket> connected_socket,
                mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
                mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
    connected_sockets_.emplace_back(std::move(connected_socket));
    server_socket_receive_handle_ = std::move(receive_pipe_handle);
    server_socket_send_handle_ = std::move(send_pipe_handle);
    std::move(callback).Run(result);
  }

  void OnReadable(MojoResult result) {
    if (result != MOJO_RESULT_OK) {
      ADD_FAILURE() << "Unexpected broken pipe with error: " << result;
      EXPECT_EQ(expected_contents_, received_contents_);
      std::move(read_callback_).Run();
      return;
    }
    std::string buffer(16, '\0');
    size_t actually_read_bytes = 0;
    result = server_socket_receive_handle_->ReadData(
        MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
        actually_read_bytes);
    if (result == MOJO_RESULT_SHOULD_WAIT)
      return;
    if (result != MOJO_RESULT_OK) {
      ADD_FAILURE() << "Unexpected read error: " << result;
      EXPECT_EQ(expected_contents_, received_contents_);
      std::move(read_callback_).Run();
      return;
    }

    received_contents_.append(
        std::string_view(buffer).substr(0, actually_read_bytes));

    if (received_contents_.size() == expected_contents_.size()) {
      EXPECT_EQ(expected_contents_, received_contents_);
      std::move(read_callback_).Run();
    }
  }

#if BUILDFLAG(IS_WIN)
  TestSocketBrokerImpl socket_broker_impl_;
#endif

  std::unique_ptr<net::URLRequestContext> url_request_context_;
  SocketFactory factory_;
  mojo::Remote<mojom::TCPServerSocket> server_socket_;
  std::vector<mojo::Remote<mojom::TCPConnectedSocket>> connected_sockets_;
  mojo::ScopedDataPipeConsumerHandle server_socket_receive_handle_;
  mojo::ScopedDataPipeProducerHandle server_socket_send_handle_;
  mojo::SimpleWatcher readable_handle_watcher_;
  net::IPEndPoint server_addr_;
  std::string expected_contents_;
  base::OnceClosure read_callback_;
  std::string received_contents_;
};

}  // namespace

class TCPSocketTest : public testing::Test {
 public:
  TCPSocketTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  TCPSocketTest(const TCPSocketTest&) = delete;
  TCPSocketTest& operator=(const TCPSocketTest&) = delete;

  ~TCPSocketTest() override {}

  void Init(net::ClientSocketFactory* mock_client_socket_factory) {
    auto context_builder = net::CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(
        mock_client_socket_factory);
    url_request_context_ = context_builder->Build();
    factory_ = std::make_unique<SocketFactory>(nullptr /*net_log*/,
                                               url_request_context_.get());
  }

  void SetUp() override { Init(nullptr); }

  // Reads |num_bytes| from |handle| or reads until an error occurs. Returns the
  // bytes read as a string.
  std::string Read(mojo::ScopedDataPipeConsumerHandle* handle,
                   size_t num_bytes) {
    std::string received_contents;
    while (received_contents.size() < num_bytes) {
      base::RunLoop().RunUntilIdle();
      std::string buffer(num_bytes, '\0');
      size_t actually_read_bytes = 0;
      MojoResult result = handle->get().ReadData(
          MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
          actually_read_bytes);
      if (result == MOJO_RESULT_SHOULD_WAIT)
        continue;
      if (result != MOJO_RESULT_OK)
        return received_contents;
      received_contents.append(
          std::string_view(buffer).substr(0, actually_read_bytes));
    }
    return received_contents;
  }

  // Creates a TCPServerSocket with the mock server socket, |socket|.
  void CreateServerSocketWithMockSocket(
      uint32_t backlog,
      mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
      std::unique_ptr<net::ServerSocket> socket) {
    auto server_socket_impl = std::make_unique<TCPServerSocket>(
        factory_.get(), nullptr /*netlog*/, TRAFFIC_ANNOTATION_FOR_TESTS);
    server_socket_impl->SetSocketForTest(std::move(socket));
    net::IPEndPoint local_addr;
    auto result = server_socket_impl->Listen(local_addr, backlog,
                                             /*ipv6_only=*/std::nullopt);
    EXPECT_TRUE(result.has_value());
    tcp_server_socket_receiver_.Add(std::move(server_socket_impl),
                                    std::move(receiver));
  }

  int CreateTCPConnectedSocketSync(
      mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver,
      mojo::PendingRemote<mojom::SocketObserver> observer,
      const std::optional<net::IPEndPoint>& local_addr,
      const net::IPEndPoint& remote_addr,
      mojo::ScopedDataPipeConsumerHandle* receive_pipe_handle_out,
      mojo::ScopedDataPipeProducerHandle* send_pipe_handle_out,
      mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
          nullptr) {
    return CreateTCPConnectedSocketSync(
        std::move(receiver), std::move(observer), local_addr,
        net::AddressList(remote_addr), receive_pipe_handle_out,
        send_pipe_handle_out, std::move(tcp_connected_socket_options));
  }

  int CreateTCPConnectedSocketSync(
      mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver,
      mojo::PendingRemote<mojom::SocketObserver> observer,
      const std::optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      mojo::ScopedDataPipeConsumerHandle* receive_pipe_handle_out,
      mojo::ScopedDataPipeProducerHandle* send_pipe_handle_out,
      mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
          nullptr) {
    base::RunLoop run_loop;
    int net_error = net::ERR_FAILED;
    factory_->CreateTCPConnectedSocket(
        local_addr, remote_addr_list, std::move(tcp_connected_socket_options),
        TRAFFIC_ANNOTATION_FOR_TESTS, std::move(receiver), std::move(observer),
        base::BindLambdaForTesting(
            [&](int result,
                const std::optional<net::IPEndPoint>& actual_local_addr,
                const std::optional<net::IPEndPoint>& peer_addr,
                mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
                mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
              net_error = result;
              if (result == net::OK) {
                EXPECT_NE(0, actual_local_addr.value().port());
                if (remote_addr_list.size() == 1) {
                  EXPECT_EQ(remote_addr_list.front(), peer_addr.value());
                }
              }
              *receive_pipe_handle_out = std::move(receive_pipe_handle);
              *send_pipe_handle_out = std::move(send_pipe_handle);
              run_loop.Quit();
            }));
    run_loop.Run();
    return net_error;
  }

  TestSocketObserver* observer() { return &test_observer_; }
  SocketFactory* factory() { return factory_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<SocketFactory> factory_;
  TestSocketObserver test_observer_;
  mojo::UniqueReceiverSet<mojom::TCPServerSocket> tcp_server_socket_receiver_;
};

#if BUILDFLAG(IS_WIN)
TEST_F(TCPSocketTest, BrokerCreateTCPServerSocketSuccess) {
  TestServer server;
  server.StartWithBroker(1 /*backlog*/, false /*fail_server_socket_creation*/);
}

TEST_F(TCPSocketTest, BrokerCreateTCPServerSocketFailure) {
  TestServer server;
  server.StartWithBroker(1 /*backlog*/, true /*fail_server_socket_creation*/);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(TCPSocketTest, ReadAndWrite) {
  const struct TestData {
    std::optional<net::IPEndPoint> client_addr;
    net::IPEndPoint server_addr;
  } kTestCases[] = {
      {std::nullopt, net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)},
      {std::nullopt, net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0)},
      {net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0),
       net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)},
      {net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0),
       net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0)},
  };
  for (auto test : kTestCases) {
    TestServer server(test.server_addr);
    server.Start(1 /*backlog*/);
    net::TestCompletionCallback accept_callback;
    server.AcceptOneConnection(accept_callback.callback());

    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

    mojo::Remote<mojom::TCPConnectedSocket> client_socket;
    EXPECT_EQ(net::OK, CreateTCPConnectedSocketSync(
                           client_socket.BindNewPipeAndPassReceiver(),
                           mojo::NullRemote() /*observer*/, test.client_addr,
                           server.server_addr(), &client_socket_receive_handle,
                           &client_socket_send_handle));
    ASSERT_EQ(net::OK, accept_callback.WaitForResult());

    // Test sending data from server to client.
    const char kTestMsg[] = "hello";
    server.SendData(kTestMsg);
    EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, strlen(kTestMsg)));

    // Test sending data from client to server.
    base::RunLoop read_run_loop;
    server.StartReading(kTestMsg, read_run_loop.QuitClosure());
    EXPECT_TRUE(
        mojo::BlockingCopyFromString(kTestMsg, client_socket_send_handle));
    read_run_loop.Run();
  }
}

TEST_F(TCPSocketTest, CannotConnectToWrongInterface) {
  const struct TestData {
    net::IPEndPoint client_addr;
    net::IPEndPoint server_addr;
  } kTestCases[] = {
      {net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0),
       net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)},
      {net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0),
       net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0)},
  };
  for (auto test : kTestCases) {
    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

    TestServer server(test.server_addr);
    server.Start(1 /*backlog*/);
    net::TestCompletionCallback accept_callback;
    server.AcceptOneConnection(accept_callback.callback());

    mojo::Remote<mojom::TCPConnectedSocket> client_socket;
    int result = CreateTCPConnectedSocketSync(
        client_socket.BindNewPipeAndPassReceiver(),
        mojo::NullRemote() /*observer*/, test.client_addr, server.server_addr(),
        &client_socket_receive_handle, &client_socket_send_handle);
    // Both net::ERR_INVALID_ARGUMENT and net::ERR_ADDRESS_UNREACHABLE can be
    // returned. On Linux, for eample, the former is returned when talking ipv4
    // to a ipv6 remote, and the latter is returned when talking ipv6 to a ipv4
    // remote. net::ERR_CONNECTION_FAILED is returned on Windows.
    EXPECT_TRUE(result == net::ERR_CONNECTION_FAILED ||
                result == net::ERR_INVALID_ARGUMENT ||
                result == net::ERR_ADDRESS_UNREACHABLE)
        << "actual result: " << result;
  }
}

TEST_F(TCPSocketTest, ServerReceivesMultipleAccept) {
  uint32_t backlog = 10;
  TestServer server;
  server.Start(backlog);

  std::vector<std::unique_ptr<net::TestCompletionCallback>> accept_callbacks;
  // Issue |backlog| Accepts(), so the queue is filled up.
  for (size_t i = 0; i < backlog; ++i) {
    auto callback = std::make_unique<net::TestCompletionCallback>();
    server.AcceptOneConnection(callback->callback());
    accept_callbacks.push_back(std::move(callback));
  }
  // Accept() beyond the queue size should fail immediately.
  net::TestCompletionCallback callback;
  server.AcceptOneConnection(callback.callback());
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES, callback.WaitForResult());

  // After handling incoming connections, all callbacks should now complete.
  std::vector<mojo::Remote<mojom::TCPConnectedSocket>> client_sockets;
  for (size_t i = 0; i < backlog; ++i) {
    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
    mojo::Remote<mojom::TCPConnectedSocket> client_socket;
    EXPECT_EQ(net::OK,
              CreateTCPConnectedSocketSync(
                  client_socket.BindNewPipeAndPassReceiver(),
                  mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/,
                  server.server_addr(), &client_socket_receive_handle,
                  &client_socket_send_handle));
    client_sockets.push_back(std::move(client_socket));
  }
  for (const auto& accept_callback : accept_callbacks) {
    EXPECT_EQ(net::OK, accept_callback->WaitForResult());
  }
}

// Check that accepted sockets can't be upgraded to TLS, since UpgradeToTLS only
// supports the client side of a TLS handshake.
TEST_F(TCPSocketTest, AcceptedSocketCantUpgradeToTLS) {
  TestServer server;
  server.Start(1 /* backlog */);

  net::TestCompletionCallback callback;
  server.AcceptOneConnection(callback.callback());

  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/,
                server.server_addr(), &client_socket_receive_handle,
                &client_socket_send_handle));

  EXPECT_EQ(net::OK, callback.WaitForResult());

  // Consumers generally close these before attempting to upgrade the socket,
  // since TCPConnectedSocket waits for the pipes to close before upgrading the
  // connection.
  client_socket_receive_handle.reset();
  client_socket_send_handle.reset();

  base::RunLoop run_loop;
  mojo::Remote<mojom::TLSClientSocket> tls_client_socket;
  server.most_recent_connected_socket()->UpgradeToTLS(
      net::HostPortPair("foopy", 443), nullptr /* options */,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      tls_client_socket.BindNewPipeAndPassReceiver(),
      mojo::NullRemote() /* observer */,
      base::BindLambdaForTesting(
          [&](int net_error,
              mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
              mojo::ScopedDataPipeProducerHandle send_pipe_handle,
              const std::optional<net::SSLInfo>& ssl_info) {
            EXPECT_EQ(net::ERR_NOT_IMPLEMENTED, net_error);
            run_loop.Quit();
          }));
}

// Tests that if a socket is closed, the other side can observe that the pipes
// are broken.
TEST_F(TCPSocketTest, SocketClosed) {
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;

  constexpr std::string_view kTestMsg = "hello";
  auto server = std::make_unique<TestServer>();
  server->Start(1 /*backlog*/);
  net::TestCompletionCallback accept_callback;
  server->AcceptOneConnection(accept_callback.callback());

  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                observer()->GetObserverRemote(), std::nullopt /*local_addr*/,
                server->server_addr(), &client_socket_receive_handle,
                &client_socket_send_handle));
  ASSERT_EQ(net::OK, accept_callback.WaitForResult());

  // Send some data from server to client.
  server->SendData(std::string(kTestMsg));
  EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, kTestMsg.size()));
  // Resetting the |server| destroys the TCPConnectedSocket ptr owned by the
  // server.
  server = nullptr;

  // Read should return EOF.
  EXPECT_EQ("", Read(&client_socket_receive_handle, 1));

  // Read from |client_socket_receive_handle| again should return that the pipe
  // is broken.
  std::vector<uint8_t> buffer(16, 0x00);
  size_t actually_read_bytes = 0;
  MojoResult mojo_result = client_socket_receive_handle->ReadData(
      MOJO_READ_DATA_FLAG_NONE, buffer, actually_read_bytes);
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, mojo_result);
  EXPECT_TRUE(client_socket_receive_handle->QuerySignalsState().peer_closed());

  // Send pipe should be closed.
  while (true) {
    base::RunLoop().RunUntilIdle();
    MojoResult r =
        client_socket_send_handle->WriteAllData(base::as_byte_span(kTestMsg));
    if (r == MOJO_RESULT_SHOULD_WAIT)
      continue;
    if (r == MOJO_RESULT_FAILED_PRECONDITION)
      break;
  }
  EXPECT_TRUE(client_socket_send_handle->QuerySignalsState().peer_closed());
  int result = observer()->WaitForWriteError();
  bool result_ok = result == net::ERR_CONNECTION_RESET ||
                   result == net::ERR_CONNECTION_ABORTED;
#if BUILDFLAG(IS_MAC)
  // On some macOS kernels, send() on a closing TCP socket can return
  // EPROTOTYPE, which is unknown to the net stack and gets mapped to
  // net::ERR_FAILED.
  // See https://crbug.com/1034991
  result_ok |= result == net::ERR_FAILED;
#endif
  EXPECT_TRUE(result_ok) << "actual result: " << result;
}

TEST_F(TCPSocketTest, ReadPipeClosed) {
  const char kTestMsg[] = "hello";
  TestServer server;
  server.Start(1 /*backlog*/);
  net::TestCompletionCallback accept_callback;
  server.AcceptOneConnection(accept_callback.callback());

  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/,
                server.server_addr(), &client_socket_receive_handle,
                &client_socket_send_handle));
  ASSERT_EQ(net::OK, accept_callback.WaitForResult());

  // Close |client_socket_receive_handle|. The socket should remain open.
  client_socket_receive_handle.reset();

  // Send should proceed as normal.
  base::RunLoop read_run_loop;
  server.StartReading(kTestMsg, read_run_loop.QuitClosure());
  EXPECT_TRUE(
      mojo::BlockingCopyFromString(kTestMsg, client_socket_send_handle));
  read_run_loop.Run();
}

TEST_F(TCPSocketTest, WritePipeClosed) {
  const char kTestMsg[] = "hello";
  TestServer server;
  server.Start(1 /*backlog*/);
  net::TestCompletionCallback accept_callback;
  server.AcceptOneConnection(accept_callback.callback());

  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/,
                server.server_addr(), &client_socket_receive_handle,
                &client_socket_send_handle));
  ASSERT_EQ(net::OK, accept_callback.WaitForResult());

  // Close |client_socket_send_handle|. The socket should remain open.
  client_socket_send_handle.reset();

  // Receive should proceed as normal.
  server.SendData(kTestMsg);
  EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, strlen(kTestMsg)));
}

// Tests that if the server socket is destroyed, any connected sockets that it
// handed out remain alive.
TEST_F(TCPSocketTest, ServerSocketClosedAcceptedSocketAlive) {
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  const char kTestMsg[] = "hello";
  TestServer server;
  server.Start(1 /*backlog*/);
  net::TestCompletionCallback accept_callback;
  server.AcceptOneConnection(accept_callback.callback());

  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/,
                server.server_addr(), &client_socket_receive_handle,
                &client_socket_send_handle));
  ASSERT_EQ(net::OK, accept_callback.WaitForResult());

  // Now destroys the server socket.
  server.DestroyServerSocket();
  base::RunLoop().RunUntilIdle();

  // Sending and receiving should still work.
  server.SendData(kTestMsg);
  EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, strlen(kTestMsg)));

  base::RunLoop read_run_loop;
  server.StartReading(kTestMsg, read_run_loop.QuitClosure());
  EXPECT_TRUE(
      mojo::BlockingCopyFromString(kTestMsg, client_socket_send_handle));
  read_run_loop.Run();
}

// Tests both async and sync cases.
class TCPSocketWithMockSocketTest
    : public TCPSocketTest,
      public ::testing::WithParamInterface<net::IoMode> {
 public:
  void SetUp() override {
    mock_client_socket_factory_.set_enable_read_if_ready(true);
    Init(&mock_client_socket_factory_);
  }

  net::MockClientSocketFactory mock_client_socket_factory_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TCPSocketWithMockSocketTest,
                         testing::Values(net::SYNCHRONOUS, net::ASYNC));

// Tests that a server socket handles Accept() correctly when the underlying
// implementation completes Accept() in sync and async mode.
TEST_P(TCPSocketWithMockSocketTest,
       ServerAcceptClientConnectionWithMockSocket) {
  net::IoMode accept_mode = GetParam();
  const uint32_t kBacklog = 10;
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, net::ERR_IO_PENDING)};
  std::vector<std::unique_ptr<net::StaticSocketDataProvider>> data_providers;
  for (size_t i = 0; i < kBacklog + 1; ++i) {
    auto provider = std::make_unique<net::StaticSocketDataProvider>(
        kReads, base::span<net::MockWrite>());
    provider->set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
    data_providers.push_back(std::move(provider));
  }
  auto mock_server_socket =
      std::make_unique<MockServerSocket>(std::move(data_providers));

  MockServerSocket* mock_server_socket_raw = mock_server_socket.get();
  mojo::Remote<mojom::TCPServerSocket> server_socket;

  // Use a mock socket to control net::ServerSocket::Accept() behavior.
  CreateServerSocketWithMockSocket(kBacklog,
                                   server_socket.BindNewPipeAndPassReceiver(),
                                   std::move(mock_server_socket));

  // Complete first Accept() using manual completion via CompleteAccept().
  mock_server_socket_raw->SetAcceptResult(net::SYNCHRONOUS,
                                          net::ERR_IO_PENDING);
  std::vector<std::unique_ptr<net::TestCompletionCallback>> accept_callbacks;
  for (size_t i = 0; i < kBacklog; ++i) {
    auto callback = std::make_unique<net::TestCompletionCallback>();
    server_socket->Accept(
        mojo::NullRemote(),
        base::BindOnce(
            [](net::CompletionOnceCallback callback, int result,
               const std::optional<net::IPEndPoint>& remote_addr,
               mojo::PendingRemote<mojom::TCPConnectedSocket> connected_socket,
               mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
               mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
              std::move(callback).Run(result);
            },
            callback->callback()));
    accept_callbacks.push_back(std::move(callback));
  }

  mock_server_socket_raw->WaitForFirstAccept();
  mock_server_socket_raw->SetAcceptResult(accept_mode, net::OK);
  mock_server_socket_raw->CompleteAccept(net::OK);

  // First net::ServerSocket::Accept() will complete asynchronously
  // internally. Other queued Accept() will complete
  // synchronously/asynchronously depending on |mode| internally.
  for (const auto& callback : accept_callbacks) {
    EXPECT_EQ(net::OK, callback->WaitForResult());
  }

  // New Accept() should complete synchronously internally. Make sure this is
  // okay.
  auto callback = std::make_unique<net::TestCompletionCallback>();
  server_socket->Accept(
      mojo::NullRemote(),
      base::BindOnce(
          [](net::CompletionOnceCallback callback, int result,
             const std::optional<net::IPEndPoint>& remote_addr,
             mojo::PendingRemote<mojom::TCPConnectedSocket> connected_socket,
             mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
             mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
            std::move(callback).Run(result);
          },
          callback->callback()));
  EXPECT_EQ(net::OK, callback->WaitForResult());
}

// Tests that TCPServerSocket::Accept() is used with a non-null
// SocketObserver and that the observer is invoked when a read error
// occurs.
TEST_P(TCPSocketWithMockSocketTest, ServerAcceptWithObserverReadError) {
  net::IoMode mode = GetParam();
  const net::MockRead kReadError[] = {net::MockRead(mode, net::ERR_TIMED_OUT)};
  std::vector<std::unique_ptr<net::StaticSocketDataProvider>> data_providers;
  auto provider = std::make_unique<net::StaticSocketDataProvider>(
      kReadError, base::span<net::MockWrite>());
  provider->set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  data_providers.push_back(std::move(provider));

  auto mock_server_socket =
      std::make_unique<MockServerSocket>(std::move(data_providers));
  mojo::Remote<mojom::TCPServerSocket> server_socket;
  CreateServerSocketWithMockSocket(1 /*backlog*/,
                                   server_socket.BindNewPipeAndPassReceiver(),
                                   std::move(mock_server_socket));

  auto callback = std::make_unique<net::TestCompletionCallback>();
  mojo::Remote<mojom::TCPConnectedSocket> connected_socket_result;
  mojo::ScopedDataPipeConsumerHandle receive_handle;
  mojo::ScopedDataPipeProducerHandle send_handle;
  server_socket->Accept(
      observer()->GetObserverRemote(),
      base::BindLambdaForTesting(
          [&](int result, const std::optional<net::IPEndPoint>& remote_addr,
              mojo::PendingRemote<mojom::TCPConnectedSocket> connected_socket,
              mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
              mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
            callback->callback().Run(result);
            connected_socket_result.Bind(std::move(connected_socket));
            receive_handle = std::move(receive_pipe_handle);
            send_handle = std::move(send_pipe_handle);
          }));
  EXPECT_EQ(net::OK, callback->WaitForResult());

  base::RunLoop().RunUntilIdle();
  std::vector<uint8_t> buffer(16, 0x00);
  size_t actually_read_bytes = 0;
  MojoResult result = receive_handle->ReadData(MOJO_READ_DATA_FLAG_NONE, buffer,
                                               actually_read_bytes);
  ASSERT_NE(MOJO_RESULT_OK, result);
  EXPECT_EQ(net::ERR_TIMED_OUT, observer()->WaitForReadError());
}

// Tests that TCPServerSocket::Accept() is used with a non-null SocketObserver
// and that the observer is invoked when a write error occurs.
TEST_P(TCPSocketWithMockSocketTest, ServerAcceptWithObserverWriteError) {
  net::IoMode mode = GetParam();
  const net::MockRead kReads[] = {net::MockRead(net::SYNCHRONOUS, net::OK)};
  const net::MockWrite kWriteError[] = {
      net::MockWrite(mode, net::ERR_TIMED_OUT)};
  std::vector<std::unique_ptr<net::StaticSocketDataProvider>> data_providers;
  std::unique_ptr<net::StaticSocketDataProvider> provider;
  provider =
      std::make_unique<net::StaticSocketDataProvider>(kReads, kWriteError);
  provider->set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  data_providers.push_back(std::move(provider));

  auto mock_server_socket =
      std::make_unique<MockServerSocket>(std::move(data_providers));
  mojo::Remote<mojom::TCPServerSocket> server_socket;
  CreateServerSocketWithMockSocket(1 /*backlog*/,
                                   server_socket.BindNewPipeAndPassReceiver(),
                                   std::move(mock_server_socket));

  auto callback = std::make_unique<net::TestCompletionCallback>();
  mojo::Remote<mojom::TCPConnectedSocket> connected_socket_result;
  mojo::ScopedDataPipeConsumerHandle receive_handle;
  mojo::ScopedDataPipeProducerHandle send_handle;
  server_socket->Accept(
      observer()->GetObserverRemote(),
      base::BindLambdaForTesting(
          [&](int result, const std::optional<net::IPEndPoint>& remote_addr,
              mojo::PendingRemote<mojom::TCPConnectedSocket> connected_socket,
              mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
              mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
            callback->callback().Run(result);
            connected_socket_result.Bind(std::move(connected_socket));
            receive_handle = std::move(receive_pipe_handle);
            send_handle = std::move(send_pipe_handle);
          }));
  EXPECT_EQ(net::OK, callback->WaitForResult());

  constexpr std::string_view kTestMsg = "abcdefghij";

  // Repeatedly write data to the |send_handle| until write fails.
  while (true) {
    base::RunLoop().RunUntilIdle();
    size_t actually_written_bytes = 0;
    MojoResult result = send_handle->WriteData(base::as_byte_span(kTestMsg),
                                               MOJO_WRITE_DATA_FLAG_NONE,
                                               actually_written_bytes);
    if (result == MOJO_RESULT_SHOULD_WAIT)
      continue;
    if (result != MOJO_RESULT_OK)
      break;
  }
  EXPECT_EQ(net::ERR_TIMED_OUT, observer()->WaitForWriteError());
}

TEST_P(TCPSocketWithMockSocketTest, ReadAndWriteMultiple) {
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  constexpr std::string_view kTestMsg = "abcdefghij";
  const int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  net::IoMode mode = GetParam();
  for (int j = 0; j < kNumIterations; ++j) {
    for (const char& c : kTestMsg) {
      reads.emplace_back(mode, &c, 1, sequence_number++);
    }
    if (j == kNumIterations - 1) {
      reads.emplace_back(mode, net::OK, sequence_number++);
    }
    for (const char& c : kTestMsg) {
      writes.emplace_back(mode, &c, 1, sequence_number++);
    }
  }
  net::StaticSocketDataProvider data_provider(reads, writes);
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  data_provider.set_connect_data(
      net::MockConnect(net::SYNCHRONOUS, net::OK, server_addr));
  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  CreateTCPConnectedSocketSync(
      client_socket.BindNewPipeAndPassReceiver(),
      mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/, server_addr,
      &client_socket_receive_handle, &client_socket_send_handle);

  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Reading kMsgSize should coalesce the 1-byte mock reads.
    EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, kTestMsg.size()));
    // Write multiple times.
    for (size_t i = 0; i < kTestMsg.size(); ++i) {
      size_t actually_written_bytes = 0;
      EXPECT_EQ(MOJO_RESULT_OK,
                client_socket_send_handle->WriteData(
                    base::as_byte_span(kTestMsg).subspan(i, 1u),
                    MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
      // Flush the 1 byte write.
      base::RunLoop().RunUntilIdle();
    }
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketWithMockSocketTest, PartialStreamSocketWrite) {
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  constexpr std::string_view kTestMsg = "abcdefghij";
  constexpr int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  net::IoMode mode = GetParam();
  for (int j = 0; j < kNumIterations; ++j) {
    for (const char& c : kTestMsg) {
      reads.emplace_back(mode, &c, 1, sequence_number++);
    }
    if (j == kNumIterations - 1) {
      reads.emplace_back(mode, net::OK, sequence_number++);
    }
    for (const char& c : kTestMsg) {
      writes.emplace_back(mode, &c, 1, sequence_number++);
    }
  }
  net::StaticSocketDataProvider data_provider(reads, writes);
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  data_provider.set_connect_data(
      net::MockConnect(net::SYNCHRONOUS, net::OK, server_addr));
  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  CreateTCPConnectedSocketSync(
      client_socket.BindNewPipeAndPassReceiver(),
      mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/, server_addr,
      &client_socket_receive_handle, &client_socket_send_handle);

  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Reading kMsgSize should coalesce the 1-byte mock reads.
    EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, kTestMsg.size()));
    // Write twice, each with kMsgSize/2 bytes which is bigger than the 1-byte
    // MockWrite(). This is to exercise that StreamSocket::Write() can do
    // partial write.
    auto [first_write, second_write] =
        base::as_byte_span(kTestMsg).split_at(kTestMsg.size() / 2);
    size_t actually_written_bytes = 0;
    EXPECT_EQ(MOJO_RESULT_OK, client_socket_send_handle->WriteData(
                                  first_write, MOJO_WRITE_DATA_FLAG_NONE,
                                  actually_written_bytes));
    // Flush the first write.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(MOJO_RESULT_OK, client_socket_send_handle->WriteData(
                                  second_write, MOJO_WRITE_DATA_FLAG_NONE,
                                  actually_written_bytes));
    // Flush the second write.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketWithMockSocketTest, ReadError) {
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  net::IoMode mode = GetParam();
  net::MockRead reads[] = {net::MockRead(mode, net::ERR_FAILED)};
  constexpr std::string_view kTestMsg = "hello!";
  net::MockWrite writes[] = {
      net::MockWrite(mode, kTestMsg.data(), kTestMsg.size(), 0)};
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  net::StaticSocketDataProvider data_provider(reads, writes);
  data_provider.set_connect_data(
      net::MockConnect(net::SYNCHRONOUS, net::OK, server_addr));
  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  CreateTCPConnectedSocketSync(
      client_socket.BindNewPipeAndPassReceiver(),
      observer()->GetObserverRemote(), std::nullopt /*local_addr*/, server_addr,
      &client_socket_receive_handle, &client_socket_send_handle);

  EXPECT_EQ("", Read(&client_socket_receive_handle, 1));
  EXPECT_EQ(net::ERR_FAILED, observer()->WaitForReadError());
  // Writes can proceed even though there is a read error.
  size_t actually_written_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            client_socket_send_handle->WriteData(base::as_byte_span(kTestMsg),
                                                 MOJO_WRITE_DATA_FLAG_NONE,
                                                 actually_written_bytes));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketWithMockSocketTest, WriteError) {
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  net::IoMode mode = GetParam();
  constexpr std::string_view kTestMsg = "hello!";
  // The first MockRead needs to complete asynchronously because otherwise it
  // can't be paused to happen after the MockWrite.
  net::MockRead reads[] = {
      net::MockRead(net::ASYNC, kTestMsg.data(), kTestMsg.size(), 1),
      net::MockRead(mode, net::OK, 2)};
  net::MockWrite writes[] = {net::MockWrite(mode, net::ERR_FAILED, 0)};
  net::SequencedSocketData data_provider(reads, writes);

  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  data_provider.set_connect_data(
      net::MockConnect(net::SYNCHRONOUS, net::OK, server_addr));
  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  CreateTCPConnectedSocketSync(
      client_socket.BindNewPipeAndPassReceiver(),
      observer()->GetObserverRemote(), std::nullopt /*local_addr*/, server_addr,
      &client_socket_receive_handle, &client_socket_send_handle);
  size_t actually_written_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            client_socket_send_handle->WriteData(base::as_byte_span(kTestMsg),
                                                 MOJO_WRITE_DATA_FLAG_NONE,
                                                 actually_written_bytes));
  EXPECT_EQ(net::ERR_FAILED, observer()->WaitForWriteError());
  // Reads can proceed even though there is a read error.
  EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, kTestMsg.size()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TCPSocketWithMockSocketTest, InitialTCPConnectedSocketOptions) {
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  std::vector<mojom::TCPKeepAliveOptionsPtr> keep_alive_options_list;

  keep_alive_options_list.emplace_back(nullptr);
  keep_alive_options_list.emplace_back(std::in_place, false, 0U);
  keep_alive_options_list.emplace_back(std::in_place, true, 100U);

  for (int receive_buffer_size :
       {-1, 0, 1024, TCPConnectedSocket::kMaxBufferSize,
        TCPConnectedSocket::kMaxBufferSize + 1}) {
    for (int send_buffer_size :
         {-1, 0, 2048, TCPConnectedSocket::kMaxBufferSize,
          TCPConnectedSocket::kMaxBufferSize + 1}) {
      for (int no_delay : {false, true}) {
        for (const auto& keep_alive_options : keep_alive_options_list) {
          mojo::Remote<mojom::TCPConnectedSocket> client_socket;
          net::StaticSocketDataProvider data_provider;
          data_provider.set_connect_data(
              net::MockConnect(GetParam(), net::OK, server_addr));
          mock_client_socket_factory_.AddSocketDataProvider(&data_provider);

          mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
              mojom::TCPConnectedSocketOptions::New();
          tcp_connected_socket_options->receive_buffer_size =
              receive_buffer_size;
          tcp_connected_socket_options->send_buffer_size = send_buffer_size;
          tcp_connected_socket_options->no_delay = no_delay;
          if (keep_alive_options) {
            tcp_connected_socket_options->keep_alive_options =
                keep_alive_options.Clone();
          }
          EXPECT_EQ(net::OK, CreateTCPConnectedSocketSync(
                                 client_socket.BindNewPipeAndPassReceiver(),
                                 mojo::NullRemote() /*observer*/,
                                 std::nullopt /*local_addr*/, server_addr,
                                 &client_socket_receive_handle,
                                 &client_socket_send_handle,
                                 std::move(tcp_connected_socket_options)));

          if (receive_buffer_size <= 0) {
            EXPECT_EQ(-1, data_provider.receive_buffer_size());
          } else if (receive_buffer_size <=
                     TCPConnectedSocket::kMaxBufferSize) {
            EXPECT_EQ(receive_buffer_size, data_provider.receive_buffer_size());
          } else {
            EXPECT_EQ(TCPConnectedSocket::kMaxBufferSize,
                      data_provider.receive_buffer_size());
          }

          if (send_buffer_size <= 0) {
            EXPECT_EQ(-1, data_provider.send_buffer_size());
          } else if (send_buffer_size <= TCPConnectedSocket::kMaxBufferSize) {
            EXPECT_EQ(send_buffer_size, data_provider.send_buffer_size());
          } else {
            EXPECT_EQ(TCPConnectedSocket::kMaxBufferSize,
                      data_provider.send_buffer_size());
          }
          EXPECT_EQ(no_delay, data_provider.no_delay());
          if (!keep_alive_options) {
            EXPECT_EQ(data_provider.keep_alive_state(),
                      net::SocketDataProvider::KeepAliveState::kDefault);
          } else {
            EXPECT_EQ(data_provider.keep_alive_state(),
                      keep_alive_options->enable
                          ? net::SocketDataProvider::KeepAliveState::kEnabled
                          : net::SocketDataProvider::KeepAliveState::kDisabled);
            if (keep_alive_options->enable) {
              EXPECT_EQ(keep_alive_options->delay,
                        data_provider.keep_alive_delay());
            }
          }
        }
      }
    }
  }
}

TEST_P(TCPSocketWithMockSocketTest, InitialTCPConnectedSocketOptionsFails) {
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  enum class FailedCall {
    SET_RECEIVE_BUFFER_SIZE,
    SET_SEND_BUFFER_SIZE,
    SET_NO_DELAY,
    SET_KEEP_ALIVE
  };
  for (const auto& failed_call :
       {FailedCall::SET_RECEIVE_BUFFER_SIZE, FailedCall::SET_SEND_BUFFER_SIZE,
        FailedCall::SET_NO_DELAY, FailedCall::SET_KEEP_ALIVE}) {
    mojo::Remote<mojom::TCPConnectedSocket> client_socket;
    net::StaticSocketDataProvider data_provider;
    data_provider.set_connect_data(
        net::MockConnect(GetParam(), net::OK, server_addr));
    switch (failed_call) {
      case FailedCall::SET_RECEIVE_BUFFER_SIZE:
        data_provider.set_set_receive_buffer_size_result(net::ERR_FAILED);
        break;
      case FailedCall::SET_SEND_BUFFER_SIZE:
        data_provider.set_set_send_buffer_size_result(net::ERR_FAILED);
        break;
      case FailedCall::SET_NO_DELAY:
        data_provider.set_set_no_delay_result(false);
        break;
      case FailedCall::SET_KEEP_ALIVE:
        data_provider.set_set_keep_alive_result(false);
        break;
    }
    mock_client_socket_factory_.AddSocketDataProvider(&data_provider);

    mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
        mojom::TCPConnectedSocketOptions::New();
    tcp_connected_socket_options->receive_buffer_size = 1;
    tcp_connected_socket_options->send_buffer_size = 2;
    tcp_connected_socket_options->no_delay = false;
    tcp_connected_socket_options->keep_alive_options =
        mojom::TCPKeepAliveOptions::New(false, 0U);

    EXPECT_EQ(net::ERR_FAILED,
              CreateTCPConnectedSocketSync(
                  client_socket.BindNewPipeAndPassReceiver(),
                  mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/,
                  server_addr, &client_socket_receive_handle,
                  &client_socket_send_handle,
                  std::move(tcp_connected_socket_options)));
  }
}

// Simulates the initial connection attempt failing, followed by another
// attempt. Used to simulate cases where the BeforeConnectionCallback is
// invoked multiple times.
TEST_P(TCPSocketWithMockSocketTest,
       InitialTCPConnectedSocketSucceedsOnSecondAttempt) {
  net::IPEndPoint server_addr_a(net::IPAddress::IPv4Localhost(), 1234);
  net::IPEndPoint server_addr_b(net::IPAddress::IPv4Localhost(), 1235);

  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  net::StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(net::MockConnect(
      GetParam(), net::OK, server_addr_b, /*first_attempt_fails=*/true));

  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);

  mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
      mojom::TCPConnectedSocketOptions::New();
  tcp_connected_socket_options->receive_buffer_size = 1;
  tcp_connected_socket_options->send_buffer_size = 2;

  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/,
                net::AddressList({server_addr_a, server_addr_b}),
                &client_socket_receive_handle, &client_socket_send_handle,
                std::move(tcp_connected_socket_options)));
}

TEST_P(TCPSocketWithMockSocketTest, SetBufferSizes) {
  typedef struct {
    int passed_buffer_size;
    int expected_buffer_size;
  } BufferSizeTestData;

  static const BufferSizeTestData kBufferSizeDataTestCases[] = {
      // Setting a buffer size < 0 is replaced by setting a buffer size of 0.
      {-1, 0},
      {1024, 1024},
      {TCPConnectedSocket::kMaxBufferSize + 1,
       TCPConnectedSocket::kMaxBufferSize},
      {0, 0}};

  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  net::StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(
      net::MockConnect(GetParam(), net::OK, server_addr));
  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);

  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/,
                server_addr, &client_socket_receive_handle,
                &client_socket_send_handle));

  EXPECT_EQ(-1, data_provider.receive_buffer_size());

  for (const BufferSizeTestData& test_case : kBufferSizeDataTestCases) {
    net::TestCompletionCallback callback;
    // Setting a buffer size < 0 is replaced by setting a buffer size of 0.
    client_socket->SetReceiveBufferSize(test_case.passed_buffer_size,
                                        callback.callback());
    EXPECT_EQ(net::OK, callback.WaitForResult());
    EXPECT_EQ(test_case.expected_buffer_size,
              data_provider.receive_buffer_size());
  }

  EXPECT_EQ(-1, data_provider.send_buffer_size());

  for (const BufferSizeTestData& test_case : kBufferSizeDataTestCases) {
    net::TestCompletionCallback callback;
    // Setting a buffer size < 0 is replaced by setting a buffer size of 0.
    client_socket->SetSendBufferSize(test_case.passed_buffer_size,
                                     callback.callback());
    EXPECT_EQ(net::OK, callback.WaitForResult());
    EXPECT_EQ(test_case.expected_buffer_size, data_provider.send_buffer_size());
  }
}

TEST_P(TCPSocketWithMockSocketTest, SetBufferSizesFails) {
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  net::StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(
      net::MockConnect(GetParam(), net::OK, server_addr));
  data_provider.set_set_receive_buffer_size_result(net::ERR_FAILED);
  data_provider.set_set_send_buffer_size_result(net::ERR_UNEXPECTED);
  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);

  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt /*local_addr*/,
                server_addr, &client_socket_receive_handle,
                &client_socket_send_handle));

  net::TestCompletionCallback receive_buffer_callback;
  client_socket->SetReceiveBufferSize(1024, receive_buffer_callback.callback());
  EXPECT_EQ(net::ERR_FAILED, receive_buffer_callback.WaitForResult());

  net::TestCompletionCallback send_buffer_callback;
  client_socket->SetSendBufferSize(1024, send_buffer_callback.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, send_buffer_callback.WaitForResult());
}

TEST_F(TCPSocketWithMockSocketTest, SetNoDelayAndKeepAlive) {
  net::StaticSocketDataProvider data_provider;
  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);

  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt, server_addr,
                &client_socket_receive_handle, &client_socket_send_handle));

  EXPECT_TRUE(data_provider.no_delay());
  {
    base::RunLoop run_loop;
    client_socket->SetNoDelay(false /* no_delay */,
                              base::BindLambdaForTesting([&](bool success) {
                                EXPECT_TRUE(success);
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_FALSE(data_provider.no_delay());
  }
  {
    base::RunLoop run_loop;
    client_socket->SetNoDelay(true /* no_delay */,
                              base::BindLambdaForTesting([&](bool success) {
                                EXPECT_TRUE(success);
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_TRUE(data_provider.no_delay());
  }

  {
    const int kKeepAliveDelay = 123;
    base::RunLoop run_loop;
    client_socket->SetKeepAlive(true /* enable */, kKeepAliveDelay,
                                base::BindLambdaForTesting([&](bool success) {
                                  EXPECT_TRUE(success);
                                  run_loop.Quit();
                                }));
    run_loop.Run();
    EXPECT_EQ(data_provider.keep_alive_state(),
              net::SocketDataProvider::KeepAliveState::kEnabled);
    EXPECT_EQ(kKeepAliveDelay, data_provider.keep_alive_delay());
  }

  {
    base::RunLoop run_loop;
    client_socket->SetKeepAlive(false /* enable */, 0 /* delay */,
                                base::BindLambdaForTesting([&](bool success) {
                                  EXPECT_TRUE(success);
                                  run_loop.Quit();
                                }));
    run_loop.Run();
    EXPECT_EQ(data_provider.keep_alive_state(),
              net::SocketDataProvider::KeepAliveState::kDisabled);
  }

  {
    const int kKeepAliveDelay = 1234;
    base::RunLoop run_loop;
    client_socket->SetKeepAlive(true /* enable */, kKeepAliveDelay,
                                base::BindLambdaForTesting([&](bool success) {
                                  EXPECT_TRUE(success);
                                  run_loop.Quit();
                                }));
    run_loop.Run();
    EXPECT_EQ(data_provider.keep_alive_state(),
              net::SocketDataProvider::KeepAliveState::kEnabled);
    EXPECT_EQ(kKeepAliveDelay, data_provider.keep_alive_delay());
  }
}

TEST_F(TCPSocketWithMockSocketTest, SetNoDelayFails) {
  net::StaticSocketDataProvider data_provider;
  data_provider.set_set_no_delay_result(false);
  data_provider.set_set_keep_alive_result(false);
  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);

  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt, server_addr,
                &client_socket_receive_handle, &client_socket_send_handle));

  {
    base::RunLoop run_loop;
    client_socket->SetNoDelay(false /* no_delay */,
                              base::BindLambdaForTesting([&](bool success) {
                                EXPECT_FALSE(success);
                                run_loop.Quit();
                              }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    client_socket->SetKeepAlive(true /* enable */, 123 /* delay */,
                                base::BindLambdaForTesting([&](bool success) {
                                  EXPECT_FALSE(success);
                                  run_loop.Quit();
                                }));
    run_loop.Run();
  }
}

TEST_F(TCPSocketWithMockSocketTest, SetOptionsAfterTLSUpgrade) {
  // Populate with some mock reads, so UpgradeToTLS() won't error out because of
  // a closed receive pipe.
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, "hello", 5 /* length */),
      net::MockRead(net::ASYNC, net::OK)};
  net::StaticSocketDataProvider data_provider(kReads,
                                              base::span<net::MockWrite>());
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::ERR_FAILED);

  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_socket);

  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateTCPConnectedSocketSync(
                client_socket.BindNewPipeAndPassReceiver(),
                mojo::NullRemote() /*observer*/, std::nullopt, server_addr,
                &client_socket_receive_handle, &client_socket_send_handle));

  // UpgradeToTLS will destroy network::TCPConnectedSocket::|socket_|. Calling
  // SetNoDelay and SetKeepAlive should error out.
  mojo::Remote<mojom::TLSClientSocket> tls_socket;
  client_socket_receive_handle.reset();
  client_socket_send_handle.reset();
  {
    base::RunLoop run_loop;
    net::HostPortPair host_port_pair("example.org", 443);
    client_socket->UpgradeToTLS(
        host_port_pair, nullptr /* ssl_config_ptr */,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        tls_socket.BindNewPipeAndPassReceiver(),
        mojo::NullRemote() /*observer */,
        base::BindLambdaForTesting(
            [&](int result,
                mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
                mojo::ScopedDataPipeProducerHandle send_pipe_handle,
                const std::optional<net::SSLInfo>& ssl_info) {
              EXPECT_EQ(net::ERR_FAILED, result);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  net::TestCompletionCallback receive_buffer_callback;
  client_socket->SetReceiveBufferSize(1024, receive_buffer_callback.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, receive_buffer_callback.WaitForResult());
  EXPECT_EQ(-1, data_provider.receive_buffer_size());

  net::TestCompletionCallback send_buffer_callback;
  client_socket->SetSendBufferSize(1024, send_buffer_callback.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, send_buffer_callback.WaitForResult());
  EXPECT_EQ(-1, data_provider.send_buffer_size());

  {
    base::RunLoop run_loop;
    client_socket->SetNoDelay(false /* no_delay */,
                              base::BindLambdaForTesting([&](bool success) {
                                EXPECT_FALSE(success);
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_TRUE(data_provider.no_delay());
  }
  {
    base::RunLoop run_loop;
    client_socket->SetKeepAlive(true /* enable */, 123 /* delay */,
                                base::BindLambdaForTesting([&](bool success) {
                                  EXPECT_FALSE(success);
                                  run_loop.Quit();
                                }));
    run_loop.Run();
  }
}

TEST_F(TCPSocketWithMockSocketTest, SocketDestroyedBeforeConnectCompletes) {
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  net::StaticSocketDataProvider data_provider(reads, writes);
  data_provider.set_connect_data(
      net::MockConnect(net::ASYNC, net::ERR_IO_PENDING));
  mock_client_socket_factory_.AddSocketDataProvider(&data_provider);

  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  net::AddressList remote_addr_list(server_addr);
  int net_error = net::OK;
  base::RunLoop run_loop;
  factory()->CreateTCPConnectedSocket(
      std::nullopt, remote_addr_list,
      nullptr /* tcp_connected_socket_options */, TRAFFIC_ANNOTATION_FOR_TESTS,
      client_socket.BindNewPipeAndPassReceiver(), mojo::NullRemote(),
      base::BindLambdaForTesting(
          [&](int result,
              const std::optional<net::IPEndPoint>& actual_local_addr,
              const std::optional<net::IPEndPoint>& peer_addr,
              mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
              mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
            net_error = result;
            run_loop.Quit();
          }));
  client_socket.reset();
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, net_error);
}

// Tests the case where net::ServerSocket::Listen() succeeds but
// net::ServerSocket::GetLocalAddress() fails. This should still be considered
// as a failure.
TEST(TCPServerSocketTest, GetLocalAddressFailedInListen) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  TCPServerSocket socket(nullptr /* delegate */, nullptr /* net_log */,
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  socket.SetSocketForTest(std::make_unique<FailingServerSocket>());
  net::IPEndPoint local_addr;
  auto result = socket.Listen(local_addr, 1, /*ipv6_only=*/std::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(net::ERR_FAILED, result.error());
}

}  // namespace network
