// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "mojo/public/cpp/system/wait.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/mojo_socket_test_util.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "services/network/socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class TCPBoundSocketTest : public testing::Test {
 public:
  TCPBoundSocketTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        url_request_context_(
            net::CreateTestURLRequestContextBuilder()->Build()),
        factory_(/*net_log=*/nullptr, url_request_context_.get()) {}

  TCPBoundSocketTest(const TCPBoundSocketTest&) = delete;
  TCPBoundSocketTest& operator=(const TCPBoundSocketTest&) = delete;

  ~TCPBoundSocketTest() override {}

  SocketFactory* factory() { return &factory_; }

  int BindSocket(const net::IPEndPoint& ip_endpoint_in,
                 mojo::Remote<mojom::TCPBoundSocket>* bound_socket,
                 net::IPEndPoint* ip_endpoint_out) {
    base::RunLoop run_loop;
    int bind_result = net::ERR_IO_PENDING;
    factory()->CreateTCPBoundSocket(
        ip_endpoint_in, TRAFFIC_ANNOTATION_FOR_TESTS,
        bound_socket->BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting(
            [&](int net_error,
                const std::optional<net::IPEndPoint>& local_addr) {
              bind_result = net_error;
              if (net_error == net::OK) {
                *ip_endpoint_out = *local_addr;
              } else {
                EXPECT_FALSE(local_addr);
              }
              run_loop.Quit();
            }));
    run_loop.Run();

    // On error, |bound_socket| should be closed.
    if (bind_result != net::OK && bound_socket->is_connected()) {
      base::RunLoop close_pipe_run_loop;
      bound_socket->set_disconnect_handler(close_pipe_run_loop.QuitClosure());
      close_pipe_run_loop.Run();
    }
    return bind_result;
  }

  int Listen(mojo::Remote<mojom::TCPBoundSocket> bound_socket,
             mojo::Remote<mojom::TCPServerSocket>* server_socket) {
    base::RunLoop bound_socket_destroyed_run_loop;
    bound_socket.set_disconnect_handler(
        bound_socket_destroyed_run_loop.QuitClosure());

    base::RunLoop run_loop;
    int listen_result = net::ERR_IO_PENDING;
    bound_socket->Listen(1 /* backlog */,
                         server_socket->BindNewPipeAndPassReceiver(),
                         base::BindLambdaForTesting([&](int net_error) {
                           listen_result = net_error;
                           run_loop.Quit();
                         }));
    run_loop.Run();

    // Whether Bind() fails or succeeds, |bound_socket| is destroyed.
    bound_socket_destroyed_run_loop.Run();

    // On error, |server_socket| should be closed.
    if (listen_result != net::OK && server_socket->is_connected()) {
      base::RunLoop close_pipe_run_loop;
      server_socket->set_disconnect_handler(close_pipe_run_loop.QuitClosure());
      close_pipe_run_loop.Run();
    }

    return listen_result;
  }

  int Connect(mojo::Remote<mojom::TCPBoundSocket> bound_socket,
              const net::IPEndPoint& expected_local_addr,
              const net::IPEndPoint& connect_to_addr,
              mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
              mojo::Remote<mojom::TCPConnectedSocket>* connected_socket,
              mojo::PendingRemote<mojom::SocketObserver> socket_observer,
              mojo::ScopedDataPipeConsumerHandle* client_socket_receive_handle,
              mojo::ScopedDataPipeProducerHandle* client_socket_send_handle) {
    base::RunLoop bound_socket_destroyed_run_loop;
    bound_socket.set_disconnect_handler(
        bound_socket_destroyed_run_loop.QuitClosure());

    int connect_result = net::ERR_IO_PENDING;
    base::RunLoop run_loop;
    bound_socket->Connect(
        net::AddressList(connect_to_addr),
        std::move(tcp_connected_socket_options),
        connected_socket->BindNewPipeAndPassReceiver(),
        std::move(socket_observer),
        base::BindLambdaForTesting(
            [&](int net_error, const std::optional<net::IPEndPoint>& local_addr,
                const std::optional<net::IPEndPoint>& remote_addr,
                mojo::ScopedDataPipeConsumerHandle receive_stream,
                mojo::ScopedDataPipeProducerHandle send_stream) {
              connect_result = net_error;
              if (net_error == net::OK) {
                EXPECT_EQ(expected_local_addr, *local_addr);
                EXPECT_EQ(connect_to_addr, *remote_addr);
                *client_socket_receive_handle = std::move(receive_stream);
                *client_socket_send_handle = std::move(send_stream);
              } else {
                EXPECT_FALSE(local_addr);
                EXPECT_FALSE(remote_addr);
                EXPECT_FALSE(receive_stream.is_valid());
                EXPECT_FALSE(send_stream.is_valid());
              }
              run_loop.Quit();
            }));
    run_loop.Run();

    // Whether Bind() fails or succeeds, |bound_socket| is destroyed.
    bound_socket_destroyed_run_loop.Run();

    // On error, |connected_socket| should be closed.
    if (connect_result != net::OK && connected_socket->is_connected()) {
      base::RunLoop close_pipe_run_loop;
      connected_socket->set_disconnect_handler(
          close_pipe_run_loop.QuitClosure());
      close_pipe_run_loop.Run();
    }

    return connect_result;
  }

  // Attempts to read exactly |expected_bytes| from |receive_handle|, or reads
  // until the pipe is closed if |expected_bytes| is 0.
  std::string ReadData(mojo::DataPipeConsumerHandle receive_handle,
                       size_t expected_bytes = 0) {
    std::string read_data;
    while (expected_bytes == 0 || read_data.size() < expected_bytes) {
      base::span<const uint8_t> buffer;
      MojoResult result =
          receive_handle.BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        task_environment_.RunUntilIdle();
        continue;
      }
      if (result != MOJO_RESULT_OK) {
        if (expected_bytes != 0)
          ADD_FAILURE() << "Read failed";
        return read_data;
      }
      read_data.append(base::as_string_view(buffer));
      receive_handle.EndReadData(buffer.size());
    }

    return read_data;
  }

  static net::IPEndPoint LocalHostWithAnyPort() {
    return net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0 /* port */);
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  SocketFactory factory_;
};

