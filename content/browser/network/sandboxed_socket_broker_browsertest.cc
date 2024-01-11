// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/network/socket_broker_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/handle.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/tcp_server_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "sandbox/policy/features.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace content {
namespace {

const char kTestResponse[] = "hello socket broker";

class SandboxedSocketBrokerBrowserTest : public ContentBrowserTest {
 public:
  SandboxedSocketBrokerBrowserTest() {
#if BUILDFLAG(IS_ANDROID)
    // On older Android the Connect callback is not called with the featurelist
    // enabled. Since it is not strictly necessary to test socket brokering core
    // functionality with the featurelist we won't enable it on Android versions
    // prior to R.
    const int sdk_version = base::android::BuildInfo::GetInstance()->sdk_int();
    check_sandbox_ = sdk_version >= base::android::SdkVersion::SDK_VERSION_R;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
    if (!sandbox::policy::features::IsNetworkSandboxSupported()) {
      check_sandbox_ = false;
    }
#endif  // BUILDFLAG(IS_WIN)

    if (check_sandbox_) {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      // Network Service Sandboxing is unconditionally enabled on these
      // platforms.
      scoped_feature_list_.InitAndEnableFeature(
          sandbox::policy::features::kNetworkServiceSandbox);
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      ForceOutOfProcessNetworkService();
    }
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    if (check_sandbox_) {
      ASSERT_TRUE(IsOutOfProcessNetworkService());
      ASSERT_TRUE(sandbox::policy::features::IsNetworkSandboxEnabled());
    }

    embedded_test_server_.RegisterRequestHandler(
        base::BindRepeating(&SandboxedSocketBrokerBrowserTest::HandleRequest,
                            base::Unretained(this)));

    ASSERT_TRUE(embedded_test_server_.InitializeAndListen());
    ContentBrowserTest::SetUp();
#else
    GTEST_SKIP();
#endif
  }

#if BUILDFLAG(IS_WIN)
  void SetUpOnMainThread() override {
    embedded_test_server_.StartAcceptingConnections();
  }
#endif

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server_.GetURL(request.relative_url);
    if (absolute_url.path() != "/test")
      return nullptr;

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(kTestResponse);
    http_response->set_content_type("text/plain");
    return http_response;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer embedded_test_server_;
  bool check_sandbox_ = true;
};

mojo::Remote<network::mojom::NetworkContext> CreateNetworkContext() {
  mojo::Remote<network::mojom::NetworkContext> network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->cert_verifier_params = GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  CreateNetworkContextInNetworkService(
      network_context.BindNewPipeAndPassReceiver(), std::move(context_params));
  return network_context;
}

void OnConnected(base::OnceClosure quit_closure,
                 int result,
                 const std::optional<net::IPEndPoint>& local_addr,
                 const std::optional<net::IPEndPoint>& peer_addr,
                 mojo::ScopedDataPipeConsumerHandle receive_stream,
                 mojo::ScopedDataPipeProducerHandle send_stream) {
  base::ScopedClosureRunner closure_runner(std::move(quit_closure));
  ASSERT_EQ(result, net::OK);
  const std::string request = "GET /test HTTP/1.0\r\n\r\n";
  ASSERT_TRUE(BlockingCopyFromString(request, std::move(send_stream)));
  std::string response;
  ASSERT_TRUE(BlockingCopyToString(std::move(receive_stream), &response));
  LOG(ERROR) << response;
  EXPECT_NE(response.find(kTestResponse), std::string::npos);
}

// Creates a TCPConnectedSocket that attempts one GET request to
// `embedded_test_server`. If `use_options` is true then a set of options are
// set for the socket.
void RunTcpEndToEndTest(
    network::mojom::NetworkContext* network_context,
    net::test_server::EmbeddedTestServer& embedded_test_server,
    bool use_options) {
  mojo::PendingRemote<network::mojom::TCPConnectedSocket>
      tcp_connected_socket_remote;
  net::AddressList addr;
  ASSERT_TRUE(embedded_test_server.GetAddressList(&addr));

  network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
      network::mojom::TCPConnectedSocketOptions::New();

  tcp_connected_socket_options->send_buffer_size = 32 * 1024;
  tcp_connected_socket_options->receive_buffer_size = 64 * 1024;
  tcp_connected_socket_options->no_delay = false;

  base::RunLoop run_loop;
  network_context->CreateTCPConnectedSocket(
      std::nullopt, addr,
      use_options ? std::move(tcp_connected_socket_options) : nullptr,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      tcp_connected_socket_remote.InitWithNewPipeAndPassReceiver(),
      mojo::NullRemote(), base::BindOnce(&OnConnected, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SandboxedSocketBrokerBrowserTest,
                       TcpEndToEndDefaultContext) {
  network::mojom::NetworkContext* network_context =
      shell()
          ->web_contents()
          ->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext();
  ASSERT_TRUE(network_context);

  RunTcpEndToEndTest(network_context, embedded_test_server_,
                     /*use_options=*/false);
}

IN_PROC_BROWSER_TEST_F(SandboxedSocketBrokerBrowserTest,
                       TcpEndToEndDefaultContextWithOptions) {
  network::mojom::NetworkContext* network_context =
      shell()
          ->web_contents()
          ->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext();
  ASSERT_TRUE(network_context);

  RunTcpEndToEndTest(network_context, embedded_test_server_,
                     /*use_options=*/true);
}

// Implementation of network::mojom::SocketBroker that tracks the number of
// times CreateTcpSocket has been called.
class CountingSocketBrokerImpl : public SocketBrokerImpl {
 public:
  void CreateTcpSocket(net::AddressFamily address_family,
                       CreateTcpSocketCallback callback) override {
    ++tcp_socket_count_;
    SocketBrokerImpl::CreateTcpSocket(address_family, std::move(callback));
  }
  int tcp_socket_count() { return tcp_socket_count_; }

 private:
  int tcp_socket_count_ = 0;
};

IN_PROC_BROWSER_TEST_F(SandboxedSocketBrokerBrowserTest,
                       TcpEndToEndBrokeredContext) {
  CountingSocketBrokerImpl socket_broker;
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  network_context_params->socket_brokers =
      network::mojom::SocketBrokerRemotes::New();
  network_context_params->socket_brokers->client =
      socket_broker.BindNewRemote();
  network_context_params->socket_brokers->server =
      socket_broker.BindNewRemote();
  auto file_paths = network::mojom::NetworkContextFilePaths::New();
  base::FilePath context_path =
      shell()->web_contents()->GetBrowserContext()->GetPath().Append(
          FILE_PATH_LITERAL("TestContext"));
  file_paths->data_directory = context_path.Append(FILE_PATH_LITERAL("Data"));
  file_paths->unsandboxed_data_path = context_path;
  file_paths->trigger_migration = true;
  network_context_params->file_paths = std::move(file_paths);
  network_context_params->cert_verifier_params = GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  network_context_params->http_cache_enabled = true;
  network_context_params->file_paths->http_cache_directory =
      shell()
          ->web_contents()
          ->GetBrowserContext()
          ->GetPath()
          .Append(FILE_PATH_LITERAL("TestContext"))
          .Append(FILE_PATH_LITERAL("Cache"));
  mojo::Remote<network::mojom::NetworkContext> network_context;
  CreateNetworkContextInNetworkService(
      network_context.BindNewPipeAndPassReceiver(),
      std::move(network_context_params));

  RunTcpEndToEndTest(network_context.get(), embedded_test_server_,
                     /*use_options=*/false);
  EXPECT_EQ(socket_broker.tcp_socket_count(), 1);
}

IN_PROC_BROWSER_TEST_F(SandboxedSocketBrokerBrowserTest,
                       TcpEndToEndCrashingService) {
  if (IsInProcessNetworkService())
    GTEST_SKIP();

  auto network_context = CreateNetworkContext();

  ASSERT_TRUE(network_context.is_bound());

  // Run test on the first network context.
  RunTcpEndToEndTest(network_context.get(), embedded_test_server_,
                     /*use_options=*/false);

  SimulateNetworkServiceCrash();

  auto network_context2 = CreateNetworkContext();
  ASSERT_TRUE(network_context2.is_bound());

  // Run the test again, in the new network service.
  RunTcpEndToEndTest(network_context2.get(), embedded_test_server_,
                     /*use_options=*/false);
}

}  // namespace
}  // namespace content
