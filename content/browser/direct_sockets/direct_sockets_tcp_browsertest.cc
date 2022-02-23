// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
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
#include "url/gurl.h"

// The tests in this file use the Network Service implementation of
// NetworkContext, to test sending and receiving of data over TCP sockets.

using testing::StartsWith;

namespace content {

namespace {

net::Error UnconditionallyPermitConnection(
    const blink::mojom::DirectSocketOptions& options) {
  DCHECK(options.remote_hostname.has_value());
  return net::OK;
}

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
      uint32_t num_bytes = 0;
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

 protected:
  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    std::string origin_list =
        GetTestOpenPageURL().spec() + "," + GetTestPageURL().spec();

    command_line->AppendSwitchASCII(switches::kRestrictedApiOrigins,
                                    origin_list);
  }

 private:
  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<network::mojom::MdnsResponder> mdns_responder_;
  mojo::Remote<network::mojom::TCPServerSocket> tcp_server_socket_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, OpenTcp_Success) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '127.0.0.1', remotePort: %d})", listening_port);

  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, OpenTcp_Success_Global) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '127.0.0.1', remotePort: %d})", listening_port);

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
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  const uint16_t listening_port = StartTcpServer();
  const std::string name = CreateMDNSHostName();
  EXPECT_TRUE(base::EndsWith(name, ".local"));

  const std::string script =
      base::StringPrintf("openTcp({remoteAddress: '%s', remotePort: %d})",
                         name.c_str(), listening_port);

#if BUILDFLAG(ENABLE_MDNS)
  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));
#else
  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
#endif  // BUILDFLAG(ENABLE_MDNS)
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, CloseTcp) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "closeTcp({remoteAddress: '127.0.0.1', remotePort: %d})", listening_port);

  EXPECT_EQ("closeTcp succeeded", EvalJs(shell(), script));
}

// Tests that we can close the writer, then the socket.
IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, CloseTcpWriter) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "closeTcp({remoteAddress: '127.0.0.1', remotePort: %d}, "
      "/*closeWriter=*/true)",
      listening_port);

  EXPECT_EQ("closeTcp succeeded", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, WriteTcp) {
  const uint32_t kRequiredBytes = 10000;
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const uint16_t listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/kRequiredBytes,
                         /*required_send_bytes=*/0, tcp_server_socket());

  const std::string script = base::StringPrintf(
      "writeTcp({remoteAddress: '127.0.0.1', remotePort: %d}, %u)",
      listening_port, kRequiredBytes);
  EXPECT_EQ("write succeeded", EvalJs(shell(), script));
  waiter.Await();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, ReadTcp) {
  const uint32_t kRequiredBytes = 150000;
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const uint16_t listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/0,
                         /*required_send_bytes=*/kRequiredBytes,
                         tcp_server_socket());

  const std::string script = base::StringPrintf(
      "readTcp({remoteAddress: '127.0.0.1', remotePort: %d}, %u)",
      listening_port, kRequiredBytes);
  EXPECT_EQ("read succeeded", EvalJs(shell(), script));
  waiter.Await();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpBrowserTest, ReadWriteTcp) {
  const uint32_t kRequiredBytes = 1000;
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const uint16_t listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/kRequiredBytes,
                         /*required_send_bytes=*/kRequiredBytes,
                         tcp_server_socket());

  const std::string script = base::StringPrintf(
      "readWriteTcp({remoteAddress: '127.0.0.1', remotePort: %d}, %u)",
      listening_port, kRequiredBytes);
  EXPECT_EQ("readWrite succeeded", EvalJs(shell(), script));
  waiter.Await();
}

}  // namespace content
