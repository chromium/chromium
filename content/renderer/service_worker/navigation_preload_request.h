// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_NAVIGATION_PRELOAD_REQUEST_H_
#define CONTENT_RENDERER_SERVICE_WORKER_NAVIGATION_PRELOAD_REQUEST_H_

#include <memory>

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"

namespace blink {
class WebServiceWorkerContextClient;
}  // namespace blink

namespace content {

// The URLLoaderClient for receiving a navigation preload response. It reports
// the response back to blink::WebServiceWorkerContextClient.
//
// This class lives on the service worker thread and is owned by
// blink::WebServiceWorkerContextClient.
class NavigationPreloadRequest final : public network::mojom::URLLoaderClient {
 public:
  // |owner| must outlive |this|.
  NavigationPreloadRequest(
      blink::WebServiceWorkerContextClient* owner,
      int fetch_event_id,
      const blink::WebURL& url,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          preload_url_loader_client_receiver);
  ~NavigationPreloadRequest() override;

  // network::mojom::URLLoaderClient:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  void MaybeReportResponseToOwner();
  void ReportErrorToOwner(const blink::WebString& message,
                          blink::WebServiceWorkerError::Mode error_mode);

  blink::WebServiceWorkerContextClient* owner_ = nullptr;

  const int fetch_event_id_ = -1;
  const blink::WebURL url_;
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_;

  std::unique_ptr<blink::WebURLResponse> response_;
  mojo::ScopedDataPipeConsumerHandle body_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_NAVIGATION_PRELOAD_REQUEST_H_