// Try to bind a socket to an address already being listened on, which should
// fail.
TEST_F(TCPBoundSocketTest, BindError) {
  // Set up a listening socket.
  mojo::Remote<mojom::TCPBoundSocket> bound_socket1;
  net::IPEndPoint bound_address1;
  ASSERT_EQ(net::OK, BindSocket(LocalHostWithAnyPort(), &bound_socket1,
                                &bound_address1));
  mojo::Remote<mojom::TCPServerSocket> server_socket;
  ASSERT_EQ(net::OK, Listen(std::move(bound_socket1), &server_socket));

  // Try to bind another socket to the listening socket's address.
  mojo::Remote<mojom::TCPBoundSocket> bound_socket2;
  net::IPEndPoint bound_address2;
  int result = BindSocket(bound_address1, &bound_socket2, &bound_address2);
  // Depending on platform, can get different errors. Some platforms can return
  // either error.
  EXPECT_TRUE(result == net::ERR_ADDRESS_IN_USE ||
              result == net::ERR_INVALID_ARGUMENT);
}

// Test the case of a connect error. To cause a connect error, bind a socket,
// but don't listen on it, and then try connecting to it using another bound
// socket.
//
// Don't run on Apple platforms because this pattern ends in a connect timeout
// on OSX (after 25+ seconds) instead of connection refused.
#if !BUILDFLAG(IS_APPLE)
TEST_F(TCPBoundSocketTest, ConnectError) {
  mojo::Remote<mojom::TCPBoundSocket> bound_socket1;
  net::IPEndPoint bound_address1;
  ASSERT_EQ(net::OK, BindSocket(LocalHostWithAnyPort(), &bound_socket1,
                                &bound_address1));

  // Trying to bind to an address currently being used for listening should
  // fail.
  mojo::Remote<mojom::TCPBoundSocket> bound_socket2;
  net::IPEndPoint bound_address2;
  ASSERT_EQ(net::OK, BindSocket(LocalHostWithAnyPort(), &bound_socket2,
                                &bound_address2));
  mojo::Remote<mojom::TCPConnectedSocket> connected_socket;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  EXPECT_EQ(net::ERR_CONNECTION_REFUSED,
            Connect(std::move(bound_socket2), bound_address2, bound_address1,
                    nullptr /* tcp_connected_socket_options */,
                    &connected_socket, mojo::NullRemote(),
                    &client_socket_receive_handle, &client_socket_send_handle));
}
#endif  // !BUILDFLAG(IS_APPLE)

