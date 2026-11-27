// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl.h"

#include <string>

#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/test/io_thread_shared_url_loader_factory_owner.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/ssl/client_cert_identity.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

class StoragePartitionImplBrowsertest : public ContentBrowserTest {
 public:
  StoragePartitionImplBrowsertest() = default;
  ~StoragePartitionImplBrowsertest() override = default;

  GURL GetTestURL() const {
    // Use '/echoheader' instead of '/echo' to avoid a disk_cache bug.
    // See https://crbug.com/792255.
    return embedded_test_server()->GetURL("/echoheader");
  }

 private:
};

class ClientCertBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  explicit ClientCertBrowserClient(
      base::OnceClosure select_certificate_callback,
      base::OnceClosure delete_delegate_callback)
      : select_certificate_callback_(std::move(select_certificate_callback)),
        delete_delegate_callback_(std::move(delete_delegate_callback)) {}

  ClientCertBrowserClient(const ClientCertBrowserClient&) = delete;
  ClientCertBrowserClient& operator=(const ClientCertBrowserClient&) = delete;

  ~ClientCertBrowserClient() override = default;

  // Returns a cancellation callback for the imaginary client certificate
  // dialog. The callback simulates Android's cancellation callback by deleting
  // |delegate|.
  base::OnceClosure SelectClientCertificate(
      BrowserContext* browser_context,
      ChildProcessId process_id,
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<ClientCertificateDelegate> delegate) override {
    std::move(select_certificate_callback_).Run();  // Unblock the test.
    return base::BindOnce(&ClientCertBrowserClient::DeleteDelegateOnCancel,
                          base::Unretained(this), std::move(delegate));
  }

  void DeleteDelegateOnCancel(
      std::unique_ptr<ClientCertificateDelegate> delegate) {
    std::move(delete_delegate_callback_).Run();
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::OnceClosure select_certificate_callback_;
  base::OnceClosure delete_delegate_callback_;
};

class ClientCertBrowserTest : public ContentBrowserTest {
 public:
  ClientCertBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Configure test server to request client certificates.
    net::SSLServerConfig ssl_server_config;
    ssl_server_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
    https_test_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN, ssl_server_config);
    https_test_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    select_certificate_run_loop_ = std::make_unique<base::RunLoop>();
    delete_delegate_run_loop_ = std::make_unique<base::RunLoop>();

    client_ = std::make_unique<ClientCertBrowserClient>(
        select_certificate_run_loop_->QuitClosure(),
        delete_delegate_run_loop_->QuitClosure());
  }

  net::EmbeddedTestServer https_test_server_;
  std::unique_ptr<ClientCertBrowserClient> client_;
  std::unique_ptr<base::RunLoop> select_certificate_run_loop_;
  std::unique_ptr<base::RunLoop> delete_delegate_run_loop_;
};

// Creates a SimpleURLLoader and starts it to download |url|. Blocks until the
// load is complete.
std::unique_ptr<network::SimpleURLLoader> DownloadUrl(
    const GURL& url,
    StoragePartition* partition) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  SimpleURLLoaderTestHelper url_loader_helper;
  url_loader->DownloadToString(
      partition->GetURLLoaderFactoryForBrowserProcess().get(),
      url_loader_helper.GetCallback(),
      /*max_body_size=*/1024 * 1024);
  url_loader_helper.WaitForCallback();
  return url_loader;
}

void CheckSimpleURLLoaderState(network::SimpleURLLoader* url_loader,
                               int net_error,
                               net::HttpStatusCode http_status_code) {
  EXPECT_EQ(net_error, url_loader->NetError());
  if (net_error != net::OK)
    return;
  ASSERT_TRUE(url_loader->ResponseInfo());
  ASSERT_TRUE(url_loader->ResponseInfo()->headers);
  EXPECT_EQ(http_status_code,
            url_loader->ResponseInfo()->headers->response_code());
}

}  // namespace

// Make sure that the NetworkContext returned by a StoragePartition works, both
// with the network service enabled and with it disabled, when one is created
// that wraps the URLRequestContext created by the BrowserContext.
IN_PROC_BROWSER_TEST_F(StoragePartitionImplBrowsertest, NetworkContext) {
  ASSERT_TRUE(embedded_test_server()->Start());

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::OriginatingProcessId::browser();
  params->automatically_assign_isolation_info = true;
  params->is_orb_enabled = false;
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
  shell()
      ->web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->CreateURLLoaderFactory(loader_factory.BindNewPipeAndPassReceiver(),
                               std::move(params));

  network::ResourceRequest request;
  network::TestURLLoaderClient client;
  request.url = embedded_test_server()->GetURL("/set-header?foo: bar");
  request.method = "GET";
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 1,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Just wait until headers are received - if the right headers are received,
  // no need to read the body.
  client.RunUntilResponseBodyArrived();
  ASSERT_TRUE(client.response_head()->headers);
  EXPECT_EQ(200, client.response_head()->headers->response_code());

  EXPECT_EQ(client.response_head()->headers->GetNormalizedHeader("foo"), "bar");
}

