// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/content_decoding_url_loader_throttle.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/content_decoding_interceptor.h"
#include "services/network/public/cpp/loading_params.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace blink {

ContentDecodingURLLoaderThrottle::ContentDecodingURLLoaderThrottle() = default;

ContentDecodingURLLoaderThrottle::~ContentDecodingURLLoaderThrottle() = default;

void ContentDecodingURLLoaderThrottle::DetachFromCurrentSequence() {}

// Called when a response is about to be processed. This is where the content
// decoding interception is performed.
void ContentDecodingURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  // If there are no decoding types specified, there's nothing to do.
  if (response_head->client_side_content_decoding_types.empty()) {
    return;
  }

  // Attempt to create the data pipe needed for content decoding.
  auto data_pipe_pair = network::ContentDecodingInterceptor::CreateDataPipePair(
      network::ContentDecodingInterceptor::ClientType::kURLLoaderThrottle);
  if (!data_pipe_pair) {
    // If pipe creation fails, cancel the request with an error.
    delegate_->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  // Intercept the response using ContentDecodingInterceptor. The provided
  // callback is used to swap the URLLoaderClientEndpoints and data pipe with
  // the interceptor's versions. This ensures that subsequent data flows through
  // the decoding pipeline.
  network::ContentDecodingInterceptor::Intercept(
      response_head->client_side_content_decoding_types,
      std::move(*data_pipe_pair),
      base::BindOnce(
          [](Delegate* delegate,
             network::mojom::URLLoaderClientEndpointsPtr& endpoints,
             mojo::ScopedDataPipeConsumerHandle& body) {
            mojo::PendingRemote<network::mojom::URLLoader> source_loader;
            mojo::PendingReceiver<network::mojom::URLLoaderClient>
                source_client_receiver;
            delegate->InterceptResponse(std::move(endpoints->url_loader),
                                        std::move(endpoints->url_loader_client),
                                        &source_loader, &source_client_receiver,
                                        &body);
            endpoints->url_loader = std::move(source_loader);
            endpoints->url_loader_client = std::move(source_client_receiver);
          },
          // Safe because the callback is called synchronously.
          base::Unretained(delegate_)),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}));
}

// Returns the name of this throttle for logging purposes.
const char*
ContentDecodingURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "ContentDecodingThrottle";
}
}  // namespace blink
