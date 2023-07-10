// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTILS_H_

#include <memory>
#include <string>

#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

namespace base {
class RunLoop;
}  // namespace base

namespace content {

class PrefetchContainer;

void MakeServableStreamingURLLoaderForTest(
    PrefetchContainer* prefetch_container,
    network::mojom::URLResponseHeadPtr head,
    const std::string body);

PrefetchStreamingURLLoader::OnPrefetchRedirectCallback
CreatePrefetchRedirectCallbackForTest(
    base::RunLoop* on_receive_redirect_loop,
    net::RedirectInfo* out_redirect_info,
    network::mojom::URLResponseHeadPtr* out_redirect_head);

void MakeServableStreamingURLLoaderWithRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
    const GURL& redirect_url);

std::vector<base::WeakPtr<PrefetchStreamingURLLoader>>
MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
    const GURL& redirect_url);

class PrefetchTestURLLoaderClient : public network::mojom::URLLoaderClient,
                                    public mojo::DataPipeDrainer::Client {
 public:
  PrefetchTestURLLoaderClient();
  ~PrefetchTestURLLoaderClient() override;

  PrefetchTestURLLoaderClient(const PrefetchTestURLLoaderClient&) = delete;
  PrefetchTestURLLoaderClient& operator=(const PrefetchTestURLLoaderClient&) =
      delete;

  mojo::PendingReceiver<network::mojom::URLLoader>
  BindURLloaderAndGetReceiver();
  mojo::PendingRemote<network::mojom::URLLoaderClient>
  BindURLLoaderClientAndGetRemote();
  void DisconnectMojoPipes();

  std::string body_content() { return body_content_; }
  uint32_t total_bytes_read() { return total_bytes_read_; }
  bool body_finished() { return body_finished_; }
  int32_t total_transfer_size_diff() { return total_transfer_size_diff_; }
  absl::optional<network::URLLoaderCompletionStatus> completion_status() {
    return completion_status_;
  }
  const std::vector<
      std::pair<net::RedirectInfo, network::mojom::URLResponseHeadPtr>>&
  received_redirects() {
    return received_redirects_;
  }

 private:
  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override;

  void OnDataComplete() override;

  mojo::Remote<network::mojom::URLLoader> remote_;
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};

  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;

  std::string body_content_;
  uint32_t total_bytes_read_{0};
  bool body_finished_{false};
  int32_t total_transfer_size_diff_{0};

  absl::optional<network::URLLoaderCompletionStatus> completion_status_;

  std::vector<std::pair<net::RedirectInfo, network::mojom::URLResponseHeadPtr>>
      received_redirects_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTILS_H_
