// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/speculation_host_impl.h"

#include "content/browser/prerender/prerender_processor.h"
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

  if (!blink::features::IsPrerender2Enabled() || candidates.empty())
    return;

  // Limit the number of started prerenders to one. If
  // `prerender_processor_` is not null, it means `this` has started a
  // prerender, and should ignore other prerender candidates.
  // TODO(crbug.com/1197133): Cancel the started prerender and start a new
  // one if the score of the new candidate is higher than the started one's.
  // TODO(crbug.com/1197133): Record the cancellation reason via UMA.
  if (prerender_processor_) {
    return;
  }

  // Find the first prerender candidate, since we limit the number of started
  // prerenders to one.
  // TODO(crbug.com/1197133): Find the candidate with the highest score.
  // TODO(crbug.com/1176054): Support cross-origin prerendering.
  // TODO(crbug.com/1197133): Record the cancellation reason of no same-origin
  // candidates via UMA.
  const auto prerender_filter =
      [&](const blink::mojom::SpeculationCandidatePtr& it) {
        return it->action == blink::mojom::SpeculationAction::kPrerender &&
               origin().IsSameOriginWith(url::Origin::Create(it->url));
      };
  const auto candidate_it =
      std::find_if(candidates.begin(), candidates.end(), prerender_filter);
  if (candidate_it == candidates.end())
    return;

  auto* rfhi = static_cast<RenderFrameHostImpl*>(render_frame_host());
  prerender_processor_ = std::make_unique<PrerenderProcessor>(*rfhi);
  const blink::mojom::SpeculationCandidatePtr& candidate = *candidate_it;

  // TODO(https://crbug.com/1197133): Set up the field of size.
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = candidate->url;
  // TODO(https://crbug.com/1197133): Set up the referrer field appropriately.
  attributes->referrer = blink::mojom::Referrer::New();
  attributes->trigger_type =
      blink::mojom::PrerenderTriggerType::kSpeculationRule;

  prerender_processor_->Start(std::move(attributes));
}

}  // namespace content
