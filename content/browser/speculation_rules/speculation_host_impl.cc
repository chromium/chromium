// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/speculation_host_impl.h"

#include "base/ranges/algorithm.h"
#include "content/browser/prerender/prerender_attributes.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/speculation_rules/prefetch/prefetch_document_manager.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

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

struct SpeculationHostImpl::PrerenderInfo {
  GURL url;
  Referrer referrer;
  int prerender_host_id;
};

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

  // DocumentService will destroy this on pipe closure or frame destruction.
  new SpeculationHostImpl(frame_host, std::move(receiver));
}

SpeculationHostImpl::SpeculationHostImpl(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver)
    : DocumentService(frame_host, std::move(receiver)),
      WebContentsObserver(WebContents::FromRenderFrameHost(frame_host)) {
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

  if (base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor)) {
    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            render_frame_host());

    prefetch_document_manager->ProcessCandidates(candidates);
  }

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

  // TODO(https://crbug.com/1292422): Move this check into
  // PrerenderHostRegistry::CreateAndStartHost().
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host());
  WebContentsDelegate* web_contents_delegate = web_contents->GetDelegate();
  if (!web_contents_delegate ||
      !web_contents_delegate->IsPrerender2Supported(*web_contents)) {
    return;
  }

  auto* rfhi = static_cast<RenderFrameHostImpl*>(render_frame_host());
  for (const auto& it : candidates) {
    if (it->action != blink::mojom::SpeculationAction::kPrerender)
      continue;

    auto [begin, end] = base::ranges::equal_range(
        started_prerenders_.begin(), started_prerenders_.end(), it->url,
        std::less<>(), &PrerenderInfo::url);
    if (begin != end) {
      // A prerender with this URL was previously triggered.
      // At the moment there is no mechanism for cancelling these.
      continue;
    }

    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfhi, blink::mojom::WebFeature::kSpeculationRulesPrerender);

    // TODO(crbug.com/1176054): Remove it after supporting cross-origin
    // prerender.
    if (!rfhi->GetLastCommittedOrigin().IsSameOriginWith(it->url)) {
      rfhi->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          base::StringPrintf(
              "The SpeculationRules API does not support cross-origin "
              "prerender yet. (initiator origin: %s, prerender origin: %s). "
              "https://crbug.com/1176054 tracks cross-origin support.",
              rfhi->GetLastCommittedOrigin().Serialize().c_str(),
              url::Origin::Create(it->url).Serialize().c_str()));
    }

    Referrer referrer(*(it->referrer));
    int prerender_host_id = registry_->CreateAndStartHost(
        PrerenderAttributes(
            it->url, PrerenderTriggerType::kSpeculationRule,
            /*embedder_histogram_suffix=*/"", referrer,
            rfhi->GetLastCommittedOrigin(), rfhi->GetLastCommittedURL(),
            rfhi->GetProcess()->GetID(), rfhi->GetFrameToken(),
            rfhi->GetFrameTreeNodeId(), rfhi->GetPageUkmSourceId(),
            ui::PAGE_TRANSITION_LINK,
            /*url_match_predicate=*/absl::nullopt),
        *web_contents);
    started_prerenders_.insert(end, {.url = it->url,
                                     .referrer = referrer,
                                     .prerender_host_id = prerender_host_id});
  }
}

void SpeculationHostImpl::CancelStartedPrerenders() {
  if (registry_) {
    for (const auto& prerender : started_prerenders_) {
      int host_id = prerender.prerender_host_id;
      if (host_id != RenderFrameHost::kNoFrameTreeNodeId)
        registry_->OnTriggerDestroyed(host_id);
    }
    started_prerenders_.clear();
  }
}

}  // namespace content
