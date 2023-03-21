// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_test_utils.h"

#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
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
                 const network::mojom::URLResponseHead& response_head) {
                NOTREACHED();
                return PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect;
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
              [](base::RunLoop* on_receive_redirect_loop,
                 const net::RedirectInfo& redirect_info,
                 const network::mojom::URLResponseHead& response_head) {
                on_receive_redirect_loop->Quit();
                return PrefetchStreamingURLLoaderStatus::kFollowRedirect;
              },
              &on_receive_redirect_loop));

  network::URLLoaderCompletionStatus status(net::OK);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = redirect_url;

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, network::mojom::URLResponseHead::New());

  test_url_loader_factory.AddResponse(
      original_url, network::mojom::URLResponseHead::New(), "test body", status,
      std::move(redirects), network::TestURLLoaderFactory::kResponseDefault);
  on_receive_redirect_loop.Run();
  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  DCHECK(streaming_loader->Servable(base::TimeDelta::Max()));
  return streaming_loader;
}

}  // namespace content
