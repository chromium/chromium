// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/mojo_socket_test_util.h"
#include "services/network/proxy_resolving_socket_factory_mojo.h"
#include "services/network/proxy_resolving_socket_mojo.h"
#include "services/network/socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// A net::TestURLRequestContext implementation that configures the proxy to be
// a PAC string.
class TestURLRequestContextWithProxy : public net::TestURLRequestContext {
 public:
  explicit TestURLRequestContextWithProxy(const std::string& pac_result)
      : TestURLRequestContext(true) {
    context_storage_.set_proxy_resolution_service(
        net::ProxyResolutionService::CreateFixedFromPacResult(
            pac_result, TRAFFIC_ANNOTATION_FOR_TESTS));
    // net::MockHostResolver maps all hosts to localhost.
    auto host_resolver = std::make_unique<net::MockHostResolver>();
    context_storage_.set_host_resolver(std::move(host_resolver));
  }

  ~TestURLRequestContextWithProxy() override {}
};

}  // namespace

class ProxyResolvingSocketTestBase {
 public:
  ProxyResolvingSocketTestBase(bool use_tls)
      : use_tls_(use_tls),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

  ~ProxyResolvingSocketTestBase() {}

  void Init(const std::string& pac_result) {
    // Init() can be called multiple times in a test. Reset the members for each
    // invocation. |context_with_proxy_| must outlive |factory_impl_|, which
    // uses the URLRequestContet.
    factory_binding_ = nullptr;
    factory_impl_ = nullptr;
    factory_ptr_.reset();
    context_with_proxy_ = nullptr;

    mock_client_socket_factory_ =
        std::make_unique<net::MockClientSocketFactory>();
    mock_client_socket_factory_->set_enable_read_if_ready(true);
    context_with_proxy_ =
        std::make_unique<TestURLRequestContextWithProxy>(pac_result);
    context_with_proxy_->set_client_socket_factory(
        mock_client_socket_factory_.get());
    context_with_proxy_->Init();

    factory_impl_ = std::make_unique<ProxyResolvingSocketFactoryMojo>(
        context_with_proxy_.get());
    factory_binding_ =
        std::make_unique<mojo::Binding<mojom::ProxyResolvingSocketFactory>>(
            factory_impl_.get(), mojo::MakeRequest(&factory_ptr_));
  }

  // Reads |num_bytes| from |handle| or reads until an error occurs. Returns the
  // bytes read as a string.
  std::string Read(mojo::ScopedDataPipeConsumerHandle* handle,
                   size_t num_bytes) {
    std::string received_contents;
    while (received_contents.size() < num_bytes) {
      base::RunLoop().RunUntilIdle();
      std::vector<char> buffer(num_bytes);
      uint32_t read_size =
          static_cast<uint32_t>(num_bytes - received_contents.size());
      MojoResult result = handle->get().ReadData(buffer.data(), &read_size,
                                                 MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT)
        continue;
      if (result != MOJO_RESULT_OK)
        return received_contents;
      received_contents.append(buffer.data(), read_size);
    }
    return received_contents;
  }

