// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPECULATION_RULES_SPECULATION_HOST_IMPL_H_
#define CONTENT_BROWSER_SPECULATION_RULES_SPECULATION_HOST_IMPL_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/speculation_host_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
class RenderFrameHost;
class PrerenderHostRegistry;
class Page;

// Receiver for speculation rules from the web platform. See
// third_party/blink/renderer/core/speculation_rules/README.md
class CONTENT_EXPORT SpeculationHostImpl final
    : public content::DocumentService<blink::mojom::SpeculationHost>,
      public WebContentsObserver {
 public:
  // Creates and binds an instance of this per-frame.
  static void Bind(
      RenderFrameHost* frame_host,
      mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver);

  ~SpeculationHostImpl() override;

  SpeculationHostImpl(const SpeculationHostImpl&) = delete;
  SpeculationHostImpl& operator=(const SpeculationHostImpl&) = delete;
  SpeculationHostImpl(SpeculationHostImpl&&) = delete;
  SpeculationHostImpl& operator=(SpeculationHostImpl&&) = delete;

  // WebContentsObserver implementation:
  void PrimaryPageChanged(Page& page) override;

 private:
  SpeculationHostImpl(
      RenderFrameHost* frame_host,
      mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver);

  void UpdateSpeculationCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr> candidates) override;

  void ProcessCandidatesForPrerender(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  void CancelStartedPrerenders();

  std::unique_ptr<SpeculationHostDelegate> delegate_;

  // TODO(https://crbug.com/1197133): Record the prerendering URLs as well so
  // that this can cancel started prerenders when candidates are updated.
  base::flat_set<int> started_prerender_host_ids_;
  base::WeakPtr<PrerenderHostRegistry> registry_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPECULATION_RULES_SPECULATION_HOST_IMPL_H_