// Test listen failure.

// All platforms except Windows use SO_REUSEADDR on server sockets by default,
// which allows binding multiple sockets to the same port at once, as long as
// nothing is listening on it yet.
//
// Apple platforms don't allow binding multiple TCP sockets to the same port
// even with SO_REUSEADDR enabled.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_APPLE)
TEST_F(TCPBoundSocketTest, ListenError) {
  // Bind a socket.
  mojo::Remote<mojom::TCPBoundSocket> bound_socket1;
  net::IPEndPoint bound_address1;
  ASSERT_EQ(net::OK, BindSocket(LocalHostWithAnyPort(), &bound_socket1,
                                &bound_address1));

  // Bind another socket to the same address, which should succeed, due to
  // SO_REUSEADDR.
  mojo::Remote<mojom::TCPBoundSocket> bound_socket2;
  net::IPEndPoint bound_address2;
  ASSERT_EQ(net::OK,
            BindSocket(bound_address1, &bound_socket2, &bound_address2));

  // Listen on the first socket, which should also succeed.
  mojo::Remote<mojom::TCPServerSocket> server_socket1;
  ASSERT_EQ(net::OK, Listen(std::move(bound_socket1), &server_socket1));

  // Listen on the second socket should fail.
  mojo::Remote<mojom::TCPServerSocket> server_socket2;
  int result = Listen(std::move(bound_socket2), &server_socket2);
  // Depending on platform, can get different errors. Some platforms can return
  // either error.
  EXPECT_TRUE(result == net::ERR_ADDRESS_IN_USE ||
              result == net::ERR_INVALID_ARGUMENT);
}
#endif  // !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_APPLE)

