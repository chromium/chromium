// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_test_utils.h"

#include "base/run_loop.h"
#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

namespace content {

std::unique_ptr<PrefetchStreamingURLLoader>
MakeServableStreamingURLLoaderForTest(network::mojom::URLResponseHeadPtr head,
                                      const std::string body) {
  const GURL kTestUrl = GURL("https://test.com");

  network::TestURLLoaderFactory test_url_loader_factory;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = kTestUrl;
  request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          &test_url_loader_factory, std::move(request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 network::mojom::URLResponseHeadPtr response_head) {
                NOTREACHED();
              }));

  network::URLLoaderCompletionStatus status(net::OK);

  test_url_loader_factory.AddResponse(
      kTestUrl, std::move(head), body, status,
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kResponseDefault);
  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  DCHECK(streaming_loader->Servable(base::TimeDelta::Max()));
  return streaming_loader;
}

PrefetchStreamingURLLoader::OnPrefetchRedirectCallback
CreatePrefetchRedirectCallbackForTest(
    base::RunLoop* on_receive_redirect_loop,
    net::RedirectInfo* out_redirect_info,
    network::mojom::URLResponseHeadPtr* out_redirect_head) {
  return base::BindRepeating(
      [](base::RunLoop* on_receive_redirect_loop,
         net::RedirectInfo* out_redirect_info,
         network::mojom::URLResponseHeadPtr* out_redirect_head,
         const net::RedirectInfo& redirect_info,
         network::mojom::URLResponseHeadPtr redirect_head) {
        *out_redirect_info = redirect_info;
        *out_redirect_head = std::move(redirect_head);
        on_receive_redirect_loop->Quit();
      },
      on_receive_redirect_loop, out_redirect_info, out_redirect_head);
}

std::unique_ptr<PrefetchStreamingURLLoader>
MakeServableStreamingURLLoaderWithRedirectForTest(const GURL& original_url,
                                                  const GURL& redirect_url) {
  network::TestURLLoaderFactory test_url_loader_factory;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = original_url;
  request->method = "GET";

  base::RunLoop on_receive_redirect_loop;
  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHeadPtr redirect_head;

  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          &test_url_loader_factory, std::move(request),
          TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::
                    kHeadReceivedWaitingOnBody;
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          CreatePrefetchRedirectCallbackForTest(
              &on_receive_redirect_loop, &redirect_info, &redirect_head));

  network::URLLoaderCompletionStatus status(net::OK);

  net::RedirectInfo original_redirect_info;
  original_redirect_info.new_url = redirect_url;

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(original_redirect_info,
                         network::mojom::URLResponseHead::New());

  test_url_loader_factory.AddResponse(
      original_url, network::mojom::URLResponseHead::New(), "test body", status,
      std::move(redirects), network::TestURLLoaderFactory::kResponseDefault);
  on_receive_redirect_loop.Run();
  streaming_loader->HandleRedirect(
      PrefetchStreamingURLLoaderStatus::kFollowRedirect, redirect_info,
      std::move(redirect_head));
  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  DCHECK(streaming_loader->Servable(base::TimeDelta::Max()));
  return streaming_loader;
}

std::vector<std::unique_ptr<PrefetchStreamingURLLoader>>
MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
    const GURL& original_url,
    const GURL& redirect_url) {
  network::TestURLLoaderFactory test_url_loader_factory;
  std::vector<std::unique_ptr<PrefetchStreamingURLLoader>> streaming_loaders;

  std::unique_ptr<network::ResourceRequest> original_request =
      std::make_unique<network::ResourceRequest>();
  original_request->url = original_url;
  original_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHeadPtr redirect_head;

  // Simulate a PrefetchStreamingURLLoader that receives a redirect that
  // requires a change in a network context. When this happens, it will stop its
  // request, but can be used to serve the redirect. A new
  // PrefetchStreamingURLLoader will be started with a request to the redirect
  // URL.
  streaming_loaders.emplace_back(std::make_unique<PrefetchStreamingURLLoader>(
      &test_url_loader_factory, std::move(original_request),
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead* head) {
        NOTREACHED();
        return PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody;
      }),
      base::BindOnce(
          [](const network::URLLoaderCompletionStatus& completion_status) {
            NOTREACHED();
          }),
      CreatePrefetchRedirectCallbackForTest(&on_receive_redirect_loop,
                                            &redirect_info, &redirect_head)));

  net::RedirectInfo original_redirect_info;
  original_redirect_info.new_url = redirect_url;

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(original_redirect_info,
                         network::mojom::URLResponseHead::New());

  test_url_loader_factory.AddResponse(
      original_url, nullptr, "", network::URLLoaderCompletionStatus(),
      std::move(redirects),
      network::TestURLLoaderFactory::kResponseOnlyRedirectsNoDestination);
  on_receive_redirect_loop.Run();
  streaming_loaders[0]->HandleRedirect(
      PrefetchStreamingURLLoaderStatus::kStopSwitchInNetworkContextForRedirect,
      redirect_info, std::move(redirect_head));

  std::unique_ptr<network::ResourceRequest> redirect_request =
      std::make_unique<network::ResourceRequest>();
  redirect_request->url = redirect_url;
  redirect_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  // Starts the followup PrefetchStreamingURLLoader.
  streaming_loaders.emplace_back(std::make_unique<PrefetchStreamingURLLoader>(
      &test_url_loader_factory, std::move(redirect_request),
      TRAFFIC_ANNOTATION_FOR_TESTS, /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody;
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED();
      })));

  network::URLLoaderCompletionStatus status(net::OK);
  test_url_loader_factory.AddResponse(
      redirect_url, network::mojom::URLResponseHead::New(), "test body", status,
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kResponseDefault);

  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  DCHECK(streaming_loaders[1]->Servable(base::TimeDelta::Max()));
  return streaming_loaders;
}

}  // namespace content
