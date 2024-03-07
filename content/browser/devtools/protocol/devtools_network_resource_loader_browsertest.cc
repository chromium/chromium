// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/devtools/protocol/devtools_network_resource_loader.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class DevtoolsNetworkResourceLoaderTest : public ContentBrowserTest {
 public:
  DevtoolsNetworkResourceLoaderTest() = default;

  // ContentBrowserTest implementation:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    const GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
    EXPECT_TRUE(NavigateToURL(web_contents(), url1));
    EXPECT_TRUE(WaitForLoadStop(web_contents()));

    base::FilePath test_source_map_path;
    base::PathService::Get(content::DIR_TEST_DATA, &test_source_map_path);
    test_source_map_path =
        test_source_map_path.AppendASCII("devtools").AppendASCII("source.map");
    base::ReadFileToString(test_source_map_path, &source_map_contents_);
  }

  const std::string& source_map_contents() { return source_map_contents_; }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  mojo::Remote<network::mojom::URLLoaderFactory> CreateURLLoaderFactory() {
    auto* frame = current_frame_host();
    auto params = URLLoaderFactoryParamsHelper::CreateForFrame(
        frame, frame->GetLastCommittedOrigin(),
        frame->GetIsolationInfoForSubresources(),
        frame->BuildClientSecurityState(),
        /**coep_reporter=*/mojo::NullRemote(), frame->GetProcess(),
        network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
        network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
        net::CookieSettingOverrides(), "DevtoolsNetworkResourceLoaderTest");
    // Let DevTools fetch resources without CORS and ORB. Source maps are valid
    // JSON and would otherwise require a CORS fetch + correct response headers.
    // See BUG(chromium:1076435) for more context.
    params->is_orb_enabled = false;
    return mojo::Remote<network::mojom::URLLoaderFactory>(
        url_loader_factory::CreatePendingRemote(
            ContentBrowserClient::URLLoaderFactoryType::kDevTools,
            url_loader_factory::TerminalParams::ForNetworkContext(
                current_frame_host()
                    ->GetProcess()
                    ->GetStoragePartition()
                    ->GetNetworkContext(),
                std::move(params))));
  }

  // Repeats |number_of_error_A| times |error_A|, then continues with |error_B|.
  static std::unique_ptr<content::URLLoaderInterceptor> SetupRequestFailForURL(
      const GURL& url,
      net::Error error) {
    return std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [error,
             &url](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url != url) {
                return false;
              }
              params->client->OnComplete(
                  network::URLLoaderCompletionStatus(error));
              return true;
            }));
  }

  static void CheckSuccess(DevtoolsNetworkResourceLoaderTest* test,
                           base::RunLoop* run_loop,
                           protocol::DevToolsNetworkResourceLoader* loader,
                           const net::HttpResponseHeaders* rh,
                           bool success,
                           int net_error,
                           std::string content) {
    EXPECT_TRUE(success);
    EXPECT_EQ(net_error, net::OK);
    ASSERT_TRUE(rh);
    EXPECT_EQ(rh->response_code(), 200);
    EXPECT_EQ(content, test->source_map_contents());
    size_t iterator = 0;
    std::string name, value;
    EXPECT_FALSE(rh->EnumerateHeaderLines(&iterator, &name, &value));
    run_loop->Quit();
  }

  std::unique_ptr<protocol::DevToolsNetworkResourceLoader> CreateLoader(
      GURL url,
      protocol::DevToolsNetworkResourceLoader::Caching caching,
      protocol::DevToolsNetworkResourceLoader::CompletionCallback callback) {
    return protocol::DevToolsNetworkResourceLoader::Create(
        CreateURLLoaderFactory(), std::move(url),
        current_frame_host()->GetLastCommittedOrigin(),
        current_frame_host()->ComputeSiteForCookies(), caching,
        protocol::DevToolsNetworkResourceLoader::Credentials::kInclude,
        std::move(callback));
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::string source_map_contents_;
};

// Tests that basic download works.
IN_PROC_BROWSER_TEST_F(DevtoolsNetworkResourceLoaderTest, BasicDownload) {
  base::RunLoop run_loop;
  const GURL source_map_url(
      embedded_test_server()->GetURL("a.com", "/devtools/source.map"));
  auto loader =
      CreateLoader(source_map_url,
                   protocol::DevToolsNetworkResourceLoader::Caching::kDefault,
                   base::BindOnce(CheckSuccess, this, &run_loop));
  run_loop.Run();
}