// Test the case bind succeeds, and transfer some data.
TEST_F(TCPBoundSocketTest, ReadWrite) {
  // Set up a listening socket.
  mojo::Remote<mojom::TCPBoundSocket> bound_socket1;
  net::IPEndPoint server_address;
  ASSERT_EQ(net::OK, BindSocket(LocalHostWithAnyPort(), &bound_socket1,
                                &server_address));
  mojo::Remote<mojom::TCPServerSocket> server_socket;
  ASSERT_EQ(net::OK, Listen(std::move(bound_socket1), &server_socket));

  // Connect to the socket with another socket.
  mojo::Remote<mojom::TCPBoundSocket> bound_socket2;
  net::IPEndPoint client_address;
  ASSERT_EQ(net::OK, BindSocket(LocalHostWithAnyPort(), &bound_socket2,
                                &client_address));
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  TestSocketObserver socket_observer;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  EXPECT_EQ(net::OK,
            Connect(std::move(bound_socket2), client_address, server_address,
                    nullptr /* tcp_connected_socket_options */, &client_socket,
                    socket_observer.GetObserverRemote(),
                    &client_socket_receive_handle, &client_socket_send_handle));

  base::RunLoop run_loop;
  mojo::Remote<mojom::TCPConnectedSocket> accept_socket;
  mojo::ScopedDataPipeConsumerHandle accept_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle accept_socket_send_handle;
  server_socket->Accept(
      mojo::NullRemote() /* ovserver */,
      base::BindLambdaForTesting(
          [&](int net_error, const std::optional<net::IPEndPoint>& remote_addr,
              mojo::PendingRemote<mojom::TCPConnectedSocket> connected_socket,
              mojo::ScopedDataPipeConsumerHandle receive_stream,
              mojo::ScopedDataPipeProducerHandle send_stream) {
            EXPECT_EQ(net_error, net::OK);
            EXPECT_EQ(*remote_addr, client_address);
            accept_socket.Bind(std::move(connected_socket));
            accept_socket_receive_handle = std::move(receive_stream);
            accept_socket_send_handle = std::move(send_stream);
            run_loop.Quit();
          }));
  run_loop.Run();

  const std::string kData = "Jumbo Shrimp";
  ASSERT_TRUE(mojo::BlockingCopyFromString(kData, client_socket_send_handle));
  EXPECT_EQ(kData, ReadData(accept_socket_receive_handle.get(), kData.size()));

  ASSERT_TRUE(mojo::BlockingCopyFromString(kData, accept_socket_send_handle));
  EXPECT_EQ(kData, ReadData(client_socket_receive_handle.get(), kData.size()));

  // Close the accept socket.
  accept_socket.reset();

  // Wait for read error on the client socket.
  EXPECT_EQ(net::OK, socket_observer.WaitForReadError());

  // Write data to the client socket until there's an error.
  while (true) {
    base::span<uint8_t> buffer;
    MojoResult result = client_socket_send_handle->BeginWriteData(
        mojo::DataPipeProducerHandle::kNoSizeHint, MOJO_WRITE_DATA_FLAG_NONE,
        buffer);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      task_environment()->RunUntilIdle();
      continue;
    }
    if (result != MOJO_RESULT_OK)
      break;
    base::ranges::fill(buffer, 0);
    client_socket_send_handle->EndWriteData(buffer.size());
  }
  // Wait for write error on the client socket. Don't check exact error, out of
  // paranoia.
  EXPECT_LT(socket_observer.WaitForWriteError(), 0);
}

// Establish a connection while passing in some options. This test doesn't check
// that the options are actually set, since there's no API for that.
TEST_F(TCPBoundSocketTest, ConnectWithOptions) {
  // Set up a listening socket.
  mojo::Remote<mojom::TCPBoundSocket> bound_socket1;
  net::IPEndPoint server_address;
  ASSERT_EQ(net::OK, BindSocket(LocalHostWithAnyPort(), &bound_socket1,
                                &server_address));
  mojo::Remote<mojom::TCPServerSocket> server_socket;
  ASSERT_EQ(net::OK, Listen(std::move(bound_socket1), &server_socket));

  // Connect to the socket with another socket.
  mojo::Remote<mojom::TCPBoundSocket> bound_socket2;
  net::IPEndPoint client_address;
  ASSERT_EQ(net::OK, BindSocket(LocalHostWithAnyPort(), &bound_socket2,
                                &client_address));
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  TestSocketObserver socket_observer;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
      mojom::TCPConnectedSocketOptions::New();
  tcp_connected_socket_options->send_buffer_size = 32 * 1024;
  tcp_connected_socket_options->receive_buffer_size = 64 * 1024;
  tcp_connected_socket_options->no_delay = false;

  EXPECT_EQ(net::OK,
            Connect(std::move(bound_socket2), client_address, server_address,
                    std::move(tcp_connected_socket_options), &client_socket,
                    socket_observer.GetObserverRemote(),
                    &client_socket_receive_handle, &client_socket_send_handle));

  base::RunLoop run_loop;
  mojo::Remote<mojom::TCPConnectedSocket> accept_socket;
  mojo::ScopedDataPipeConsumerHandle accept_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle accept_socket_send_handle;
  server_socket->Accept(
      mojo::NullRemote() /* ovserver */,
      base::BindLambdaForTesting(
          [&](int net_error, const std::optional<net::IPEndPoint>& remote_addr,
              mojo::PendingRemote<mojom::TCPConnectedSocket> connected_socket,
              mojo::ScopedDataPipeConsumerHandle receive_stream,
              mojo::ScopedDataPipeProducerHandle send_stream) {
            EXPECT_EQ(net_error, net::OK);
            EXPECT_EQ(*remote_addr, client_address);
            accept_socket.Bind(std::move(connected_socket));
            accept_socket_receive_handle = std::move(receive_stream);
            accept_socket_send_handle = std::move(send_stream);
            run_loop.Quit();
          }));
  run_loop.Run();

  const std::string kData = "Jumbo Shrimp";
  ASSERT_TRUE(mojo::BlockingCopyFromString(kData, client_socket_send_handle));
  EXPECT_EQ(kData, ReadData(accept_socket_receive_handle.get(), kData.size()));

  ASSERT_TRUE(mojo::BlockingCopyFromString(kData, accept_socket_send_handle));
  EXPECT_EQ(kData, ReadData(client_socket_receive_handle.get(), kData.size()));
}

