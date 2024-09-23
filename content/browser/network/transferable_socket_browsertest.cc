// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/tcp_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "sandbox/policy/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/transferable_socket.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {
namespace {

class TransferableSocketBrowserTest : public ContentBrowserTest {
 public:
  TransferableSocketBrowserTest() {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      // Network Service Sandboxing is unconditionally enabled on these
      // platforms.
      scoped_feature_list_.InitAndEnableFeature(
          sandbox::policy::features::kNetworkServiceSandbox);
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      ForceOutOfProcessNetworkService();
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    if (!sandbox::policy::features::IsNetworkSandboxSupported()) {
      // On *some* Windows, sandboxing cannot be enabled. We skip all the tests
      // on such platforms.
      GTEST_SKIP();
    }
#elif BUILDFLAG(IS_ANDROID)
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SdkVersion::SDK_VERSION_R) {
      // Android below R does not support transfer of sockets.
      GTEST_SKIP();
    }
#endif

    // These assertions need to precede ContentBrowserTest::SetUp to prevent the
    // test body from running when one of the assertions fails.
    ASSERT_TRUE(IsOutOfProcessNetworkService());
    ASSERT_TRUE(sandbox::policy::features::IsNetworkSandboxEnabled());
    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TransferableSocketBrowserTest, TransferSocket) {
  size_t request_attempts = 0;
  base::RunLoop server_loop;
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&request_attempts,
       &server_loop](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        request_attempts++;
        server_loop.Quit();
        return response;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure the network service has started, before attempting to get the
  // PID.
  network_service_test().FlushForTesting();

  mojo::PendingRemote<network::mojom::NetworkServiceTest>
      network_service_pending;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_pending.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<net::TCPSocket> socket =
      net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
  socket->Open(net::AddressFamily::ADDRESS_FAMILY_IPV4);
  socket->DetachFromThread();

  net::IPEndPoint endpoint(net::IPAddress::IPv4Localhost(),
                           embedded_test_server()->port());
  base::RunLoop connect_run_loop;
  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &net::TCPSocket::Connect, base::Unretained(socket.get()), endpoint,
          base::BindLambdaForTesting([&connect_run_loop](int result) {
            EXPECT_EQ(result, net::OK);
            connect_run_loop.Quit();
          })),
      base::BindLambdaForTesting([&connect_run_loop](int result) {
        if (result == net::OK) {
          connect_run_loop.Quit();
        }
        EXPECT_EQ(result, net::ERR_IO_PENDING);
      }));
  connect_run_loop.Run();
  socket->DetachFromThread();
#if BUILDFLAG(IS_WIN)
  // Obtain the running process id of the network service, as this is needed to
  // duplicate the socket on Windows only.
  auto processes = ServiceProcessHost::GetRunningProcessInfo();
  base::Process network_process;
  for (const auto& process : processes) {
    if (process.IsService<network::mojom::NetworkService>()) {
      ASSERT_FALSE(network_process.IsValid());
      network_process = process.GetProcess().Duplicate();
    }
  }
  ASSERT_TRUE(network_process.IsValid());
  network::TransferableSocket transferable(
      socket->ReleaseSocketDescriptorForTesting(), network_process);
#else
  base::test::TestFuture<net::SocketDescriptor> socket_descriptor;
  GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        return socket->ReleaseSocketDescriptorForTesting();
      }),
      socket_descriptor.GetCallback());
  network::TransferableSocket transferable(socket_descriptor.Get());
#endif

  {
    base::RunLoop network_service_runloop;
    network_service_test()->MakeRequestToServer(
        std::move(transferable), endpoint,
        base::BindLambdaForTesting([&network_service_runloop](bool result) {
          EXPECT_TRUE(result);
          network_service_runloop.Quit();
        }));
    network_service_runloop.Run();
  }
  server_loop.Run();
  EXPECT_EQ(1U, request_attempts);
}

}  // namespace
}  // namespace content
