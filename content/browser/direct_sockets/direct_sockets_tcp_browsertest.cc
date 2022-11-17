// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/browser/direct_sockets/direct_sockets_test_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "url/gurl.h"

// The tests in this file use the Network Service implementation of
// NetworkContext, to test sending and receiving of data over TCP sockets.

using testing::StartsWith;

namespace content {

namespace {

constexpr char kLocalhostAddress[] = "127.0.0.1";

class ReadWriteWaiter {
 public:
  ReadWriteWaiter(
      uint32_t required_receive_bytes,
      uint32_t required_send_bytes,
      mojo::Remote<network::mojom::TCPServerSocket>& tcp_server_socket)
      : required_receive_bytes_(required_receive_bytes),
        required_send_bytes_(required_send_bytes) {
    tcp_server_socket->Accept(
        /*observer=*/mojo::NullRemote(),
        base::BindRepeating(&ReadWriteWaiter::OnAccept,
                            base::Unretained(this)));
  }

  void Await() { run_loop_.Run(); }

 private:
  void OnAccept(
      int result,
      const absl::optional<net::IPEndPoint>& remote_addr,
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> accepted_socket,
      mojo::ScopedDataPipeConsumerHandle consumer_handle,
      mojo::ScopedDataPipeProducerHandle producer_handle) {
    DCHECK_EQ(result, net::OK);
    DCHECK(!accepted_socket_);
    accepted_socket_.Bind(std::move(accepted_socket));

    if (required_receive_bytes_ > 0) {
      receive_stream_ = std::move(consumer_handle);
      read_watcher_ = std::make_unique<mojo::SimpleWatcher>(
          FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
      read_watcher_->Watch(
          receive_stream_.get(),
          MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
          base::BindRepeating(&ReadWriteWaiter::OnReadReady,
                              base::Unretained(this)));
      read_watcher_->ArmOrNotify();
    }

    if (required_send_bytes_ > 0) {
      send_stream_ = std::move(producer_handle);
      write_watcher_ = std::make_unique<mojo::SimpleWatcher>(
          FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
      write_watcher_->Watch(
          send_stream_.get(),
          MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
          base::BindRepeating(&ReadWriteWaiter::OnWriteReady,
                              base::Unretained(this)));
      write_watcher_->ArmOrNotify();
    }
  }

  void OnReadReady(MojoResult result, const mojo::HandleSignalsState& state) {
    ReadData();
  }

  void OnWriteReady(MojoResult result, const mojo::HandleSignalsState& state) {
    WriteData();
  }

  void ReadData() {
    while (true) {
      DCHECK(receive_stream_.is_valid());
      DCHECK_LT(bytes_received_, required_receive_bytes_);
      const void* buffer = nullptr;
      uint32_t num_bytes = 0;
      MojoResult mojo_result = receive_stream_->BeginReadData(
          &buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
      if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
        read_watcher_->ArmOrNotify();
        return;
      }
      DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

      // This is guaranteed by Mojo.
      DCHECK_GT(num_bytes, 0u);

      const unsigned char* current = static_cast<const unsigned char*>(buffer);
      const unsigned char* const end = current + num_bytes;
      while (current < end) {
        EXPECT_EQ(*current, bytes_received_ % 256);
        ++current;
        ++bytes_received_;
      }

      mojo_result = receive_stream_->EndReadData(num_bytes);
      DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

      if (bytes_received_ == required_receive_bytes_) {
        if (bytes_sent_ == required_send_bytes_)
          run_loop_.Quit();
        return;
      }
    }
  }

  void WriteData() {
    while (true) {
      DCHECK(send_stream_.is_valid());
      DCHECK_LT(bytes_sent_, required_send_bytes_);
      void* buffer = nullptr;
      uint32_t num_bytes =
          static_cast<uint32_t>(required_send_bytes_ - bytes_sent_);
      MojoResult mojo_result = send_stream_->BeginWriteData(
          &buffer, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
      if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
        write_watcher_->ArmOrNotify();
        return;
      }
      DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

      // This is guaranteed by Mojo.
      DCHECK_GT(num_bytes, 0u);

      num_bytes = std::min(num_bytes, required_send_bytes_ - bytes_sent_);

      unsigned char* current = static_cast<unsigned char*>(buffer);
      unsigned char* const end = current + num_bytes;
      while (current != end) {
        *current = bytes_sent_ % 256;
        ++current;
        ++bytes_sent_;
      }

      mojo_result = send_stream_->EndWriteData(num_bytes);
      DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

      if (bytes_sent_ == required_send_bytes_) {
        if (bytes_received_ == required_receive_bytes_)
          run_loop_.Quit();
        return;
      }
    }
  }

  const uint32_t required_receive_bytes_;
  const uint32_t required_send_bytes_;
  base::RunLoop run_loop_;
  mojo::Remote<network::mojom::TCPConnectedSocket> accepted_socket_;
  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  mojo::ScopedDataPipeProducerHandle send_stream_;
  std::unique_ptr<mojo::SimpleWatcher> read_watcher_;
  std::unique_ptr<mojo::SimpleWatcher> write_watcher_;
  uint32_t bytes_received_ = 0;
  uint32_t bytes_sent_ = 0;
};

}  // anonymous namespace

class DirectSocketsTcpBrowserTest : public ContentBrowserTest {
 public:
  ~DirectSocketsTcpBrowserTest() override = default;