// Make sure the factory info returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| works.
IN_PROC_BROWSER_TEST_F(StoragePartitionImplBrowsertest,
                       GetURLLoaderFactoryForBrowserProcessIOThread) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto pending_shared_url_loader_factory =
      shell()
          ->web_contents()
          ->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcessIOThread();

  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      std::move(pending_shared_url_loader_factory));

  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Make sure the factory info returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| doesn't
// crash if it's called after the StoragePartition is deleted.
IN_PROC_BROWSER_TEST_F(StoragePartitionImplBrowsertest,
                       BrowserIOPendingFactoryAfterStoragePartitionGone) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true);
  auto* partition = browser_context->GetDefaultStoragePartition();
  auto pending_shared_url_loader_factory =
      partition->GetURLLoaderFactoryForBrowserProcessIOThread();

  browser_context.reset();

  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      std::move(pending_shared_url_loader_factory));

  EXPECT_EQ(net::ERR_FAILED,
            factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Make sure the factory constructed from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| doesn't
// crash if it's called after the StoragePartition is deleted.
IN_PROC_BROWSER_TEST_F(StoragePartitionImplBrowsertest,
                       DISABLED_BrowserIOFactoryAfterStoragePartitionGone) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true);
  auto* partition = browser_context->GetDefaultStoragePartition();
  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      partition->GetURLLoaderFactoryForBrowserProcessIOThread());

  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));

  browser_context.reset();

  EXPECT_EQ(net::ERR_FAILED,
            factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Checks that the network::URLLoaderIntercpetor works as expected with the
// SharedURLLoaderFactory returned by StoragePartitionImpl.
IN_PROC_BROWSER_TEST_F(StoragePartitionImplBrowsertest, URLLoaderInterceptor) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kEchoUrl(embedded_test_server()->GetURL("/echo"));

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true);
  auto* partition = browser_context->GetDefaultStoragePartition();

  // Run a request the first time without the interceptor set, as the
  // StoragePartitionImpl lazily creates the factory and we want to make sure
  // it will create a new one once the interceptor is set (and not simply reuse
  // the cached one).
  {
    std::unique_ptr<network::SimpleURLLoader> url_loader =
        DownloadUrl(kEchoUrl, partition);
    CheckSimpleURLLoaderState(url_loader.get(), net::OK, net::HTTP_OK);
  }

  // Use a URLLoaderInterceptor to simulate an error.
  {
    URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
        [&](URLLoaderInterceptor::RequestParams* params) -> bool {
          if (params->url_request.url != kEchoUrl)
            return false;
          params->client->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_NOT_IMPLEMENTED));
          return true;
        }));
    std::unique_ptr<network::SimpleURLLoader> url_loader =
        DownloadUrl(kEchoUrl, partition);
    CheckSimpleURLLoaderState(url_loader.get(), net::ERR_NOT_IMPLEMENTED,
                              net::HTTP_OK);
  }

  // Run one more time without the interceptor, we should be back to the
  // original behavior.
  {
    std::unique_ptr<network::SimpleURLLoader> url_loader =
        DownloadUrl(kEchoUrl, partition);
    CheckSimpleURLLoaderState(url_loader.get(), net::OK, net::HTTP_OK);
  }
}

