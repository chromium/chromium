// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCHER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCHER_H_

#include "content/browser/preloading/speculation_host_devtools_observer.h"
#include "content/public/browser/speculation_host_delegate.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;
class PreloadingPredictor;

// Handles speculation-rules bases prefetches.
// TODO(isaboori  crbug.com/1384496): Currently Prefetcher class supports the
// integration of speculation rule based prefetches with the DevTools. It serves
// as an abstraction layer to avoid implementing the DevTools logic twice in
// SpeculationHostDelegate and PrefetchDocumentManager. After the refactoring of
// SpeculationHostDelegate, we can get rid of this layer and implement all the
// logic in a single class by merging existing Prefetcher and
// PrefetchDocumentManager into a single class. We should also rename
// SpeculationHostDevToolsObserver to PrefetcherDevToolsObserver and the class
// declaration should be moved to this header file.
class CONTENT_EXPORT Prefetcher : public SpeculationHostDevToolsObserver {
 public:
  Prefetcher() = delete;
  explicit Prefetcher(RenderFrameHost& render_frame_host);
  ~Prefetcher();

  // SpeculationHostDevToolsObserver implementation:
  void OnStartSinglePrefetch(
      const std::string& request_id,
      const network::ResourceRequest& request,
      std::optional<
          std::pair<const GURL&,
                    const network::mojom::URLResponseHeadDevToolsInfo&>>
          redirect_info) override;
  void OnPrefetchResponseReceived(
      const GURL& url,
      const std::string& request_id,
      const network::mojom::URLResponseHead& response) override;
  void OnPrefetchRequestComplete(
      const std::string& request_id,
      const network::URLLoaderCompletionStatus& status) override;
  void OnPrefetchBodyDataReceived(const std::string& request_id,
                                  const std::string& body,
                                  bool is_base64_encoded) override;
  mojo::PendingRemote<network::mojom::DevToolsObserver>
  MakeSelfOwnedNetworkServiceDevToolsObserver() override;

  RenderFrameHost& render_frame_host() const { return *render_frame_host_; }

  RenderFrameHostImpl* render_frame_host_impl() const {
    return render_frame_host_impl_;
  }

  void ProcessCandidatesForPrefetch(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  bool MaybePrefetch(blink::mojom::SpeculationCandidatePtr candidate,
                     const PreloadingPredictor& enacting_predictor);

  // Whether the prefetch attempt for target |url| failed or discarded.
  bool IsPrefetchAttemptFailedOrDiscarded(const GURL& url);

 private:
  // content::PreloadingDecider, which inherits content::DocumentUserData, owns
  // `this`, so `this` can access `render_frame_host_` safely.
  const raw_ref<content::RenderFrameHost> render_frame_host_;

  // content::PreloadingDecider, which inherits content::DocumentUserData, owns
  // `this`, so `this` can access `render_frame_host_impl_` safely.
  const raw_ptr<content::RenderFrameHostImpl> render_frame_host_impl_;

  std::unique_ptr<SpeculationHostDelegate> delegate_;

  base::WeakPtrFactory<Prefetcher> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCHER_H_
