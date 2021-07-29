// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/speculation_host_impl.h"

#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

bool CandidatesAreValid(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  for (const auto& candidate : candidates) {
    // These non-http candidates should be filtered out in Blink and
    // SpeculationHostImpl should not see them. If SpeculationHostImpl receives
    // non-http candidates, it may mean the renderer process has a bug
    // or is compromised.
    if (!candidate->url.SchemeIsHTTPOrHTTPS()) {
      mojo::ReportBadMessage("SH_NON_HTTP");
      return false;
    }
  }
  return true;
}

}  // namespace

// static
void SpeculationHostImpl::Bind(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver) {
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

  // DocumentServiceBase will destroy this on pipe closure or frame destruction.
  new SpeculationHostImpl(frame_host, std::move(receiver));
}

SpeculationHostImpl::SpeculationHostImpl(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver)
    : DocumentServiceBase(frame_host, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = GetContentClient()->browser()->CreateSpeculationHostDelegate(
      *render_frame_host());
  if (blink::features::IsPrerender2Enabled()) {
    auto* rfhi = static_cast<RenderFrameHostImpl*>(frame_host);
    registry_ = rfhi->delegate()->GetPrerenderHostRegistry()->GetWeakPtr();
  }
}

SpeculationHostImpl::~SpeculationHostImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CancelStartedPrerenders();
}

void SpeculationHostImpl::PrimaryPageChanged(Page& page) {
  // Listen to the change of the primary page. Since only the primary page can
  // trigger speculationrules, the change of the primary page indicates that the
  // trigger associated with this host is destroyed, so the browser should
  // cancel the prerenders that are initiated by it.
  // We cannot do it in the destructor only, because DocumentService can be
  // deleted asynchronously, but we want to make sure to cancel prerendering
  // before the next primary page swaps in so that the next page can trigger a
  // new prerender without hitting the max number of running prerenders.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CancelStartedPrerenders();
}

void SpeculationHostImpl::UpdateSpeculationCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!CandidatesAreValid(candidates))
    return;

  // Only handle messages from an active main frame.
  if (!render_frame_host()->IsActive())
    return;
  if (render_frame_host()->GetParent())
    return;

  // Let `delegate_` process the candidates that it is interested in.
  if (delegate_)
    delegate_->ProcessCandidates(candidates);

  ProcessCandidatesForPrerender(candidates);
}

void SpeculationHostImpl::ProcessCandidatesForPrerender(
    const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  if (!registry_ || candidates.empty())
    return;
  DCHECK(blink::features::IsPrerender2Enabled());
  WebContentsDelegate* web_contents_delegate =
      content::WebContents::FromRenderFrameHost(render_frame_host())
          ->GetDelegate();
  if (!web_contents_delegate ||
      !web_contents_delegate->IsPrerender2Supported()) {
    return;
  }

  auto* rfhi = static_cast<RenderFrameHostImpl*>(render_frame_host());
  for (const auto& it : candidates) {
    if (it->action != blink::mojom::SpeculationAction::kPrerender)
      continue;

    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfhi, blink::mojom::WebFeature::kSpeculationRulesPrerender);

    // TODO(crbug.com/1176054): Support cross-origin prerendering.
    // TODO(crbug.com/1197133): Record the cancellation reason of no
    // same-origin candidates via UMA.
    if (!origin().IsSameOriginWith(url::Origin::Create(it->url)))
      continue;

    // TODO(https://crbug.com/1217903): Set up `attributes->size`.
    auto attributes = blink::mojom::PrerenderAttributes::New();
    attributes->url = it->url;
    attributes->referrer = std::move(it->referrer);
    attributes->trigger_type =
        blink::mojom::PrerenderTriggerType::kSpeculationRule;
    int prerender_host_id =
        registry_->CreateAndStartHost(std::move(attributes), *rfhi);
    if (prerender_host_id != RenderFrameHost::kNoFrameTreeNodeId)
      started_prerender_host_ids_.insert(prerender_host_id);
  }
}

void SpeculationHostImpl::CancelStartedPrerenders() {
  if (registry_) {
    for (const auto id : started_prerender_host_ids_)
      registry_->OnTriggerDestroyed(id);
    started_prerender_host_ids_.clear();
  }
}

}  // namespace content
