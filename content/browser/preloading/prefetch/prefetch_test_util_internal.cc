// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// Make some broadly reasonable redirect info.
net::RedirectInfo SyntheticRedirect(const GURL& new_url) {
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = new_url;
  redirect_info.new_site_for_cookies = net::SiteForCookies::FromUrl(new_url);
  redirect_info.new_referrer = std::string();
  return redirect_info;
}

}  // namespace

std::ostream& operator<<(std::ostream& ostream, PrefetchReusableForTests v) {
  switch (v) {
    case PrefetchReusableForTests::kDisabled:
      return ostream << "AllowMultipleUses Disabled";
    case PrefetchReusableForTests::kEnabled:
      return ostream << "AllowMultipleUses Enabled";
  }
}

std::vector<PrefetchReusableForTests> PrefetchReusableValuesForTests() {
  return std::vector<PrefetchReusableForTests>{
      PrefetchReusableForTests::kDisabled, PrefetchReusableForTests::kEnabled};
}

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

  base::WeakPtr<PrefetchResponseReader> weak_response_reader =
      prefetch_container->GetResponseReaderForCurrentPrefetch();
  auto weak_streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      &test_url_loader_factory, *request, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
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
        NOTREACHED_IN_MIGRATION();
      }),
      UseNewWaitLoop() ? base::BindOnce(&PrefetchContainer::OnDeterminedHead2,
                                        prefetch_container->GetWeakPtr())
                       : base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                                        prefetch_container->GetWeakPtr()),
      weak_response_reader);

  prefetch_container->SetStreamingURLLoader(weak_streaming_loader);

  network::URLLoaderCompletionStatus status(net::OK);

  test_url_loader_factory.AddResponse(
      kTestUrl, std::move(head), body, status,
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kResponseDefault);
  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  CHECK(weak_streaming_loader);
  CHECK(weak_response_reader);
  CHECK(weak_response_reader->Servable(base::TimeDelta::Max()));
}

network::TestURLLoaderFactory::PendingRequest
MakeManuallyServableStreamingURLLoaderForTest(
    PrefetchContainer* prefetch_container) {
  const GURL kTestUrl = GURL("https://test.com");

  network::TestURLLoaderFactory test_url_loader_factory;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = kTestUrl;
  request->method = "GET";

  auto weak_streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      &test_url_loader_factory, *request, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead* head) {
        return std::optional<PrefetchErrorOnResponseReceived>();
      }),
      base::BindOnce(&PrefetchContainer::OnPrefetchComplete,
                     prefetch_container->GetWeakPtr()),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED_IN_MIGRATION();
      }),
      UseNewWaitLoop() ? base::BindOnce(&PrefetchContainer::OnDeterminedHead2,
                                        prefetch_container->GetWeakPtr())
                       : base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                                        prefetch_container->GetWeakPtr()),
      prefetch_container->GetResponseReaderForCurrentPrefetch());

  prefetch_container->SetStreamingURLLoader(weak_streaming_loader);

  CHECK_EQ(test_url_loader_factory.pending_requests()->size(), 1u);
  return std::move(test_url_loader_factory.pending_requests()->at(0));
}

OnPrefetchRedirectCallback CreatePrefetchRedirectCallbackForTest(
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

  auto weak_first_response_reader =
      prefetch_container->GetResponseReaderForCurrentPrefetch();
  auto weak_streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      &test_url_loader_factory, *request, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
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
      UseNewWaitLoop() ? base::BindOnce(&PrefetchContainer::OnDeterminedHead2,
                                        prefetch_container->GetWeakPtr())
                       : base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                                        prefetch_container->GetWeakPtr()),
      weak_first_response_reader);

  prefetch_container->SetStreamingURLLoader(weak_streaming_loader);

  network::URLLoaderCompletionStatus status(net::OK);

  net::RedirectInfo original_redirect_info = SyntheticRedirect(redirect_url);

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(original_redirect_info,
                         network::mojom::URLResponseHead::New());

  test_url_loader_factory.AddResponse(
      original_url, network::mojom::URLResponseHead::New(), "test body", status,
      std::move(redirects), network::TestURLLoaderFactory::kResponseDefault);
  on_receive_redirect_loop.Run();

  prefetch_container->AddRedirectHop(redirect_info);

  CHECK(weak_streaming_loader);
  weak_streaming_loader->HandleRedirect(
      PrefetchRedirectStatus::kFollow, redirect_info, std::move(redirect_head));

  // GetResponseReaderForCurrentPrefetch() now points to a new ResponseReader
  // after `AddRedirectHop()` above.
  CHECK(weak_streaming_loader);
  auto weak_second_response_reader =
      prefetch_container->GetResponseReaderForCurrentPrefetch();
  weak_streaming_loader->SetResponseReader(weak_second_response_reader);

  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  CHECK(weak_streaming_loader);
  CHECK(weak_first_response_reader);
  CHECK(weak_second_response_reader);
  CHECK(weak_second_response_reader->Servable(base::TimeDelta::Max()));
}

void MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
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
  auto weak_first_streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      &test_url_loader_factory, *original_request, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead* head) {
        NOTREACHED_IN_MIGRATION();
        return std::optional<PrefetchErrorOnResponseReceived>();
      }),
      base::BindOnce(
          [](const network::URLLoaderCompletionStatus& completion_status) {
            NOTREACHED_IN_MIGRATION();
          }),
      CreatePrefetchRedirectCallbackForTest(&on_receive_redirect_loop,
                                            &redirect_info, &redirect_head),
      UseNewWaitLoop() ? base::BindOnce(&PrefetchContainer::OnDeterminedHead2,
                                        prefetch_container->GetWeakPtr())
                       : base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                                        prefetch_container->GetWeakPtr()),
      prefetch_container->GetResponseReaderForCurrentPrefetch());

  prefetch_container->SetStreamingURLLoader(weak_first_streaming_loader);

  net::RedirectInfo original_redirect_info = SyntheticRedirect(redirect_url);

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(original_redirect_info,
                         network::mojom::URLResponseHead::New());

  test_url_loader_factory.AddResponse(
      original_url, nullptr, "", network::URLLoaderCompletionStatus(),
      std::move(redirects),
      network::TestURLLoaderFactory::kResponseOnlyRedirectsNoDestination);
  on_receive_redirect_loop.Run();

  prefetch_container->AddRedirectHop(redirect_info);

  CHECK(weak_first_streaming_loader);
  weak_first_streaming_loader->HandleRedirect(
      PrefetchRedirectStatus::kSwitchNetworkContext, redirect_info,
      std::move(redirect_head));

  std::unique_ptr<network::ResourceRequest> redirect_request =
      std::make_unique<network::ResourceRequest>();
  redirect_request->url = redirect_url;
  redirect_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  // Starts the followup PrefetchStreamingURLLoader.
  // GetResponseReaderForCurrentPrefetch() now points to a new ResponseReader
  // after `AddRedirectHop()` above.
  base::WeakPtr<PrefetchResponseReader> weak_second_response_reader =
      prefetch_container->GetResponseReaderForCurrentPrefetch();
  auto weak_second_streaming_loader =
      PrefetchStreamingURLLoader::CreateAndStart(
          &test_url_loader_factory, *redirect_request,
          TRAFFIC_ANNOTATION_FOR_TESTS,
          /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return std::optional<PrefetchErrorOnResponseReceived>();
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
                NOTREACHED_IN_MIGRATION();
              }),
          UseNewWaitLoop()
              ? base::BindOnce(&PrefetchContainer::OnDeterminedHead2,
                               prefetch_container->GetWeakPtr())
              : base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                               prefetch_container->GetWeakPtr()),
          weak_second_response_reader);

  prefetch_container->SetStreamingURLLoader(weak_second_streaming_loader);

  network::URLLoaderCompletionStatus status(net::OK);
  test_url_loader_factory.AddResponse(
      redirect_url, network::mojom::URLResponseHead::New(), "test body", status,
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kResponseDefault);

  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  // `weak_first_streaming_loader` should be deleted after
  // `HandleRedirect(kSwitchNetworkContext)`.
  CHECK(!weak_first_streaming_loader);

  CHECK(weak_second_streaming_loader);
  CHECK(weak_second_response_reader);
  CHECK(weak_second_response_reader->Servable(base::TimeDelta::Max()));
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
  NOTREACHED_IN_MIGRATION();
}

void PrefetchTestURLLoaderClient::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  CHECK(!cached_metadata);
  CHECK(!body_);
  body_ = std::move(body);

  if (auto_draining_) {
    StartDraining();
  }
}

void PrefetchTestURLLoaderClient::StartDraining() {
  // Drains |body_| into |body_content_|
  CHECK(body_);
  CHECK(!pipe_drainer_);
  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body_));
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
  NOTREACHED_IN_MIGRATION();
}

void PrefetchTestURLLoaderClient::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  total_transfer_size_diff_ += transfer_size_diff;
}

void PrefetchTestURLLoaderClient::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  completion_status_ = status;
}

void PrefetchTestURLLoaderClient::OnDataAvailable(
    base::span<const uint8_t> data) {
  body_content_.append(base::as_string_view(data));
  total_bytes_read_ += data.size();
}

void PrefetchTestURLLoaderClient::OnDataComplete() {
  body_finished_ = true;
}

}  // namespace content