// Test that a TCPBoundSocket can be upgraded to TLS once connected.
TEST_F(TCPBoundSocketTest, UpgradeToTLS) {
  // Simplest way to set up an TLS server is to use the embedded test server.
  net::test_server::EmbeddedTestServer test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  test_server.RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        std::unique_ptr<net::test_server::BasicHttpResponse> basic_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        basic_response->set_content(request.relative_url);
        return basic_response;
      }));
  ASSERT_TRUE(test_server.Start());

  mojo::Remote<mojom::TCPBoundSocket> bound_socket;
  net::IPEndPoint client_address;
  ASSERT_EQ(net::OK,
            BindSocket(LocalHostWithAnyPort(), &bound_socket, &client_address));
  mojo::Remote<mojom::TCPConnectedSocket> client_socket;
  TestSocketObserver socket_observer;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;

  EXPECT_EQ(net::OK,
            Connect(std::move(bound_socket), client_address,
                    net::IPEndPoint(net::IPAddress::IPv4Localhost(),
                                    test_server.host_port_pair().port()),
                    nullptr /* tcp_connected_socket_options */, &client_socket,
                    socket_observer.GetObserverRemote(),
                    &client_socket_receive_handle, &client_socket_send_handle));

  // Need to closed these pipes for UpgradeToTLS to complete.
  client_socket_receive_handle.reset();
  client_socket_send_handle.reset();

  base::RunLoop run_loop;
  mojo::Remote<mojom::TLSClientSocket> tls_client_socket;
  client_socket->UpgradeToTLS(
      test_server.host_port_pair(), nullptr /* options */,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      tls_client_socket.BindNewPipeAndPassReceiver(),
      mojo::NullRemote() /* observer */,
      base::BindLambdaForTesting(
          [&](int net_error,
              mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
              mojo::ScopedDataPipeProducerHandle send_pipe_handle,
              const std::optional<net::SSLInfo>& ssl_info) {
            EXPECT_EQ(net::OK, net_error);
            client_socket_receive_handle = std::move(receive_pipe_handle);
            client_socket_send_handle = std::move(send_pipe_handle);
            run_loop.Quit();
          }));
  run_loop.Run();

  const char kPath[] = "/foo";

  // Send an HTTP request.
  std::string request = base::StringPrintf("GET %s HTTP/1.0\r\n\r\n", kPath);
  EXPECT_TRUE(mojo::BlockingCopyFromString(request, client_socket_send_handle));

  // Read the response, and make sure it looks reasonable.
  std::string response = ReadData(client_socket_receive_handle.get());
  EXPECT_EQ("HTTP/", response.substr(0, 5));
  // The response body should be the path, so make sure the response ends with
  // the path.
  EXPECT_EQ(kPath, response.substr(response.length() - strlen(kPath)));
}

}  // namespace
}  // namespace network
