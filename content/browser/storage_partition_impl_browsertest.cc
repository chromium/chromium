// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl.h"

#include <string>

#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/test/storage_partition_test_utils.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

class StoragePartititionImplBrowsertest : public ContentBrowserTest {
 public:
  StoragePartititionImplBrowsertest() {}
  ~StoragePartititionImplBrowsertest() override {}

  GURL GetTestURL() const {
    // Use '/echoheader' instead of '/echo' to avoid a disk_cache bug.
    // See https://crbug.com/792255.
    return embedded_test_server()->GetURL("/echoheader");
  }

 private:
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
IN_PROC_BROWSER_TEST_F(StoragePartititionImplBrowsertest, NetworkContext) {
  ASSERT_TRUE(embedded_test_server()->Start());

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
  BrowserContext::GetDefaultStoragePartition(
      shell()->web_contents()->GetBrowserContext())
      ->GetNetworkContext()
      ->CreateURLLoaderFactory(loader_factory.BindNewPipeAndPassReceiver(),
                               std::move(params));

  network::ResourceRequest request;
  network::TestURLLoaderClient client;
  request.url = embedded_test_server()->GetURL("/set-header?foo: bar");
  request.method = "GET";
  network::mojom::URLLoaderPtr loader;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1, network::mojom::kURLLoadOptionNone,
      request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Just wait until headers are received - if the right headers are received,
  // no need to read the body.
  client.RunUntilResponseBodyArrived();
  ASSERT_TRUE(client.response_head()->headers);
  EXPECT_EQ(200, client.response_head()->headers->response_code());

  std::string foo_header_value;
  ASSERT_TRUE(client.response_head()->headers->GetNormalizedHeader(
      "foo", &foo_header_value));
  EXPECT_EQ("bar", foo_header_value);
}

// Make sure the factory info returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| works.
IN_PROC_BROWSER_TEST_F(StoragePartititionImplBrowsertest,
                       GetURLLoaderFactoryForBrowserProcessIOThread) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto shared_url_loader_factory_info =
      BrowserContext::GetDefaultStoragePartition(
          shell()->web_contents()->GetBrowserContext())
          ->GetURLLoaderFactoryForBrowserProcessIOThread();

  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      std::move(shared_url_loader_factory_info));

  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Make sure the factory info returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| doesn't
// crash if it's called after the StoragePartition is deleted.
IN_PROC_BROWSER_TEST_F(StoragePartititionImplBrowsertest,
                       BrowserIOFactoryInfoAfterStoragePartitionGone) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true);
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context.get());
  auto shared_url_loader_factory_info =
      partition->GetURLLoaderFactoryForBrowserProcessIOThread();

  browser_context.reset();

  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      std::move(shared_url_loader_factory_info));

  EXPECT_EQ(net::ERR_FAILED,
            factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Make sure the factory constructed from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| doesn't
// crash if it's called after the StoragePartition is deleted.
IN_PROC_BROWSER_TEST_F(StoragePartititionImplBrowsertest,
                       BrowserIOFactoryAfterStoragePartitionGone) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true);
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context.get());
  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      partition->GetURLLoaderFactoryForBrowserProcessIOThread());

  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));

  browser_context.reset();

  EXPECT_EQ(net::ERR_FAILED,
            factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Checks that the network::URLLoaderIntercpetor works as expected with the
// SharedURLLoaderFactory returned by StoragePartititionImpl.
IN_PROC_BROWSER_TEST_F(StoragePartititionImplBrowsertest,
                       URLLoaderInterceptor) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kEchoUrl(embedded_test_server()->GetURL("/echo"));

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true);
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context.get());

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

}  // namespace content