// This test is fetching a source map from a cross-origin URL. While the fetch
// isn't a CORS fetch, the source map is JSON, so this tests that ORB is
// disabled.
IN_PROC_BROWSER_TEST_F(DevtoolsNetworkResourceLoaderTest,
                       BasicDownloadCrossOrigin) {
  base::RunLoop run_loop;
  const GURL source_map_url(
      embedded_test_server()->GetURL("b.com", "/devtools/source.map"));

  auto loader =
      CreateLoader(source_map_url,
                   protocol::DevToolsNetworkResourceLoader::Caching::kDefault,
                   base::BindOnce(CheckSuccess, this, &run_loop));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DevtoolsNetworkResourceLoaderTest,
                       BasicDownloadCrossOriginNoCache) {
  base::RunLoop run_loop;
  const GURL source_map_url(
      embedded_test_server()->GetURL("b.com", "/devtools/source.map"));
  URLLoaderMonitor monitor({source_map_url});

  auto loader = CreateLoader(
      source_map_url, protocol::DevToolsNetworkResourceLoader::Caching::kBypass,
      base::BindOnce(CheckSuccess, this, &run_loop));
  run_loop.Run();
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(source_map_url);
  EXPECT_TRUE(request->load_flags & net::LOAD_BYPASS_CACHE);
}

IN_PROC_BROWSER_TEST_F(DevtoolsNetworkResourceLoaderTest,
                       BasicDownloadCacheControl) {
  base::RunLoop run_loop;
  const GURL source_map_url(
      embedded_test_server()->GetURL("a.com", "/echoheader?Cache-Control"));

  auto complete_callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         protocol::DevToolsNetworkResourceLoader* loader,
         const net::HttpResponseHeaders* rh, bool success, int net_error,
         std::string content) {
        EXPECT_TRUE(success);
        EXPECT_EQ(net_error, net::OK);
        EXPECT_EQ(rh->response_code(), 200);
        EXPECT_EQ(content, "no-cache");
        // Not asserting headers for echoheader response.
        run_loop->Quit();
      },
      &run_loop);

  auto loader = CreateLoader(
      source_map_url, protocol::DevToolsNetworkResourceLoader::Caching::kBypass,
      std::move(complete_callback));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DevtoolsNetworkResourceLoaderTest, HTTPError404) {
  base::RunLoop run_loop;
  const GURL source_map_url(embedded_test_server()->GetURL("/page404.html"));

  auto complete_callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         protocol::DevToolsNetworkResourceLoader* loader,
         const net::HttpResponseHeaders* rh, bool success, int net_error,
         std::string content) {
        EXPECT_FALSE(success);
        ASSERT_EQ(net_error, net::ERR_HTTP_RESPONSE_CODE_FAILURE);
        EXPECT_EQ(rh->response_code(), 404);
        EXPECT_EQ(content, "");
        size_t iterator = 0;
        std::string name, value;
        EXPECT_TRUE(rh->EnumerateHeaderLines(&iterator, &name, &value));
        EXPECT_EQ(name, "Content-type");
        EXPECT_EQ(value, "text/html");
        EXPECT_FALSE(rh->EnumerateHeaderLines(&iterator, &name, &value));
        run_loop->Quit();
      },
      &run_loop);

  auto loader = CreateLoader(
      source_map_url, protocol::DevToolsNetworkResourceLoader::Caching::kBypass,
      std::move(complete_callback));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DevtoolsNetworkResourceLoaderTest, GenericFailure) {
  const GURL source_map_url(
      embedded_test_server()->GetURL("a.com", "/devtools/source.map"));

  auto interceptor = SetupRequestFailForURL(source_map_url, net::ERR_FAILED);
  base::RunLoop run_loop;
  auto complete_callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         protocol::DevToolsNetworkResourceLoader* loader,
         const net::HttpResponseHeaders* rh, bool success, int net_error,
         std::string content) {
        EXPECT_FALSE(success);
        EXPECT_EQ(net_error, net::ERR_FAILED);
        EXPECT_EQ(content, "");
        EXPECT_FALSE(rh);
        run_loop->Quit();
      },
      &run_loop);

  auto loader = CreateLoader(
      source_map_url, protocol::DevToolsNetworkResourceLoader::Caching::kBypass,
      std::move(complete_callback));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DevtoolsNetworkResourceLoaderTest,
                       BasicDownloadRequestParams) {
  RenderFrameHostImpl* frame = current_frame_host();
  const GURL source_map_url(
      embedded_test_server()->GetURL("a.com", "/devtools/source.map"));

  URLLoaderMonitor monitor({source_map_url});
  base::RunLoop run_loop;

  auto loader =
      CreateLoader(source_map_url,
                   protocol::DevToolsNetworkResourceLoader::Caching::kDefault,
                   base::BindOnce(CheckSuccess, this, &run_loop));
  run_loop.Run();

  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(source_map_url);
  EXPECT_TRUE(
      frame->ComputeSiteForCookies().IsEquivalent(request->site_for_cookies));
  EXPECT_EQ(frame->GetLastCommittedOrigin(), request->request_initiator);
  EXPECT_FALSE(request->load_flags & net::LOAD_BYPASS_CACHE);
}

}  // namespace content