  int CreateSocketSync(
      mojom::ProxyResolvingSocketRequest request,
      mojom::SocketObserverPtr socket_observer,
      net::IPEndPoint* peer_addr_out,
      const GURL& url,
      mojo::ScopedDataPipeConsumerHandle* receive_pipe_handle_out,
      mojo::ScopedDataPipeProducerHandle* send_pipe_handle_out) {
    base::RunLoop run_loop;
    int net_error = net::ERR_FAILED;
    factory_ptr_->CreateProxyResolvingSocket(
        url, use_tls_,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::move(request), std::move(socket_observer),
        base::BindLambdaForTesting(
            [&](int result, const base::Optional<net::IPEndPoint>& local_addr,
                const base::Optional<net::IPEndPoint>& peer_addr,
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

  mojom::ProxyResolvingSocketFactory* factory() { return factory_ptr_.get(); }

 private:
  const bool use_tls_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<net::MockClientSocketFactory> mock_client_socket_factory_;
  std::unique_ptr<TestURLRequestContextWithProxy> context_with_proxy_;
  mojom::ProxyResolvingSocketFactoryPtr factory_ptr_;
  std::unique_ptr<mojo::Binding<mojom::ProxyResolvingSocketFactory>>
      factory_binding_;
  std::unique_ptr<ProxyResolvingSocketFactoryMojo> factory_impl_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolvingSocketTestBase);
};

class ProxyResolvingSocketTest : public ProxyResolvingSocketTestBase,
                                 public testing::TestWithParam<bool> {
 public:
  ProxyResolvingSocketTest() : ProxyResolvingSocketTestBase(GetParam()) {}

  ~ProxyResolvingSocketTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolvingSocketTest);
};

INSTANTIATE_TEST_CASE_P(/* no prefix */,
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
    mojom::ProxyResolvingSocketPtr socket;
    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
    net::IPEndPoint actual_remote_addr;
    EXPECT_EQ(net::OK, CreateSocketSync(mojo::MakeRequest(&socket),
                                        nullptr /* socket_observer*/,
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

    mojom::ProxyResolvingSocketPtr socket;
    mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
    mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
    int status = CreateSocketSync(
        mojo::MakeRequest(&socket), nullptr /* socket_observer*/,
        nullptr /* peer_addr_out */, kDestination,
        &client_socket_receive_handle, &client_socket_send_handle);
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
  mojom::ProxyResolvingSocketPtr socket;
  const char kTestMsg[] = "abcdefghij";
  const size_t kMsgSize = strlen(kTestMsg);
  const int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  for (int j = 0; j < kNumIterations; ++j) {
    for (size_t i = 0; i < kMsgSize; ++i) {
      reads.push_back(
          net::MockRead(net::ASYNC, &kTestMsg[i], 1, sequence_number++));
    }
    if (j == kNumIterations - 1) {
      reads.push_back(net::MockRead(net::ASYNC, net::OK, sequence_number++));
    }
    for (size_t i = 0; i < kMsgSize; ++i) {
      writes.push_back(
          net::MockWrite(net::ASYNC, &kTestMsg[i], 1, sequence_number++));
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
  EXPECT_EQ(net::OK, CreateSocketSync(mojo::MakeRequest(&socket),
                                      nullptr /* socket_observer */,
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
      uint32_t num_bytes = 1;
      EXPECT_EQ(MOJO_RESULT_OK,
                client_socket_send_handle->WriteData(
                    &kTestMsg[i], &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
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

  ~ProxyResolvingSocketMojoTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolvingSocketMojoTest);
};

// Tests that when ProxyResolvingSocketPtr is destroyed but not the
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
  mojom::ProxyResolvingSocketPtr socket;
  base::RunLoop run_loop;
  int net_error = net::OK;
  factory()->CreateProxyResolvingSocket(
      kDestination, false,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      mojo::MakeRequest(&socket), nullptr /* observer */,
      base::BindLambdaForTesting(
          [&](int result, const base::Optional<net::IPEndPoint>& local_addr,
              const base::Optional<net::IPEndPoint>& peer_addr,
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

  mojom::ProxyResolvingSocketPtr socket;
  mojo::ScopedDataPipeConsumerHandle client_socket_receive_handle;
  mojo::ScopedDataPipeProducerHandle client_socket_send_handle;
  TestSocketObserver test_observer;

  int status = CreateSocketSync(
      mojo::MakeRequest(&socket), test_observer.GetObserverPtr(),
      nullptr /* peer_addr_out */, kDestination, &client_socket_receive_handle,
      &client_socket_send_handle);
  EXPECT_EQ(net::OK, status);

  EXPECT_EQ(kMsg, Read(&client_socket_receive_handle, kMsgLen));
  EXPECT_EQ(net::ERR_CONNECTION_ABORTED, test_observer.WaitForReadError());

  EXPECT_TRUE(mojo::BlockingCopyFromString(kMsg, client_socket_send_handle));
  EXPECT_EQ(net::ERR_TIMED_OUT, test_observer.WaitForWriteError());

  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

}  // namespace network
