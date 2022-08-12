// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_HOST_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_HOST_IMPL_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/speculation_host_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
class PrerenderHostRegistry;
class Page;

// Receiver for speculation rules from the web platform. See
// third_party/blink/renderer/core/speculation_rules/README.md
class CONTENT_EXPORT SpeculationHostImpl final
    : public content::DocumentService<blink::mojom::SpeculationHost>,
      public WebContentsObserver,
      public SpeculationHostDevToolsObserver {
 public:
  // Creates and binds an instance of this per-frame.
  static void Bind(
      RenderFrameHost* frame_host,
      mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver);

  SpeculationHostImpl(const SpeculationHostImpl&) = delete;
  SpeculationHostImpl& operator=(const SpeculationHostImpl&) = delete;
  SpeculationHostImpl(SpeculationHostImpl&&) = delete;
  SpeculationHostImpl& operator=(SpeculationHostImpl&&) = delete;

  // WebContentsObserver implementation:
  void PrimaryPageChanged(Page& page) override;

  // SpeculationHostDevToolsObserver implementation:
  void OnStartSinglePrefetch(const std::string& request_id,
                             const network::ResourceRequest& request) override;
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

 private:
  SpeculationHostImpl(
      RenderFrameHost& frame_host,
      mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver);
  ~SpeculationHostImpl() override;

  void UpdateSpeculationCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr> candidates) override;

  void ProcessCandidatesForPrerender(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  void CancelStartedPrerenders();

  std::unique_ptr<SpeculationHostDelegate> delegate_;

  // TODO(https://crbug.com/1197133): Cancel started prerenders when candidates
  // are updated.
  // This is kept sorted by URL.
  struct PrerenderInfo;
  std::vector<PrerenderInfo> started_prerenders_;

  base::WeakPtr<PrerenderHostRegistry> registry_;

  base::WeakPtrFactory<SpeculationHostImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_HOST_IMPL_H_
