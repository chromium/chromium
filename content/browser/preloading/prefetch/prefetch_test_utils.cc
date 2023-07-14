// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_test_utils.h"

#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace content {

void MakeServableStreamingURLLoaderForTest(
    PrefetchContainer* prefetch_container,
    network::mojom::URLResponseHeadPtr head,
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
              }),
          base::BindOnce(&PrefetchContainer::OnReceivedHead,
                         prefetch_container->GetWeakPtr()),
          prefetch_container->GetResponseReaderForCurrentPrefetch());

  auto weak_streaming_loader = streaming_loader->GetWeakPtr();
  prefetch_container->TakeStreamingURLLoader(std::move(streaming_loader));

  network::URLLoaderCompletionStatus status(net::OK);

  test_url_loader_factory.AddResponse(
      kTestUrl, std::move(head), body, status,
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kResponseDefault);
  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  DCHECK(weak_streaming_loader);
  DCHECK(weak_streaming_loader->Servable(base::TimeDelta::Max()));
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

void MakeServableStreamingURLLoaderWithRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
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
          CreatePrefetchRedirectCallbackForTest(&on_receive_redirect_loop,
                                                &redirect_info, &redirect_head),
          base::BindOnce(&PrefetchContainer::OnReceivedHead,
                         prefetch_container->GetWeakPtr()),
          prefetch_container->GetResponseReaderForCurrentPrefetch());

  auto weak_streaming_loader = streaming_loader->GetWeakPtr();
  prefetch_container->TakeStreamingURLLoader(std::move(streaming_loader));

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

  prefetch_container->AddRedirectHop(redirect_url);

  DCHECK(weak_streaming_loader);
  weak_streaming_loader->HandleRedirect(
      PrefetchStreamingURLLoaderStatus::kFollowRedirect, redirect_info,
      std::move(redirect_head));

  // GetResponseReaderForCurrentPrefetch() now points to a new ResponseReader
  // after `AddRedirectHop()` above.
  DCHECK(weak_streaming_loader);
  weak_streaming_loader->SetResponseReader(
      prefetch_container->GetResponseReaderForCurrentPrefetch());

  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  DCHECK(weak_streaming_loader);
  DCHECK(weak_streaming_loader->Servable(base::TimeDelta::Max()));
}

std::vector<base::WeakPtr<PrefetchStreamingURLLoader>>
MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
    const GURL& redirect_url) {
  network::TestURLLoaderFactory test_url_loader_factory;
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
  auto first_streaming_loader = std::make_unique<PrefetchStreamingURLLoader>(
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
                                            &redirect_info, &redirect_head),
      base::BindOnce(&PrefetchContainer::OnReceivedHead,
                     prefetch_container->GetWeakPtr()),
      prefetch_container->GetResponseReaderForCurrentPrefetch());

  auto weak_first_streaming_loader = first_streaming_loader->GetWeakPtr();
  prefetch_container->TakeStreamingURLLoader(std::move(first_streaming_loader));

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

  prefetch_container->AddRedirectHop(redirect_url);

  DCHECK(weak_first_streaming_loader);
  weak_first_streaming_loader->HandleRedirect(
      PrefetchStreamingURLLoaderStatus::kStopSwitchInNetworkContextForRedirect,
      redirect_info, std::move(redirect_head));

  std::unique_ptr<network::ResourceRequest> redirect_request =
      std::make_unique<network::ResourceRequest>();
  redirect_request->url = redirect_url;
  redirect_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  // Starts the followup PrefetchStreamingURLLoader.
  // GetResponseReaderForCurrentPrefetch() now points to a new ResponseReader
  // after `AddRedirectHop()` above.
  auto second_streaming_loader = std::make_unique<PrefetchStreamingURLLoader>(
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
      }),
      base::BindOnce(&PrefetchContainer::OnReceivedHead,
                     prefetch_container->GetWeakPtr()),
      prefetch_container->GetResponseReaderForCurrentPrefetch());

  auto weak_second_streaming_loader = second_streaming_loader->GetWeakPtr();
  prefetch_container->TakeStreamingURLLoader(
      std::move(second_streaming_loader));

  network::URLLoaderCompletionStatus status(net::OK);
  test_url_loader_factory.AddResponse(
      redirect_url, network::mojom::URLResponseHead::New(), "test body", status,
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kResponseDefault);

  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  DCHECK(weak_second_streaming_loader);
  DCHECK(weak_second_streaming_loader->Servable(base::TimeDelta::Max()));

  return {weak_first_streaming_loader, weak_second_streaming_loader};
}

PrefetchTestURLLoaderClient::PrefetchTestURLLoaderClient() = default;
PrefetchTestURLLoaderClient::~PrefetchTestURLLoaderClient() = default;

mojo::PendingReceiver<network::mojom::URLLoader>
PrefetchTestURLLoaderClient::BindURLloaderAndGetReceiver() {
  return remote_.BindNewPipeAndPassReceiver();
}

mojo::PendingRemote<network::mojom::URLLoaderClient>
PrefetchTestURLLoaderClient::BindURLLoaderClientAndGetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void PrefetchTestURLLoaderClient::DisconnectMojoPipes() {
  remote_.reset();
  receiver_.reset();
}

void PrefetchTestURLLoaderClient::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  NOTREACHED();
}

void PrefetchTestURLLoaderClient::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK(!cached_metadata);

  // Drains |body| into |body_content_|
  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
}

void PrefetchTestURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  received_redirects_.emplace_back(redirect_info, std::move(head));
}

void PrefetchTestURLLoaderClient::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  NOTREACHED();
}

void PrefetchTestURLLoaderClient::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  total_transfer_size_diff_ += transfer_size_diff;
}

void PrefetchTestURLLoaderClient::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  completion_status_ = status;
}

void PrefetchTestURLLoaderClient::OnDataAvailable(const void* data,
                                                  size_t num_bytes) {
  body_content_.append(std::string(static_cast<const char*>(data), num_bytes));
  total_bytes_read_ += num_bytes;
}

void PrefetchTestURLLoaderClient::OnDataComplete() {
  body_finished_ = true;
}

}  // namespace content
