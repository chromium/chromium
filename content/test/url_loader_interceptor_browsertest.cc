// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/url_loader_interceptor.h"

#include "base/command_line.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/filename_util.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_url_loader_client.h"

namespace content {
namespace {

class URLLoaderInterceptorTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void Test() { EXPECT_TRUE(NavigateToURL(shell(), GetPageURL())); }

  GURL GetPageURL() {
    return embedded_test_server()->GetURL("/page_with_image.html");
  }

  bool DidImageLoad() {
    return EvalJs(shell(),
                  "document.getElementsByTagName('img')[0].naturalHeight")
               .ExtractInt() != 0;
  }

  GURL GetImageURL() { return embedded_test_server()->GetURL("/blank.jpg"); }
};

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, MonitorFrame) {
  bool seen = false;
  GURL url = GetPageURL();
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == url) {
          EXPECT_EQ(params->process_id, 0);
          EXPECT_FALSE(seen);
          seen = true;
        }
        return false;
      }));
  Test();
  EXPECT_TRUE(seen);
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, InterceptFrame) {
  bool seen = false;
  GURL url = GetPageURL();
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        EXPECT_EQ(params->url_request.url, url);
        EXPECT_EQ(params->process_id, 0);
        seen = true;
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_FAILED;
        params->client->OnComplete(status);
        return true;
      }));
  EXPECT_FALSE(NavigateToURL(shell(), GetPageURL()));
  EXPECT_TRUE(seen);
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, InterceptFrameWithFileScheme) {
  bool seen = false;
  base::FilePath path = GetTestFilePath(nullptr, "empty.html");
  GURL url = net::FilePathToFileURL(path);
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        EXPECT_EQ(params->url_request.url, url);
        EXPECT_EQ(params->process_id, network::mojom::kBrowserProcessId);
        seen = true;
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_FAILED;
        params->client->OnComplete(status);
        return true;
      }));
  EXPECT_FALSE(NavigateToURL(shell(), url));
  EXPECT_TRUE(seen);
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest,
                       AsynchronousInitializationInterceptFrame) {
  bool seen = false;
  GURL url = GetPageURL();
  base::RunLoop run_loop;
  URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting(
          [&](URLLoaderInterceptor::RequestParams* params) {
            EXPECT_EQ(params->url_request.url, url);
            EXPECT_EQ(params->process_id, 0);
            seen = true;
            network::URLLoaderCompletionStatus status;
            status.error_code = net::ERR_FAILED;
            params->client->OnComplete(status);
            return true;
          }),
      /*completion_status_callback=*/{}, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(NavigateToURL(shell(), GetPageURL()));
  EXPECT_TRUE(seen);
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest,
                       AsynchronousDestructionIsAppliedImmediately) {
  const GURL url = GetPageURL();
  {
    bool seen = false;
    base::RunLoop run_loop;
    URLLoaderInterceptor interceptor(
        base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) {
              EXPECT_EQ(params->url_request.url, url);
              EXPECT_EQ(params->process_id, 0);
              seen = true;
              network::URLLoaderCompletionStatus status;
              status.error_code = net::ERR_FAILED;
              params->client->OnComplete(status);
              return true;
            }),
        /*completion_status_callback=*/{}, run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_FALSE(NavigateToURL(shell(), url));
    EXPECT_TRUE(seen);
  }
  EXPECT_TRUE(NavigateToURL(shell(), url));
}