  GURL GetTestOpenPageURL() {
    return embedded_test_server()->GetURL("/direct_sockets/open.html");
  }

  GURL GetTestPageURL() {
    return embedded_test_server()->GetURL("/direct_sockets/tcp.html");
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return browser_context()->GetDefaultStoragePartition()->GetNetworkContext();
  }

  std::string CreateMDNSHostName() {
    DCHECK(!mdns_responder_.is_bound());
    GetNetworkContext()->CreateMdnsResponder(
        mdns_responder_.BindNewPipeAndPassReceiver());

    std::string name;
    base::RunLoop run_loop;
    mdns_responder_->CreateNameForAddress(
        net::IPAddress::IPv4Localhost(),
        base::BindLambdaForTesting(
            [&name, &run_loop](const std::string& name_out,
                               bool announcement_scheduled) {
              name = name_out;
              run_loop.Quit();
            }));
    run_loop.Run();
    return name;
  }

  // Returns the port listening for TCP connections.
  uint16_t StartTcpServer() {
    net::IPEndPoint local_addr;
    base::RunLoop run_loop;
    GetNetworkContext()->CreateTCPServerSocket(
        net::IPEndPoint(net::IPAddress::IPv4Localhost(),
                        /*port=*/0),
        /*backlog=*/5,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        tcp_server_socket_.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting(
            [&local_addr, &run_loop](
                int32_t result,
                const absl::optional<net::IPEndPoint>& local_addr_out) {
              DCHECK_EQ(result, net::OK);
              DCHECK(local_addr_out.has_value());
              local_addr = *local_addr_out;
              run_loop.Quit();
            }));
    run_loop.Run();
    return local_addr.port();
  }

  mojo::Remote<network::mojom::TCPServerSocket>& tcp_server_socket() {
    return tcp_server_socket_;
  }

  raw_ptr<content::test::AsyncJsRunner> GetAsyncJsRunner() const {
    return runner_.get();
  }

  void ConnectJsSocket(int port = 0) const {
    const std::string open_socket = JsReplace(
        R"(
          socket = new TCPSocket($1, $2);
          await socket.opened;
        )",
        kLocalhostAddress, port);

    ASSERT_TRUE(
        EvalJs(shell(), content::test::WrapAsync(open_socket)).value.is_none());
  }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    client_ = std::make_unique<test::IsolatedWebAppContentBrowserClient>(
        url::Origin::Create(GetTestPageURL()));
    scoped_client_ =
        std::make_unique<ScopedContentBrowserClientSetting>(client_.get());
    runner_ =
        std::make_unique<content::test::AsyncJsRunner>(shell()->web_contents());

    ASSERT_TRUE(NavigateToURL(shell(), GetTestPageURL()));
  }

  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUp();
  }

 private:
  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kIsolatedWebApps};
  mojo::Remote<network::mojom::MdnsResponder> mdns_responder_;
  mojo::Remote<network::mojom::TCPServerSocket> tcp_server_socket_;

  std::unique_ptr<test::IsolatedWebAppContentBrowserClient> client_;
  std::unique_ptr<ScopedContentBrowserClientSetting> scoped_client_;
  std::unique_ptr<content::test::AsyncJsRunner> runner_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, OpenTcp_Success) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  const int listening_port = StartTcpServer();
  const std::string script =
      JsReplace("openTcp($1, $2)", net::IPAddress::IPv4Localhost().ToString(),
                listening_port);

  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, OpenTcp_Success_Global) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  const int listening_port = StartTcpServer();
  const std::string script =
      JsReplace("openTcp($1, $2)", net::IPAddress::IPv4Localhost().ToString(),
                listening_port);

  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1211492 Keep failing on Mac11.3
