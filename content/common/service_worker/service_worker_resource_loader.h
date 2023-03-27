// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_LOADER_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_LOADER_H_

#include "base/check_op.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {
// A common interface in between:
// - ServiceWorkerMainResourceLoader in the browser
// - ServiceWorkerSubresourceLoader in the renderer
//
// Represents how to commit a response being fetch from ServiceWorker.
//
// To implement feature RaceNetworkRequest (crbug.com/1420517), we store into
// this common class whether the response came from the ServiceWorker fetch
// handler or from a direct network request.
class ServiceWorkerResourceLoader {
 public:
  enum class FetchResponseFrom {
    kNoResponseYet,
    kServiceWorker,
    kWithoutServiceWorker,
  };

  ServiceWorkerResourceLoader();
  virtual ~ServiceWorkerResourceLoader();

  FetchResponseFrom fetch_response_from() { return fetch_response_from_; }

  void set_fetch_response_from(FetchResponseFrom fetch_response_from) {
    DCHECK_EQ(fetch_response_from_, FetchResponseFrom::kNoResponseYet);
    fetch_response_from_ = fetch_response_from;
  }

  void reset_fetch_response_from() {
    fetch_response_from_ = FetchResponseFrom::kNoResponseYet;
  }

  // Calls url_loader_client_->OnReceiveResponse() with given |response_head|.
  virtual void CommitResponseHeaders(
      const network::mojom::URLResponseHeadPtr& response_head) = 0;

  // Calls url_loader_client_->OnReceiveResponse() with |response_body| and
  // |cached_metadata|.
  virtual void CommitResponseBody(
      const network::mojom::URLResponseHeadPtr& response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) = 0;

  // Creates and sends an empty response's body with the net::OK status.
  // Sends net::ERR_INSUFFICIENT_RESOURCES when it can't be created.
  virtual void CommitEmptyResponseAndComplete() = 0;

  // Calls url_loader_client_->OnComplete(). |reason| will be recorded as an
  // argument of TRACE_EVENT.
  virtual void CommitCompleted(int error_code, const char* reason) = 0;

 private:
  FetchResponseFrom fetch_response_from_ = FetchResponseFrom::kNoResponseYet;
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_LOADER_H_
