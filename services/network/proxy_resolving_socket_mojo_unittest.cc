// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/webrtc/fake_ssl_client_socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/mojo_socket_test_util.h"
#include "services/network/proxy_resolving_socket_factory_mojo.h"
#include "services/network/proxy_resolving_socket_mojo.h"
#include "services/network/socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class ProxyResolvingSocketTestBase {
 public:
  ProxyResolvingSocketTestBase(bool use_tls)
      : use_tls_(use_tls),
        fake_tls_handshake_(false),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  ProxyResolvingSocketTestBase(const ProxyResolvingSocketTestBase&) = delete;
  ProxyResolvingSocketTestBase& operator=(const ProxyResolvingSocketTestBase&) =
      delete;

  ~ProxyResolvingSocketTestBase() {}

  void Init(const std::string& pac_result) {
    // Init() can be called multiple times in a test. Reset the members for each
    // invocation. `context_` must outlive `factory_impl_`, which uses the
    // URLRequestContext.
    factory_receiver_ = nullptr;
    factory_impl_ = nullptr;
    factory_remote_.reset();
    context_ = nullptr;

    mock_client_socket_factory_ =
        std::make_unique<net::MockClientSocketFactory>();
    mock_client_socket_factory_->set_enable_read_if_ready(true);
    auto context_builder = net::CreateTestURLRequestContextBuilder();
    context_builder->set_proxy_resolution_service(
        net::ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
            pac_result, TRAFFIC_ANNOTATION_FOR_TESTS));
    context_builder->set_client_socket_factory_for_testing(
        mock_client_socket_factory_.get());
    context_ = context_builder->Build();

    factory_impl_ =
        std::make_unique<ProxyResolvingSocketFactoryMojo>(context_.get());
    factory_receiver_ =
        std::make_unique<mojo::Receiver<mojom::ProxyResolvingSocketFactory>>(
            factory_impl_.get(), factory_remote_.BindNewPipeAndPassReceiver());
  }

  // Reads |num_bytes| from |handle| or reads until an error occurs. Returns the
  // bytes read as a string.
  std::string Read(mojo::ScopedDataPipeConsumerHandle* handle,
                   size_t num_bytes) {
    std::string received_contents;
    while (received_contents.size() < num_bytes) {
      base::RunLoop().RunUntilIdle();
      std::string buffer(num_bytes - received_contents.size(), '\0');
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

  int CreateSocketSync(
      mojo::PendingReceiver<mojom::ProxyResolvingSocket> receiver,
      mojo::PendingRemote<mojom::SocketObserver> socket_observer,
      net::IPEndPoint* peer_addr_out,
      const GURL& url,
      mojo::ScopedDataPipeConsumerHandle* receive_pipe_handle_out,
      mojo::ScopedDataPipeProducerHandle* send_pipe_handle_out) {
    base::RunLoop run_loop;
    int net_error = net::ERR_FAILED;
    network::mojom::ProxyResolvingSocketOptionsPtr options =
        network::mojom::ProxyResolvingSocketOptions::New();
    options->use_tls = use_tls_;
    options->fake_tls_handshake = fake_tls_handshake_;
    factory_remote_->CreateProxyResolvingSocket(
        url, net::NetworkAnonymizationKey(), std::move(options),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::move(receiver), std::move(socket_observer),
        base::BindLambdaForTesting(
            [&](int result, const std::optional<net::IPEndPoint>& local_addr,
                const std::optional<net::IPEndPoint>& peer_addr,
                mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
                mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
              net_error = result;
              if (net_error == net::OK)
                EXPECT_NE(0, local_addr.value().port());
              if (peer_addr_out && peer_addr)
                *peer_addr_out = peer_addr.value();
              *receive_pipe_handle_out = std::move(receive_pipe_handle);
              *send_pipe_handle_out = std::move(send_pipe_handle);
              run_loop.Quit();
            }));
    run_loop.Run();
    return net_error;
  }

  net::MockClientSocketFactory* mock_client_socket_factory() {
    return mock_client_socket_factory_.get();
  }

  bool use_tls() const { return use_tls_; }
  void set_fake_tls_handshake(bool val) { fake_tls_handshake_ = val; }

  mojom::ProxyResolvingSocketFactory* factory() {
    return factory_remote_.get();
  }

 private:
  const bool use_tls_;
  bool fake_tls_handshake_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::MockClientSocketFactory> mock_client_socket_factory_;
  std::unique_ptr<net::URLRequestContext> context_;
  mojo::Remote<mojom::ProxyResolvingSocketFactory> factory_remote_;
  std::unique_ptr<mojo::Receiver<mojom::ProxyResolvingSocketFactory>>
      factory_receiver_;
  std::unique_ptr<ProxyResolvingSocketFactoryMojo> factory_impl_;
};

class ProxyResolvingSocketTest : public ProxyResolvingSocketTestBase,
                                 public testing::TestWithParam<bool> {
 public:
  ProxyResolvingSocketTest() : ProxyResolvingSocketTestBase(GetParam()) {}

  ProxyResolvingSocketTest(const ProxyResolvingSocketTest&) = delete;
  ProxyResolvingSocketTest& operator=(const ProxyResolvingSocketTest&) = delete;

  ~ProxyResolvingSocketTest() override {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         ProxyResolvingSocketTest,
                         ::testing::Bool());

// Tests that the connection is established to the proxy.
TEST_P(ProxyResolvingSocketTest, ConnectToProxy) {
  const GURL kDestination("https://example.com:443");
  const int kProxyPort = 8009;
  const int kDirectPort = 443;
  for (bool is_direct : {true, false}) {
    net::MockClientSocketFactory socket_factory;
    std::unique_ptr<net::URLRequestContext> context;
    if (is_direct) {
      Init("DIRECT");
    } else {
      Init(base::StringPrintf("PROXY myproxy.com:%d", kProxyPort));
    }
    // Note that this read is not consumed when |!is_direct|.
    net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 Success\r\n\r\n"),
                             net::MockRead(net::ASYNC, net::OK)};
    // Note that this write is not consumed when |is_direct|.
    net::MockWrite writes[] = {
        net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                       "Host: example.com:443\r\n"
                       "Proxy-Connection: keep-alive\r\n\r\n")};
    net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
    mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

    net::StaticSocketDataProvider socket_data(reads, writes);
    net::IPEndPoint remote_addr(net::IPAddress(127, 0, 0, 1),
                                is_direct ? kDirectPort : kProxyPort);
    socket_data.set_connect_data(
        net::MockConnect(net::ASYNC, net::OK, remote_addr));
    mock_client_socket_factory()->AddSocketDataProvider(&socket_data);
    mojo::PendingRemote<mojom::ProxyResolvingSocket> socket;
    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
    net::IPEndPoint actual_remote_addr;
    EXPECT_EQ(net::OK, CreateSocketSync(socket.InitWithNewPipeAndPassReceiver(),
                                        mojo::NullRemote() /* socket_observer*/,
                                        &actual_remote_addr, kDestination,
                                        &client_socket_receive_handle,
                                        &client_socket_send_handle));
    // Consume all read data.
    base::RunLoop().RunUntilIdle();
    if (!is_direct) {
      EXPECT_EQ(net::IPEndPoint(), actual_remote_addr);
      EXPECT_TRUE(socket_data.AllReadDataConsumed());
      EXPECT_TRUE(socket_data.AllWriteDataConsumed());
    } else {
      EXPECT_EQ(remote_addr.ToString(), actual_remote_addr.ToString());
      EXPECT_TRUE(socket_data.AllReadDataConsumed());
      EXPECT_FALSE(socket_data.AllWriteDataConsumed());
    }
    EXPECT_EQ(use_tls(), ssl_socket.ConnectDataConsumed());
  }
}