#define MAYBE_OpenTcp_MDNS DISABLED_OpenTcp_MDNS
#else
#define MAYBE_OpenTcp_MDNS OpenTcp_MDNS
#endif
IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, MAYBE_OpenTcp_MDNS) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  const int listening_port = StartTcpServer();
  const std::string name = CreateMDNSHostName();
  EXPECT_TRUE(base::EndsWith(name, ".local"));

  const std::string script =
      JsReplace("openTcp($1, $2)", name.c_str(), listening_port);

#if BUILDFLAG(ENABLE_MDNS)
  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));
#else
  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
#endif  // BUILDFLAG(ENABLE_MDNS)
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, CloseTcp) {
  const int listening_port = StartTcpServer();
  const std::string script =
      JsReplace("closeTcp($1, $2)", net::IPAddress::IPv4Localhost().ToString(),
                listening_port);

  EXPECT_EQ("closeTcp succeeded", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, WriteTcp) {
  constexpr int32_t kRequiredBytes = 10000;

  const int listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/kRequiredBytes,
                         /*required_send_bytes=*/0, tcp_server_socket());

  const std::string script = JsReplace(
      "writeTcp($1, $2, {}, $3)", net::IPAddress::IPv4Localhost().ToString(),
      listening_port, kRequiredBytes);

  EXPECT_EQ("write succeeded", EvalJs(shell(), script));
  waiter.Await();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, WriteLargeTcpPacket) {
  // The default capacity of TCPSocket mojo pipe is 65536 bytes. This test
  // verifies that out asynchronous writing logic actually works.
  constexpr uint32_t defaultMojoPipeCapacity = (1 << 16);
  constexpr int32_t kRequiredBytes = 3 * defaultMojoPipeCapacity + 1;

  const int listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/kRequiredBytes,
                         /*required_send_bytes=*/0, tcp_server_socket());

  const std::string script =
      JsReplace("writeLargeTcpPacket($1, $2, $3)",
                net::IPAddress::IPv4Localhost().ToString(), listening_port,
                kRequiredBytes);

  EXPECT_EQ("writeLargeTcpPacket succeeded", EvalJs(shell(), script));
  waiter.Await();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, ReadTcp) {
  constexpr int32_t kRequiredBytes = 150000;

  const int listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/0,
                         /*required_send_bytes=*/kRequiredBytes,
                         tcp_server_socket());

  const std::string script = JsReplace(
      "readTcp($1, $2, {}, $3)", net::IPAddress::IPv4Localhost().ToString(),
      listening_port, kRequiredBytes);
  EXPECT_EQ("read succeeded", EvalJs(shell(), script));
  waiter.Await();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, ReadWriteTcp) {
  constexpr int32_t kRequiredBytes = 1000;

  const int listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/kRequiredBytes,
                         /*required_send_bytes=*/kRequiredBytes,
                         tcp_server_socket());
  const std::string script =
      JsReplace("readWriteTcp($1, $2, {}, $3)",
                net::IPAddress::IPv4Localhost().ToString(), listening_port,
                kRequiredBytes);
  EXPECT_EQ("readWrite succeeded", EvalJs(shell(), script));
  waiter.Await();
}

class MockTcpNetworkContext : public content::test::MockNetworkContext {
 public:
  MockTcpNetworkContext() : pipe_capacity_(1) {}
  explicit MockTcpNetworkContext(uint32_t pipe_capacity)
      : pipe_capacity_(pipe_capacity) {}
  ~MockTcpNetworkContext() override = default;

  // network::TestNetworkContext:
  void CreateTCPConnectedSocket(
      const absl::optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override {
    const net::IPEndPoint& peer_addr = remote_addr_list.front();

    mojo::ScopedDataPipeProducerHandle producer;
    MojoResult producer_result =
        mojo::CreateDataPipe(pipe_capacity_, producer, producer_complement_);
    DCHECK_EQ(MOJO_RESULT_OK, producer_result);

    mojo::ScopedDataPipeConsumerHandle consumer;
    MojoResult consumer_result =
        CreateDataPipe(nullptr, consumer_complement_, consumer);
    DCHECK_EQ(MOJO_RESULT_OK, consumer_result);

    observer_.Bind(std::move(observer));

    std::move(callback).Run(
        net::OK, net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0}, peer_addr,
        std::move(consumer), std::move(producer));
  }

  mojo::Remote<network::mojom::SocketObserver>& get_observer() {
    return observer_;
  }