IN_PROC_BROWSER_TEST_F(ClientCertBrowserTest,
                       InvokeClientCertCancellationCallback) {
  ASSERT_TRUE(https_test_server_.Start());

  // Navigate to "/echo". We expect this to get blocked on the client cert.
  shell()->LoadURL(https_test_server_.GetURL("/echo"));

  // Wait for SelectClientCertificate() to be invoked.
  select_certificate_run_loop_->Run();

  // Navigate away to cancel the original request, triggering the cancellation
  // callback that was returned by SelectClientCertificate.
  shell()->LoadURL(GURL("about:blank"));

  // Wait for DeleteDelegateOnCancel() to be invoked.
  delete_delegate_run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(StoragePartitionImplBrowsertest,
                       IsGuestAndDevToolsGetStoragePartition) {
  net::test_server::ControllableHttpResponse slow_sw_response(
      embedded_test_server(), "/slow_sw.js");
  net::test_server::ControllableHttpResponse shared_worker_response(
      embedded_test_server(), "/shared_worker.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  BrowserContext* browser_context =
      shell()->web_contents()->GetBrowserContext();
  StoragePartition* default_partition =
      browser_context->GetDefaultStoragePartition();
  ASSERT_TRUE(default_partition);
  EXPECT_FALSE(default_partition->IsGuest());

  StoragePartitionConfig isolated_config = StoragePartitionConfig::Create(
      browser_context, "isolated_domain", "isolated_partition",
      /*in_memory=*/true);
  StoragePartition* isolated_partition =
      browser_context->GetStoragePartition(isolated_config);
  ASSERT_TRUE(isolated_partition);
  EXPECT_FALSE(isolated_partition->IsGuest());

  StoragePartitionConfig guest_config = StoragePartitionConfig::Create(
      browser_context, "guest_domain", "guest_partition",
      /*in_memory=*/true);
  scoped_refptr<SiteInstance> guest_site_instance =
      SiteInstance::CreateForGuest(browser_context, guest_config);
  StoragePartition* guest_partition =
      browser_context->GetStoragePartition(guest_site_instance.get());
  ASSERT_TRUE(guest_partition);
  EXPECT_TRUE(guest_partition->IsGuest());

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(default_partition,
            DevToolsAgentHost::GetOrCreateFor(shell()->web_contents())
                ->GetStoragePartition());
  EXPECT_EQ(default_partition,
            DevToolsAgentHost::GetOrCreateForTab(shell()->web_contents())
                ->GetStoragePartition());

  // `ServiceWorkerDevToolsAgentHost` resolves its `StoragePartition` before a
  // worker process is allocated during main-script fetch.
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     "window.swDone = navigator.serviceWorker"
                     ".register('/slow_sw.js').then(() => 'ok');",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  slow_sw_response.WaitForRequest();

  scoped_refptr<DevToolsAgentHost> sw_host;
  for (const auto& host : DevToolsAgentHost::GetOrCreateAll()) {
    if (host->GetType() == DevToolsAgentHost::kTypeServiceWorker &&
        host->GetURL().spec().contains("slow_sw.js")) {
      sw_host = host;
      break;
    }
  }
  ASSERT_TRUE(sw_host);
  EXPECT_EQ(nullptr, sw_host->GetProcessHost());
  EXPECT_EQ(default_partition, sw_host->GetStoragePartition());

  slow_sw_response.Send(
      "HTTP/1.1 200 OK\r\nContent-Type: text/javascript\r\n\r\n// sw\n");
  slow_sw_response.Done();
  EXPECT_EQ("ok", EvalJs(shell()->web_contents(), "window.swDone"));

  // `SharedWorkerDevToolsAgentHost` preserves its `StoragePartition` and
  // supports attaching sessions even after transitioning to
  // `WORKER_TERMINATED`.
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     "window.sharedDone = new Promise(resolve => {"
                     "  const w = new SharedWorker('/shared_worker.js');"
                     "  w.port.onmessage = () => resolve('ok');"
                     "});",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  shared_worker_response.WaitForRequest();
  shared_worker_response.Send(
      "HTTP/1.1 200 OK\r\nContent-Type: text/javascript\r\n\r\n"
      "onconnect = (e) => e.ports[0].postMessage('ready');\n");
  shared_worker_response.Done();
  EXPECT_EQ("ok", EvalJs(shell()->web_contents(), "window.sharedDone"));

  scoped_refptr<DevToolsAgentHost> shared_worker_host;
  for (const auto& host : DevToolsAgentHost::GetOrCreateAll()) {
    if (host->GetType() == DevToolsAgentHost::kTypeSharedWorker &&
        host->GetURL().spec().contains("shared_worker.js")) {
      shared_worker_host = host;
      break;
    }
  }
  ASSERT_TRUE(shared_worker_host);
  EXPECT_NE(nullptr, shared_worker_host->GetProcessHost());
  EXPECT_EQ(default_partition, shared_worker_host->GetStoragePartition());

  EXPECT_TRUE(shared_worker_host->Close());
  EXPECT_EQ(nullptr, shared_worker_host->GetProcessHost());
  EXPECT_EQ(default_partition, shared_worker_host->GetStoragePartition());

  TestDevToolsProtocolClient client;
  EXPECT_TRUE(shared_worker_host->AttachClient(&client));
  EXPECT_TRUE(shared_worker_host->IsAttached());
  shared_worker_host->DetachClient(&client);
}

}  // namespace content
