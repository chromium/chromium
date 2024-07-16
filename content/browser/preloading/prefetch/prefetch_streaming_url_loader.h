// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace content {

class PrefetchResponseReader;

// `PrefetchStreamingURLLoader` is self-owned throughout its lifetime, and
// deleted asynchronously when `prefetch_url_loader_` is finished or canceled
// (e.g. on non-followed redirects or `CancelIfNotServing()`).
class CONTENT_EXPORT PrefetchStreamingURLLoader
    : public network::mojom::URLLoaderClient {
 public:
  static base::WeakPtr<PrefetchStreamingURLLoader> CreateAndStart(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const network::ResourceRequest& request,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      base::TimeDelta timeout_duration,
      OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
      OnPrefetchResponseCompletedCallback
          on_prefetch_response_completed_callback,
      OnPrefetchRedirectCallback on_prefetch_redirect_callback,
      base::OnceClosure on_determined_head_callback,
      base::WeakPtr<PrefetchResponseReader> response_reader);

  // Must be called only from `CreateAndStart()`.
  PrefetchStreamingURLLoader(
      OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
      OnPrefetchResponseCompletedCallback
          on_prefetch_response_completed_callback,
      OnPrefetchRedirectCallback on_prefetch_redirect_callback,
      base::OnceClosure on_determined_head_callback);

  ~PrefetchStreamingURLLoader() override;

  PrefetchStreamingURLLoader(const PrefetchStreamingURLLoader&) = delete;
  PrefetchStreamingURLLoader& operator=(const PrefetchStreamingURLLoader&) =
      delete;

  void SetResponseReader(base::WeakPtr<PrefetchResponseReader> response_reader);

  // Informs the URL loader of how to handle the most recent redirect. This
  // should only be called after |on_prefetch_redirect_callback_| is called. The
  // value of |new_status| should only be one of the following:
  // - |kFollowRedirect|, if the redirect should be followed by |this|.
  // - |kStopSwitchInNetworkContextForRedirect|, if the redirect will be
  //   followed by a different |PrefetchStreamingURLLoader| due to a change in
  //   network context.
  // - |kFailedInvalidRedirect|, if the redirect should not be followed by
  //   |this|.
  void HandleRedirect(PrefetchRedirectStatus redirect_status,
                      const net::RedirectInfo& redirect_info,
                      network::mojom::URLResponseHeadPtr redirect_head);

  // Called from PrefetchResponseReader.
  void SetPriority(net::RequestPriority priority, int32_t intra_priority_value);
  void PauseReadingBodyFromNet();
  void ResumeReadingBodyFromNet();

  base::WeakPtr<PrefetchStreamingURLLoader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnStartServing();

  // Cancels the prefetching and schedule deletion, if any of its corresponding
  // `PrefetchResponseReader` does NOT start serving. This can cancel the
  // prefetching prematurely and leave `this` and `PrefetchResponseReader`
  // stalled.
  // TODO(crbug.com/40064891): Consider cleaning up this behavior (== existing
  // behavior, previously as `ResetAllStreamingURLLoaders()`).
  void CancelIfNotServing();

  // Only for CHECK()ing.
  NOINLINE bool IsDeletionScheduledForCHECK() const;

  void SetOnDeletionScheduledForTests(
      base::OnceClosure on_deletion_scheduled_for_tests);

 private:
  void Start(network::mojom::URLLoaderFactory* url_loader_factory,
             const network::ResourceRequest& request,
             const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
             base::TimeDelta timeout_duration);

  // Disconnect prefetching URLLoader and schedule deletion of `this`.
  // Currently this itself doesn't mark `this` or corresponding
  // `PrefetchResponseReader` as failed.
  void DisconnectPrefetchURLLoaderMojo();

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(
      const network::URLLoaderCompletionStatus& completion_status) override;

  std::unique_ptr<PrefetchStreamingURLLoader> self_pointer_;

  // The timer that triggers a timeout when a request takes too long.
  base::OneShotTimer timeout_timer_;

  // Set if any of corresponding `PrefetchResponseReader` starts serving.
  bool used_for_serving_{false};

  // The URL loader used to request the prefetch.
  mojo::Remote<network::mojom::URLLoader> prefetch_url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient>
      prefetch_url_loader_client_receiver_{this};

  // Callbacks used to inform the caller of specific events of the prefetch
  // request.
  OnPrefetchResponseStartedCallback on_prefetch_response_started_callback_;
  OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback_;
  OnPrefetchRedirectCallback on_prefetch_redirect_callback_;

  // Called once non-redirect header is determined, i.e. successfully received
  // or fetch failed.
  base::OnceClosure on_determined_head_callback_;

  // Called when deletion is scheduled. Only for testing corner cases around
  // deletion.
  base::OnceClosure on_deletion_scheduled_for_tests_;

  base::WeakPtr<PrefetchResponseReader> response_reader_;

  base::WeakPtrFactory<PrefetchStreamingURLLoader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_H_