TEST_P(ProxyResolvingSocketTest, ConnectError) {
  const struct TestData {
    // Whether the error is encountered synchronously as opposed to
    // asynchronously.
    bool is_error_sync;
    // Whether it is using a direct connection as opposed to a proxy connection.
    bool is_direct;
  } kTestCases[] = {
      {true, true}, {true, false}, {false, true}, {false, false},
  };
  const GURL kDestination("https://example.com:443");
  for (auto test : kTestCases) {
    std::unique_ptr<net::URLRequestContext> context;
    if (test.is_direct) {
      Init("DIRECT");
    } else {
      Init("PROXY myproxy.com:89");
    }
    net::StaticSocketDataProvider socket_data;
    socket_data.set_connect_data(net::MockConnect(
        test.is_error_sync ? net::SYNCHRONOUS : net::ASYNC, net::ERR_FAILED));
    mock_client_socket_factory()->AddSocketDataProvider(&socket_data);

    mojo::PendingRemote<mojom::ProxyResolvingSocket> socket;
    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
    int status = CreateSocketSync(socket.InitWithNewPipeAndPassReceiver(),
                                  mojo::NullRemote() /* socket_observer*/,
                                  nullptr /* peer_addr_out */, kDestination,
                                  &client_socket_receive_handle,
                                  &client_socket_send_handle);
    if (test.is_direct) {
      EXPECT_EQ(net::ERR_FAILED, status);
    } else {
      EXPECT_EQ(net::ERR_PROXY_CONNECTION_FAILED, status);
    }
    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  }
}

