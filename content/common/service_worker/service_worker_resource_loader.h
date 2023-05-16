// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_LOADER_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_LOADER_H_

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
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
  // Indicates where the response comes from.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FetchResponseFrom {
    kNoResponseYet = 0,
    kServiceWorker = 1,
    kWithoutServiceWorker = 2,
    kMaxValue = kWithoutServiceWorker,
  };

  ServiceWorkerResourceLoader();
  virtual ~ServiceWorkerResourceLoader();

  FetchResponseFrom fetch_response_from() { return fetch_response_from_; }

  void SetFetchResponseFrom(FetchResponseFrom fetch_response_from);
  void reset_fetch_response_from() {
    fetch_response_from_ = FetchResponseFrom::kNoResponseYet;
  }

  // Tells if the class is main resource's class or not.
  virtual bool IsMainResourceLoader() = 0;

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

  // Calls url_loader_client_->OnReceiveRedirect().
  virtual void HandleRedirect(
      const net::RedirectInfo& redirect_info,
      const network::mojom::URLResponseHeadPtr& response_head) = 0;

 private:
  FetchResponseFrom fetch_response_from_ = FetchResponseFrom::kNoResponseYet;
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_LOADER_H_
