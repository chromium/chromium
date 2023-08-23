// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_LOADER_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_LOADER_H_

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "content/common/content_export.h"
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
class CONTENT_EXPORT ServiceWorkerResourceLoader {
 public:
  // Indicates where the response comes from.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FetchResponseFrom {
    kNoResponseYet = 0,
    kServiceWorker = 1,
    kWithoutServiceWorker = 2,
    // For subresources, the redirect mode is "follow". When redirects happen,
    // the resource loader restarts the request process after FollowRedirect()
    // is called. This value indicates that intermediate state. This state has
    // to be updated to either |kServiceWorker| or |kWithoutServiceWorker| after
    // receiving the final response.
    kSubresourceLoaderIsHandlingRedirect = 3,
    // When ServiceWorkerAutoPreload is enabled, in most cases the response from
    // |kServiceWorker| is expected. However, when the fetch handler result is
    // fallback, the browser tries to use the response from the network request.
    // In this case |commit_responsibility_| is transitioned from
    // |kServiceWorker| to |kWithoutServiceWorker|, but we don't want to permit
    // that transition in normal cases. This state is a special intermediate
    // state to bridge those states, which is used only to handle fallback with
    // ServiceWorkerAutoPreload.
    kAutoPreloadHandlingFallback = 4,
    kMaxValue = kAutoPreloadHandlingFallback,
  };

  // Indicates what kind of preload request is dispatched before starting
  // the ServiceWorker.
  //
  // kNone: No preload request is triggered. This is the default state.
  // kRaceNetworkRequest:
  //    RaceNetworkRequest is triggered.
  //    TODO(crbug.com/1420517) This will be passed to the renderer and block
  //    the corresponding request from the ServiceWorker.
  // kNavigationPreload:
  //    Enabled when Navigation Preload is triggered.
  // kAutoPreload:
  //    AutoPreload is triggered. This is consumed in the fetch handler or
  //    the fallback request.
  enum class DispatchedPreloadType {
    kNone,
    kRaceNetworkRequest,
    kNavigationPreload,
    kAutoPreload,
  };

  ServiceWorkerResourceLoader();
  virtual ~ServiceWorkerResourceLoader();

  void RecordFetchResponseFrom();

  FetchResponseFrom commit_responsibility() { return commit_responsibility_; }
  void SetCommitResponsibility(FetchResponseFrom fetch_response_from);

  DispatchedPreloadType dispatched_preload_type() {
    return dispatched_preload_type_;
  }
  void SetDispatchedPreloadType(DispatchedPreloadType type);

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
  FetchResponseFrom commit_responsibility_ = FetchResponseFrom::kNoResponseYet;
  DispatchedPreloadType dispatched_preload_type_ = DispatchedPreloadType::kNone;
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_LOADER_H_