// Tests writing to and reading from a mojom::ProxyResolvingSocket.
TEST_P(ProxyResolvingSocketTest, BasicReadWrite) {
  Init("DIRECT");
  mojo::PendingRemote<mojom::ProxyResolvingSocket> socket;
  constexpr std::string_view kTestMsg = "abcdefghij";
  constexpr size_t kMsgSize = kTestMsg.size();
  constexpr int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  for (int j = 0; j < kNumIterations; ++j) {
    for (size_t i = 0; i < kMsgSize; ++i) {
      reads.emplace_back(net::ASYNC, &kTestMsg[i], 1, sequence_number++);
    }
    if (j == kNumIterations - 1) {
      reads.emplace_back(net::ASYNC, net::OK, sequence_number++);
    }
    for (size_t i = 0; i < kMsgSize; ++i) {
      writes.emplace_back(net::ASYNC, &kTestMsg[i], 1, sequence_number++);
    }
  }
  net::StaticSocketDataProvider data_provider(reads, writes);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_data(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_data);
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  const GURL kDestination("http://example.com");
  EXPECT_EQ(net::OK, CreateSocketSync(socket.InitWithNewPipeAndPassReceiver(),
                                      mojo::NullRemote() /* socket_observer */,
                                      nullptr /* peer_addr_out */, kDestination,
                                      &client_socket_receive_handle,
                                      &client_socket_send_handle));
  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Reading kMsgSize should coalesce the 1-byte mock reads.
    EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, kMsgSize));
    // Write multiple times.
    for (size_t i = 0; i < kMsgSize; ++i) {
      size_t actually_written_bytes = 0;
      EXPECT_EQ(MOJO_RESULT_OK,
                client_socket_send_handle->WriteData(
                    base::as_byte_span(kTestMsg).subspan(i, 1),
                    MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
      // Flush the 1 byte write.
      base::RunLoop().RunUntilIdle();
    }
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
  EXPECT_EQ(use_tls(), ssl_data.ConnectDataConsumed());
}

// Tests that exercise logic related to mojo.
class ProxyResolvingSocketMojoTest : public ProxyResolvingSocketTestBase,
                                     public testing::Test {
 public:
  ProxyResolvingSocketMojoTest() : ProxyResolvingSocketTestBase(false) {}

  ProxyResolvingSocketMojoTest(const ProxyResolvingSocketMojoTest&) = delete;
  ProxyResolvingSocketMojoTest& operator=(const ProxyResolvingSocketMojoTest&) =
      delete;

  ~ProxyResolvingSocketMojoTest() override {}
};

TEST_F(ProxyResolvingSocketMojoTest, ConnectWithFakeTLSHandshake) {
  const GURL kDestination("https://example.com:443");
  const char kTestMsg[] = "abcdefghij";
  const size_t kMsgSize = strlen(kTestMsg);

  Init("DIRECT");
  set_fake_tls_handshake(true);

  std::string_view client_hello =
      webrtc::FakeSSLClientSocket::GetSslClientHello();
  std::string_view server_hello =
      webrtc::FakeSSLClientSocket::GetSslServerHello();
  std::vector<net::MockRead> reads = {
      net::MockRead(net::ASYNC, server_hello.data(), server_hello.length(), 1),
      net::MockRead(net::ASYNC, 2, kTestMsg),
      net::MockRead(net::ASYNC, net::OK, 3)};

  std::vector<net::MockWrite> writes = {net::MockWrite(
      net::ASYNC, client_hello.data(), client_hello.length(), 0)};

  net::StaticSocketDataProvider data_provider(reads, writes);
  data_provider.set_connect_data(net::MockConnect(net::ASYNC, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);

  mojo::PendingRemote<mojom::ProxyResolvingSocket> socket;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  net::IPEndPoint actual_remote_addr;
  EXPECT_EQ(net::OK, CreateSocketSync(socket.InitWithNewPipeAndPassReceiver(),
                                      mojo::NullRemote() /* socket_observer*/,
                                      &actual_remote_addr, kDestination,
                                      &client_socket_receive_handle,
                                      &client_socket_send_handle));

  EXPECT_EQ(kTestMsg, Read(&client_socket_receive_handle, kMsgSize));
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests that when ProxyResolvingSocket remote is destroyed but not the
// ProxyResolvingSocketFactory, the connect callback is not dropped.
// Regression test for https://crbug.com/862608.
TEST_F(ProxyResolvingSocketMojoTest, SocketDestroyedBeforeConnectCompletes) {
  Init("DIRECT");
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;

  net::StaticSocketDataProvider data_provider(reads, writes);
  data_provider.set_connect_data(net::MockConnect(net::ASYNC, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  const GURL kDestination("http://example.com");
  mojo::PendingRemote<mojom::ProxyResolvingSocket> socket;
  base::RunLoop run_loop;
  int net_error = net::OK;
  factory()->CreateProxyResolvingSocket(
      kDestination, net::NetworkAnonymizationKey(), nullptr,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      socket.InitWithNewPipeAndPassReceiver(),
      mojo::NullRemote() /* observer */,
      base::BindLambdaForTesting(
          [&](int result, const std::optional<net::IPEndPoint>& local_addr,
              const std::optional<net::IPEndPoint>& peer_addr,
              mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
              mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
            net_error = result;
          }));
  socket.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_ABORTED, net_error);
}

TEST_F(ProxyResolvingSocketMojoTest, SocketObserver) {
  Init("DIRECT");

  const char kMsg[] = "message!";
  const char kMsgLen = strlen(kMsg);

  std::vector<net::MockRead> reads = {
      net::MockRead(kMsg),
      net::MockRead(net::ASYNC, net::ERR_CONNECTION_ABORTED)};
  std::vector<net::MockWrite> writes = {
      net::MockWrite(net::ASYNC, net::ERR_TIMED_OUT)};

  net::StaticSocketDataProvider data_provider(reads, writes);
  data_provider.set_connect_data(net::MockConnect(net::ASYNC, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);

  const GURL kDestination("http://example.com");

  mojo::PendingRemote<mojom::ProxyResolvingSocket> socket;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  TestSocketObserver test_observer;

  int status = CreateSocketSync(
      socket.InitWithNewPipeAndPassReceiver(),
      test_observer.GetObserverRemote(), nullptr /* peer_addr_out */,
      kDestination, &client_socket_receive_handle, &client_socket_send_handle);
  EXPECT_EQ(net::OK, status);

  EXPECT_EQ(kMsg, Read(&client_socket_receive_handle, kMsgLen));
  EXPECT_EQ(net::ERR_CONNECTION_ABORTED, test_observer.WaitForReadError());

  EXPECT_TRUE(mojo::BlockingCopyFromString(kMsg, client_socket_send_handle));
  EXPECT_EQ(net::ERR_TIMED_OUT, test_observer.WaitForWriteError());

  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

}  // namespace network