  mojo::ScopedDataPipeProducerHandle& get_consumer_complement() {
    return consumer_complement_;
  }
  mojo::ScopedDataPipeConsumerHandle& get_producer_complement() {
    return producer_complement_;
  }

 private:
  mojo::ScopedDataPipeProducerHandle consumer_complement_;
  mojo::ScopedDataPipeConsumerHandle producer_complement_;

  mojo::Remote<network::mojom::SocketObserver> observer_;

  const uint32_t pipe_capacity_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, ReadTcpOnReadError) {
  MockTcpNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  ConnectJsSocket();

  const std::string async_script =
      "readTcpOnError(socket, /*expected_read_success=*/false);";
  auto future = GetAsyncJsRunner()->RunScript(async_script);

  {
    // Simulate pipe shutdown on read error. Read requests must reject.
    mock_network_context.get_observer()->OnReadError(net::ERR_NOT_IMPLEMENTED);
    mock_network_context.get_consumer_complement().reset();
  }

  EXPECT_THAT(future->Get(), ::testing::HasSubstr("readTcpOnError succeeded."));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, ReadTcpOnPeerClosed) {
  MockTcpNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  ConnectJsSocket();

  const std::string async_script =
      "readTcpOnError(socket, /*expected_read_success=*/true);";
  auto future = GetAsyncJsRunner()->RunScript(async_script);

  {
    // Simulate pipe shutdown on peer closed. Read requests must resolve with
    // done = true.
    mock_network_context.get_observer()->OnReadError(net::OK);
    mock_network_context.get_consumer_complement().reset();
  }

  EXPECT_THAT(future->Get(), ::testing::HasSubstr("readTcpOnError succeeded."));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, WriteTcpOnWriteError) {
  MockTcpNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  ConnectJsSocket();

  const std::string async_script = "writeTcpOnError(socket);";
  auto future = GetAsyncJsRunner()->RunScript(async_script);

  {
    // Simulate pipe shutdown on write error.
    mock_network_context.get_observer()->OnWriteError(net::ERR_NOT_IMPLEMENTED);
    mock_network_context.get_producer_complement().reset();
  }

  EXPECT_THAT(future->Get(),
              ::testing::HasSubstr("writeTcpOnError succeeded."));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest,
                       ReadWriteTcpOnSocketObserverError) {
  MockTcpNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  ConnectJsSocket();

  const std::string async_script = "readWriteTcpOnError(socket);";
  auto future = GetAsyncJsRunner()->RunScript(async_script);

  mock_network_context.get_observer().reset();
  mock_network_context.get_consumer_complement().reset();
  mock_network_context.get_producer_complement().reset();

  EXPECT_THAT(future->Get(),
              ::testing::HasSubstr("readWriteTcpOnError succeeded."));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest,
                       BarrierCallbackFiresWithErrorOnReadWriteError) {
  MockTcpNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  ConnectJsSocket();

  const std::string async_script =
      "waitForClosedPromise(socket, /*expected_closed_result=*/false);";
  auto future = GetAsyncJsRunner()->RunScript(async_script);

  {
    mock_network_context.get_observer()->OnReadError(net::ERR_UNEXPECTED);
    mock_network_context.get_consumer_complement().reset();
    mock_network_context.get_producer_complement().reset();
    mock_network_context.get_observer()->OnWriteError(net::ERR_UNEXPECTED);
  }

  EXPECT_THAT(future->Get(),
              ::testing::HasSubstr("waitForClosedPromise succeeded."));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest,
                       BarrierCallbackFiresWithOkOnReaderAndWriterClose) {
  MockTcpNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  ConnectJsSocket();

  const std::string async_script =
      "waitForClosedPromise(socket, /*expected_closed_result=*/true, "
      "/*cancel_reader=*/true, /*close_writer=*/true);";
  auto future = GetAsyncJsRunner()->RunScript(async_script);

  EXPECT_THAT(future->Get(),
              ::testing::HasSubstr("waitForClosedPromise succeeded."));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest,
                       BarrierCallbackFiresWithOkOnPeerAndWriterClose) {
  MockTcpNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  ConnectJsSocket();

  const std::string async_script =
      "waitForClosedPromise(socket, /*expected_closed_result=*/true, "
      "/*cancel_reader=*/false, /*close_writer=*/true);";
  auto future = GetAsyncJsRunner()->RunScript(async_script);

  // Simulate peer closed event.
  mock_network_context.get_observer()->OnReadError(net::OK);
  mock_network_context.get_consumer_complement().reset();

  EXPECT_THAT(future->Get(),
              ::testing::HasSubstr("waitForClosedPromise succeeded."));
}

}  // namespace content
