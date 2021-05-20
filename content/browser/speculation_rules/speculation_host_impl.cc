// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/speculation_host_impl.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

// static
void SpeculationHostImpl::Bind(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver) {
  // Note: Currently SpeculationHostImpl doesn't trigger prerendering.
  // TODO(crbug.com/1197133): Support prerendering.
  // TODO(crbug.com/1190338): Allow SpeculationHostDelegate to participate in
  // this feature check.
  if (!base::FeatureList::IsEnabled(
          blink::features::kSpeculationRulesPrefetchProxy) &&
      !blink::features::IsPrerender2Enabled()) {
    mojo::ReportBadMessage(
        "Speculation rules must be enabled to bind to "
        "blink.mojom.SpeculationHost in the browser.");
    return;
  }

  // FrameServiceBase will destroy this on pipe closure or frame destruction.
  new SpeculationHostImpl(frame_host, std::move(receiver));
}

SpeculationHostImpl::SpeculationHostImpl(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver)
    : FrameServiceBase(frame_host, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_ = GetContentClient()->browser()->CreateSpeculationHostDelegate(
      *render_frame_host());
}

SpeculationHostImpl::~SpeculationHostImpl() = default;

void SpeculationHostImpl::UpdateSpeculationCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only handle messages from the main frame of the primary frame tree.
  if (!render_frame_host()->IsCurrent())
    return;
  if (render_frame_host()->GetParent())
    return;

  // Let `delegate_` process the candidates that it is interested in.
  if (delegate_)
    delegate_->ProcessCandidates(candidates);

  // TODO(crbug.com/1197133): process prerender candidates.
}

}  // namespace content