class TestBrowserClientWithHeaderClient
    : public ContentBrowserTestContentBrowserClient,
      public network::mojom::TrustedURLLoaderHeaderClient {
 private:
  // ContentBrowserClient:
  void WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      const net::IsolationInfo& isolation_info,
      std::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
      override {
    if (header_client) {
      receivers_.Add(this, header_client->InitWithNewPipeAndPassReceiver());
    }
  }

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {}
  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {}

  mojo::ReceiverSet<network::mojom::TrustedURLLoaderHeaderClient> receivers_;
};

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest,
                       InterceptFrameWithHeaderClient) {
  TestBrowserClientWithHeaderClient browser_client;

  bool seen = false;
  GURL url = GetPageURL();
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        EXPECT_EQ(params->url_request.url, url);
        EXPECT_EQ(params->process_id, 0);
        seen = true;
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_FAILED;
        params->client->OnComplete(status);
        return true;
      }));
  EXPECT_FALSE(NavigateToURL(shell(), GetPageURL()));
  EXPECT_TRUE(seen);
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, MonitorSubresource) {
  bool seen = false;
  GURL url = GetImageURL();
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == url) {
          EXPECT_NE(params->process_id, 0);
          EXPECT_FALSE(seen);
          seen = true;
        }
        return false;
      }));
  Test();
  EXPECT_TRUE(seen);
  EXPECT_TRUE(DidImageLoad());
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, InterceptSubresource) {
  bool seen = false;
  GURL url = GetImageURL();
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == url) {
          seen = true;
          network::URLLoaderCompletionStatus status;
          status.error_code = net::ERR_FAILED;
          params->client->OnComplete(status);
          return true;
        }
        return false;
      }));
  Test();
  EXPECT_FALSE(DidImageLoad());
  EXPECT_TRUE(seen);
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, InterceptBrowser) {
  bool seen = false;
  GURL url = GetImageURL();
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;
  network::ResourceRequest request;
  request.url = url;

  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        seen = true;
        EXPECT_EQ(params->url_request.url, url);
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_FAILED;
        params->client->OnComplete(status);
        return true;
      }));
  auto* factory = shell()
                      ->web_contents()
                      ->GetBrowserContext()
                      ->GetDefaultStoragePartition()
                      ->GetURLLoaderFactoryForBrowserProcess()
                      .get();
  factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
      client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client.completion_status().error_code);
  EXPECT_TRUE(seen);
}

IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, WriteResponse) {
  std::string body("<html>Hello</html>");
  network::TestURLLoaderClient client;
  URLLoaderInterceptor::WriteResponse(
      "HTTP/1.1 200 OK\nContent-type: text/html\n\n", body, &client);
  client.RunUntilComplete();

  EXPECT_EQ(client.response_head()->headers->response_code(), 200);
  EXPECT_EQ(client.response_head()->mime_type, "text/html");

  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client.response_body_release(), &response));
  EXPECT_EQ(response, body);
}

// Passes in headers (e.g. not reading from disk).
IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, WriteResponseFromFile1) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::string body("<!doctype html>\n<p>hello</p>\n");
  std::string headers("HTTP/1.1 404\n");
  network::TestURLLoaderClient client;
  URLLoaderInterceptor::WriteResponse("content/test/data/hello.html", &client,
                                      &headers);
  client.RunUntilComplete();

  EXPECT_EQ(client.response_head()->headers->response_code(), 404);

  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client.response_body_release(), &response));
  EXPECT_EQ(response, body);
}

// Headers read from disk.
IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, WriteResponseFromFile2) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::string body("<!doctype html>\n<p>hello</p>\n");
  network::TestURLLoaderClient client;
  URLLoaderInterceptor::WriteResponse("content/test/data/hello.html", &client);
  client.RunUntilComplete();

  EXPECT_EQ(client.response_head()->headers->response_code(), 200);

  std::string mime_type;
  EXPECT_TRUE(client.response_head()->headers->GetMimeType(&mime_type));
  EXPECT_EQ(mime_type, "text/html");

  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client.response_body_release(), &response));
  EXPECT_EQ(response, body);
}

// Headers generated.
IN_PROC_BROWSER_TEST_F(URLLoaderInterceptorTest, WriteResponseFromFile3) {
  base::ScopedAllowBlockingForTesting allow_io;
  network::TestURLLoaderClient client;
  URLLoaderInterceptor::WriteResponse("content/test/data/empty.html", &client);
  client.RunUntilComplete();

  EXPECT_EQ(client.response_head()->headers->response_code(), 200);

  std::string mime_type;
  EXPECT_TRUE(client.response_head()->headers->GetMimeType(&mime_type));
  EXPECT_EQ(mime_type, "text/html");

  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client.response_body_release(), &response));
  EXPECT_TRUE(response.empty());
}

}  // namespace
}  // namespace content
